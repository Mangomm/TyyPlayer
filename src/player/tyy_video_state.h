#ifndef TYY_VIDEO_STATE_H
#define TYY_VIDEO_STATE_H

#include <inttypes.h>
#include <math.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <string>
#include <atomic>
#include <condition_variable>
#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#define CONFIG_AVFILTER 1
#define CONFIG_RTSP_DEMUXER 1
#define CONFIG_MMSH_PROTOCOL 1
#define CONFIG_AVDEVICE 1

extern "C"
{
#include "libavutil/avstring.h"
#include "libavutil/channel_layout.h"
#include "libavutil/eval.h"
#include "libavutil/mathematics.h"
#include "libavutil/pixdesc.h"
#include "libavutil/imgutils.h"
#include "libavutil/dict.h"
#include "libavutil/fifo.h"
#include "libavutil/parseutils.h"
#include "libavutil/samplefmt.h"
#include "libavutil/avassert.h"
#include "libavutil/time.h"
#include "libavutil/bprint.h"
#include "libavformat/avformat.h"
#include "libavdevice/avdevice.h"
#include "libswscale/swscale.h"
#include "libavutil/opt.h"
#include "libavcodec/avfft.h"
#include "libavcodec/avcodec.h"
#include "libswresample/swresample.h"
#include "libavutil/display.h"

#if CONFIG_AVFILTER
# include "libavfilter/avfilter.h"
# include "libavfilter/buffersink.h"
# include "libavfilter/buffersrc.h"
#endif
}

#include "tyy_properties.h"
#include "tyy_player_api.h"
#include "tyy_ffmpeg_hw.h"
#include "tyy_ffmpeg_d3d11va.h"
#include "tyy_ffplay_core.h"

#include <SDL.h>
#include <SDL_thread.h>
#ifdef _WIN32
#undef main
#endif

namespace TyyPlayer {

class TyyVideoState;

int decode_interrupt_cb(void *ctx);
void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
int audio_thread(void *arg);
int video_thread(void *arg);
int subtitle_thread(void *arg);
int read_thread(void *arg);
void video_refresh(void *opaque, double *remaining_time);
void refresh_loop_wait_event(TyyVideoState *is, SDL_Event *event);
int write_thread(void *arg);

enum MediaEndActionFlag {
	MediaEndAction_Default = 0,
	MediaEndAction_KeepDisplay,
};

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define EXTERNAL_CLOCK_MIN_FRAMES 2
#define EXTERNAL_CLOCK_MAX_FRAMES 10

#define SDL_AUDIO_MIN_BUFFER_SIZE 512

#define SDL_AUDIO_MAX_CALLBACKS_PER_SEC 30

#define SDL_VOLUME_STEP (0.75)

#define AV_SYNC_THRESHOLD_MIN 0.04

#define AV_SYNC_THRESHOLD_MAX 0.1

#define AV_SYNC_FRAMEDUP_THRESHOLD 0.040

#define AV_NOSYNC_THRESHOLD 10.0

#define SAMPLE_CORRECTION_PERCENT_MAX 10

#define EXTERNAL_CLOCK_SPEED_MIN  0.900
#define EXTERNAL_CLOCK_SPEED_MAX  1.010
#define EXTERNAL_CLOCK_SPEED_STEP 0.001

#define AUDIO_DIFF_AVG_NB   30

#define REFRESH_RATE 0.005

#define SAMPLE_ARRAY_SIZE (8 * 65536)

#define CURSOR_HIDE_DELAY 1000000

#define USE_ONEPASS_SUBTITLE_RENDER 1

#define VIDEO_PICTURE_QUEUE_SIZE	3
#define SUBPICTURE_QUEUE_SIZE		16
#define SAMPLE_QUEUE_SIZE           9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))

#define USE_SONIC
#ifdef USE_SONIC
#include "tyy_sonic.h"
#endif

enum {
		AV_SYNC_AUDIO_MASTER,
		AV_SYNC_VIDEO_MASTER,
		AV_SYNC_EXTERNAL_CLOCK,
	};

	class TyyVideoState
	{
		friend int decode_interrupt_cb(void *ctx);
		friend void sdl_audio_callback(void *opaque, Uint8 *stream, int len);
		friend int audio_thread(void *arg);
		friend int video_thread(void *arg);
		friend int subtitle_thread(void *arg);
		friend int read_thread(void *arg);
		friend void video_refresh(void *opaque, double *remaining_time);
		friend void refresh_loop_wait_event(TyyVideoState *is, SDL_Event *event);
		friend int write_thread(void *arg);

	public:
		TyyVideoState();
		TyyVideoState(HWND hwnd);
		~TyyVideoState();

		void set_event_callback(TyyPlayerEventCallback callback, void *user_data);

		bool play(const Properties &properties, int &return_val);

		bool close();

		void toggle_pause();

		void step_next_frame();

		bool set_audio_volume(int volume);

		bool set_mute(bool status);

		bool set_mute();

		int get_audio_volume();

		bool startup_capture(std::string all_path);

		int get_fps();

		double get_fps_safe();

		bool has_video_track();

		bool has_audio_track();

		void set_media_end_flag(MediaEndActionFlag end_flag);

		MediaEndActionFlag get_media_end_flag();

		void set_force_refresh(bool fr);

		void on_user_fb_seek(int fb, int seek_interval = 10);

		void on_user_seek(double percent);

		void change_speed(float speed);

#if CONFIG_AVFILTER

		void change_speed_filter(float speed);

		void change_speed_audio_filter(float speed);

		void change_speed_video_filter(float speed);
#endif

		void *grow_array(void *array, int elem_size, int *size, int new_size);

		void get_fast_version(int& a, int& b, int& c);

	private:

		void set_properties(const Properties &properties);

		void video_state_memset_zero();

		bool init(const Properties &properties);

		void deinit();

		int open_input();

		bool sdl_init();

		void sdl_destroy();

		void stream_component_close(int stream_index);

		void stream_close();

		int do_exit(int arg);

		void read_thread_close();

		std::string get_ffmpeg_error(int err_val);

		int64_t get_valid_channel_layout(int64_t channel_layout, int channels);

		int check_stream_specifier(AVFormatContext *s, AVStream *st, const char *spec);
		AVDictionary **setup_find_stream_info_opts(AVFormatContext *s,
            AVDictionary *codec_opts);

		AVDictionary *filter_codec_opts(AVDictionary *opts, enum AVCodecID codec_id,
            AVFormatContext *s, AVStream *st, AVCodec *codec);

		int stream_component_open(int stream_index);

		int configure_audio_filters(const char *afilters, int force_output_format);

#if CONFIG_AVFILTER

        int configure_filtergraph(AVFilterGraph *graph,
                                  const char *filtergraph,
                                  AVFilterContext *source_ctx,
                                  AVFilterContext *sink_ctx);
#endif

        int audio_open(void *opaque,
                       int64_t wanted_channel_layout,
                       int wanted_nb_channels,
                       int wanted_sample_rate,
                       struct AudioParams *audio_hw_params);

		int audio_decode_frame();

        int cmp_audio_fmts(enum AVSampleFormat fmt1,
                           int64_t channel_count1,
                           enum AVSampleFormat fmt2,
                           int64_t channel_count2);

		int synchronize_audio(int nb_samples);

		int get_master_sync_type();

		double get_master_clock();

		int get_video_frame(AVFrame *frame);

#if CONFIG_AVFILTER

		int configure_video_filters(AVFilterGraph *graph, const char *vfilters, AVFrame *frame);
#endif

		double get_rotation(AVStream *st);

		int queue_picture(AVFrame *src_frame, double pts, double duration, int64_t pos, int serial);

		void video_display();

		void video_image_display();

		int upload_texture(SDL_Texture **tex, AVFrame *frame, struct SwsContext **img_convert_ctx);

		void get_sdl_pix_fmt_and_blendmode(int format, Uint32 *sdl_pix_fmt, SDL_BlendMode *sdl_blendmode);

        int realloc_texture(SDL_Texture **texture,
                            Uint32 new_format,
                            int new_width,
                            int new_height,
                            SDL_BlendMode blendmode,
                            int init_texture);

		void set_sdl_yuv_conversion_mode(AVFrame *frame);

		double vp_duration(Frame *vp, Frame *nextvp);

		double compute_target_delay(double delay);

		void update_video_pts(double pts, int64_t pos, int serial);

		void step_to_next_frame();

		void stream_toggle_pause();

		int stream_has_enough_packets(AVStream *st, int stream_id, PacketQueue *queue);

		void stream_seek(int64_t pos, int64_t rel, int seek_by_bytes);

		static enum AVPixelFormat get_hw_format(AVCodecContext* s, const enum AVPixelFormat* pix_fmts);

	public:

		void sonic_speed();

	private:

		int save_frame_to_jpeg(const char *filename, const AVFrame* frame);

		int save_frame_to_bmp(const char *filename, const AVFrame* frame);

		bool save_as_bmp(const char *filename, AVFrame *pFrameRGB, int width, int height, int index, int bpp);

	public:
		SDL_Thread	*_read_tid = NULL;
		SDL_Thread	*_write_tid = NULL;
		AVInputFormat	*_iformat = NULL;
		int		_abort_request = 0;
		int	    _abort_request_w = 0;
		int		_force_refresh = 0;
		int		_paused = 0;
		int		_last_paused = 0;
		int		_queue_attachments_req = 0;
		int		_seek_req = 0;
		int		_seek_flags;
		int64_t		_seek_pos;
		int64_t		_seek_rel;
		int		_read_pause_return = 0;

		PacketQueue _videoq;
		PacketQueue _audioq;
		PacketQueue _subtitleq;

		FrameQueue	_pictq;
		FrameQueue	_subpq;
		FrameQueue	_sampq;

		Clock	_audclk;
		Clock	_vidclk;
		Clock	_extclk;

		std::string _filename;
		HWND _h_display_window;
		SDL_Window *_window = NULL;
		SDL_Renderer *_renderer = NULL;
		SDL_RendererInfo _renderer_info = { 0 };
		int _width, _height, _xleft, _ytop;
		SDL_cond *_continue_read_thread = NULL;
		int _audio_clock_serial;
		double _startup_volume = 50;
		double	_audio_volume;
		int			_muted;
		int _av_sync_type;

		double _frame_rate;
		std::atomic<int> _capture_count;

		std::string _capture_path;

		int _video_stream = -1;
		int _audio_stream = -1;
		int _subtitle_stream = -1;
		int _last_video_stream = -1, _last_audio_stream = -1, _last_subtitle_stream = -1;
		int _eof = 0;
		AVFormatContext *_ic = NULL;
		double _max_frame_duration;
		int		_realtime;
		enum SHOW_MODE {
			SHOW_MODE_NONE = -1,
			SHOW_MODE_VIDEO = 0,
			SHOW_MODE_WAVES,
			SHOW_MODE_RDFT,
			SHOW_MODE_NB
		} _show_mode;
#if CONFIG_AVFILTER
		struct AudioParams _audio_filter_src;
#endif
#if CONFIG_AVFILTER
		int _vfilter_idx;
		AVFilterContext *_in_video_filter = NULL;
		AVFilterContext *_out_video_filter = NULL;
		AVFilterContext *_in_audio_filter = NULL;
		AVFilterContext *_out_audio_filter = NULL;
		AVFilterGraph *_agraph = NULL;
#endif

		Decoder _auddec;
		Decoder _viddec;
		Decoder _subdec;

		double			_audio_clock;

		double			_audio_diff_cum;
		double			_audio_diff_avg_coef;
		double			_audio_diff_threshold;
		int             _audio_diff_avg_count;

		AVStream		*_audio_st = NULL;

		int             _audio_hw_buf_size;

		uint8_t			*_audio_buf = NULL;
		uint8_t			*_audio_buf1 = NULL;
		unsigned int		_audio_buf_size;
		unsigned int		_audio_buf1_size;
		int			_audio_buf_index;

		int			_audio_write_buf_size;

		struct AudioParams _audio_src;

		struct AudioParams _audio_tgt;
		struct SwrContext *_swr_ctx = NULL;
		int _frame_drops_early;
		int _frame_drops_late;

		int16_t _sample_array[SAMPLE_ARRAY_SIZE];
		int _sample_array_index;
		int _last_i_start;
		RDFTContext *_rdft = NULL;
		int _rdft_bits;
		FFTSample *_rdft_data = NULL;

		int _xpos;
		double _last_vis_time;
		SDL_Texture *_vis_texture = NULL;

		SDL_Texture *_sub_texture = NULL;
		SDL_Texture *_vid_texture = NULL;

		AVStream *_subtitle_st = NULL;

		double _frame_timer;
		double _frame_last_returned_time;
		double _frame_last_filter_delay;

		AVStream *_video_st = NULL;

		struct SwsContext *_img_convert_ctx = NULL;
		struct SwsContext *_sub_convert_ctx = NULL;

		int _step = 0;

		int _audio_disable = 0;
		int _video_disable = 0;

		int _seek_by_bytes = -1;
		int64_t _start_time = AV_NOPTS_VALUE;
		const char* _wanted_stream_spec[AVMEDIA_TYPE_NB] = { 0 };
		int _subtitle_disable = 0;
#if CONFIG_AVFILTER
		const char **_vfilters_list = NULL;
		int _nb_vfilters = 0;
		char *_afilters = NULL;
#endif
		int _filter_nbthreads = 0;

#if CONFIG_AVFILTER
		double _audio_filter_speed = 1.0;
		int _req_afilter_reconfigure = 0;
#endif

		int _always_on_top;
		int _borderless;

		AVInputFormat *_file_iformat = NULL;

		AVDictionary *_sws_dict = NULL;
		AVDictionary *_swr_opts = NULL;
		AVDictionary *_format_opts = NULL, *_codec_opts = NULL, *_resample_opts = NULL;
		int _genpts = 0;
		int _find_stream_info = 1;

		const char *_window_title = NULL;

		int64_t _duration = AV_NOPTS_VALUE;
		int _show_status = 1;

		int _default_width = 640;
		int _default_height = 480;
		int _screen_width = 0;
		int _screen_height = 0;
		int _screen_left = SDL_WINDOWPOS_CENTERED;
		int _screen_top = SDL_WINDOWPOS_CENTERED;
		int _lowres = 0;
		const char *_audio_codec_name = NULL;
		const char *_subtitle_codec_name = NULL;
		const char *_video_codec_name = NULL;
		int _fast = 0;
		int64_t _audio_callback_time = 0;
		SDL_AudioDeviceID _audio_dev;

		int _decoder_reorder_pts = -1;
		int _framedrop = -1;
		int _infinite_buffer = -1;
		int _loop = 1;
		int _autoexit = 0;
		int _cursor_hidden = 0;
		int64_t _cursor_last_shown;
		int _display_disable = 0;
		double _rdft_speed = 0.02;

		int _is_full_screen;

		unsigned _sws_flags = SWS_BICUBIC;
		int _autorotate = 1;

		Properties _properties;
		int _win_id;
		TyyPlayerEventCallback _event_callback = NULL;
		void *_event_user_data = NULL;

		int _player_state = 0;
		int64_t _last_read_packet_time = av_gettime();
		int _timeout = 5;
		MediaEndActionFlag _end_flag = MediaEndAction_KeepDisplay;
		std::mutex _mutex_play_close;
		std::mutex _sdl_init_mutex;
		std::condition_variable _sdl_init_cond;
		bool _sdl_init_finished = false;
		bool _sdl_init_success = false;

#ifdef USE_SONIC
		sonicStreamStruct *_audio_speed_convert = NULL;
		bool _change_speed_req = 0;
		float _play_speed = 1.0f;
#endif

	public:
		int _write_packet_fast = 0;

		ACHardwareAccelerateType _hwaccel = AC_HARDWAREACCELERATETYPE_DISABLED;

		TFFMPEG_HW *_ffmpeg_hw = NULL;
		TD3D11VA_Decoder *_d3d11va_decoder = NULL;
		enum AVPixelFormat _hw_pix_fmt = AV_PIX_FMT_NONE;

        std::atomic<Event2SDL> _event_type{ Event2SDL_UNKOWN };
	};

}

#endif
