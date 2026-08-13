#include "tyy_ffplay_core.h"
#include "tyy_log.h"

using namespace TyyPlayer;

AVPacket TyyPlayer::flush_pkt;

namespace TyyPlayer {

PacketQueue::PacketQueue() {
	memset_pkt_queue();
}

PacketQueue::~PacketQueue() {

}

void PacketQueue::memset_pkt_queue() {
	_first_pkt = _last_pkt = NULL;
	_nb_packets = _size = 0;
	_duration = 0;
	_abort_request = 0;
	_serial = 0;
	_mutex = NULL;
	_cond = NULL;
}

int PacketQueue::packet_queue_init() {

	memset_pkt_queue();

	_mutex = SDL_CreateMutex();
	if (!_mutex) {
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateMutex(): %s\n", SDL_GetError());
		return AVERROR(ENOMEM);
	}
	_cond = SDL_CreateCond();
	if (!_cond) {
		av_log(NULL, AV_LOG_FATAL, "SDL_CreateCond(): %s\n", SDL_GetError());
		return AVERROR(ENOMEM);
	}
	_abort_request = 1;

	return 0;
}

void PacketQueue::packet_queue_flush()
{
	MyAVPacketList *pkt, *pkt1;

	if (!_mutex) {
		return;
	}
	SDL_LockMutex(_mutex);

	for (pkt = _first_pkt; pkt; pkt = pkt1) {
		pkt1 = pkt->next;
		av_packet_unref(&pkt->pkt);
		av_freep(&pkt);
	}
	_last_pkt = NULL;
	_first_pkt = NULL;
	_nb_packets = 0;
	_size = 0;
	_duration = 0;

	SDL_UnlockMutex(_mutex);
}

void PacketQueue::packet_queue_destroy()
{
	packet_queue_flush();
	if (_mutex) {
		SDL_DestroyMutex(_mutex);
		_mutex = NULL;
	}
	if (_mutex) {
		SDL_DestroyCond(_cond);
		_cond = NULL;
	}

}

int PacketQueue::packet_queue_put_private(AVPacket *pkt)
{
	MyAVPacketList *pkt1;

	if (_abort_request)
		return -1;

	pkt1 = (MyAVPacketList*)av_malloc(sizeof(MyAVPacketList));
	if (!pkt1)
		return -1;

	pkt1->pkt = *pkt;
	pkt1->next = NULL;
	if (pkt == &flush_pkt)
	{
		_serial++;
	}
	pkt1->serial = _serial;

	if (!_last_pkt)
		_first_pkt = pkt1;
	else
		_last_pkt->next = pkt1;
	_last_pkt = pkt1;

	_nb_packets++;
	TYYINFO("_nb_packets: {}", _nb_packets);
	_size += pkt1->pkt.size + sizeof(*pkt1);
	_duration += pkt1->pkt.duration;

	SDL_CondSignal(_cond);

	return 0;
}

int PacketQueue::packet_queue_put(AVPacket *pkt)
{
	int ret;

	SDL_LockMutex(_mutex);
	ret = packet_queue_put_private(pkt);
	SDL_UnlockMutex(_mutex);

	if (pkt != &flush_pkt && ret < 0)
		av_packet_unref(pkt);

	return ret;
}

int PacketQueue::packet_queue_put_nullpacket(int stream_index)
{
	AVPacket pkt1, *pkt = &pkt1;
	av_init_packet(pkt);
	pkt->data = NULL;
	pkt->size = 0;
	pkt->stream_index = stream_index;
	return packet_queue_put(pkt);
}

void PacketQueue::packet_queue_abort()
{
	SDL_LockMutex(_mutex);

	_abort_request = 1;

	SDL_CondSignal(_cond);

	SDL_UnlockMutex(_mutex);
}

int PacketQueue::packet_queue_get(AVPacket *pkt, int block, int *serial)
{
	MyAVPacketList *pkt1;
	int ret;

	SDL_LockMutex(_mutex);

	for (;;) {

		if (_abort_request) {
			ret = -1;
			break;
		}

		pkt1 = _first_pkt;
		if (pkt1) {
			_first_pkt = pkt1->next;
			if (!_first_pkt)
				_last_pkt = NULL;

			_nb_packets--;
			_size -= pkt1->pkt.size + sizeof(*pkt1);
			_duration -= pkt1->pkt.duration;

			*pkt = pkt1->pkt;
			if (serial)
				*serial = pkt1->serial;

			av_free(pkt1);
			ret = 1;

			break;
		}
		else if (!block) {
			ret = 0;
			break;
		}
		else {

			SDL_CondWait(_cond, _mutex);
		}

	}

	SDL_UnlockMutex(_mutex);

	return ret;
}

void PacketQueue::packet_queue_start()
{
	SDL_LockMutex(_mutex);
	_abort_request = 0;
	packet_queue_put_private(&flush_pkt);
	SDL_UnlockMutex(_mutex);
}

PacketQueue::pktStatus PacketQueue::packet_queue_get_status() {

	PacketQueue::pktStatus status = { 0 };

	SDL_LockMutex(_mutex);
	status.nbPackets = _nb_packets;
	status.size = _size;
	status.duration = _duration;
	SDL_UnlockMutex(_mutex);

	return status;
}

Clock::Clock() {

}

Clock::~Clock() {

}

void Clock::init_clock(int *queue_serial)
{
	_speed = 1.0;
	_paused = 0;
	_queue_serial = queue_serial;

	set_clock(NAN, -1);
}

double Clock::get_clock()
{
	if (*_queue_serial != _serial)
		return NAN;
	if (_paused) {
		return _pts;
	}
	else {

		double time = av_gettime_relative() / 1000000.0;

		return _pts_drift + time - (time - _last_updated) * (1.0 - _speed);
	}
}

void Clock::set_clock_at(double pts, int serial, double time)
{
	_pts = pts;
	_last_updated = time;
	_pts_drift = _pts - time;
	_serial = serial;
}

void Clock::set_clock(double pts, int serial)
{
	double time = av_gettime_relative() / 1000000.0;
	set_clock_at(pts, serial, time);
}

void Clock::set_clock_speed(double speed)
{
	set_clock(get_clock(), _serial);
	_speed = speed;
}

void Clock::sync_clock_to_slave(Clock *slave)
{

	double clock = get_clock();

	double slave_clock = slave->get_clock();

	if (!isnan(slave_clock) && (isnan(clock) || fabs(clock - slave_clock) > AV_NOSYNC_THRESHOLD))
		set_clock(slave_clock, slave->_serial);
}

void Clock::set_clock_flush() {
	_pts = 0;
	_pts_drift = 0;
	_last_updated = 0;
	_speed = 1;
	_serial = 0;
	_paused = 0;
	if (_queue_serial) {
		_queue_serial = 0;
	}

}

FrameQueue::FrameQueue() {
	memset_fq();
}

FrameQueue::~FrameQueue() {
	memset_fq();
}

void FrameQueue::memset_fq() {
	memset(&_queue, 0, sizeof(Frame) * FRAME_QUEUE_SIZE);
	_rindex = 0;
	_windex = 0;
	_size = 0;
	_max_size = 0;
	_keep_last = 0;
	_rindex_shown = 0;
	_mutex = NULL;
	_cond = NULL;
	_pktq = NULL;
}

int FrameQueue::frame_queue_init(PacketQueue *pktq, int max_size, int keep_last)
{
	int i;
	memset_fq();

	if (!(_mutex = SDL_CreateMutex())) {

		return AVERROR(ENOMEM);
	}
	if (!(_cond = SDL_CreateCond())) {

		return AVERROR(ENOMEM);
	}
	_pktq = pktq;
	_max_size = FFMIN(max_size, FRAME_QUEUE_SIZE);
	_keep_last = !!keep_last;
	for (i = 0; i < _max_size; i++)
		if (!(_queue[i].frame = av_frame_alloc()))
			return AVERROR(ENOMEM);

	return 0;
}

void FrameQueue::frame_queue_unref_item(Frame *vp)
{
	av_frame_unref(vp->frame);
	avsubtitle_free(&vp->sub);
}

void FrameQueue::frame_queue_destory()
{
	int i;

	for (i = 0; i < _max_size; i++) {
		Frame *vp = &_queue[i];

		frame_queue_unref_item(vp);

		av_frame_free(&vp->frame);
	}

	if (_mutex) {
		SDL_DestroyMutex(_mutex);
		_mutex = NULL;
	}
	if (_cond) {
		SDL_DestroyCond(_cond);
		_cond = NULL;
	}
}

void FrameQueue::frame_queue_signal()
{
	SDL_LockMutex(_mutex);
	SDL_CondSignal(_cond);
	SDL_UnlockMutex(_mutex);
}

Frame *FrameQueue::frame_queue_peek_last()
{
	return &_queue[_rindex];
}

Frame *FrameQueue::frame_queue_peek()
{
	return &_queue[(_rindex + _rindex_shown) % _max_size];
}

Frame *FrameQueue::frame_queue_peek_next()
{
	return &_queue[(_rindex + _rindex_shown + 1) % _max_size];
}

Frame *FrameQueue::frame_queue_peek_writable()
{

	SDL_LockMutex(_mutex);
	while (_size >= _max_size &&
		!_pktq->_abort_request) {
		SDL_CondWait(_cond, _mutex);
	}
	SDL_UnlockMutex(_mutex);

	if (_pktq->_abort_request)
		return NULL;

	return &_queue[_windex];
}

Frame *FrameQueue::frame_queue_peek_readable()
{

	SDL_LockMutex(_mutex);
	while (_size - _rindex_shown <= 0 &&
		!_pktq->_abort_request) {
		SDL_CondWait(_cond, _mutex);
	}
	SDL_UnlockMutex(_mutex);

	if (_pktq->_abort_request)
		return NULL;

	return &_queue[(_rindex + _rindex_shown) % _max_size];
}

void FrameQueue::frame_queue_push()
{

	if (++_windex == _max_size)
		_windex = 0;

	SDL_LockMutex(_mutex);
	_size++;
	SDL_CondSignal(_cond);
	SDL_UnlockMutex(_mutex);
}

void FrameQueue::frame_queue_next()
{

	if (_keep_last && !_rindex_shown) {
		_rindex_shown = 1;
		return;
	}

	frame_queue_unref_item(&_queue[_rindex]);
	if (++_rindex == _max_size)
		_rindex = 0;
	SDL_LockMutex(_mutex);
	_size--;
	SDL_CondSignal(_cond);
	SDL_UnlockMutex(_mutex);
}

int FrameQueue::frame_queue_nb_remaining()
{

	return _size - _rindex_shown;
}

int64_t FrameQueue::frame_queue_last_pos()
{

	Frame *fp = &_queue[_rindex];
	if (_rindex_shown && fp->serial == _pktq->_serial)
		return fp->pos;
	else
		return -1;
}

Decoder::Decoder() {

}

Decoder::~Decoder() {

}

void Decoder::decoder_init(AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond) {
	memset(&_pkt, 0, sizeof(AVPacket));
	_queue = NULL;
	_avctx = NULL;
	_pkt_serial = 0;
	_finished = 0;
	_packet_pending = 0;
	_empty_queue_cond = NULL;
	_start_pts = 0;
	_start_pts_tb = { 0, 0 };
	_next_pts = 0;
	_next_pts_tb = { 0, 0 };
	_decoder_tid = NULL;

	_avctx = avctx;
	_queue = queue;
	_empty_queue_cond = empty_queue_cond;
	_start_pts = AV_NOPTS_VALUE;
	_pkt_serial = -1;
}

int Decoder::decoder_decode_frame(AVFrame *frame, AVSubtitle *sub) {
	int ret = AVERROR(EAGAIN);

	for (;;) {
		AVPacket pkt;

		if (_queue->_serial == _pkt_serial) {
			do {
				if (_queue->_abort_request) {

					return -1;
				}

				switch (_avctx->codec_type) {
				case AVMEDIA_TYPE_VIDEO:
					ret = avcodec_receive_frame(_avctx, frame);
					if (ret >= 0) {

						if (-1 == -1) {

							frame->pts = frame->best_effort_timestamp;
						}

						else if (0) {
							frame->pts = frame->pkt_dts;
						}

					}
					break;

				case AVMEDIA_TYPE_AUDIO:
					ret = avcodec_receive_frame(_avctx, frame);
					if (ret >= 0) {
						AVRational tb = { 1, frame->sample_rate };
						if (frame->pts != AV_NOPTS_VALUE) {

							frame->pts = av_rescale_q(frame->pts, _avctx->pkt_timebase, tb);
						}
						else if (_next_pts != AV_NOPTS_VALUE) {

							frame->pts = av_rescale_q(_next_pts, _next_pts_tb, tb);
						}
						if (frame->pts != AV_NOPTS_VALUE) {

							_next_pts = frame->pts + frame->nb_samples;
							_next_pts_tb = tb;
						}
					}
					break;

				}

				if (ret == AVERROR_EOF) {
					_finished = _pkt_serial;

					avcodec_flush_buffers(_avctx);
					return 0;
				}

				if (ret >= 0) {

					return 1;
				}

			} while (ret != AVERROR(EAGAIN));

		}

		do {

			if (_queue->_nb_packets == 0)
				SDL_CondSignal(_empty_queue_cond);

			if (_packet_pending) {
				av_packet_move_ref(&pkt, &_pkt);
				_packet_pending = 0;
			}
			else {

				if (_queue->packet_queue_get(&pkt, 1, &_pkt_serial) < 0)
					return -1;
			}
			if (_queue->_serial != _pkt_serial) {

				av_packet_unref(&pkt);
			}
		} while (_queue->_serial != _pkt_serial);

		if (pkt.data == flush_pkt.data) {

			avcodec_flush_buffers(_avctx);
			_finished = 0;
			_next_pts = _start_pts;
			_next_pts_tb = _start_pts_tb;
		}
		else {

			if (_avctx->codec_type == AVMEDIA_TYPE_SUBTITLE) {
				int got_frame = 0;
				ret = avcodec_decode_subtitle2(_avctx, sub, &got_frame, &pkt);
				if (ret < 0) {

					ret = AVERROR(EAGAIN);
				}
				else {
					if (got_frame && !pkt.data) {

						_packet_pending = 1;
						av_packet_move_ref(&_pkt, &pkt);
					}

					ret = got_frame ? 0 : (pkt.data ? AVERROR(EAGAIN) : AVERROR_EOF);
				}
			}
			else {
				if (avcodec_send_packet(_avctx, &pkt) == AVERROR(EAGAIN)) {

					av_log(_avctx, AV_LOG_ERROR, "Receive_frame and send_packet both returned EAGAIN, which is an API violation.\n");
					TYYERROR("Receive_frame and send_packet both returned EAGAIN, which is an API violation.");
					_packet_pending = 1;
					av_packet_move_ref(&_pkt, &pkt);
				}
			}
			av_packet_unref(&pkt);
		}

	}

}

void Decoder::decoder_destroy() {
	av_packet_unref(&_pkt);
	avcodec_free_context(&_avctx);
}

void Decoder::decoder_abort(FrameQueue *fq)
{
	_queue->packet_queue_abort();
	fq->frame_queue_signal();
	SDL_WaitThread(_decoder_tid, NULL);
	_decoder_tid = NULL;
	_queue->packet_queue_flush();
}

int Decoder::decoder_start(int(*fn)(void *), const char *thread_name, void* arg) {
	_queue->packet_queue_start();
	_decoder_tid = SDL_CreateThread(fn, thread_name, arg);
	if (!_decoder_tid) {
		av_log(NULL, AV_LOG_ERROR, "SDL_CreateThread(): %s\n", SDL_GetError());
		return AVERROR(ENOMEM);
	}

	return 0;
}

}
