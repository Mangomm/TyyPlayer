#ifndef TYY_PLAYER_API_H
#define TYY_PLAYER_API_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(TYY_PLAYER_STATIC)
#define TYY_PLAYER_API
#elif defined(_WIN32)
#if defined(TYY_PLAYER_BUILD_LIBRARY)
#define TYY_PLAYER_API __declspec(dllexport)
#else
#define TYY_PLAYER_API __declspec(dllimport)
#endif
#else
#define TYY_PLAYER_API __attribute__((visibility("default")))
#endif

typedef void *TyyPlayerHandle;

typedef enum TYY_PLAYER_ERROR
{
    TYY_PLAYER_ERROR_OK = 0,
    TYY_PLAYER_ERROR_INVALID_PARAM = -1,
    TYY_PLAYER_ERROR_OPEN_INPUT_FAILED = -2,
    TYY_PLAYER_ERROR_STREAM_NOT_FOUND = -3,
    TYY_PLAYER_ERROR_OPEN_DECODER_FAILED = -4,
    TYY_PLAYER_ERROR_ALLOC_FAILED = -5,
    TYY_PLAYER_ERROR_STATE_FAILED = -6,
    TYY_PLAYER_ERROR_RENDER_FAILED = -7,
    TYY_PLAYER_ERROR_PLAY_FAILED = -8,
    TYY_PLAYER_ERROR_OPTION_NOT_FOUND = -9,
    TYY_PLAYER_ERROR_FIND_STREAM_INFO_FAILED = -10,
    TYY_PLAYER_ERROR_EOF = -11,
    TYY_PLAYER_ERROR_NETWORK_FAILED = -12
} TYY_PLAYER_ERROR;

typedef enum TYY_PLAYER_EVENT
{
    TYY_PLAYER_EVENT_DISCONNECT = 2000
} TYY_PLAYER_EVENT;

typedef enum TYY_PLAYER_DECODER_TYPE
{
    TYY_PLAYER_DECODER_TYPE_SOFTWARE = 0,
    TYY_PLAYER_DECODER_TYPE_AUTO = 1,
    TYY_PLAYER_DECODER_TYPE_D3D11VA = 2
} TYY_PLAYER_DECODER_TYPE;

typedef struct TyyPlayerStatistics
{
    int valid;
    double display_fps;
    double stream_fps;
    double master_clock;
    double av_diff;
    int audio_queue_size;
    int video_queue_size;
    int subtitle_queue_size;
    int frame_drops;
    long long faulty_dts;
    long long faulty_pts;
    long long duration;
    long long bit_rate;
    long long video_bit_rate;
    long long audio_bit_rate;
    int width;
    int height;
    int sample_rate;
    int channels;
    char video_decoder[128];
    char video_decoder_detail[256];
    char audio_decoder[128];
    char audio_decoder_detail[256];
} TyyPlayerStatistics;

typedef void (*TyyPlayerEventCallback)(void *user_data, int event_code, int error_code);

/**
* @brief 创建播放器对象
* @author: tyy
* @param 无
* @return 播放器句柄
* @note:
*/
TYY_PLAYER_API TyyPlayerHandle tyy_player_create();

/**
* @brief 销毁播放器对象
* @author: tyy
* @param[in] handle 播放器句柄
* @return 无
* @note:
*/
TYY_PLAYER_API void tyy_player_destroy(TyyPlayerHandle handle);

/**
* @brief 设置播放器渲染窗口
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] win_id Qt控件窗口句柄
* @return 错误码
* @note: 当前Qt界面仍通过该接口传入每一路分屏窗口
*/
TYY_PLAYER_API int tyy_player_set_window(TyyPlayerHandle handle, unsigned long long win_id);

/**
* @brief 设置播放器事件回调
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] callback 事件回调函数
* @param[in] user_data 用户自定义数据
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_set_event_callback(
        TyyPlayerHandle handle,
        TyyPlayerEventCallback callback,
        void *user_data);

/**
* @brief 设置播放器解码器类型
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] decoder_type 解码器类型，参考TYY_PLAYER_DECODER_TYPE
* @return 错误码
* @note: 该设置只对后续打开的视频生效，已经播放的视频需要关闭后重新打开
*/
TYY_PLAYER_API int tyy_player_set_decoder_type(TyyPlayerHandle handle, int decoder_type);

/**
* @brief 获取播放器统计信息
* @author: tyy
* @param[in] handle 播放器句柄
* @param[out] statistics 统计信息
* @return 错误码
* @note: 优先返回播放中底层实时统计信息
*/
TYY_PLAYER_API int tyy_player_get_statistics(TyyPlayerHandle handle, TyyPlayerStatistics *statistics);

/**
* @brief 打开媒体资源
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] url 媒体路径或URL
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_open(TyyPlayerHandle handle, const char *url);

/**
* @brief 启动播放
* @author: tyy
* @param[in] handle 播放器句柄
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_start(TyyPlayerHandle handle);

/**
* @brief 暂停或恢复
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] pause 是否暂停
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_pause(TyyPlayerHandle handle, int pause);

/**
* @brief 快进或快退
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] forward 是否前进
* @param[in] seek_interval 跳转间隔，单位秒
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_seek(TyyPlayerHandle handle, int forward, int seek_interval);

/**
* @brief 单步播放下一帧
* @author: tyy
* @param[in] handle 播放器句柄
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_step_to_next_frame(TyyPlayerHandle handle);

/**
* @brief 设置播放音量
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] volume 音量值，范围0到100
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_set_volume(TyyPlayerHandle handle, int volume);

/**
* @brief 设置播放倍速
* @author: tyy
* @param[in] handle 播放器句柄
* @param[in] speed 播放倍速
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_set_speed(TyyPlayerHandle handle, float speed);

/**
* @brief 停止播放
* @author: tyy
* @param[in] handle 播放器句柄
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_stop(TyyPlayerHandle handle);

/**
* @brief 关闭媒体资源
* @author: tyy
* @param[in] handle 播放器句柄
* @return 错误码
* @note:
*/
TYY_PLAYER_API int tyy_player_close(TyyPlayerHandle handle);

#ifdef __cplusplus
}
#endif

#endif
