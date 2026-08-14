#ifndef TYY_FFPLAY_CORE_H
#define TYY_FFPLAY_CORE_H

#include <inttypes.h>
#include <math.h>
#include <stdint.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/time.h"
}

#include <SDL.h>
#include <SDL_thread.h>

namespace TyyPlayer {

#define MAX_QUEUE_SIZE (15 * 1024 * 1024)
#define MIN_FRAMES 25
#define VIDEO_PICTURE_QUEUE_SIZE 3
#define SUBPICTURE_QUEUE_SIZE 16
#define SAMPLE_QUEUE_SIZE 9
#define FRAME_QUEUE_SIZE FFMAX(SAMPLE_QUEUE_SIZE, FFMAX(VIDEO_PICTURE_QUEUE_SIZE, SUBPICTURE_QUEUE_SIZE))
#define AV_NOSYNC_THRESHOLD 10.0

extern AVPacket flush_pkt;

typedef enum
{
    AC_HARDWAREACCELERATETYPE_DISABLED,
    AC_HARDWAREACCELERATETYPE_AUTO,
    AC_HARDWAREACCELERATETYPE_D3D11VA
} ACHardwareAccelerateType;

typedef enum
{
    Event2SDL_UNKOWN,
    Event2SDL_NULL,
    Event2SDL_REFRESH
} Event2SDL;

typedef struct HWDevice
{
    // 硬件设备名称
    const char *name;
    // FFmpeg硬件设备类型
    enum AVHWDeviceType type;
    // FFmpeg硬件设备引用
    AVBufferRef *device_ref;
} HWDevice;

typedef struct MyAVPacketList
{
    // 解封装后的数据包
    AVPacket pkt;
    // 下一个队列节点
    struct MyAVPacketList *next;
    // 播放序列号
    int serial;
} MyAVPacketList;

class PacketQueue
{
public:
    /**
    * @brief 构造数据包队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    PacketQueue();

    /**
    * @brief 析构数据包队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    virtual ~PacketQueue();

public:
    /**
    * @brief 初始化数据包队列
    * @author: tyy
    * @param 无
    * @return 错误码
    * @note:
    */
    int packet_queue_init();

    /**
    * @brief 清空数据包队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void packet_queue_flush();

    /**
    * @brief 销毁数据包队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void packet_queue_destroy();

    /**
    * @brief 写入数据包到队列
    * @author: tyy
    * @param[in] pkt 数据包
    * @return 错误码
    * @note: 调用前需要外部持有队列锁
    */
    int packet_queue_put_private(AVPacket *pkt);

    /**
    * @brief 写入数据包到队列
    * @author: tyy
    * @param[in] pkt 数据包
    * @return 错误码
    * @note:
    */
    int packet_queue_put(AVPacket *pkt);

    /**
    * @brief 写入空数据包到队列
    * @author: tyy
    * @param[in] stream_index 流索引
    * @return 错误码
    * @note: 空包用于通知解码器刷新缓存帧
    */
    int packet_queue_put_nullpacket(int stream_index);

    /**
    * @brief 从队列读取数据包
    * @author: tyy
    * @param[out] pkt 数据包
    * @param[in] block 是否阻塞等待
    * @param[out] serial 播放序列号
    * @return 小于0表示中断，0表示无数据，大于0表示读取成功
    * @note:
    */
    int packet_queue_get(AVPacket *pkt, int block, int *serial);

    /**
    * @brief 中断数据包队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void packet_queue_abort();

    /**
    * @brief 启动数据包队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note: 写入flush_pkt作为新播放序列的起点
    */
    void packet_queue_start();

    typedef struct pktStatus
    {
        // 数据包数量
        int nbPackets;
        // 队列缓存大小
        int size;
        // 队列缓存总时长
        int64_t duration;
    } pktStatus;

    /**
    * @brief 获取队列状态
    * @author: tyy
    * @param 无
    * @return 队列状态
    * @note:
    */
    pktStatus packet_queue_get_status();

private:
    /**
    * @brief 重置数据包队列成员
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void memset_pkt_queue();

public:
    // 队首节点
    MyAVPacketList *_first_pkt;
    // 队尾节点
    MyAVPacketList *_last_pkt;
    // 数据包数量
    int _nb_packets;
    // 队列缓存大小
    int _size;
    // 队列缓存总时长
    int64_t _duration;
    // 中断请求标记
    int _abort_request;
    // 播放序列号
    int _serial;
    // 队列互斥锁
    SDL_mutex *_mutex;
    // 队列条件变量
    SDL_cond *_cond;
};

typedef struct AudioParams
{
    // 采样率
    int freq;
    // 通道数
    int channels;
    // 通道布局
    int64_t channel_layout;
    // 音频采样格式
    enum AVSampleFormat fmt;
    // 单个采样单元字节数
    int frame_size;
    // 每秒字节数
    int bytes_per_sec;
} AudioParams;

class Clock
{
public:
    /**
    * @brief 构造时钟对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    Clock();

    /**
    * @brief 析构时钟对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~Clock();

public:
    /**
    * @brief 初始化时钟
    * @author: tyy
    * @param[in] queue_serial 队列序列号
    * @return 无
    * @note:
    */
    void init_clock(int *queue_serial);

    /**
    * @brief 获取当前时钟
    * @author: tyy
    * @param 无
    * @return 当前时钟值
    * @note:
    */
    double get_clock();

    /**
    * @brief 设置指定时间点的时钟
    * @author: tyy
    * @param[in] pts 显示时间戳
    * @param[in] serial 播放序列号
    * @param[in] time 系统时间
    * @return 无
    * @note:
    */
    void set_clock_at(double pts, int serial, double time);

    /**
    * @brief 设置时钟
    * @author: tyy
    * @param[in] pts 显示时间戳
    * @param[in] serial 播放序列号
    * @return 无
    * @note:
    */
    void set_clock(double pts, int serial);

    /**
    * @brief 设置时钟速度
    * @author: tyy
    * @param[in] speed 时钟速度
    * @return 无
    * @note:
    */
    void set_clock_speed(double speed);

    /**
    * @brief 同步到从时钟
    * @author: tyy
    * @param[in] slave 从时钟
    * @return 无
    * @note:
    */
    void sync_clock_to_slave(Clock *slave);

    /**
    * @brief 清空时钟状态
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void set_clock_flush();

public:
    // 当前时钟基准
    double _pts;
    // 当前pts和系统时间的差值
    double _pts_drift;
    // 最后一次更新时间
    double _last_updated;
    // 时钟速度
    double _speed;
    // 播放序列号
    int _serial;
    // 暂停标记
    int _paused;
    // 当前数据包队列序列号
    int *_queue_serial;
};

typedef struct Frame
{
    // 解码帧
    AVFrame *frame;
    // 字幕数据
    AVSubtitle sub;
    // 播放序列号
    int serial;
    // 显示时间戳，单位秒
    double pts;
    // 帧持续时间，单位秒
    double duration;
    // 输入文件字节位置
    int64_t pos;
    // 图像宽度
    int width;
    // 图像高度
    int height;
    // AVPixelFormat或AVSampleFormat
    int format;
    // 像素宽高比
    AVRational sar;
    // 是否已经上传到渲染纹理
    int uploaded;
    // 是否垂直翻转
    int flip_v;
} Frame;

class FrameQueue
{
public:
    /**
    * @brief 构造帧队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    FrameQueue();

    /**
    * @brief 析构帧队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~FrameQueue();

public:
    /**
    * @brief 初始化帧队列
    * @author: tyy
    * @param[in] pktq 对应的数据包队列
    * @param[in] max_size 最大帧数量
    * @param[in] keep_last 是否保留最后一帧
    * @return 错误码
    * @note:
    */
    int frame_queue_init(PacketQueue *pktq, int max_size, int keep_last);

    /**
    * @brief 释放帧队列元素
    * @author: tyy
    * @param[in] vp 帧队列元素
    * @return 无
    * @note:
    */
    void frame_queue_unref_item(Frame *vp);

    /**
    * @brief 销毁帧队列
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void frame_queue_destory();

    /**
    * @brief 唤醒帧队列等待线程
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void frame_queue_signal();

    /**
    * @brief 获取上一帧
    * @author: tyy
    * @param 无
    * @return 帧队列元素
    * @note:
    */
    Frame *frame_queue_peek_last();

    /**
    * @brief 获取当前帧
    * @author: tyy
    * @param 无
    * @return 帧队列元素
    * @note:
    */
    Frame *frame_queue_peek();

    /**
    * @brief 获取下一帧
    * @author: tyy
    * @param 无
    * @return 帧队列元素
    * @note:
    */
    Frame *frame_queue_peek_next();

    /**
    * @brief 获取可写帧
    * @author: tyy
    * @param 无
    * @return 帧队列元素
    * @note: 队列满时会阻塞等待
    */
    Frame *frame_queue_peek_writable();

    /**
    * @brief 获取可读帧
    * @author: tyy
    * @param 无
    * @return 帧队列元素
    * @note: 队列空时会阻塞等待
    */
    Frame *frame_queue_peek_readable();

    /**
    * @brief 提交可写帧
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void frame_queue_push();

    /**
    * @brief 移动到下一帧
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void frame_queue_next();

    /**
    * @brief 获取剩余可读帧数量
    * @author: tyy
    * @param 无
    * @return 剩余帧数量
    * @note:
    */
    int frame_queue_nb_remaining();

    /**
    * @brief 获取最后一帧文件位置
    * @author: tyy
    * @param 无
    * @return 文件位置
    * @note:
    */
    int64_t frame_queue_last_pos();

private:
    /**
    * @brief 重置帧队列成员
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void memset_fq();

public:
    // 帧循环队列
    Frame _queue[FRAME_QUEUE_SIZE];
    // 读索引
    int _rindex;
    // 写索引
    int _windex;
    // 当前帧数量
    int _size;
    // 最大帧数量
    int _max_size;
    // 是否保留最后一帧
    int _keep_last;
    // 当前读帧是否已经显示
    int _rindex_shown;
    // 队列互斥锁
    SDL_mutex *_mutex;
    // 队列条件变量
    SDL_cond *_cond;
    // 对应的数据包队列
    PacketQueue *_pktq;
};

class Decoder
{
public:
    /**
    * @brief 构造解码器对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    Decoder();

    /**
    * @brief 析构解码器对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~Decoder();

public:
    /**
    * @brief 初始化解码器
    * @author: tyy
    * @param[in] avctx 解码器上下文
    * @param[in] queue 数据包队列
    * @param[in] empty_queue_cond 空队列唤醒条件变量
    * @return 无
    * @note:
    */
    void decoder_init(AVCodecContext *avctx, PacketQueue *queue, SDL_cond *empty_queue_cond);

    /**
    * @brief 解码一帧数据
    * @author: tyy
    * @param[out] frame 解码后的音视频帧
    * @param[out] sub 解码后的字幕帧
    * @return 解码结果
    * @note:
    */
    int decoder_decode_frame(AVFrame *frame, AVSubtitle *sub);

    /**
    * @brief 销毁解码器
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void decoder_destroy();

    /**
    * @brief 中断解码器
    * @author: tyy
    * @param[in] fq 解码器对应的帧队列
    * @return 无
    * @note:
    */
    void decoder_abort(FrameQueue *fq);

    /**
    * @brief 启动解码线程
    * @author: tyy
    * @param[in] fn 线程函数
    * @param[in] thread_name 线程名称
    * @param[in] arg 线程参数
    * @return 错误码
    * @note:
    */
    int decoder_start(int(*fn)(void *), const char *thread_name, void *arg);

public:
    // 当前缓存数据包
    AVPacket _pkt;
    // 解码器对应的数据包队列
    PacketQueue *_queue;
    // 解码器上下文
    AVCodecContext *_avctx;
    // 当前数据包序列号
    int _pkt_serial;
    // 解码完成标记
    int _finished;
    // 是否存在待重新发送的数据包
    int _packet_pending;
    // 空队列唤醒条件变量
    SDL_cond *_empty_queue_cond;
    // 起始pts
    int64_t _start_pts;
    // 起始pts timebase
    AVRational _start_pts_tb;
    // 下一帧pts
    int64_t _next_pts;
    // 下一帧pts timebase
    AVRational _next_pts_tb;
    // 解码线程句柄
    SDL_Thread *_decoder_tid;
};

}

#endif
