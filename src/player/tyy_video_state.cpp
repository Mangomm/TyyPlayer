#include <sstream>
#include <time.h>
#include <future>
#include <thread>
#include <mutex>
#include <chrono>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#endif
#include "tyy_video_state.h"
#include "tyy_log.h"

using namespace TyyPlayer;
#define FF_QUIT_EVENT    (SDL_USEREVENT + 2)
#define FF_INITIATIVE_QUIT_EVENT    (SDL_USEREVENT + 3)
static const char *TYY_VIDEO_STATE_PROPERTY_URL = "url";
static const char *TYY_VIDEO_STATE_PROPERTY_WIN_ID = "win_id";
static const double TYY_FLOAT_EPS = 1e-8;

static bool tyy_less(double left, double right)
{
	return (left - right) < (-TYY_FLOAT_EPS);
}

static bool tyy_more(double left, double right)
{
	return (left - right) > TYY_FLOAT_EPS;
}

#if 1

namespace TyyPlayer {

static const struct TextureFormatEntry {
	enum AVPixelFormat format;
	int texture_fmt;
} sdl_texture_format_map[] = {
	{ AV_PIX_FMT_RGB8,           SDL_PIXELFORMAT_RGB332 },
	{ AV_PIX_FMT_RGB444,         SDL_PIXELFORMAT_RGB444 },
	{ AV_PIX_FMT_RGB555,         SDL_PIXELFORMAT_RGB555 },
	{ AV_PIX_FMT_BGR555,         SDL_PIXELFORMAT_BGR555 },
	{ AV_PIX_FMT_RGB565,         SDL_PIXELFORMAT_RGB565 },
	{ AV_PIX_FMT_BGR565,         SDL_PIXELFORMAT_BGR565 },
	{ AV_PIX_FMT_RGB24,          SDL_PIXELFORMAT_RGB24 },
	{ AV_PIX_FMT_BGR24,          SDL_PIXELFORMAT_BGR24 },
	{ AV_PIX_FMT_0RGB32,         SDL_PIXELFORMAT_RGB888 },
	{ AV_PIX_FMT_0BGR32,         SDL_PIXELFORMAT_BGR888 },
	{ AV_PIX_FMT_NE(RGB0, 0BGR), SDL_PIXELFORMAT_RGBX8888 },
	{ AV_PIX_FMT_NE(BGR0, 0RGB), SDL_PIXELFORMAT_BGRX8888 },
	{ AV_PIX_FMT_RGB32,          SDL_PIXELFORMAT_ARGB8888 },
	{ AV_PIX_FMT_RGB32_1,        SDL_PIXELFORMAT_RGBA8888 },
	{ AV_PIX_FMT_BGR32,          SDL_PIXELFORMAT_ABGR8888 },
	{ AV_PIX_FMT_BGR32_1,        SDL_PIXELFORMAT_BGRA8888 },
	{ AV_PIX_FMT_YUV420P,        SDL_PIXELFORMAT_IYUV },
	{ AV_PIX_FMT_YUYV422,        SDL_PIXELFORMAT_YUY2 },
	{ AV_PIX_FMT_UYVY422,        SDL_PIXELFORMAT_UYVY },
	{ AV_PIX_FMT_NONE,           SDL_PIXELFORMAT_UNKNOWN },
};

TyyVideoState::TyyVideoState() {
	TYYTRACE("TyyVideoState Constructor function");
}

TyyVideoState::TyyVideoState(HWND hwnd) {
	TYYTRACE("TyyVideoState(HWND hwnd) Constructor function");
	_h_display_window = hwnd;
}

TyyVideoState::~TyyVideoState() {
	TYYTRACE("TyyVideoState Destructor function");
}

void TyyVideoState::set_event_callback(TyyPlayerEventCallback callback, void *user_data) {
	_event_callback = callback;
	_event_user_data = user_data;
}

void TyyVideoState::set_properties(const Properties &properties) {
	_properties = properties;
}

int decode_interrupt_cb(void *ctx)
{
	TyyVideoState *is = (TyyVideoState *)ctx;

	if (is->_abort_request || (av_gettime() - is->_last_read_packet_time > is->_timeout * 1000 * 1000)) {
		return 1;
	}
	return 0;
}

int is_realtime(AVFormatContext *s)
{

	if (!strcmp(s->iformat->name, "rtp")
		|| !strcmp(s->iformat->name, "rtsp")
		|| !strcmp(s->iformat->name, "sdp")
		)
		return 1;

	if (s->pb && (!strncmp(s->url, "rtp:", 4)
		|| !strncmp(s->url, "udp:", 4)
		)
		)
		return 1;
	return 0;
}

int64_t TyyVideoState::get_valid_channel_layout(int64_t channel_layout, int channels)
{

	if (channel_layout && av_get_channel_layout_nb_channels(channel_layout) == channels)
		return channel_layout;
	else
		return 0;
}

#if CONFIG_AVFILTER

int TyyVideoState::configure_filtergraph(AVFilterGraph *graph, const char *filtergraph, AVFilterContext *source_ctx, AVFilterContext *sink_ctx)
{
	int ret, i;
	int nb_filters = graph->nb_filters;
	AVFilterInOut *outputs = NULL, *inputs = NULL;

	if (filtergraph) {

		outputs = avfilter_inout_alloc();
		inputs = avfilter_inout_alloc();
		if (!outputs || !inputs) {
			ret = AVERROR(ENOMEM);
			goto fail;
		}

		outputs->name = av_strdup("in");
		outputs->filter_ctx = source_ctx;
		outputs->pad_idx = 0;
		outputs->next = NULL;

		inputs->name = av_strdup("out");
		inputs->filter_ctx = sink_ctx;
		inputs->pad_idx = 0;
		inputs->next = NULL;

		if ((ret = avfilter_graph_parse_ptr(graph, filtergraph, &inputs, &outputs, NULL)) < 0)
			goto fail;
	}

	else {
		if ((ret = avfilter_link(source_ctx, 0, sink_ctx, 0)) < 0)
			goto fail;
	}

	for (i = 0; i < graph->nb_filters - nb_filters; i++)
		FFSWAP(AVFilterContext*, graph->filters[i], graph->filters[i + nb_filters]);

	ret = avfilter_graph_config(graph, NULL);

fail:
	avfilter_inout_free(&outputs);
	avfilter_inout_free(&inputs);

	return ret;
}

int TyyVideoState::configure_audio_filters(const char *afilters, int force_output_format)
{
	static const enum AVSampleFormat sample_fmts[] = { AV_SAMPLE_FMT_S16, AV_SAMPLE_FMT_NONE };
	int sample_rates[2] = { 0, -1 };
	int64_t channel_layouts[2] = { 0, -1 };
	int channels[2] = { 0, -1 };
	AVFilterContext *filt_asrc = NULL, *filt_asink = NULL;
	char aresample_swr_opts[512] = "";
	AVDictionaryEntry *e = NULL;
	char asrc_args[256];
	int ret;

	avfilter_graph_free(&_agraph);
	if (!(_agraph = avfilter_graph_alloc()))
		return AVERROR(ENOMEM);
	_agraph->nb_threads = _filter_nbthreads;

	while ((e = av_dict_get(_swr_opts, "", e, AV_DICT_IGNORE_SUFFIX)))
		av_strlcatf(aresample_swr_opts, sizeof(aresample_swr_opts), "%s=%s:", e->key, e->value);

	if (strlen(aresample_swr_opts))
		aresample_swr_opts[strlen(aresample_swr_opts) - 1] = '\0';

	av_opt_set(_agraph, "aresample_swr_opts", aresample_swr_opts, 0);

	ret = snprintf(asrc_args, sizeof(asrc_args),
		"sample_rate=%d:sample_fmt=%s:channels=%d:time_base=%d/%d",
		_audio_filter_src.freq, av_get_sample_fmt_name(_audio_filter_src.fmt), _audio_filter_src.channels, 1, _audio_filter_src.freq);
	if (_audio_filter_src.channel_layout)
		snprintf(asrc_args + ret, sizeof(asrc_args) - ret, ":channel_layout=0x%lld", _audio_filter_src.channel_layout);

	ret = avfilter_graph_create_filter(&filt_asrc, avfilter_get_by_name("abuffer"), "ffplay_abuffer", asrc_args, NULL, _agraph);
	if (ret < 0)
		goto end;

	ret = avfilter_graph_create_filter(&filt_asink, avfilter_get_by_name("abuffersink"), "ffplay_abuffersink", NULL, NULL, _agraph);
	if (ret < 0)
		goto end;

	if ((ret = av_opt_set_int_list(filt_asink, "sample_fmts", sample_fmts, AV_SAMPLE_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
		goto end;
	if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 1, AV_OPT_SEARCH_CHILDREN)) < 0)
		goto end;

	if (force_output_format) {
		channel_layouts[0] = _audio_tgt.channel_layout;
		channels[0] = _audio_tgt.channels;
		sample_rates[0] = _audio_tgt.freq;
		if ((ret = av_opt_set_int(filt_asink, "all_channel_counts", 0, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
		if ((ret = av_opt_set_int_list(filt_asink, "channel_layouts", channel_layouts, -1, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
		if ((ret = av_opt_set_int_list(filt_asink, "channel_counts", channels, -1, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
		if ((ret = av_opt_set_int_list(filt_asink, "sample_rates", sample_rates, -1, AV_OPT_SEARCH_CHILDREN)) < 0)
			goto end;
	}

	if ((ret = configure_filtergraph(_agraph, afilters, filt_asrc, filt_asink)) < 0)
		goto end;

	_in_audio_filter = filt_asrc;
	_out_audio_filter = filt_asink;

end:
	if (ret < 0) {
		avfilter_graph_free(&_agraph);
		_agraph = NULL;
	}

	return ret;
}
#endif

int TyyVideoState::get_master_sync_type() {
	if (_av_sync_type == AV_SYNC_VIDEO_MASTER) {
		if (_video_st)
			return AV_SYNC_VIDEO_MASTER;
		else
			return AV_SYNC_AUDIO_MASTER;
	}
	else if (_av_sync_type == AV_SYNC_AUDIO_MASTER) {
		if (_audio_st)
			return AV_SYNC_AUDIO_MASTER;
		else

			return AV_SYNC_VIDEO_MASTER;
	}
	else {
		return AV_SYNC_EXTERNAL_CLOCK;
	}
}

double TyyVideoState::get_master_clock()
{
	double val;

	switch (get_master_sync_type()) {
	case AV_SYNC_VIDEO_MASTER:
		val = _vidclk.get_clock();
		break;
	case AV_SYNC_AUDIO_MASTER:
		val = _audclk.get_clock();
		break;
	default:
		val = _extclk.get_clock();
		break;
	}
	return val;
}

int TyyVideoState::synchronize_audio(int nb_samples)
{
	int wanted_nb_samples = nb_samples;

	if (get_master_sync_type() != AV_SYNC_AUDIO_MASTER) {
		double diff, avg_diff;
		int min_nb_samples, max_nb_samples;

		diff = _audclk.get_clock() - get_master_clock();

		if (!isnan(diff) && fabs(diff) < AV_NOSYNC_THRESHOLD) {

			_audio_diff_cum = diff + _audio_diff_avg_coef * _audio_diff_cum;

			if (_audio_diff_avg_count < AUDIO_DIFF_AVG_NB) {

				_audio_diff_avg_count++;
			}
			else {

				avg_diff = _audio_diff_cum * (1.0 - _audio_diff_avg_coef);

				if (fabs(avg_diff) >= _audio_diff_threshold) {

					wanted_nb_samples = nb_samples + (int)(diff * _audio_src.freq);
					min_nb_samples = ((nb_samples * (100 - SAMPLE_CORRECTION_PERCENT_MAX) / 100));
					max_nb_samples = ((nb_samples * (100 + SAMPLE_CORRECTION_PERCENT_MAX) / 100));

					wanted_nb_samples = av_clip(wanted_nb_samples, min_nb_samples, max_nb_samples);
				}

			}
		}
		else {

			_audio_diff_avg_count = 0;
			_audio_diff_cum = 0;
		}
	}

	return wanted_nb_samples;
}

int TyyVideoState::audio_decode_frame()
{
	int data_size, resampled_data_size;
	int64_t dec_channel_layout;
	av_unused double audio_clock0;
	int wanted_nb_samples;
	Frame *af;

	if (_paused)
		return -1;

	do {
#if defined(_WIN32)
		while (_sampq.frame_queue_nb_remaining() == 0) {

			if ((av_gettime_relative() - _audio_callback_time) > 1000000LL * _audio_hw_buf_size / _audio_tgt.bytes_per_sec / 2) {
				return -1;
			}

			av_usleep(1000);
		}
#endif

		if (!(af = _sampq.frame_queue_peek_readable()))
			return -1;

		_sampq.frame_queue_next();
	} while (af->serial != _audioq._serial);

	data_size = av_samples_get_buffer_size(NULL, af->frame->channels, af->frame->nb_samples, (AVSampleFormat)af->frame->format, 1);

	dec_channel_layout =
		(af->frame->channel_layout &&
			af->frame->channels == av_get_channel_layout_nb_channels(af->frame->channel_layout)) ?
		af->frame->channel_layout : av_get_default_channel_layout(af->frame->channels);

	wanted_nb_samples = synchronize_audio(af->frame->nb_samples);

	if (af->frame->format != _audio_src.fmt ||
		dec_channel_layout != _audio_src.channel_layout ||
		af->frame->sample_rate != _audio_src.freq ||

		(wanted_nb_samples != af->frame->nb_samples && !_swr_ctx)
		)
	{
		swr_free(&_swr_ctx);

		_swr_ctx = swr_alloc_set_opts(NULL,
			_audio_tgt.channel_layout,
			_audio_tgt.fmt,
			_audio_tgt.freq,
			dec_channel_layout,
			(AVSampleFormat)af->frame->format,
			af->frame->sample_rate,
			0, NULL);

		if (!_swr_ctx || swr_init(_swr_ctx) < 0) {
			av_log(NULL, AV_LOG_ERROR,
				"Cannot create sample rate converter for conversion of %d Hz %s %d channels to %d Hz %s %d channels!\n",
				af->frame->sample_rate, av_get_sample_fmt_name((AVSampleFormat)af->frame->format), af->frame->channels,
				_audio_tgt.freq, av_get_sample_fmt_name(_audio_tgt.fmt), _audio_tgt.channels);
			swr_free(&_swr_ctx);
			return -1;
		}

		_audio_src.channel_layout = dec_channel_layout;
		_audio_src.channels = af->frame->channels;
		_audio_src.freq = af->frame->sample_rate;
		_audio_src.fmt = (AVSampleFormat)af->frame->format;
	}

	if (_swr_ctx) {

		const uint8_t **in = (const uint8_t **)af->frame->extended_data;

		uint8_t **out = &_audio_buf1;

		int out_count = (int64_t)wanted_nb_samples * _audio_tgt.freq / af->frame->sample_rate
			+ 256;
		int out_size = av_samples_get_buffer_size(NULL, _audio_tgt.channels,
			out_count, _audio_tgt.fmt, 0);
		int len2;
		if (out_size < 0) {
			av_log(NULL, AV_LOG_ERROR, "av_samples_get_buffer_size() failed\n");
			return -1;
		}

		if (wanted_nb_samples != af->frame->nb_samples) {

			int sample_delta = (wanted_nb_samples - af->frame->nb_samples) * _audio_tgt.freq
				/ af->frame->sample_rate;

			int compensation_distance = wanted_nb_samples * _audio_tgt.freq / af->frame->sample_rate;

			if (swr_set_compensation(_swr_ctx, sample_delta, compensation_distance) < 0) {
				av_log(NULL, AV_LOG_ERROR, "swr_set_compensation() failed\n");
				return -1;
			}
		}

		av_fast_malloc(&_audio_buf1, &_audio_buf1_size, out_size);
		if (!_audio_buf1)
			return AVERROR(ENOMEM);

		len2 = swr_convert(_swr_ctx, out, out_count, in, af->frame->nb_samples);
		if (len2 < 0) {
			av_log(NULL, AV_LOG_ERROR, "swr_convert() failed\n");
			return -1;
		}
		if (len2 == out_count) {

			av_log(NULL, AV_LOG_WARNING, "audio buffer is probably too small\n");
			if (swr_init(_swr_ctx) < 0)
				swr_free(&_swr_ctx);
		}

		_audio_buf = _audio_buf1;
		resampled_data_size = len2 * _audio_tgt.channels * av_get_bytes_per_sample(_audio_tgt.fmt);
	}
	else {

		_audio_buf = af->frame->data[0];
		resampled_data_size = data_size;
	}

	audio_clock0 = _audio_clock;

	if (!isnan(af->pts))
		_audio_clock = af->pts + (double)af->frame->nb_samples / af->frame->sample_rate;
	else
		_audio_clock = NAN;
	_audio_clock_serial = af->serial;

	{
		static double last_clock = _audio_clock;
		TYYTRACE("audio: delay={:0.3f} clock={:0.3f} clock0={:0.3f}",
			_audio_clock - last_clock,
			_audio_clock, audio_clock0);
		last_clock = _audio_clock;
	}

	return resampled_data_size;
}

void TyyVideoState::sonic_speed() {
	if (av_get_bytes_per_sample(_audio_tgt.fmt) == 2) {

		int out_samples = _audio_buf_size /
			(_audio_tgt.channels * av_get_bytes_per_sample(_audio_tgt.fmt));
		int write_ret = sonicWriteShortToStream(_audio_speed_convert,
			(short *)_audio_buf,
			out_samples);

		int availableSamples = sonicSamplesAvailable(_audio_speed_convert);

		TYYTRACE("_audio_buf_size(bytes): {}, target channel: {}, target fmt bytes: {}, \
					""origin out_samples: {}, sonicSamplesAvailable: {}",
			_audio_buf_size, _audio_tgt.channels, av_get_bytes_per_sample(_audio_tgt.fmt),
			out_samples, availableSamples);

		int out_size = (availableSamples) * sizeof(short) * _audio_tgt.channels;
		av_fast_malloc(&_audio_buf1, &_audio_buf1_size, out_size);
		if (write_ret) {
			int read_sonic_samples = sonicReadShortFromStream(_audio_speed_convert,
				(short *)_audio_buf1, availableSamples);
			_audio_buf = _audio_buf1;
			_audio_buf_size = read_sonic_samples * sizeof(short) * _audio_tgt.channels;
			_audio_buf_index = 0;

			TYYTRACE("out_size(bytes): {}, _audio_buf1_size(bytes): {}, read_sonic_samples: {}, \
						""_audio_buf_size for speed change: {}",
				out_size, _audio_buf1_size, read_sonic_samples, _audio_buf_size);
		}
	}
	else if (av_get_bytes_per_sample(_audio_tgt.fmt) == 4) {
		int out_samples = _audio_buf_size /
			(_audio_tgt.channels * av_get_bytes_per_sample(_audio_tgt.fmt));
		int write_ret = sonicWriteFloatToStream(_audio_speed_convert,
			(float *)_audio_buf,
			out_samples);

		int availableSamples = sonicSamplesAvailable(_audio_speed_convert);

		TYYTRACE("_audio_buf_size(bytes): {}, target channel: {}, target fmt bytes: {}, \
					origin out_samples: {}, sonicSamplesAvailable: {}",
			_audio_buf_size, _audio_tgt.channels, av_get_bytes_per_sample(_audio_tgt.fmt),
			out_samples, availableSamples);

		int out_size = (availableSamples) * sizeof(float) * _audio_tgt.channels;
		av_fast_malloc(&_audio_buf1, &_audio_buf1_size, out_size);
		if (write_ret) {
			int read_sonic_samples = sonicReadFloatFromStream(_audio_speed_convert,
				(float *)_audio_buf1, availableSamples);
			_audio_buf = _audio_buf1;
			_audio_buf_size = read_sonic_samples * sizeof(float) * _audio_tgt.channels;
			_audio_buf_index = 0;

			TYYTRACE("out_size(bytes): {}, _audio_buf1_size(bytes): {}, read_sonic_samples: {}, \
						_audio_buf_size for speed change: {}",
				out_size, _audio_buf1_size, read_sonic_samples, _audio_buf_size);
	}
	}
}

void sdl_audio_callback(void *opaque, Uint8 *stream, int len)
{
	TyyVideoState *is = (TyyVideoState*)opaque;
	int audio_size, len1;

	is->_audio_callback_time = av_gettime_relative();

	while (len > 0) {

		if (is->_audio_buf_index >= is->_audio_buf_size) {
			audio_size = is->audio_decode_frame();
			if (audio_size < 0) {

				is->_audio_buf = NULL;

				is->_audio_buf_size = SDL_AUDIO_MIN_BUFFER_SIZE / is->_audio_tgt.frame_size * is->_audio_tgt.frame_size;
			}
			else {

				is->_audio_buf_size = audio_size;
			}

			is->_audio_buf_index = 0;

#ifdef USE_SONIC
			if (is->_change_speed_req){
				is->_change_speed_req = 0;
				if (is->_audio_speed_convert){
					sonicDestroyStream(is->_audio_speed_convert);
					is->_audio_speed_convert = NULL;
				}
				is->_audio_speed_convert = sonicCreateStream(is->_audio_tgt.freq, is->_audio_tgt.channels);

				sonicSetSpeed(is->_audio_speed_convert, is->_play_speed);
				sonicSetPitch(is->_audio_speed_convert, 1.0);
				sonicSetRate(is->_audio_speed_convert, 1.0);
			}
			if (!(tyy_more(is->_play_speed, 0.99) && tyy_less(is->_play_speed, 1.01)) && is->_audio_buf){
				is->sonic_speed();
			}
#endif

		}

		if (!is->_audio_buf_size)
			continue;

		len1 = is->_audio_buf_size - is->_audio_buf_index;
		if (len1 > len)
			len1 = len;

		if (!is->_muted && is->_audio_buf && is->_audio_volume == SDL_MIX_MAXVOLUME)
			memcpy(stream, (uint8_t *)is->_audio_buf + is->_audio_buf_index, len1);
		else {

			memset(stream, 0, len1);

			if (!is->_muted && is->_audio_buf)
				SDL_MixAudioFormat(stream, (uint8_t *)is->_audio_buf + is->_audio_buf_index,
					AUDIO_S16SYS, len1, is->_audio_volume);
		}

		len -= len1;
		stream += len1;

		is->_audio_buf_index += len1;

	}

	is->_audio_write_buf_size = is->_audio_buf_size - is->_audio_buf_index;

	if (!isnan(is->_audio_clock)) {
		is->_audclk.set_clock_at(is->_audio_clock - (double)(2 * is->_audio_hw_buf_size + is->_audio_write_buf_size) / is->_audio_tgt.bytes_per_sec,
			is->_audio_clock_serial,
			is->_audio_callback_time / 1000000.0);

		is->_extclk.sync_clock_to_slave(&is->_audclk);
	}

}

int TyyVideoState::audio_open(void *opaque, int64_t wanted_channel_layout, int wanted_nb_channels, int wanted_sample_rate, struct AudioParams *audio_hw_params)
{
	SDL_AudioSpec wanted_spec, spec;
	const char *env;
	static const int next_nb_channels[] = { 0, 0, 1, 6, 2, 6, 4, 6 };
	static const int next_sample_rates[] = { 0, 44100, 48000, 96000, 192000 };
	int next_sample_rate_idx = FF_ARRAY_ELEMS(next_sample_rates) - 1;

	env = SDL_getenv("SDL_AUDIO_CHANNELS");
	if (env) {
		wanted_nb_channels = atoi(env);
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);
	}

	if (!wanted_channel_layout || wanted_nb_channels != av_get_channel_layout_nb_channels(wanted_channel_layout)) {
		wanted_channel_layout = av_get_default_channel_layout(wanted_nb_channels);

		wanted_channel_layout &= ~AV_CH_LAYOUT_STEREO_DOWNMIX;
	}

	wanted_nb_channels = av_get_channel_layout_nb_channels(wanted_channel_layout);
	wanted_spec.channels = wanted_nb_channels;

	wanted_spec.freq = wanted_sample_rate;
	if (wanted_spec.freq <= 0 || wanted_spec.channels <= 0) {
		av_log(NULL, AV_LOG_ERROR, "Invalid sample rate or channel count!\n");
		return -1;
	}

	while (next_sample_rate_idx && next_sample_rates[next_sample_rate_idx] >= wanted_spec.freq)
		next_sample_rate_idx--;

	wanted_spec.format = AUDIO_S16SYS;
	wanted_spec.silence = 0;

	wanted_spec.samples = FFMAX(SDL_AUDIO_MIN_BUFFER_SIZE, 2 << av_log2(wanted_spec.freq / SDL_AUDIO_MAX_CALLBACKS_PER_SEC));
	wanted_spec.callback = sdl_audio_callback;
	wanted_spec.userdata = opaque;

	TYYINFO("audio_open request, channel_layout: {}, channels: {}, sample_rate: {}, samples: {}",
		wanted_channel_layout, wanted_spec.channels, wanted_spec.freq, wanted_spec.samples);

	while (!(_audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted_spec, &spec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE))) {
		TYYERROR("SDL_OpenAudio ({} channels, {} Hz): {}", wanted_spec.channels, wanted_spec.freq, SDL_GetError());
		wanted_spec.channels = next_nb_channels[FFMIN(7, wanted_spec.channels)];
		if (!wanted_spec.channels) {
			wanted_spec.freq = next_sample_rates[next_sample_rate_idx--];
			wanted_spec.channels = wanted_nb_channels;
			if (!wanted_spec.freq) {
				TYYERROR("No more combinations to try, audio open failed");
				return -1;
			}
		}
		wanted_channel_layout = av_get_default_channel_layout(wanted_spec.channels);
	}

	if (spec.format != AUDIO_S16SYS) {
		TYYERROR("SDL advised audio format {} is not supported", spec.format);
		return -1;
	}

	if (spec.channels != wanted_spec.channels) {
		wanted_channel_layout = av_get_default_channel_layout(spec.channels);
		if (!wanted_channel_layout) {
			TYYERROR("SDL advised channel count {} is not supported", spec.channels);
			return -1;
		}
	}

	audio_hw_params->fmt = AV_SAMPLE_FMT_S16;
	audio_hw_params->freq = spec.freq;
	audio_hw_params->channel_layout = wanted_channel_layout;
	audio_hw_params->channels = spec.channels;

	audio_hw_params->frame_size = av_samples_get_buffer_size(NULL, audio_hw_params->channels, 1, audio_hw_params->fmt, 1);

	audio_hw_params->bytes_per_sec = av_samples_get_buffer_size(NULL, audio_hw_params->channels, audio_hw_params->freq, audio_hw_params->fmt, 1);
	if (audio_hw_params->bytes_per_sec <= 0 || audio_hw_params->frame_size <= 0) {
		TYYERROR("av_samples_get_buffer_size failed, bytes_per_sec: {}, frame_size: {}",
			audio_hw_params->bytes_per_sec, audio_hw_params->frame_size);
		return -1;
	}

	TYYINFO("audio_open success, device: {}, channels: {}, freq: {}, format: {}, samples: {}, size: {}, frame_size: {}, bytes_per_sec: {}",
		_audio_dev, spec.channels, spec.freq, spec.format, spec.samples, spec.size,
		audio_hw_params->frame_size, audio_hw_params->bytes_per_sec);

	return spec.size;
}

inline int TyyVideoState::cmp_audio_fmts(enum AVSampleFormat fmt1, int64_t channel_count1,
	enum AVSampleFormat fmt2, int64_t channel_count2)
{

	if (channel_count1 == 1 && channel_count2 == 1)
		return av_get_packed_sample_fmt(fmt1) != av_get_packed_sample_fmt(fmt2);
	else
		return channel_count1 != channel_count2 || fmt1 != fmt2;
}

int audio_thread(void *arg)
{
	TyyVideoState *is = (TyyVideoState*)arg;
	if (!is) {
		TYYERROR("arg param is null");
		return -1;
	}

	AVFrame *frame = av_frame_alloc();
	Frame *af;
#if CONFIG_AVFILTER
	int last_serial = -1;
	int64_t dec_channel_layout;
	int reconfigure;
#endif
	int got_frame = 0;
	AVRational tb;
	int ret = 0;

#if CONFIG_AVFILTER
	double pts, pos, duration;
	int serial;
#endif

	if (!frame)
		return AVERROR(ENOMEM);

	do {

		if ((got_frame = is->_auddec.decoder_decode_frame(frame, NULL)) < 0)
			goto the_end;

		if (got_frame) {
			tb = { 1, frame->sample_rate };

#if CONFIG_AVFILTER

			dec_channel_layout = is->get_valid_channel_layout(frame->channel_layout, frame->channels);
			reconfigure =
				is->cmp_audio_fmts(is->_audio_filter_src.fmt, is->_audio_filter_src.channels, (AVSampleFormat)frame->format, frame->channels)
				||
				is->_audio_filter_src.channel_layout != dec_channel_layout ||
				is->_audio_filter_src.freq != frame->sample_rate ||
				is->_auddec._pkt_serial != last_serial;

			pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
			pos = frame->pkt_pos;
			serial = is->_auddec._pkt_serial;
			duration = av_q2d({ frame->nb_samples, frame->sample_rate });

			if (reconfigure || is->_req_afilter_reconfigure) {
				char buf1[1024], buf2[1024];
				if (is->_req_afilter_reconfigure)
					TYYDEBUG("handle audio filter set speed");
				is->_req_afilter_reconfigure = 0;
				av_get_channel_layout_string(buf1, sizeof(buf1), -1, is->_audio_filter_src.channel_layout);
				av_get_channel_layout_string(buf2, sizeof(buf2), -1, dec_channel_layout);
				av_log(NULL, AV_LOG_DEBUG,
					"Audio frame changed from rate:%d ch:%d fmt:%s layout:%s serial:%d to rate:%d ch:%d fmt:%s layout:%s serial:%d\n",
					is->_audio_filter_src.freq, is->_audio_filter_src.channels, av_get_sample_fmt_name(is->_audio_filter_src.fmt), buf1, last_serial,
					frame->sample_rate, frame->channels, av_get_sample_fmt_name((AVSampleFormat)frame->format), buf2, is->_auddec._pkt_serial);

				is->_audio_filter_src.fmt = (AVSampleFormat)frame->format;
				is->_audio_filter_src.channels = frame->channels;
				is->_audio_filter_src.channel_layout = dec_channel_layout;
				is->_audio_filter_src.freq = frame->sample_rate;
				last_serial = is->_auddec._pkt_serial;

				if ((ret = is->configure_audio_filters(is->_afilters, 1)) < 0)
					goto the_end;
			}

			if ((ret = av_buffersrc_add_frame(is->_in_audio_filter, frame)) < 0)
				goto the_end;

			while ((ret = av_buffersink_get_frame_flags(is->_out_audio_filter, frame, 0)) >= 0) {

				tb = av_buffersink_get_time_base(is->_out_audio_filter);
#endif

				if (!(af = is->_sampq.frame_queue_peek_writable()))
					goto the_end;

#if CONFIG_AVFILTER

				af->pts = pts;
				af->pos = pos;
				af->serial = serial;
				af->duration = duration;

#else
				af->pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);
				af->pos = frame->pkt_pos;
				af->serial = is->_auddec._pkt_serial;
				af->duration = av_q2d({ frame->nb_samples, frame->sample_rate });
#endif

				av_frame_move_ref(af->frame, frame);
				is->_sampq.frame_queue_push();

#if CONFIG_AVFILTER

				if (is->_audioq._serial != is->_auddec._pkt_serial) {
					break;
				}

			}

			if (ret == AVERROR_EOF)
				is->_auddec._finished = is->_auddec._pkt_serial;
#endif
		}

	} while (ret >= 0 || ret == AVERROR(EAGAIN) || ret == AVERROR_EOF);

the_end:
#if CONFIG_AVFILTER
	avfilter_graph_free(&is->_agraph);
#endif
	av_frame_free(&frame);
	return ret;
}

int TyyVideoState::get_video_frame(AVFrame *frame)
{
	int got_picture;

	if ((got_picture = _viddec.decoder_decode_frame(frame, NULL)) < 0) {
		return -1;
	}

	if (got_picture) {
		double dpts = NAN;

		if (frame->pts != AV_NOPTS_VALUE)
			dpts = av_q2d(_video_st->time_base) * frame->pts;

		frame->sample_aspect_ratio = av_guess_sample_aspect_ratio(_ic, _video_st, frame);

		if (_framedrop  >0 ||
			(_framedrop && get_master_sync_type() != AV_SYNC_VIDEO_MASTER))
		{
			if (frame->pts != AV_NOPTS_VALUE) {
				double diff = dpts - get_master_clock();
				if (!isnan(diff) &&
					fabs(diff) < AV_NOSYNC_THRESHOLD &&
					diff - _frame_last_filter_delay < 0 &&
					_viddec._pkt_serial == _vidclk._serial &&
					_videoq._nb_packets) {

					_frame_drops_early++;
					TYYDEBUG("_WINID: {}, diff: {}, _frame_last_filter_delay: {}, \
						_viddec._pkt_serial: {}, _vidclk._serial: {}, _frame_drops_early: {}, _videoq._nb_packets: {}",
						diff, _frame_drops_early, _viddec._pkt_serial, _vidclk._serial, _frame_drops_early, _videoq._nb_packets);
					av_frame_unref(frame);
					got_picture = 0;
				}
			}
		}

	}

	return got_picture;
}

double TyyVideoState::get_rotation(AVStream *st)
{

	uint8_t* displaymatrix = av_stream_get_side_data(st, AV_PKT_DATA_DISPLAYMATRIX, NULL);
	double theta = 0;
	if (displaymatrix)
		theta = -av_display_rotation_get((int32_t*)displaymatrix);

	theta -= 360 * floor(theta / 360 + 0.9 / 360);

	if (fabs(theta - 90 * round(theta / 90)) > 2)
		av_log(NULL, AV_LOG_WARNING, "Odd rotation angle.\n"
			"If you want to help, upload a sample "
			"of this file to ftp://upload.ffmpeg.org/incoming/ "
			"and contact the ffmpeg-devel mailing list. (ffmpeg-devel@ffmpeg.org)");

	return theta;
}

#if CONFIG_AVFILTER

int TyyVideoState::configure_video_filters(AVFilterGraph *graph, const char *vfilters, AVFrame *frame)
{
	enum AVPixelFormat pix_fmts[FF_ARRAY_ELEMS(sdl_texture_format_map)];
	char sws_flags_str[512] = "";
	char buffersrc_args[256];
	int ret;
	AVFilterContext *filt_src = NULL, *filt_out = NULL, *last_filter = NULL;
	AVCodecParameters *codecpar = _video_st->codecpar;
	AVRational fr = av_guess_frame_rate(_ic, _video_st, NULL);
	AVDictionaryEntry *e = NULL;
	int nb_pix_fmts = 0;
	int i, j;

	for (i = 0; i < _renderer_info.num_texture_formats; i++) {
		for (j = 0; j < FF_ARRAY_ELEMS(sdl_texture_format_map) - 1; j++) {
			if (_renderer_info.texture_formats[i] == sdl_texture_format_map[j].texture_fmt) {
				pix_fmts[nb_pix_fmts++] = sdl_texture_format_map[j].format;
				break;
			}
		}
	}
	pix_fmts[nb_pix_fmts] = AV_PIX_FMT_NONE;

	while ((e = av_dict_get(_sws_dict, "", e, AV_DICT_IGNORE_SUFFIX))) {
		if (!strcmp(e->key, "sws_flags")) {
			av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", "flags", e->value);
		}
		else
			av_strlcatf(sws_flags_str, sizeof(sws_flags_str), "%s=%s:", e->key, e->value);
	}
	if (strlen(sws_flags_str))
		sws_flags_str[strlen(sws_flags_str) - 1] = '\0';

	graph->scale_sws_opts = av_strdup(sws_flags_str);

	snprintf(buffersrc_args, sizeof(buffersrc_args),
		"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
		frame->width, frame->height, frame->format,
		_video_st->time_base.num, _video_st->time_base.den,
		codecpar->sample_aspect_ratio.num, FFMAX(codecpar->sample_aspect_ratio.den, 1));
	if (fr.num && fr.den)
		av_strlcatf(buffersrc_args, sizeof(buffersrc_args), ":frame_rate=%d/%d", fr.num, fr.den);

	if ((ret = avfilter_graph_create_filter(&filt_src,
		avfilter_get_by_name("buffer"),
		"ffplay_buffer", buffersrc_args, NULL,
		graph)) < 0)
		goto fail;

	ret = avfilter_graph_create_filter(&filt_out,
		avfilter_get_by_name("buffersink"),
		"ffplay_buffersink", NULL, NULL, graph);
	if (ret < 0)
		goto fail;

	if ((ret = av_opt_set_int_list(filt_out, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
		goto fail;

	last_filter = filt_out;

#define INSERT_FILT(name, arg) do {                                          \
    AVFilterContext *filt_ctx;                                               \
    \
    ret = avfilter_graph_create_filter(&filt_ctx,                            \
    avfilter_get_by_name(name),           \
    "ffplay_" name, arg, NULL, graph);    \
    if (ret < 0)                                                             \
    goto fail;                                                           \
    \
    ret = avfilter_link(filt_ctx, 0, last_filter, 0);                        \
    if (ret < 0)                                                             \
    goto fail;                                                           \
    \
    last_filter = filt_ctx;                                                  \
} while (0)

	if (_autorotate) {
		double theta = get_rotation(_video_st);

		if (fabs(theta - 90) < 1.0) {
			INSERT_FILT("transpose", "clock");

		}
		else if (fabs(theta - 180) < 1.0) {
			INSERT_FILT("hflip", NULL);
			INSERT_FILT("vflip", NULL);
		}
		else if (fabs(theta - 270) < 1.0) {
			INSERT_FILT("transpose", "cclock");
		}
		else if (fabs(theta) > 1.0) {
			char rotate_buf[64];
			snprintf(rotate_buf, sizeof(rotate_buf), "%f*PI/180", theta);
			INSERT_FILT("rotate", rotate_buf);
		}
	}

	if ((ret = configure_filtergraph(graph, vfilters, filt_src, last_filter)) < 0)
		goto fail;

	_in_video_filter = filt_src;
	_out_video_filter = filt_out;

fail:
	return ret;
}
#endif

int TyyVideoState::queue_picture(AVFrame *src_frame, double pts, double duration, int64_t pos, int serial)
{
	Frame *vp;

#if defined(DEBUG_SYNC)
	printf("frame_type=%c pts=%0.3f\n",
		av_get_picture_type_char(src_frame->pict_type), pts);
#endif

	if (!(vp = _pictq.frame_queue_peek_writable()))
		return -1;

	vp->sar = src_frame->sample_aspect_ratio;
	vp->uploaded = 0;

	vp->width = src_frame->width;
	vp->height = src_frame->height;
	vp->format = src_frame->format;

	vp->pts = pts;
	vp->duration = duration;
	vp->pos = pos;
	vp->serial = serial;

	av_frame_move_ref(vp->frame, src_frame);

	_pictq.frame_queue_push();

	return 0;
}

int video_thread(void *arg)
{
	TyyVideoState *is = (TyyVideoState*)arg;
	AVFrame *frame = av_frame_alloc();
	double pts;
	double duration;
	int ret;
	int64_t t1;

	AVRational tb = is->_video_st->time_base;

	AVRational frame_rate = av_guess_frame_rate(is->_ic, is->_video_st, NULL);

#if CONFIG_AVFILTER
	AVFilterGraph *graph = NULL;
	AVFilterContext *filt_out = NULL, *filt_in = NULL;
	int last_w = 0;
	int last_h = 0;
	enum AVPixelFormat last_format = (AVPixelFormat)-2;
	int last_serial = -1;
	int last_vfilter_idx = 0;

#endif

	if (!frame)
		return AVERROR(ENOMEM);

	for (;;) {

		ret = is->get_video_frame(frame);
		if (ret < 0)
			goto the_end;
		if (!ret)
			continue;

		t1 = av_gettime_relative();

#if CONFIG_AVFILTER
		if (is->_hwaccel != AC_HARDWAREACCELERATETYPE_DISABLED)
			goto hwaccel;

		if (last_w != frame->width
			|| last_h != frame->height
			|| last_format != frame->format
			|| last_serial != is->_viddec._pkt_serial
			|| last_vfilter_idx != is->_vfilter_idx) {
			av_log(NULL, AV_LOG_DEBUG,
				"Video frame changed from size:%dx%d format:%s serial:%d ---to--- size:%dx%d format:%s serial:%d\n",
				last_w, last_h,
				(const char *)av_x_if_null(av_get_pix_fmt_name(last_format), "none"), last_serial,
				frame->width, frame->height,
				(const char *)av_x_if_null(av_get_pix_fmt_name((AVPixelFormat)frame->format), "none"), is->_viddec._pkt_serial);

			avfilter_graph_free(&graph);

			graph = avfilter_graph_alloc();
			if (!graph) {
				ret = AVERROR(ENOMEM);
				goto the_end;
			}
			graph->nb_threads = is->_filter_nbthreads;

			if ((ret = is->configure_video_filters(graph, is->_vfilters_list ? is->_vfilters_list[is->_vfilter_idx] : NULL, frame)) < 0) {

				TYYDEBUG("video_thread receive beidong abort request, _abort_request: {}, err: {}",
					is->_abort_request, is->get_ffmpeg_error(ret).c_str());
				is->_abort_request_w = 3;
				goto the_end;
			}

			filt_in = is->_in_video_filter;
			filt_out = is->_out_video_filter;
			last_w = frame->width;
			last_h = frame->height;
			last_format = (AVPixelFormat)frame->format;
			last_serial = is->_viddec._pkt_serial;
			last_vfilter_idx = is->_vfilter_idx;
			frame_rate = av_buffersink_get_frame_rate(filt_out);
		}

		ret = av_buffersrc_add_frame(filt_in, frame);
		if (ret < 0)
			goto the_end;

		while (ret >= 0) {
			is->_frame_last_returned_time = av_gettime_relative() / 1000000.0;

			ret = av_buffersink_get_frame_flags(filt_out, frame, 0);
			if (ret < 0) {
				if (ret == AVERROR_EOF) {
					is->_viddec._finished = is->_viddec._pkt_serial;
				}
				ret = 0;
				break;
			}

			is->_frame_last_filter_delay = av_gettime_relative() / 1000000.0 - is->_frame_last_returned_time;
			if (fabs(is->_frame_last_filter_delay) > AV_NOSYNC_THRESHOLD / 10.0)
				is->_frame_last_filter_delay = 0;

			tb = av_buffersink_get_time_base(filt_out);
#endif

		hwaccel:

			TYYDEBUG("fiter elp: {}(ms)", (av_gettime_relative() - t1) / 1000);

			duration = (frame_rate.num && frame_rate.den ? av_q2d({ frame_rate.den, frame_rate.num }) : 0);

			pts = (frame->pts == AV_NOPTS_VALUE) ? NAN : frame->pts * av_q2d(tb);

			t1 = av_gettime_relative();
			ret = is->queue_picture(frame, pts, duration, frame->pkt_pos, is->_viddec._pkt_serial);
			TYYDEBUG("queue_picture elp: {}(ms)", (av_gettime_relative() - t1) / 1000);

			av_frame_unref(frame);

			if (is->_hwaccel != AC_HARDWAREACCELERATETYPE_DISABLED) {
				break;
			}

#if CONFIG_AVFILTER
			if (is->_hwaccel != AC_HARDWAREACCELERATETYPE_DISABLED) {
				if (is->_videoq._serial != is->_viddec._pkt_serial)
					break;
			}
		}
#endif

		if (ret < 0)
			goto the_end;

	}

the_end:
#if CONFIG_AVFILTER
	avfilter_graph_free(&graph);
#endif
	av_frame_free(&frame);
	return 0;
}

int subtitle_thread(void *arg)
{
	TyyVideoState *is = (TyyVideoState *)arg;
	Frame *sp;
	int got_subtitle;
	double pts;

	for (;;) {

		if (!(sp = is->_subpq.frame_queue_peek_writable()))
			return 0;

		if ((got_subtitle = is->_subdec.decoder_decode_frame(NULL, &sp->sub)) < 0)
			break;

		pts = 0;

		if (got_subtitle && sp->sub.format == 0) {
			if (sp->sub.pts != AV_NOPTS_VALUE)
				pts = sp->sub.pts / (double)AV_TIME_BASE;

			sp->pts = pts;
			sp->serial = is->_subdec._pkt_serial;
			sp->width = is->_subdec._avctx->width;
			sp->height = is->_subdec._avctx->height;
			sp->uploaded = 0;

			is->_subpq.frame_queue_push();
		}
		else if (got_subtitle) {
			avsubtitle_free(&sp->sub);
		}
	}
	return 0;
}

int TyyVideoState::check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec)
{
	int ret = avformat_match_stream_specifier(s, st, spec);
	if (ret < 0)
		av_log(s, AV_LOG_ERROR, "Invalid stream specifier: %s.\n", spec);
	return ret;
}

AVDictionary **TyyVideoState::setup_find_stream_info_opts(AVFormatContext *s,
	AVDictionary *codec_opts)
{
	int i;
	AVDictionary **opts;

	if (!s->nb_streams)
		return NULL;

	opts = (AVDictionary **)av_calloc(s->nb_streams, sizeof(*opts));
	if (!opts) {
		av_log(NULL, AV_LOG_ERROR,
			"Could not alloc memory for stream options.\n");
		return NULL;
	}

	for (i = 0; i < s->nb_streams; i++)
		opts[i] = filter_codec_opts(codec_opts, s->streams[i]->codecpar->codec_id,
			s, s->streams[i], NULL);

	return opts;
}

AVDictionary* TyyVideoState::filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
	AVFormatContext *s, AVStream *st, AVCodec *codec)
{
	AVDictionary    *ret = NULL;
	AVDictionaryEntry *t = NULL;

	int            flags = s->oformat ? AV_OPT_FLAG_ENCODING_PARAM
		: AV_OPT_FLAG_DECODING_PARAM;
	char          prefix = 0;

	const AVClass    *cc = avcodec_get_class();

	if (!codec)
		codec = s->oformat ? (AVCodec *)avcodec_find_encoder(codec_id)
		: (AVCodec *)avcodec_find_decoder(codec_id);

	switch (st->codecpar->codec_type) {
	case AVMEDIA_TYPE_VIDEO:
		prefix = 'v';
		flags |= AV_OPT_FLAG_VIDEO_PARAM;
		break;
	case AVMEDIA_TYPE_AUDIO:
		prefix = 'a';
		flags |= AV_OPT_FLAG_AUDIO_PARAM;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		prefix = 's';
		flags |= AV_OPT_FLAG_SUBTITLE_PARAM;
		break;
	}

	while (t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX)) {
		char *p = strchr(t->key, ':');

		if (p)
			switch (check_stream_specifier(s, st, p + 1)) {
			case  1: *p = 0; break;
			case  0:         continue;
			default:         return NULL;
			}

		if (av_opt_find(&cc, t->key, NULL, flags, AV_OPT_SEARCH_FAKE_OBJ) ||
			!codec ||
			(codec->priv_class &&
				av_opt_find(&codec->priv_class, t->key, NULL, flags,
					AV_OPT_SEARCH_FAKE_OBJ)))
			av_dict_set(&ret, t->key, t->value, 0);
		else if (t->key[0] == prefix &&
			av_opt_find(&cc, t->key + 1, NULL, flags,
				AV_OPT_SEARCH_FAKE_OBJ))
			av_dict_set(&ret, t->key + 1, t->value, 0);

		if (p)
			*p = ':';
	}

	return ret;
}

int TyyVideoState::stream_component_open(int stream_index)
{
	AVFormatContext *ic = _ic;
	AVCodecContext *avctx;
	AVCodec *codec;
	const char *forced_codec_name = NULL;
	AVDictionary *opts = NULL;
	AVDictionaryEntry *t = NULL;
	int sample_rate, nb_channels;
	int64_t channel_layout;
	int ret = 0;
	int stream_lowres = _lowres;
	AVHWDeviceType type;
	int i = 0;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return -1;

	avctx = avcodec_alloc_context3(NULL);
	if (!avctx)
		return AVERROR(ENOMEM);

	ret = avcodec_parameters_to_context(avctx, ic->streams[stream_index]->codecpar);
	if (ret < 0)
		goto fail;

	avctx->pkt_timebase = ic->streams[stream_index]->time_base;

	if (avctx->coded_width == 0 || avctx->coded_height == 0) {
		avctx->coded_width = avctx->width;
		avctx->coded_height = avctx->height;
	}

	codec = (AVCodec *)avcodec_find_decoder(avctx->codec_id);

	switch (avctx->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		_last_audio_stream = stream_index;
		forced_codec_name = _audio_codec_name;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		_last_subtitle_stream = stream_index;
		forced_codec_name = _subtitle_codec_name;
		break;
	case AVMEDIA_TYPE_VIDEO:
		_last_video_stream = stream_index;
		forced_codec_name = _video_codec_name;
		break;
	}

	if (forced_codec_name)
		codec = (AVCodec *)avcodec_find_decoder_by_name(forced_codec_name);
	if (!codec) {
		if (forced_codec_name)
			TYYERROR("No codec could be found with name '{}'", forced_codec_name);
		else
			TYYERROR("No decoder could be found for codec {}", avcodec_get_name(avctx->codec_id));

		ret = AVERROR(EINVAL);
		goto fail;
	}

	avctx->codec_id = codec->id;

	if (stream_lowres > codec->max_lowres) {
		av_log(avctx, AV_LOG_WARNING, "The maximum value for lowres supported by the decoder is %d\n", codec->max_lowres);
		stream_lowres = codec->max_lowres;
	}
	avctx->lowres = stream_lowres;

	if (_fast)
		avctx->flags2 |= AV_CODEC_FLAG2_FAST;

	opts = filter_codec_opts(_codec_opts, avctx->codec_id, ic, ic->streams[stream_index], codec);

	t = NULL;
	while ((t = av_dict_get(opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
		TYYINFO("codec key: {}, value: {}", t->key, t->value);
	}

	if (!av_dict_get(opts, "threads", NULL, 0))
		av_dict_set(&opts, "threads", "auto", 0);
	if (stream_lowres)
		av_dict_set_int(&opts, "lowres", stream_lowres, 0);

	{
		int a = -1, b = -1, c = -1;
		get_fast_version(a, b, c);
		if (a < 58) {

			if (avctx->codec_type == AVMEDIA_TYPE_VIDEO || avctx->codec_type == AVMEDIA_TYPE_AUDIO)
				av_dict_set(&opts, "refcounted_frames", "1", 0);
		}
		else {

			av_dict_set(&opts, "flags", "+copy_opaque", AV_DICT_MULTIKEY);
		}
	}

	if (avctx->codec_type == AVMEDIA_TYPE_VIDEO) {
		if (AC_HARDWAREACCELERATETYPE_D3D11VA == _hwaccel) {
			type = av_hwdevice_find_type_by_name("d3d11va");
			if (type == AV_HWDEVICE_TYPE_NONE) {
				TYYERROR("Device type {} is not supported.", "d3d11va");
				TYYINFO("Available device types:");
				while ((type = av_hwdevice_iterate_types(type)) != AV_HWDEVICE_TYPE_NONE)
					TYYINFO(" {}", av_hwdevice_get_type_name(type));
				ret = AVERROR(EINVAL);
				goto fail;
			}

			for (i = 0;; i++) {
				const AVCodecHWConfig* config = avcodec_get_hw_config(codec, i);
				if (!config) {
					TYYERROR("Decoder {} does not support device type.",
						codec->name);
					ret = AVERROR(EINVAL);
					goto fail;
				}
				if (config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX &&
					config->methods & AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX &&
					config->device_type == type) {
					_hw_pix_fmt = config->pix_fmt;
					break;
				}
			}

			_ffmpeg_hw = (TFFMPEG_HW*)av_mallocz(sizeof(TFFMPEG_HW));
			if (!_ffmpeg_hw) {
				TYYERROR("TFFMPEG_HW malloc failed");
				ret = AVERROR(ENOMEM);
				goto fail;
			}
			_d3d11va_decoder = (TD3D11VA_Decoder*)av_mallocz(sizeof(TD3D11VA_Decoder));
			if (!_ffmpeg_hw) {
				TYYERROR("TD3D11VA_Decoder malloc failed");
				ret = AVERROR(ENOMEM);
				goto fail;
			}
			avctx->get_format = TD3D11VA_Decoder::d3d11_get_hw_format;
			if ((ret = _ffmpeg_hw->hw_decoder_init(avctx, type, _h_display_window)) < 0) {
				TYYERROR("hw_decoder_init failed, {}", get_ffmpeg_error(ret).c_str());
				goto fail;
			}
			_d3d11va_decoder->_hwnd = _h_display_window;
			ret = _d3d11va_decoder->d3d11_init(avctx, type);
			if (ret < 0) {
				TYYERROR("init_d3d11_from_ffmpeg failed");
				goto fail;
			}
			avctx->pix_fmt = _hw_pix_fmt;
		}
	}

	if ((ret = avcodec_open2(avctx, codec, &opts)) < 0) {
		goto fail;
	}
	if ((t = av_dict_get(opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		av_log(NULL, AV_LOG_ERROR, "Option %s not found.\n", t->key);
		ret = AVERROR_OPTION_NOT_FOUND;
		goto fail;
	}

	_eof = 0;
	ic->streams[stream_index]->discard = AVDISCARD_DEFAULT;

	TYYDEBUG("avctx->codec_type: {}", avctx->codec_type);

	switch (avctx->codec_type) {

	case AVMEDIA_TYPE_AUDIO: {
#if CONFIG_AVFILTER
		_audio_filter_src.channels = avctx->channels;
		_audio_filter_src.channel_layout = get_valid_channel_layout(avctx->channel_layout, avctx->channels);
		_audio_filter_src.fmt = avctx->sample_fmt;
		_audio_filter_src.freq = avctx->sample_rate;
		sample_rate = avctx->sample_rate;
		nb_channels = avctx->channels;
		channel_layout = avctx->channel_layout;
#else
		sample_rate = avctx->sample_rate;
		nb_channels = avctx->channels;
		channel_layout = avctx->channel_layout;
#endif

		if ((ret = audio_open(this, channel_layout, nb_channels, sample_rate, &_audio_tgt)) < 0)
			goto fail;

		_audio_hw_buf_size = ret;
		_audio_src = _audio_tgt;

		_audio_buf_size = 0;
		_audio_buf_index = 0;

		_audio_diff_avg_coef = exp(log(0.01) / AUDIO_DIFF_AVG_NB);
		_audio_diff_avg_count = 0;

		_audio_diff_threshold = (double)(_audio_hw_buf_size) / _audio_tgt.bytes_per_sec;

		_audio_stream = stream_index;
		_audio_st = ic->streams[stream_index];

		_auddec.decoder_init(avctx, &_audioq, _continue_read_thread);

		if ((_ic->iformat->flags & (AVFMT_NOBINSEARCH | AVFMT_NOGENSEARCH | AVFMT_NO_BYTE_SEEK)) && !_ic->iformat->read_seek) {
			_auddec._start_pts = _audio_st->start_time;
			_auddec._start_pts_tb = _audio_st->time_base;
		}

		if ((ret = _auddec.decoder_start(audio_thread, "audio_decoder", this)) < 0)
			goto out;

		SDL_PauseAudioDevice(_audio_dev, 0);
		break;
	}

	case AVMEDIA_TYPE_VIDEO: {
		_video_stream = stream_index;
		_video_st = ic->streams[stream_index];

		_viddec.decoder_init(avctx, &_videoq, _continue_read_thread);

		if ((ret = _viddec.decoder_start(video_thread, "video_decoder", this)) < 0)
			goto out;
		_queue_attachments_req = 1;
		break;
	}

	case AVMEDIA_TYPE_SUBTITLE:
		_subtitle_stream = stream_index;
		_subtitle_st = ic->streams[stream_index];

		_subdec.decoder_init(avctx, &_subtitleq, _continue_read_thread);
		if ((ret = _subdec.decoder_start(subtitle_thread, "subtitle_decoder", this)) < 0)
			goto out;
		break;
	default:
		break;
	}
	goto out;

fail:
	avcodec_free_context(&avctx);
	if (_ffmpeg_hw) {
		_ffmpeg_hw->hw_decoder_uninit();
		av_free(_ffmpeg_hw);
		_ffmpeg_hw = NULL;
	}
	if (_d3d11va_decoder) {
		_d3d11va_decoder->d3d11_uninit();
		av_free(_d3d11va_decoder);
		_d3d11va_decoder = NULL;
	}
out:
	av_dict_free(&opts);

	return ret;
}

void TyyVideoState::toggle_pause()
{
	stream_toggle_pause();
	_step = 0;
}

void TyyVideoState::stream_toggle_pause()
{
	if (_paused) {
		_frame_timer += av_gettime_relative() / 1000000.0 - _vidclk._last_updated;
		if (_read_pause_return != AVERROR(ENOSYS)) {
			_vidclk._paused = 0;
		}

		_vidclk.set_clock(_vidclk.get_clock(), _vidclk._serial);
	}

	_extclk.set_clock(_extclk.get_clock(), _extclk._serial);
	_paused = _audclk._paused = _vidclk._paused = _extclk._paused = !_paused;
}

void TyyVideoState::step_to_next_frame()
{

	if (_paused)
		stream_toggle_pause();
	_step = 1;
}

int TyyVideoState::stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue) {

	return stream_id < 0 ||
		queue->_abort_request ||
		(st->disposition & AV_DISPOSITION_ATTACHED_PIC) ||
		queue->_nb_packets > MIN_FRAMES
		&& (!queue->_duration ||
			av_q2d(st->time_base) * queue->_duration > 1.0);
}

void TyyVideoState::stream_seek(int64_t pos, int64_t rel, int seek_by_bytes)
{

	if (!_seek_req) {
		_seek_pos = pos;
		_seek_rel = rel;
		_seek_flags &= ~AVSEEK_FLAG_BYTE;
		if (seek_by_bytes)
			_seek_flags |= AVSEEK_FLAG_BYTE;
		_seek_req = 1;
		SDL_CondSignal(_continue_read_thread);
	}
}

std::string TyyVideoState::get_ffmpeg_error(int err_val) {
	char str_error[512] = { 0 };
	av_strerror(err_val, str_error, sizeof(str_error) - 1);
	return str_error;
}

void TyyVideoState::stream_close(){

	TYYDEBUG("stream_close start");

	_abort_request = 1;

	if (_read_tid != NULL) {
		SDL_WaitThread(_read_tid, NULL);
		_read_tid = NULL;
	}

	TYYDEBUG("stream_close SDL_WaitThread ok");

	if (_write_tid != NULL) {
		SDL_WaitThread(_write_tid, NULL);
		_write_tid = NULL;
	}

	TYYDEBUG("stream_close while ok");

	if (_audio_stream >= 0)
		stream_component_close(_audio_stream);
	if (_video_stream >= 0)
		stream_component_close(_video_stream);
	if (_subtitle_stream >= 0)
		stream_component_close(_subtitle_stream);

	avformat_close_input(&_ic);

	TYYDEBUG("stream_component_close ok");

	if (_videoq._mutex) {
		_videoq.packet_queue_destroy();
		_videoq._mutex = NULL;
	}
	if (_audioq._mutex) {
		_audioq.packet_queue_destroy();
		_audioq._mutex = NULL;
	}
	if (_subtitleq._mutex) {
		_subtitleq.packet_queue_destroy();
		_subtitleq._mutex = NULL;
	}

	TYYDEBUG("packet_queue_destroy ok");

	_pictq.frame_queue_destory();
	_sampq.frame_queue_destory();
	_subpq.frame_queue_destory();

	TYYDEBUG("frame_queue_destory ok");

	if (_continue_read_thread != NULL) {
		SDL_DestroyCond(_continue_read_thread);
		_continue_read_thread = NULL;
	}

	TYYDEBUG("_continue_read_thread ok");

	if (_img_convert_ctx) {
		sws_freeContext(_img_convert_ctx);
		_img_convert_ctx = NULL;
	}
	if (_sub_convert_ctx) {
		sws_freeContext(_sub_convert_ctx);
		_sub_convert_ctx = NULL;
	}

	TYYDEBUG("sws_freeContext ok");

	TYYDEBUG("stream_close end");

}

int TyyVideoState::open_input() {
	TYYTRACE("open_input start");

	AVFormatContext *ic = NULL;
	int err, i, ret = -1;

	AVDictionaryEntry *t;
	int scan_all_pmts_set = 0;
	int64_t api_t1;

	_last_video_stream = _video_stream = -1;
	_last_audio_stream = _audio_stream = -1;
	_last_subtitle_stream = _subtitle_stream = -1;

	_eof = 0;

	ic = avformat_alloc_context();
	if (!ic) {
		TYYERROR("Could not allocate context");
		ret = TYY_PLAYER_ERROR_ALLOC_FAILED;
		goto fail;
	}

	ic->interrupt_callback.callback = decode_interrupt_cb;
	ic->interrupt_callback.opaque = this;

	{
		if (!strncmp(_filename.c_str(), "rtsp", 4)) {
			av_dict_set(&_format_opts, "rtsp_transport", "tcp", 0);
		}

		av_dict_set(&_format_opts, "analyzeduration", "2000000", 0);
		av_dict_set(&_format_opts, "probesize", "10240", 0);
	}

	if (!av_dict_get(_format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE)) {
		av_dict_set(&_format_opts, "scan_all_pmts", "1", AV_DICT_DONT_OVERWRITE);
		scan_all_pmts_set = 1;
	}

	t = NULL;
	while ((t = av_dict_get(_format_opts, "", t, AV_DICT_IGNORE_SUFFIX))) {
		TYYINFO("key: {}, value: {}", t->key, t->value);
	}

	api_t1 = av_gettime_relative();
	_last_read_packet_time = av_gettime();
	TYYINFO("_timeout: {}", _timeout);
	err = avformat_open_input(&ic, _filename.c_str(), _iformat, &_format_opts);
	if (err < 0) {
		TYYERROR("avformat_open_input {} faild, use time(ms): {}, errstr: {}",
			_filename.c_str(), (av_gettime_relative() - api_t1) / 1000.0, get_ffmpeg_error(err).c_str());
		ret = TYY_PLAYER_ERROR_OPEN_INPUT_FAILED;
		goto fail;
	}

	TYYDEBUG("open input file successful, use time(ms): {}, url: {}",
		(av_gettime_relative() - api_t1) / 1000.0, _filename.c_str());

	if (scan_all_pmts_set)
		av_dict_set(&_format_opts, "scan_all_pmts", NULL, AV_DICT_MATCH_CASE);

	if ((t = av_dict_get(_format_opts, "", NULL, AV_DICT_IGNORE_SUFFIX))) {
		TYYERROR("Option {} not found", t->key);
		ret = TYY_PLAYER_ERROR_OPTION_NOT_FOUND;
		goto fail;
	}

	_ic = ic;

	if (_genpts)
		ic->flags |= AVFMT_FLAG_GENPTS;

	av_format_inject_global_side_data(ic);

	if (_find_stream_info) {

		AVDictionary **opts = setup_find_stream_info_opts(ic, _codec_opts);
		int orig_nb_streams = ic->nb_streams;

		api_t1 = av_gettime_relative();
		_last_read_packet_time = av_gettime();
		ret = avformat_find_stream_info(ic, opts);

		for (i = 0; i < orig_nb_streams; i++) {

			av_dict_free(&opts[i]);
		}

		av_freep(&opts);

		if (ret < 0) {
			TYYERROR("avformat_find_stream_info faild, errstr: {}", get_ffmpeg_error(ret).c_str());
			ret = TYY_PLAYER_ERROR_FIND_STREAM_INFO_FAILED;
			goto fail;
		}

		TYYDEBUG("find stream info successful, use time(ms): {}, url: {}", (av_gettime_relative() - api_t1) / 1000.0, _filename.c_str());
	}

	ret = 0;

fail:
	if (ic && !_ic)
		avformat_close_input(&ic);

	TYYTRACE("open_input end");

	return ret;
}

int read_thread(void *arg)
{
	TyyVideoState *is = (TyyVideoState*)arg;
	if (!is) {
		TYYERROR("arg param is null");
		return -1;
	}

	TYYTRACE("WINID: {}, start", is->_win_id);

	{
		std::unique_lock<std::mutex> locker(is->_sdl_init_mutex);
		// fix: 避免阻塞Qt主线程，读线程内部等待写线程完成SDL_Init后再打开音频。
		is->_sdl_init_cond.wait(locker, [is] { return is->_sdl_init_finished || is->_abort_request; });
		if (!is->_sdl_init_success) {
			TYYERROR("WINID: {}, read thread wait sdl init failed", is->_win_id);
			return TYY_PLAYER_ERROR_PLAY_FAILED;
		}
	}

	AVFormatContext *ic = NULL;
	int i, ret;

	int st_index[AVMEDIA_TYPE_NB] = { -1, -1, -1, -1, -1 };

	AVPacket pkt1, *pkt = &pkt1;
	int64_t stream_start_time;
	int pkt_in_play_range = 0;
	SDL_mutex *wait_mutex = NULL;
	int64_t pkt_ts;
	int64_t api_t1;

	wait_mutex = SDL_CreateMutex();
	if (!wait_mutex) {
		TYYERROR("SDL_CreateMutex(): {}", SDL_GetError());
		ret = TYY_PLAYER_ERROR_ALLOC_FAILED;
		goto fail;
	}

	ic = is->_ic;

	if (ic->pb)

		ic->pb->eof_reached = 0;

	if (is->_seek_by_bytes < 0) {

		int flag = ic->iformat->flags & AVFMT_TS_DISCONT;
		int cmp = strcmp("ogg", ic->iformat->name);
		is->_seek_by_bytes = !!(flag) && cmp;
	}
	is->_max_frame_duration = (ic->iformat->flags & AVFMT_TS_DISCONT) ? 10.0 : 3600.0;

	if (is->_start_time != AV_NOPTS_VALUE) {
		int64_t timestamp;

		timestamp = is->_start_time;

		if (ic->start_time != AV_NOPTS_VALUE)

			timestamp += ic->start_time;

		ret = avformat_seek_file(ic, -1, INT64_MIN, timestamp, INT64_MAX, 0);
		if (ret < 0) {

			TYYERROR("WINID {} could not seek to position {}", is->_win_id, (double)timestamp / AV_TIME_BASE);
		}
	}

	is->_realtime = is_realtime(ic);

	for (i = 0; i < ic->nb_streams; i++) {
		AVStream *st = ic->streams[i];
		enum AVMediaType type = st->codecpar->codec_type;
		st->discard = AVDISCARD_ALL;
		if (type >= 0 && is->_wanted_stream_spec[type] && st_index[type] == -1) {
			if (avformat_match_stream_specifier(ic, st, is->_wanted_stream_spec[type]) > 0)
				st_index[type] = i;
		}
	}

	for (i = 0; i < AVMEDIA_TYPE_NB; i++) {
		if (is->_wanted_stream_spec[i] && st_index[i] == -1) {

			TYYERROR("Stream specifier {} does not match any {} stream\n", is->_wanted_stream_spec[i], av_get_media_type_string((AVMediaType)i));

			st_index[i] = -1;
		}
	}

	if (!is->_video_disable)
		st_index[AVMEDIA_TYPE_VIDEO] = av_find_best_stream(ic, AVMEDIA_TYPE_VIDEO, st_index[AVMEDIA_TYPE_VIDEO], -1, NULL, 0);
	if (!is->_audio_disable)
		st_index[AVMEDIA_TYPE_AUDIO] = av_find_best_stream(ic, AVMEDIA_TYPE_AUDIO, st_index[AVMEDIA_TYPE_AUDIO], st_index[AVMEDIA_TYPE_VIDEO], NULL, 0);
	if (!is->_video_disable && !is->_subtitle_disable)
		st_index[AVMEDIA_TYPE_SUBTITLE] = av_find_best_stream(ic, AVMEDIA_TYPE_SUBTITLE, st_index[AVMEDIA_TYPE_SUBTITLE],
		(st_index[AVMEDIA_TYPE_AUDIO] >= 0 ? st_index[AVMEDIA_TYPE_AUDIO] : st_index[AVMEDIA_TYPE_VIDEO]), NULL, 0);

	if ((ic->streams
		&& ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]
		&& ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate.den == 0
		&& ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate.num == 0)
		|| (ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate.num == 0
			&& ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate.den == 1))
	{
		is->_frame_rate = 25;
	}
	else {
		is->_frame_rate = ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate.num / ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->avg_frame_rate.den;
	}

	is->_show_mode = TyyVideoState::SHOW_MODE_NONE;

	if (ic->streams[st_index[AVMEDIA_TYPE_VIDEO]] && ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar) {
		TYYINFO("av_format_openinput() url: {}, width: {}, height: {}",
			is->_filename.c_str(),
			ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar->width,
			ic->streams[st_index[AVMEDIA_TYPE_VIDEO]]->codecpar->height);
	} else {
		TYYINFO("codecpar->width is null");
	}

	TYYDEBUG("vi :{}, ai: {}, di: {}, si: {}, ai: {}", st_index[AVMEDIA_TYPE_VIDEO], st_index[AVMEDIA_TYPE_AUDIO],
		st_index[AVMEDIA_TYPE_DATA], st_index[AVMEDIA_TYPE_SUBTITLE], st_index[AVMEDIA_TYPE_ATTACHMENT]);

	if (st_index[AVMEDIA_TYPE_AUDIO] >= 0) {
		is->stream_component_open(st_index[AVMEDIA_TYPE_AUDIO]);
	}

	ret = -1;
	if (st_index[AVMEDIA_TYPE_VIDEO] >= 0) {
		ret = is->stream_component_open(st_index[AVMEDIA_TYPE_VIDEO]);
	}
	if (is->_show_mode == TyyVideoState::SHOW_MODE_NONE) {

		is->_show_mode = ret >= 0 ? TyyVideoState::SHOW_MODE_VIDEO : TyyVideoState::SHOW_MODE_RDFT;
	}

	if (st_index[AVMEDIA_TYPE_SUBTITLE] >= 0) {
		ret = is->stream_component_open(st_index[AVMEDIA_TYPE_SUBTITLE]);

	}

	if (is->_video_stream < 0 && is->_audio_stream < 0) {
		TYYERROR("Failed to open file '{}' or configure filtergraph, err: {}",
			is->_filename, is->get_ffmpeg_error(ret).c_str());
		ret = TYY_PLAYER_ERROR_STREAM_NOT_FOUND;
		goto fail;
	}

	if (is->_infinite_buffer < 0 && is->_realtime)
		is->_infinite_buffer = 1;

	api_t1 = av_gettime_relative();
	while (true) {
		if (is->_abort_request) {
			break;
		}

		is->_last_read_packet_time = av_gettime();
		ret = av_read_frame(ic, pkt);
		if (ret >= 0) {
			if ((pkt->flags & AV_PKT_FLAG_KEY) && is->_video_stream == pkt->stream_index) {
				is->_videoq.packet_queue_put(pkt);
				break;
			}
			else {

				av_packet_unref(pkt);
			}
		}
		else {
			TYYWARN("while find key pkt error, ret: {}, url: {}", ret, is->_filename.c_str());
			if(ret == AVERROR_EOF)
				goto fail;
		}
	}

    TYYDEBUG("find key pkt, use time(ms): {}, url: {}",
             (av_gettime_relative() - api_t1) / 1000.0, is->_filename.c_str());
    TYYDEBUG("WINID: {}, enter read thread for loop, url: {}",
             is->_win_id, is->_filename.c_str());

	for (;;) {
		if (is->_abort_request)
			break;

		if (is->_paused != is->_last_paused) {
			is->_last_paused = is->_paused;
			if (is->_paused)
				is->_read_pause_return = av_read_pause(ic);
			else
				av_read_play(ic);
		}

#if CONFIG_RTSP_DEMUXER || CONFIG_MMSH_PROTOCOL
        if (is->_paused && (!strcmp(ic->iformat->name, "rtsp")
                            || (ic->pb && !strncmp(is->_filename.c_str(), "mmsh:", 5)))) {
			SDL_Delay(10);
			continue;
		}
#endif

		if (is->_seek_req) {
			int64_t seek_target = is->_seek_pos;
			int64_t seek_min = is->_seek_rel > 0 ? seek_target - is->_seek_rel + 2 : INT64_MIN;
			int64_t seek_max = is->_seek_rel < 0 ? seek_target - is->_seek_rel - 2 : INT64_MAX;

			ret = avformat_seek_file(is->_ic, -1, seek_min, seek_target, seek_max, is->_seek_flags);
			if (ret < 0) {
				av_log(NULL, AV_LOG_ERROR, "%s: error while seeking\n", is->_ic->url);
			}
			else {

				if (is->_audio_stream >= 0) {
					is->_audioq.packet_queue_flush();
					is->_audioq.packet_queue_put(&flush_pkt);
				}
				if (is->_subtitle_stream >= 0) {
					is->_subtitleq.packet_queue_flush();
					is->_subtitleq.packet_queue_put(&flush_pkt);
				}
				if (is->_video_stream >= 0) {
					is->_videoq.packet_queue_flush();
					is->_videoq.packet_queue_put(&flush_pkt);
				}
				if (is->_seek_flags & AVSEEK_FLAG_BYTE) {
					is->_extclk.set_clock(NAN, 0);
				}
				else {

					is->_extclk.set_clock(seek_target / (double)AV_TIME_BASE, 0);
				}

				TYYDEBUG("read thread seek ok, video pkt queue: {}, audio pkt queue: {}",
					is->_videoq._nb_packets, is->_audioq._nb_packets);
			}
			is->_seek_req = 0;
			is->_queue_attachments_req = 1;
			is->_eof = 0;
			if (is->_paused)
				is->step_to_next_frame();
		}

		if (is->_queue_attachments_req) {

			if (is->_video_st && is->_video_st->disposition & AV_DISPOSITION_ATTACHED_PIC) {
				AVPacket copy = { 0 };
				if ((ret = av_packet_ref(&copy, &is->_video_st->attached_pic)) < 0)
					goto fail;
				is->_videoq.packet_queue_put(&copy);
				is->_videoq.packet_queue_put_nullpacket(is->_video_stream);
			}
			is->_queue_attachments_req = 0;
		}

		if (is->_infinite_buffer < 1 &&
			(is->_audioq._size + is->_videoq._size + is->_subtitleq._size > MAX_QUEUE_SIZE
				|| (is->stream_has_enough_packets(is->_audio_st, is->_audio_stream, &is->_audioq) &&
					is->stream_has_enough_packets(is->_video_st, is->_video_stream, &is->_videoq) &&
					is->stream_has_enough_packets(is->_subtitle_st, is->_subtitle_stream, &is->_subtitleq)))) {

			SDL_LockMutex(wait_mutex);

			SDL_CondWaitTimeout(is->_continue_read_thread, wait_mutex, 10);
			SDL_UnlockMutex(wait_mutex);
			continue;
		}

		if (!is->_paused
			&&
			(!is->_audio_st
				|| (is->_auddec._finished == is->_audioq._serial
					&& is->_sampq.frame_queue_nb_remaining() == 0))
			&& (!is->_video_st
				|| (is->_viddec._finished == is->_videoq._serial
					&& is->_pictq.frame_queue_nb_remaining() == 0)))
		{
			if (is->_realtime) {
				ret = TYY_PLAYER_ERROR_EOF;

				TYYERROR("_realtime flush nullpacket ok, and present decoder all frame, read thread exit, url: {}", is->_filename.c_str());
				goto fail;
			}
			if (is->_loop != 1  && (!is->_loop || --is->_loop)) {

				is->stream_seek(is->_start_time != AV_NOPTS_VALUE ? is->_start_time : 0, 0, 0);
			}
			else if (is->_autoexit) {

				TYYWARN("_autoexit, url: {}", is->_filename.c_str());
				ret = TYY_PLAYER_ERROR_EOF;
				goto fail;
			}
		}

		is->_last_read_packet_time = av_gettime();
		ret = av_read_frame(ic, pkt);

		if (ret < 0) {
			TYYWARN("av_read_frame ret < 0, ret: {}, is->_eof: {}", is->get_ffmpeg_error(ret).c_str(), is->_eof);
			if ((ret == AVERROR_EOF || avio_feof(ic->pb)) && !is->_eof)
			{

				if (is->_video_stream >= 0)
					is->_videoq.packet_queue_put_nullpacket(is->_video_stream);
				if (is->_audio_stream >= 0)
					is->_audioq.packet_queue_put_nullpacket(is->_audio_stream);
				if (is->_subtitle_stream >= 0)
					is->_subtitleq.packet_queue_put_nullpacket(is->_subtitle_stream);
				is->_eof = 1;
				TYYWARN("read_frame read EOF, url: {}", is->_filename.c_str());
			}

			if (ic->pb && ic->pb->error) {

				TYYERROR("{} ic->pb->error, errstr: {}", is->_filename.c_str(), is->get_ffmpeg_error(ret).c_str());
				ret = TYY_PLAYER_ERROR_NETWORK_FAILED;

				goto fail;
			}

			SDL_LockMutex(wait_mutex);
			SDL_CondWaitTimeout(is->_continue_read_thread, wait_mutex, 10);
			SDL_UnlockMutex(wait_mutex);

			continue;
		}
		else {
			is->_eof = 0;
		}

		stream_start_time = ic->streams[pkt->stream_index]->start_time;
		pkt_ts = pkt->pts == AV_NOPTS_VALUE ? pkt->dts : pkt->pts;

		pkt_in_play_range = is->_duration == AV_NOPTS_VALUE ||
			(pkt_ts - (stream_start_time != AV_NOPTS_VALUE ? stream_start_time : 0)) *
			av_q2d(ic->streams[pkt->stream_index]->time_base) -
			(double)(is->_start_time != AV_NOPTS_VALUE ? is->_start_time : 0) / 1000000
			<= ((double)is->_duration / 1000000);

		if (pkt->stream_index == is->_audio_stream && pkt_in_play_range) {
			is->_audioq.packet_queue_put(pkt);
		}else if (pkt->stream_index == is->_video_stream && pkt_in_play_range
			&& !(is->_video_st->disposition & AV_DISPOSITION_ATTACHED_PIC)) {
			is->_videoq.packet_queue_put(pkt);
		}else if (pkt->stream_index == is->_subtitle_stream && pkt_in_play_range) {
			is->_subtitleq.packet_queue_put(pkt);
		}else {
			TYYDEBUG("drop pkt type: {}",
				pkt->stream_index == is->_video_stream ? "video" :
				pkt->stream_index == is->_audio_stream ? "audio" :
				"unkown");
			av_packet_unref(pkt);
		}

	}

	ret = 0;

fail:
	if (ic && !is->_ic)
		avformat_close_input(&ic);

	if (is->_abort_request) {
		TYYDEBUG("receive abort request, _abort_request: {}", is->_abort_request);
		is->_abort_request_w = 1;
	}

	if (is->_abort_request == 0 && ret != 0) {
		TYYDEBUG("receive beidong abort request, _abort_request: {}", is->_abort_request);
		is->_abort_request_w = 2;
		if (is->_event_callback) {

			std::thread asyncth(is->_event_callback,
                                is->_event_user_data,
                                TYY_PLAYER_EVENT_DISCONNECT,
                                ret);
			asyncth.detach();
			TYYDEBUG("WINID: {}, start async call to handle disconnect", is->_win_id);
		}
	}

	if (wait_mutex) {
		SDL_DestroyMutex(wait_mutex);
		wait_mutex = NULL;
	}

	TYYERROR("WINID: {}, read_thread exit, ret: {}", is->_win_id, ret);
	return 0;
}

void TyyVideoState::read_thread_close() {
	if (_ic) {
		avformat_close_input(&_ic);
		_ic = NULL;
	}
}

void TyyVideoState::deinit() {

	_videoq.packet_queue_destroy();
	_audioq.packet_queue_destroy();
	_subtitleq.packet_queue_destroy();

	_pictq.frame_queue_destory();
	_sampq.frame_queue_destory();
	_subpq.frame_queue_destory();

	if (_continue_read_thread != NULL) {
		SDL_DestroyCond(_continue_read_thread);
		_continue_read_thread = NULL;
	}

}

bool TyyVideoState::sdl_init() {

	TYYTRACE("sdl_init start");

	int flags;

	if (_h_display_window == NULL) {
		TYYERROR("sdl_init handle is null");
		return false;
	}

	flags = SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER;

	if (_audio_disable) {
		flags &= ~SDL_INIT_AUDIO;
	}
	else {
		if (!SDL_getenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE"))
			SDL_setenv("SDL_AUDIO_ALSA_SET_BUFFER_SIZE", "1", 1);
	}

	if (_video_disable)
		flags &= ~SDL_INIT_VIDEO;

	int64_t api_t1;
	api_t1 = av_gettime_relative();
	static std::once_flag sdlinitflag;
	std::call_once(sdlinitflag, [=] {
        if (SDL_Init(flags)) {
            TYYERROR("Could not initialize SDL, errno: {}", SDL_GetError());
            TYYERROR("Did you set the DISPLAY variable?");
            return false;
        }
        TYYDEBUG("SDL_Init successful, use time(ms): {}", (av_gettime_relative() - api_t1) / 1000.0);
	});
    TYYDEBUG("SDL_Init successful, use time(ms): {}, url: {}",
             (av_gettime_relative() - api_t1) / 1000.0, _filename.c_str());

	SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
	SDL_EventState(SDL_USEREVENT, SDL_IGNORE);
	SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);
	SDL_EventState(SDL_WINDOWEVENT_SIZE_CHANGED, SDL_IGNORE);
	SDL_EventState(SDL_WINDOWEVENT_RESIZED, SDL_IGNORE);

	av_init_packet(&flush_pkt);
	flush_pkt.data = (uint8_t *)&flush_pkt;

	_window = SDL_CreateWindowFrom(_h_display_window);
	if (!_window) {
		TYYDEBUG("SDL_CreateWindowFrom get null window");
	}

	SDL_ShowWindow(_window);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

	if (_window) {

		_renderer = SDL_CreateRenderer(_window, -1, SDL_RENDERER_ACCELERATED);
		if (!_renderer) {
			TYYWARN("Failed to initialize a hardware accelerated renderer: {}", SDL_GetError());
			_renderer = SDL_CreateRenderer(_window, -1, 0);
		}
		if (_renderer) {
			if (!SDL_GetRendererInfo(_renderer, &_renderer_info)) {
				TYYINFO("Initialized {} renderer.", _renderer_info.name);
			}
			SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);
			SDL_RenderClear(_renderer);
			SDL_RenderPresent(_renderer);
		}
	}

	if (!_window || !_renderer || !_renderer_info.num_texture_formats) {
		TYYERROR("Failed to create window or renderer: {}, winId: {}", SDL_GetError(), _win_id);
		return false;
	}

	TYYTRACE("sdl_init end");

	return true;
}

void TyyVideoState::sdl_destroy() {
	if (_renderer) {
		SDL_RenderClear(_renderer);
		SDL_RenderPresent(_renderer);
		TYYDEBUG("SDL_RenderClear _renderer ok");
	}
	if (_vis_texture) {
		SDL_DestroyTexture(_vis_texture);
		_vis_texture = NULL;
		TYYDEBUG("clean _vis_texture ok");
	}
	if (_vid_texture) {
		SDL_DestroyTexture(_vid_texture);
		_vid_texture = NULL;
		TYYDEBUG("clean _vid_texture ok");
	}
	if (_sub_texture) {
		SDL_DestroyTexture(_sub_texture);
		_sub_texture = NULL;
		TYYDEBUG("clean _sub_texture ok");
	}

	if (_renderer) {
		SDL_DestroyRenderer(_renderer);
		_renderer = NULL;
		TYYDEBUG("clean _renderer ok");
	}

	if (_window) {
		SDL_DestroyWindow(_window);
		_window = NULL;
#ifdef _WIN32
		ShowWindow(_h_display_window, 1);
#endif
		TYYDEBUG("clean _window ok");

	}
}

bool TyyVideoState::init(const Properties &properties) {

	TYYTRACE("start");

	std::string url = properties.get_property(TYY_VIDEO_STATE_PROPERTY_URL, std::string());
	uint64_t win_id = properties.get_property(TYY_VIDEO_STATE_PROPERTY_WIN_ID, static_cast<uint64_t>(0));
	HWND handle = reinterpret_cast<HWND>(static_cast<uintptr_t>(win_id));

	if (handle == NULL) {
		TYYERROR("handle error, it is null");
		return false;
	}
	if (url.empty()) {
		TYYERROR("url error, it is empty");
		return false;
	}

	video_state_memset_zero();

	_properties = properties;
	_filename = url;
	_h_display_window = handle;
	_win_id = static_cast<int>(win_id);
#ifdef _WIN32
	TYYINFO("video state hwnd: {}, is_window: {}", fmt::ptr(_h_display_window), IsWindow(_h_display_window));
#else
	TYYINFO("video state window id: {}", win_id);
#endif

	_audio_disable = 0;

#if CONFIG_AVDEVICE
	avdevice_register_all();
#endif
	avformat_network_init();

	_iformat = NULL;
	_ytop = 0;
	_xleft = 0;

	if (_pictq.frame_queue_init(&_videoq, VIDEO_PICTURE_QUEUE_SIZE, 1) > 0) {
		goto fail;
	}
	if (_subpq.frame_queue_init(&_subtitleq, SUBPICTURE_QUEUE_SIZE, 0) < 0) {
		goto fail;
	}
	if (_sampq.frame_queue_init(&_audioq, SAMPLE_QUEUE_SIZE, 1) < 0) {
		goto fail;
	}

	if (_videoq.packet_queue_init() < 0) {
		goto fail;
	}
	if (_audioq.packet_queue_init() < 0) {
		goto fail;
	}
	if (_subtitleq.packet_queue_init() < 0) {
		goto fail;
	}

	if (!(_continue_read_thread = SDL_CreateCond())) {
		TYYERROR("create _continue_read_thread SDL_CreateCond() failed, errno: {}", SDL_GetError());
		goto fail;
	}

	_vidclk.init_clock(&_videoq._serial);
	_audclk.init_clock(&_audioq._serial);
	_extclk.init_clock(&_extclk._serial);
	_audio_clock_serial = -1;

	_startup_volume = 50;

	_startup_volume = av_clip(_startup_volume, 0, 100);
	_startup_volume = av_clip(SDL_MIX_MAXVOLUME * _startup_volume / 100, 0, SDL_MIX_MAXVOLUME);
	_audio_volume = _startup_volume;
	_muted = 0;
	_av_sync_type = AV_SYNC_AUDIO_MASTER;

	_frame_rate = -1;
	_capture_count = 0;
	_capture_path = "";
	_cursor_hidden = 0;

	_abort_request = 0;
	_write_packet_fast = 0;
	_timeout = 5;

	_video_codec_name = _properties.get_property("vcodec", "") == std::string("") ? NULL : _properties.get_property("vcodec", "");
	_audio_codec_name = _properties.get_property("acodec", "") == std::string("") ? NULL : _properties.get_property("acodec", "");
	_subtitle_codec_name = _properties.get_property("scodec", "") == std::string("") ? NULL : _properties.get_property("scodec", "");

	_hwaccel = (ACHardwareAccelerateType)(_properties.get_property("hwaccel_type", 0) == 0
		? AC_HARDWAREACCELERATETYPE_DISABLED : _properties.get_property("hwaccel_type", 0));

	TYYTRACE("end");

	return true;

fail:
	avformat_network_deinit();
	_pictq.frame_queue_destory();
	_subpq.frame_queue_destory();
	_sampq.frame_queue_destory();
	_videoq.packet_queue_destroy();
	_audioq.packet_queue_destroy();
	_subtitleq.packet_queue_destroy();
	return false;
}

void TyyVideoState::video_state_memset_zero() {

	TYYDEBUG("video_state_memset_zero memset zero start");

	_read_tid = NULL;
	_write_tid = NULL;
	_iformat = NULL;
	_abort_request = 0;
	_abort_request_w = 0;
	_force_refresh = 0;
	_paused = 0;
	_last_paused = 0;
	_queue_attachments_req = 0;
	_seek_req = 0;
	_seek_flags = 0;
	_seek_pos = 0;
	_seek_rel = 0;
	_read_pause_return = 0;

	_filename = "";
	_h_display_window = NULL;
	_window = NULL;
	_renderer = NULL;
	_renderer_info = { 0 };
	_width = 0;
	_height = 0;
	_xleft = 0;
	_ytop = 0;
	_continue_read_thread = NULL;
	_audio_clock_serial = 0;
	_startup_volume = 50;
	_audio_volume = 0;
	_muted = 0;
	_av_sync_type = AV_SYNC_AUDIO_MASTER;

	_frame_rate = -1;
	_capture_count = 0;
	_capture_path.clear();

	_last_video_stream = _video_stream = -1;
	_last_audio_stream = _audio_stream = -1;
	_last_subtitle_stream = _subtitle_stream = -1;
	_eof = 0;
	_ic = NULL;
	_max_frame_duration = 0;
	_realtime = 0;
	_show_mode = SHOW_MODE_NONE;
#if CONFIG_AVFILTER
	memset(&_audio_filter_src, 0, sizeof(_audio_filter_src));
#endif
#if CONFIG_AVFILTER
	_vfilter_idx = 0;
	_in_video_filter = NULL;
	_out_video_filter = NULL;
	_in_audio_filter = NULL;
	_out_audio_filter = NULL;
	_agraph = NULL;
#endif

	_audio_clock = 0;

	_audio_diff_cum = 0;
	_audio_diff_avg_coef = 0;
	_audio_diff_threshold = 0;
	_audio_diff_avg_count = 0;

	_audio_st = NULL;

	_audio_hw_buf_size = 0;

	_audio_buf = NULL;
	_audio_buf1 = NULL;
	_audio_buf_size = 0;
	_audio_buf1_size = 0;
	_audio_buf_index = 0;

	_audio_write_buf_size = 0;

	memset(&_audio_src, 0, sizeof(_audio_src));
	memset(&_audio_tgt, 0, sizeof(_audio_tgt));
	_swr_ctx = NULL;
	_frame_drops_early = 0;
	_frame_drops_late = 0;

	memset(_sample_array, 0, sizeof(_sample_array));
	_sample_array_index = 0;
	_last_i_start = 0;
	_rdft = NULL;
	_rdft_bits = 0;
	_rdft_data = NULL;

	_xpos = 0;
	_last_vis_time = 0;
	_vis_texture = NULL;

	_sub_texture = NULL;
	_vid_texture = NULL;

	_subtitle_st = NULL;

	_frame_timer = 0;
	_frame_last_returned_time = 0;
	_frame_last_filter_delay = 0;

	_video_st = NULL;

	_img_convert_ctx = NULL;
	_sub_convert_ctx = NULL;

	_step = 0;

	_audio_disable = 0;
	_video_disable = 0;

	_seek_by_bytes = -1;
	_start_time = AV_NOPTS_VALUE;
	_wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };
	_subtitle_disable = 0;
#if CONFIG_AVFILTER
	_vfilters_list = NULL;
	_nb_vfilters = 0;
	_afilters = NULL;
#endif
	_filter_nbthreads = 0;

	_always_on_top = 0;
	_borderless = 0;

	_file_iformat = NULL;

	_sws_dict = NULL;
	_swr_opts = NULL;
	_format_opts = NULL;
	_codec_opts = NULL;
	_resample_opts = NULL;

	_genpts = 0;
	_find_stream_info = 1;

	_window_title = NULL;

	_duration = AV_NOPTS_VALUE;
	_show_status = 1;

	_default_width = 640;
	_default_height = 480;
	_screen_width = 0;
	_screen_height = 0;
	_screen_left = SDL_WINDOWPOS_CENTERED;
	_screen_top = SDL_WINDOWPOS_CENTERED;
	_lowres = 0;
	_audio_codec_name = NULL;
	_subtitle_codec_name = NULL;
	_video_codec_name = NULL;
	_fast = 0;
	_audio_callback_time = 0;
	_audio_dev = 0;

	_decoder_reorder_pts = -1;
	_framedrop = -1;
	_infinite_buffer = -1;
	_loop = 1;
	_autoexit = 0;
	_cursor_hidden = 0;
	_cursor_last_shown = 0;
	_display_disable = 0;
	_rdft_speed = 0.02;

	_is_full_screen = 0;

	_sws_flags = SWS_BICUBIC;
	_autorotate = 1;

	_player_state = 0;

	_last_read_packet_time = av_gettime();
	_timeout = 5;

	_write_packet_fast = 0;

#ifdef USE_SONIC
	_audio_speed_convert = NULL;
	_change_speed_req = 0;
	_play_speed = 1.0f;
#endif
#if CONFIG_AVFILTER
	_req_afilter_reconfigure = 0;
	_audio_filter_speed = 1.0;
#endif

	_hwaccel = AC_HARDWAREACCELERATETYPE_DISABLED;
	_hw_pix_fmt = AV_PIX_FMT_NONE;
	_ffmpeg_hw = NULL;
	_d3d11va_decoder = NULL;

	_event_type = ROEvent2SDL_UNKOWN;

	TYYDEBUG("video_state_memset_zero memset zero end");
}

double TyyVideoState::vp_duration(Frame *vp, Frame *nextvp)
{
	if (vp->serial == nextvp->serial) {
		double duration = nextvp->pts - vp->pts;
		if (isnan(duration)
			|| duration <= 0
			|| duration > _max_frame_duration
			) {
			return vp->duration;
		}
		else {
			return duration;
		}
	}
	else {
		return 0.0;
	}
}

double TyyVideoState::compute_target_delay(double delay)
{
	double sync_threshold, diff = 0;

	if (get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {

		diff = _vidclk.get_clock() - get_master_clock();

		sync_threshold = FFMAX(AV_SYNC_THRESHOLD_MIN, FFMIN(AV_SYNC_THRESHOLD_MAX, delay));

		if (!isnan(diff) && fabs(diff) < _max_frame_duration) {

			if (diff <= -sync_threshold) {
				delay = FFMAX(0, delay + diff);
			}
			else if (diff >= sync_threshold && delay > AV_SYNC_FRAMEDUP_THRESHOLD) {

				delay = delay + diff;

			}
			else if (diff >= sync_threshold) {

				delay = 2 * delay;
			}
			else {

			}
		}
		else if (fabs(diff) < _max_frame_duration) {

		}
	}
	else {

	}

	return delay;
}

void TyyVideoState::update_video_pts(double pts, int64_t pos, int serial) {

	_vidclk.set_clock(pts, serial);
	_extclk.sync_clock_to_slave(&_vidclk);
}

void TyyVideoState::get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode)
{
	int i;
	*sdl_blendmode = SDL_BLENDMODE_NONE;
	*sdl_pix_fmt = SDL_PIXELFORMAT_UNKNOWN;

	if (format == AV_PIX_FMT_RGB32 ||
		format == AV_PIX_FMT_RGB32_1 ||
		format == AV_PIX_FMT_BGR32 ||
		format == AV_PIX_FMT_BGR32_1)
		*sdl_blendmode = SDL_BLENDMODE_BLEND;

	for (i = 0; i < FF_ARRAY_ELEMS(sdl_texture_format_map) - 1; i++) {
		if (format == sdl_texture_format_map[i].format) {
			*sdl_pix_fmt = sdl_texture_format_map[i].texture_fmt;
			return;
	}
	}
}

int TyyVideoState::realloc_texture(SDL_Texture **texture, Uint32 new_format, int new_width, int new_height, SDL_BlendMode blendmode, int init_texture)
{
	Uint32 format;
	int access, w, h;

	if (!*texture || SDL_QueryTexture(*texture, &format, &access, &w, &h) < 0 || new_width != w || new_height != h || new_format != format) {
		void *pixels;
		int pitch;
		if (*texture)
			SDL_DestroyTexture(*texture);
		if (!(*texture = SDL_CreateTexture(_renderer, new_format, SDL_TEXTUREACCESS_STREAMING, new_width, new_height)))
			return -1;
		if (SDL_SetTextureBlendMode(*texture, blendmode) < 0)
			return -1;
		if (init_texture) {
			if (SDL_LockTexture(*texture, NULL, &pixels, &pitch) < 0)
				return -1;
			memset(pixels, 0, pitch * new_height);

			SDL_UnlockTexture(*texture);
		}
		av_log(NULL, AV_LOG_VERBOSE, "Created %dx%d texture with %s.\n", new_width, new_height, SDL_GetPixelFormatName(new_format));
	}

	return 0;
}

int TyyVideoState::upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx) {
	int ret = 0;
	Uint32 sdl_pix_fmt;
	SDL_BlendMode sdl_blendmode;

	get_sdl_pix_fmt_and_blendmode(frame->format, &sdl_pix_fmt, &sdl_blendmode);

	if (realloc_texture(tex,
		sdl_pix_fmt == SDL_PIXELFORMAT_UNKNOWN ? SDL_PIXELFORMAT_ARGB8888 : sdl_pix_fmt,
		frame->width, frame->height, sdl_blendmode, 0) < 0)
		return -1;

	switch (sdl_pix_fmt) {

	case SDL_PIXELFORMAT_UNKNOWN:

		*img_convert_ctx = sws_getCachedContext(*img_convert_ctx,
			frame->width, frame->height, (AVPixelFormat)frame->format,
			frame->width, frame->height, AV_PIX_FMT_BGRA,
			_sws_flags, NULL, NULL, NULL);
		if (*img_convert_ctx != NULL) {
			uint8_t *pixels[4];
			int pitch[4];
			if (!SDL_LockTexture(*tex, NULL, (void **)pixels, pitch)) {
				sws_scale(*img_convert_ctx, (const uint8_t * const *)frame->data, frame->linesize,
					0, frame->height, pixels, pitch);
				SDL_UnlockTexture(*tex);
			}
		}
		else {
			av_log(NULL, AV_LOG_FATAL, "Cannot initialize the conversion context\n");
			ret = -1;
		}
		break;

	case SDL_PIXELFORMAT_IYUV:
		if (frame->linesize[0] > 0 && frame->linesize[1] > 0 && frame->linesize[2] > 0) {
			ret = SDL_UpdateYUVTexture(*tex, NULL,
				frame->data[0], frame->linesize[0],
				frame->data[1], frame->linesize[1],
				frame->data[2], frame->linesize[2]);
		}
		else if (frame->linesize[0] < 0 && frame->linesize[1] < 0 && frame->linesize[2] < 0) {
			ret = SDL_UpdateYUVTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0],
				frame->data[1] + frame->linesize[1] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[1],
				frame->data[2] + frame->linesize[2] * (AV_CEIL_RSHIFT(frame->height, 1) - 1), -frame->linesize[2]);
		}
		else {

			av_log(NULL, AV_LOG_ERROR, "Mixed negative and positive linesizes are not supported.\n");
			return -1;
		}
		break;

	default:
		if (frame->linesize[0] < 0) {
			ret = SDL_UpdateTexture(*tex, NULL, frame->data[0] + frame->linesize[0] * (frame->height - 1), -frame->linesize[0]);
		}
		else {
			ret = SDL_UpdateTexture(*tex, NULL, frame->data[0], frame->linesize[0]);
		}
		break;
	}

	return ret;
}

void TyyVideoState::set_sdl_yuv_conversion_mode(AVFrame *frame)
{
#if SDL_VERSION_ATLEAST(2,0,8)
	SDL_YUV_CONVERSION_MODE mode = SDL_YUV_CONVERSION_AUTOMATIC;
	if (frame && (frame->format == AV_PIX_FMT_YUV420P || frame->format == AV_PIX_FMT_YUYV422 || frame->format == AV_PIX_FMT_UYVY422)) {
		if (frame->color_range == AVCOL_RANGE_JPEG)
			mode = SDL_YUV_CONVERSION_JPEG;
		else if (frame->colorspace == AVCOL_SPC_BT709)
			mode = SDL_YUV_CONVERSION_BT709;
		else if (frame->colorspace == AVCOL_SPC_BT470BG || frame->colorspace == AVCOL_SPC_SMPTE170M || frame->colorspace == AVCOL_SPC_SMPTE240M)
			mode = SDL_YUV_CONVERSION_BT601;
	}

	SDL_SetYUVConversionMode(mode);
#endif
}

int TyyVideoState::save_frame_to_jpeg(const char *filename, const AVFrame* frame)
{
	char error_msg_buf[256] = { 0 };

	AVCodec *jpeg_codec = (AVCodec *)avcodec_find_encoder(AV_CODEC_ID_MJPEG);
	if (!jpeg_codec)
		return -1;
	AVCodecContext *jpeg_codec_ctx = avcodec_alloc_context3(jpeg_codec);
	if (!jpeg_codec_ctx)
		return -2;

	jpeg_codec_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P;
	jpeg_codec_ctx->width = frame->width;
	jpeg_codec_ctx->height = frame->height;
	jpeg_codec_ctx->time_base = { 1, 25 };
	jpeg_codec_ctx->framerate = { 25, 1 };
	AVDictionary *encoder_opts = NULL;

	av_dict_set(&encoder_opts, "flags", "+qscale", 0);
	av_dict_set(&encoder_opts, "qmax", "2", 0);
	av_dict_set(&encoder_opts, "qmin", "2", 0);

	int ret = avcodec_open2(jpeg_codec_ctx, jpeg_codec, &encoder_opts);
	if (ret < 0) {
		printf("avcodec open failed:%s\n", av_make_error_string(error_msg_buf, sizeof(error_msg_buf), ret));
		avcodec_free_context(&jpeg_codec_ctx);
		return -3;
	}
	av_dict_free(&encoder_opts);

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	ret = avcodec_send_frame(jpeg_codec_ctx, frame);
	if (ret < 0) {
		printf("Error: %s\n", av_make_error_string(error_msg_buf, sizeof(error_msg_buf), ret));
		avcodec_free_context(&jpeg_codec_ctx);
		return -4;
	}
	ret = 0;
	while (ret >= 0) {

		ret = avcodec_receive_packet(jpeg_codec_ctx, &pkt);
		if (ret == AVERROR(EAGAIN))
			continue;
		if (ret == AVERROR_EOF) {
			ret = 0;
			break;
		}

		FILE *outfile = fopen(filename, "wb");
		if (!outfile) {
			printf("fopen %s failed\n", filename);
			ret = -1;
			break;
		}
		if (fwrite((char*)pkt.data, 1, pkt.size, outfile) == pkt.size) {
			ret = 0;
		}
		else {
			printf("fwrite %s failed\n", filename);
			ret = -1;
		}
		fclose(outfile);

		ret = 0;
		break;
	}

	avcodec_free_context(&jpeg_codec_ctx);
	return ret;
}

int TyyVideoState::save_frame_to_bmp(const char *filename, const AVFrame* frame) {

	AVFrame* pFrameRGB = av_frame_alloc();
	if (nullptr == pFrameRGB) {
		return -1;
	}

	uint8_t *pictureBuf = (uint8_t*)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_BGRA, frame->width, frame->height, 1));

	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize, pictureBuf, AV_PIX_FMT_BGRA, frame->width, frame->height, 1);

	struct SwsContext *pSwsCtx = sws_getContext(frame->width, frame->height, (AVPixelFormat)frame->format, frame->width, frame->height,
		AV_PIX_FMT_BGRA, SWS_BICUBIC, NULL, NULL, NULL);

	AVFrame* pFrame = const_cast<AVFrame*>(frame);
	pFrame->data[0] += pFrame->linesize[0] * (frame->height - 1);
	pFrame->linesize[0] *= -1;
	pFrame->data[1] += pFrame->linesize[1] * (frame->height / 2 - 1);
	pFrame->linesize[1] *= -1;
	pFrame->data[2] += pFrame->linesize[2] * (frame->height / 2 - 1);
	pFrame->linesize[2] *= -1;

	sws_scale(pSwsCtx, frame->data, frame->linesize, 0, frame->height, pFrameRGB->data, pFrameRGB->linesize);

	save_as_bmp(filename, pFrameRGB, frame->width, frame->height, 0, 32);

	sws_freeContext(pSwsCtx);
	av_free(pictureBuf);
	pictureBuf = nullptr;
	av_frame_free(&pFrameRGB);

	return true;
}

bool TyyVideoState::save_as_bmp(const char *filename, AVFrame *pFrameRGB, int width, int height, int index, int bpp)
{
	FILE *fp;
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	fopen_s(&fp, filename, "wb+");
	if (nullptr == fp) {
		return FALSE;
	}

	bmpheader.bfType = 0x4d42;
	bmpheader.bfSize = (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) + (width * height * bpp / 8);
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = width;

	bmpinfo.biHeight = height;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = bpp;
	bmpinfo.biCompression = BI_RGB;

	bmpinfo.biSizeImage = width * height * bpp / 8;
	bmpinfo.biXPelsPerMeter = 100;
	bmpinfo.biYPelsPerMeter = 100;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;

	fwrite(&bmpheader, sizeof(bmpheader), 1, fp);
	fwrite(&bmpinfo, sizeof(bmpinfo), 1, fp);
	fwrite(pFrameRGB->data[0], width * height * bpp / 8, 1, fp);

	fclose(fp);

	return true;
}

void TyyVideoState::video_image_display()
{
	Frame *vp;

	vp = _pictq.frame_queue_peek_last();

	if (!vp->uploaded) {

		if (upload_texture(&_vid_texture, vp->frame, &_img_convert_ctx) < 0)
			return;
		vp->uploaded = 1;
		vp->flip_v = vp->frame->linesize[0] < 0;

	}

	set_sdl_yuv_conversion_mode(vp->frame);

	SDL_RenderCopy(_renderer, _vid_texture, NULL, NULL);

	set_sdl_yuv_conversion_mode(NULL);

}

void TyyVideoState::video_display()
{

	SDL_SetRenderDrawColor(_renderer, 0, 0, 0, 255);

	Frame* vp;
	vp = _pictq.frame_queue_peek_last();
	if(vp->format == AV_PIX_FMT_D3D11)
	{
		_d3d11va_decoder->d3d11va_retrieve_data(_viddec._avctx, vp->frame);
		return;
	}

	SDL_RenderClear(_renderer);

	if (_audio_st && _show_mode != SHOW_MODE_VIDEO)

		;
	else if (_video_st) {
		video_image_display();
	}

	SDL_RenderPresent(_renderer);

	if (_capture_count > 0 && _capture_path.empty() == false) {
		Frame *vp = _pictq.frame_queue_peek_last();

		int ret = save_frame_to_bmp(_capture_path.c_str(), vp->frame);
		if (ret < 0) {

			TYYWARN("captrue failed, ret: {}", ret);
		}

		_capture_count--;
		if (_capture_count < 0) {
			_capture_count = 0;
	}
	}
}

void video_refresh(void *opaque, double *remaining_time)
{
	TyyVideoState *is = (TyyVideoState*)opaque;
	double time;

	Frame *sp, *sp2;

	if (!is->_paused && is->get_master_sync_type() == AV_SYNC_EXTERNAL_CLOCK && is->_realtime)

	if (!is->_display_disable && is->_show_mode != TyyVideoState::SHOW_MODE_VIDEO && is->_audio_st) {

	}

	if (is->_video_st) {
	retry:

		if (is->_pictq.frame_queue_nb_remaining() == 0) {

		}
		else {
			double last_duration, duration, delay;
			Frame *vp, *lastvp;
			int framenum;

			framenum = is->_pictq.frame_queue_nb_remaining();

			lastvp = is->_pictq.frame_queue_peek_last();
			vp = is->_pictq.frame_queue_peek();

			if (vp->serial != is->_videoq._serial) {

				is->_pictq.frame_queue_next();
				goto retry;
			}

			if (lastvp->serial != vp->serial) {

				is->_frame_timer = av_gettime_relative() / 1000000.0;
			}

			if (is->_paused)
			{
				goto display;

			}

			last_duration = is->vp_duration(lastvp, vp);

			delay = is->compute_target_delay(last_duration);

			if (!(tyy_more(is->_play_speed, 0.99) && tyy_less(is->_play_speed, 1.01)) && is->_audio_stream < 0) {
				delay = delay / is->_play_speed;
			}

			time = av_gettime_relative() / 1000000.0;

			if (time < is->_frame_timer + delay) {

				*remaining_time = FFMIN(is->_frame_timer + delay - time, *remaining_time);

				if (!is->_write_packet_fast) {
					goto display;
				}else {

					if (is->get_master_sync_type() != AV_SYNC_VIDEO_MASTER) {
						goto display;
					}
					else {
						if (!is->_realtime) {
							goto display;
						}

						if (framenum >= 3) {

							delay = 0.000;
							*remaining_time = delay;

						}
						else if (framenum >= 2) {
							delay = 0.01;
							*remaining_time = delay;
#ifdef  _WIN32
							timeBeginPeriod(1);
							Sleep((int64_t)(*remaining_time * 1000.0));
							timeEndPeriod(1);
#else
							av_usleep((int64_t)(*remaining_time * 1000000.0));
#endif

							*remaining_time = 0;
						}
						else if (framenum >= 1) {
							delay = 0.015;
							*remaining_time = delay;
#ifdef  _WIN32
							timeBeginPeriod(1);
							Sleep((int64_t)(*remaining_time * 1000.0));
							timeEndPeriod(1);
#else
							av_usleep((int64_t)(*remaining_time * 1000000.0));
#endif
							*remaining_time = 0;

						}
						else {
							SDL_CondSignal(is->_continue_read_thread);

							delay = (*remaining_time);
							*remaining_time = delay;
							goto display;
						}

					}
				}

			}
			else {
			}

			if (is->_write_packet_fast && is->get_master_sync_type() == AV_SYNC_VIDEO_MASTER && is->_realtime) {
				if (framenum >= VIDEO_PICTURE_QUEUE_SIZE) {
					delay = 0.000;
					*remaining_time = delay;
				}
			}

			is->_frame_timer += delay;
			if (delay > 0 && time - is->_frame_timer > AV_SYNC_THRESHOLD_MAX) {
				is->_frame_timer = time;
			}

			SDL_LockMutex(is->_pictq._mutex);
			if (!isnan(vp->pts))
				is->update_video_pts(vp->pts, vp->pos, vp->serial);
			SDL_UnlockMutex(is->_pictq._mutex);

			if (is->_pictq.frame_queue_nb_remaining() > 1) {
				Frame *nextvp = is->_pictq.frame_queue_peek_next();
				duration = is->vp_duration(vp, nextvp);

				if (!is->_step
					&& (is->_framedrop>0 ||
					(is->_framedrop && is->get_master_sync_type() != AV_SYNC_VIDEO_MASTER))
					&& time > is->_frame_timer + duration
					) {

					is->_frame_drops_late++;
					is->_pictq.frame_queue_next();

					goto retry;
				}
			}

			if (is->_subtitle_st) {
				while (is->_subpq.frame_queue_nb_remaining() > 0) {

					sp = is->_subpq.frame_queue_peek();

					if (is->_subpq.frame_queue_nb_remaining() > 1)
						sp2 = is->_subpq.frame_queue_peek_next();
					else
						sp2 = NULL;

					if (sp->serial != is->_subtitleq._serial

						|| (is->_vidclk._pts > (sp->pts + ((float)sp->sub.end_display_time / 1000)))
						|| (sp2 && is->_vidclk._pts > (sp2->pts + ((float)sp2->sub.start_display_time / 1000))))
					{

						if (sp->uploaded) {
							int i;

							for (i = 0; i < sp->sub.num_rects; i++) {
								AVSubtitleRect *sub_rect = sp->sub.rects[i];
								uint8_t *pixels;
								int pitch, j;

								if (!SDL_LockTexture(is->_sub_texture, (SDL_Rect *)sub_rect, (void **)&pixels, &pitch)) {
									for (j = 0; j < sub_rect->h; j++, pixels += pitch) {
										memset(pixels, 0, sub_rect->w << 2);

									}
									SDL_UnlockTexture(is->_sub_texture);
								}
							}
						}

						is->_subpq.frame_queue_next();
					}
					else {
						break;
					}

				}

			}

			is->_pictq.frame_queue_next();
			is->_force_refresh = 1;

			if (is->_step && !is->_paused)
				is->stream_toggle_pause();

		}

	display:

		if (!is->_display_disable && is->_force_refresh && is->_show_mode == TyyVideoState::SHOW_MODE_VIDEO && is->_pictq._rindex_shown) {
			is->video_display();
		}

	}

	is->_force_refresh = 0;
#if 0
	show_status = 0;
	if (show_status) {
		static int64_t last_time;
		int64_t cur_time;
		int aqsize, vqsize, sqsize;
		double av_diff;

		cur_time = av_gettime_relative();
		if (!last_time || (cur_time - last_time) >= 30000) {
			aqsize = 0;
			vqsize = 0;
			sqsize = 0;
			if (is->audio_st)
				aqsize = is->audioq.size;
			if (is->video_st)
				vqsize = is->videoq.size;
			if (is->subtitle_st)
				sqsize = is->subtitleq.size;
			av_diff = 0;
			if (is->audio_st && is->video_st)
				av_diff = get_clock(&is->audclk) - get_clock(&is->vidclk);
			else if (is->video_st)
				av_diff = get_master_clock(is) - get_clock(&is->vidclk);
			else if (is->audio_st)
				av_diff = get_master_clock(is) - get_clock(&is->audclk);
			av_log(NULL, AV_LOG_INFO,
				"%7.2f %s:%7.3f fd=%4d aq=%5dKB vq=%5dKB sq=%5dB f=%"PRId64"/%"PRId64"   \r",
				get_master_clock(is),
				(is->audio_st && is->video_st) ? "A-V" : (is->video_st ? "M-V" : (is->audio_st ? "M-A" : "   ")),
				av_diff,
				is->frame_drops_early + is->frame_drops_late,
				aqsize / 1024,
				vqsize / 1024,
				sqsize,
				is->video_st ? is->viddec.avctx->pts_correction_num_faulty_dts : 0,
				is->video_st ? is->viddec.avctx->pts_correction_num_faulty_pts : 0);
			fflush(stdout);
			last_time = cur_time;
		}
	}
#endif
}

void refresh_loop_wait_event(TyyVideoState *is, SDL_Event *event) {
	double remaining_time = 0.0;

	while (1) {

		if (!is->_cursor_hidden && av_gettime_relative() - is->_cursor_last_shown > CURSOR_HIDE_DELAY) {

			is->_cursor_hidden = 1;
		}

		if (is->_abort_request_w > 0 && is->_abort_request == 1) {

			TYYINFO("refresh_loop_wait_event receive abort request, break while");
			break;
		}else if ((is->_abort_request_w > 0 && is->_abort_request == 0) || is->_event_type == ROEvent2SDL_REFRESH) {
			is->_event_type = ROEvent2SDL_NULL;

			if (is->_end_flag == MediaEndAction_Default) {
				TYYERROR("refresh_loop_wait_event _end_flag: {}", MediaEndAction_Default);
				break;
			}
			else if (is->_end_flag == MediaEndAction_KeepDisplay) {

				TYYDEBUG("keep last frame, WINDID: {}, url: {}", is->_win_id, is->_filename.c_str());
				is->_force_refresh = 1;
			}
		}

		if (remaining_time > 0.0) {
#ifdef  _WIN32
			timeBeginPeriod(1);
			Sleep((int64_t)(remaining_time * 1000.0));
			timeEndPeriod(1);
#else

			av_usleep((int64_t)(remaining_time * 1000000.0));
#endif
		}

		remaining_time = REFRESH_RATE;
		if (is->_show_mode != TyyVideoState::SHOW_MODE_NONE &&
			(!is->_paused
				|| is->_force_refresh)
			) {

			video_refresh(is, &remaining_time);
		}

	}
}

int write_thread(void *arg) {

	SDL_Event event;
	std::string url;
	TyyVideoState *cur_stream = (TyyVideoState*)arg;
	if (NULL == cur_stream) {
		TYYERROR("arg is null, unkown error");
		return -1;
	}

	url = cur_stream->_filename;

	if ((cur_stream->_window == NULL || cur_stream->_renderer == NULL) && cur_stream->sdl_init() == false) {
		TYYERROR("write_thread1 sdl_init fail");
		{
			std::lock_guard<std::mutex> locker(cur_stream->_sdl_init_mutex);
			cur_stream->_sdl_init_finished = true;
			cur_stream->_sdl_init_success = false;
		}
		cur_stream->_sdl_init_cond.notify_all();
		return -1;
	}
	{
		std::lock_guard<std::mutex> locker(cur_stream->_sdl_init_mutex);
		cur_stream->_sdl_init_finished = true;
		cur_stream->_sdl_init_success = true;
	}
	cur_stream->_sdl_init_cond.notify_all();

	for (;;) {
		if (cur_stream->_abort_request_w > 0 && cur_stream->_abort_request == 1) {

			TYYWARN("write_thread receive positive abort request, request code: {}, exit write_thread, url: {}", cur_stream->_abort_request_w, url.c_str());
			break;
		}else if (cur_stream->_abort_request_w > 0 && cur_stream->_abort_request == 0) {

			if (cur_stream->_end_flag == MediaEndAction_Default) {
				TYYERROR("write_thread recevice passive signal, url: {}", url.c_str());
				break;
			} else if (cur_stream->_end_flag == MediaEndAction_KeepDisplay) {

			}
		}

		TYYTRACE("write_thread, cur_stream: {}", fmt::ptr(cur_stream));
		refresh_loop_wait_event(cur_stream, &event);

	}

	cur_stream->sdl_destroy();
	TYYDEBUG("WINID: {}, sdl_destroy ok", cur_stream->_win_id);

	return 0;
}

int TyyVideoState::do_exit(int arg) {
	(void)arg;

	stream_close();

#if CONFIG_AVFILTER
	for (int i = 0; i < _nb_vfilters; i++) {
		av_free((void*)_vfilters_list[i]);
		_vfilters_list[i] = NULL;
	}
	av_freep(&_vfilters_list);
#endif
#if CONFIG_AVFILTER
	avfilter_graph_free(&_agraph);
	_agraph = NULL;
#endif

	avformat_network_deinit();

	return 0;
}

bool TyyVideoState::play(const Properties &properties, int &return_val) {
	TYYTRACE("WINID: {}, start", _win_id);

	std::lock_guard<std::mutex> lg(_mutex_play_close);

	std::string url = properties.get_property(TYY_VIDEO_STATE_PROPERTY_URL, std::string());
	uint64_t win_id = properties.get_property(TYY_VIDEO_STATE_PROPERTY_WIN_ID, static_cast<uint64_t>(0));
	if (url.empty() || win_id == 0) {
		return_val = TYY_PLAYER_ERROR_INVALID_PARAM;
		return false;
	}

	if (_player_state) {
		return_val = TYY_PLAYER_ERROR_STATE_FAILED;
		return false;
	}

	bool ret = init(properties);
	if (ret == false) {
		return_val = TYY_PLAYER_ERROR_STATE_FAILED;
		TYYERROR("TyyVideoState init failed");
		return false;
	}

	ret = open_input();
	if (ret != 0) {
		return_val = TYY_PLAYER_ERROR_OPEN_INPUT_FAILED;
		TYYERROR("WINID: {}, open_input failed", _win_id);
		deinit();
		return false;
	}

	{
		std::lock_guard<std::mutex> locker(_sdl_init_mutex);
		_sdl_init_finished = false;
		_sdl_init_success = false;
	}

	// fix: SDL_Init必须在渲染写线程执行，读线程打开音频前等待写线程初始化完成。
	_write_tid = SDL_CreateThread(write_thread, "write_thread", this);
	if (!_write_tid) {
		return_val = TYY_PLAYER_ERROR_PLAY_FAILED;
		TYYERROR("create write thread failed, errno: {}", SDL_GetError());
		return false;
	}

	_read_tid = SDL_CreateThread(read_thread, "read_thread", this);
	if (!_read_tid) {
		return_val = TYY_PLAYER_ERROR_PLAY_FAILED;
		TYYERROR("create read thread failed, errno: {}", SDL_GetError());
		_abort_request_w = 1;
		_sdl_init_cond.notify_all();
		return false;
	}

	TYYTRACE("WINID: {}, end", _win_id);

	_player_state = 1;
	return_val = 0;
	return true;
}

//bool TyyVideoState::play(int type, void* data, int data_size, int &return_val) {
//	(void)type;

//	TYYTRACE("WINID: {}, start", _win_id);

//	std::lock_guard<std::mutex> lg(_mutex_play_close);

//	if (!data || data_size <= 0) {
//		return_val = TYY_PLAYER_ERROR_INVALID_PARAM;
//		return false;
//	}

//	if (_player_state) {
//		return_val = TYY_PLAYER_ERROR_STATE_FAILED;
//		return false;
//	}

//	playerParam *ffParam = (playerParam*)data;
//	Properties properties;
//	properties.set_property(TYY_VIDEO_STATE_PROPERTY_WIN_ID,
//						   static_cast<uint64_t>(reinterpret_cast<uintptr_t>(ffParam->playStruct.playWnd)));
//	properties.set_property(TYY_VIDEO_STATE_PROPERTY_URL, ffParam->playStruct.url);
//	bool ret = init(properties);
//	if (ret == false) {
//		return_val = TYY_PLAYER_ERROR_STATE_FAILED;
//		TYYERROR("TyyVideoState init failed");
//		return false;
//	}

//	ret = open_input();
//	if (ret != 0) {
//		return_val = TYY_PLAYER_ERROR_OPEN_INPUT_FAILED;
//		TYYERROR("open_input failed");
//		deinit();
//		return false;
//	}

//	_read_tid = SDL_CreateThread(read_thread, "read_thread", this);
//	if (!_read_tid) {
//		return_val = TYY_PLAYER_ERROR_PLAY_FAILED;
//		TYYERROR("create read thread failed, errno: {}", SDL_GetError());
//		return false;
//	}

//	_write_tid = SDL_CreateThread(write_thread, "write_thread", this);
//	if (!_write_tid) {
//		return_val = TYY_PLAYER_ERROR_PLAY_FAILED;
//		TYYERROR("create write thread failed, errno: {}", SDL_GetError());
//		return false;
//	}

//	TYYTRACE("end");

//	_player_state = 1;
//	return_val = 0;
//	return true;
//}

void TyyVideoState::stream_component_close(int stream_index)
{
	AVFormatContext *ic = _ic;
	AVCodecParameters *codecpar;

	if (stream_index < 0 || stream_index >= ic->nb_streams)
		return;

	codecpar = ic->streams[stream_index]->codecpar;

	switch (codecpar->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		_auddec.decoder_abort(&_sampq);
		SDL_CloseAudioDevice(_audio_dev);
		_auddec.decoder_destroy();
		swr_free(&_swr_ctx);
		av_freep(&_audio_buf1);
		_audio_buf1_size = 0;
		_audio_buf = NULL;

		if (_rdft) {
			av_rdft_end(_rdft);
			av_freep(&_rdft_data);
			_rdft = NULL;
			_rdft_bits = 0;
		}
		break;
	case AVMEDIA_TYPE_VIDEO:
		_viddec.decoder_abort(&_pictq);
		_viddec.decoder_destroy();
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		_subdec.decoder_abort(&_subpq);
		_subdec.decoder_destroy();
		break;
	default:
		break;
	}

	ic->streams[stream_index]->discard = AVDISCARD_ALL;
	switch (codecpar->codec_type) {
	case AVMEDIA_TYPE_AUDIO:
		_audio_st = NULL;
		_audio_stream = -1;
		break;
	case AVMEDIA_TYPE_VIDEO:
		_video_st = NULL;
		_video_stream = -1;
		break;
	case AVMEDIA_TYPE_SUBTITLE:
		_subtitle_st = NULL;
		_subtitle_stream = -1;
		break;
	default:
		break;
	}
}

bool TyyVideoState::close() {

	std::lock_guard<std::mutex> lg(_mutex_play_close);

	if (!_player_state) {
		return true;
	}

	do_exit(0);
	_player_state = 0;

#ifdef USE_SONIC
	if (_audio_speed_convert) {
		sonicDestroyStream(_audio_speed_convert);
		_audio_speed_convert = NULL;
	}
	_change_speed_req = 0;
	_play_speed = 1.0f;
#endif

#if	CONFIG_AVFILTER
	if (_afilters){
		av_free(_afilters);
		_afilters = NULL;
	}
#endif

	if (_ffmpeg_hw) {
		_ffmpeg_hw->hw_decoder_uninit();
		av_free(_ffmpeg_hw);
		_ffmpeg_hw = NULL;
	}
	if (_d3d11va_decoder) {
		_d3d11va_decoder->d3d11_uninit();
		av_free(_d3d11va_decoder);
		_d3d11va_decoder = NULL;
	}

	return true;
}

bool TyyVideoState::set_audio_volume(int volume) {

	if (volume > 100 || volume < 0) {
		return false;
	}

	double sdl_volume = (volume / 100.0) * 128;
	_audio_volume = sdl_volume;

	return true;
}

bool TyyVideoState::set_mute(bool status) {

	_muted = status == 1 ? 1 : 0;
	return true;
}

bool TyyVideoState::set_mute() {
	_muted = 0;
	return true;
}

int TyyVideoState::get_audio_volume() {
	return _muted == 0 ? (_audio_volume / 128.0) * 100 : 0;
}

bool TyyVideoState::startup_capture(std::string all_path) {
	_capture_count++;
	_capture_path = all_path;
	return true;
}

int TyyVideoState::get_fps() {
	return _frame_rate;
}

double TyyVideoState::get_fps_safe() {
	return _frame_rate;
}

bool TyyVideoState::has_video_track() {

	int i = 0;
	for (i = 0; i < _ic->nb_streams; i++) {
		AVStream *st = _ic->streams[i];
		if (!st) {
			continue;
		}
		enum AVMediaType type = st->codecpar->codec_type;
		if (AVMEDIA_TYPE_VIDEO == type) {
			return true;
		}
	}
	return false;
}

bool TyyVideoState::has_audio_track() {

	int i = 0;
	for (i = 0; i < _ic->nb_streams; i++) {
		AVStream *st = _ic->streams[i];
		if (!st) {
			continue;
		}
		enum AVMediaType type = st->codecpar->codec_type;
		if (AVMEDIA_TYPE_AUDIO == type) {
			return true;
		}
	}
	return false;
}

void TyyVideoState::set_media_end_flag(MediaEndActionFlag end_flag) {
	_end_flag = end_flag;
}

MediaEndActionFlag TyyVideoState::get_media_end_flag() {
	return _end_flag;
}

void TyyVideoState::set_force_refresh(bool fr) {

	_event_type = fr == true ? ROEvent2SDL_REFRESH : ROEvent2SDL_NULL;
}

void TyyVideoState::on_user_fb_seek(int fb, int seek_interval) {
	TYYDEBUG("fb seek req");
	double incr, pos;
	if (fb == 1) {

		incr = seek_interval ? -seek_interval : -10.0;
	}
	else if (fb == 2) {

		incr = seek_interval ? seek_interval : 10.0;
	}

	if (_seek_by_bytes) {

		pos = -1;
		if (pos < 0 && _video_stream >= 0)
			pos = _pictq.frame_queue_last_pos();
		if (pos < 0 && _audio_stream >= 0)
			pos = _sampq.frame_queue_last_pos();
		if (pos < 0)
			pos = avio_tell(_ic->pb);
		if (_ic->bit_rate)
			incr *= _ic->bit_rate / 8.0;
		else
			incr *= 180000.0;
		pos += incr;
		stream_seek(pos, incr, 1);
	}else {

		pos = get_master_clock();
		if (isnan(pos)) {
			pos = (double)_seek_pos / AV_TIME_BASE;
			TYYWARN("forward/back seek pts return null, referring to the pts value of the last seek");
		}

		TYYDEBUG("seek ts pos: {}, incr: {}", pos, incr);

		pos += incr;
		if (_ic->start_time != AV_NOPTS_VALUE && pos < _ic->start_time / (double)AV_TIME_BASE)
			pos = _ic->start_time / (double)AV_TIME_BASE;
		stream_seek((int64_t)(pos * AV_TIME_BASE), (int64_t)(incr * AV_TIME_BASE), 0);
	}

}

void TyyVideoState::on_user_seek(double percent) {
	TYYDEBUG("seek req");
	if (_seek_by_bytes || _ic->duration <= 0) {
		uint64_t size = avio_size(_ic->pb);
		stream_seek(size * percent, 0, 1);
	}else {
		int64_t ts;
		int ns, hh, mm, ss;
		int tns, thh, tmm, tss;
		tns = _ic->duration / 1000000LL;
		thh = tns / 3600;
		tmm = (tns % 3600) / 60;
		tss = (tns % 60);

		ns = percent * tns;
		hh = ns / 3600;
		mm = (ns % 3600) / 60;
		ss = (ns % 60);
		TYYDEBUG("Seek to {:2.0f} ({:02d}:{:02d}:{:02d}) of total duration ({:02d}:{:02d}:{:02d})", percent * 100,
			hh, mm, ss, thh, tmm, tss);

		ts = percent * _ic->duration;
		if (_ic->start_time != AV_NOPTS_VALUE)
			ts += _ic->start_time;

		stream_seek(ts, 0, 0);
	}
}

void TyyVideoState::change_speed(float speed) {
#ifdef USE_SONIC
	_play_speed = speed;
	_change_speed_req = 1;
#endif
}

#if CONFIG_AVFILTER

void TyyVideoState::change_speed_filter(float speed) {
	(void)speed;

}

void TyyVideoState::change_speed_audio_filter(float speed) {
	if (tyy_less(speed, 0.5) || tyy_more(speed, 2))
		return;
	_audio_filter_speed = speed;
#if CONFIG_AVFILTER
	if (!_afilters){
		_afilters = (char *)av_malloc(32);
	}
	sprintf(_afilters, "atempo=%lf", speed);
	_req_afilter_reconfigure = 1;
#endif
}

void TyyVideoState::change_speed_video_filter(float speed) {
	if (tyy_less(speed, 0.5) || tyy_more(speed, 2))
		return;
	_audio_filter_speed = speed;
#if CONFIG_AVFILTER
	if (!_vfilters_list) {

		_vfilters_list = (const char**)grow_array(_vfilters_list, sizeof(*_vfilters_list), &_nb_vfilters, _nb_vfilters + 1);
		_vfilters_list[_nb_vfilters - 1] = (const char*)av_malloc(1024);
	}
	double speed_convert = 1 / speed;
	sprintf((char*)_vfilters_list[_nb_vfilters - 1], "setpts=%.1lf*PTS", speed_convert);
	_req_afilter_reconfigure = 1;
#endif
}

#endif

void *TyyVideoState::grow_array(void *array, int elem_size, int *size, int new_size)
{
	if (new_size >= INT_MAX / elem_size) {
		av_log(NULL, AV_LOG_ERROR, "Array too big.\n");
		return NULL;
	}
	if (*size < new_size) {
		uint8_t *tmp = (uint8_t *)av_realloc_array(array, new_size, elem_size);
		if (!tmp)
			return NULL;
		memset(tmp + *size*elem_size, 0, (new_size - *size) * elem_size);
		*size = new_size;
		return tmp;
	}
	return array;
}

void TyyVideoState::get_fast_version(int &a, int &b, int &c) {

	int version = avutil_version();
	a = version / (int)pow(2, 16);
	b = (int)(version - a * pow(2, 16)) / (int)pow(2, 8);
	c = version % (int)pow(2, 8);
}

enum AVPixelFormat TyyVideoState::get_hw_format(AVCodecContext* s, const enum AVPixelFormat* pix_fmts)
{
	(void)s;
	(void)pix_fmts;

	return AV_PIX_FMT_D3D11;

}

}

#endif
