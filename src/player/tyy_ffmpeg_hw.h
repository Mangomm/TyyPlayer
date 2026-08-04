#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/time.h"
#ifdef __cplusplus
}
#endif

namespace TyyPlayer {

class TFFMPEG_HW {
public:
    /**
    * @brief 构造FFmpeg硬件设备对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    TFFMPEG_HW();

    /**
    * @brief 析构FFmpeg硬件设备对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    virtual ~TFFMPEG_HW();

    /**
    * @brief 初始化硬件解码设备
    * @author: tyy
    * @param[in] ctx 解码器上下文
    * @param[in] type 硬件设备类型
    * @param[in] hwnd 渲染窗口句柄
    * @return 错误码
    * @note:
    */
    int hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type, HWND hwnd);

    /**
    * @brief 反初始化硬件解码设备
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void hw_decoder_uninit();

    /**
    * @brief 获取硬件设备上下文
    * @author: tyy
    * @param 无
    * @return 硬件设备上下文
    * @note:
    */
    AVBufferRef* get_hw_device_ctx();

private:
    // FFmpeg硬件设备上下文
    AVBufferRef *_hw_device_ctx;
};

}
