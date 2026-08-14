#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <tchar.h>
#include <dxgi.h>
#include <dxgi1_3.h>
#ifdef __cplusplus
}
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

typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
typedef struct ID3D11VideoDevice ID3D11VideoDevice;
typedef struct ID3D11VideoContext ID3D11VideoContext;
typedef struct ID3D11RenderTargetView ID3D11RenderTargetView;
typedef struct ID3D11VideoProcessorEnumerator ID3D11VideoProcessorEnumerator;
typedef struct ID3D11VideoProcessor ID3D11VideoProcessor;

namespace TyyPlayer {

class TD3D11VA_Decoder {
public:
    /**
    * @brief 构造D3D11VA解码辅助对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    TD3D11VA_Decoder();

    /**
    * @brief 析构D3D11VA解码辅助对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    virtual ~TD3D11VA_Decoder();

    /**
    * @brief 初始化D3D11VA解码资源
    * @author: tyy
    * @param[in] ctx 解码器上下文
    * @param[in] type 硬件设备类型
    * @return 错误码
    * @note:
    */
    int d3d11_init(AVCodecContext* ctx, const enum AVHWDeviceType type);

    /**
    * @brief 释放D3D11VA解码资源
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void d3d11_uninit();

    /**
    * @brief 获取D3D11VA硬件像素格式
    * @author: tyy
    * @param[in] ctx 解码器上下文
    * @param[in] pix_fmts 候选像素格式列表
    * @return 匹配的像素格式
    * @note:
    */
    static enum AVPixelFormat d3d11_get_hw_format(AVCodecContext* ctx, const enum AVPixelFormat* pix_fmts);

    /**
    * @brief 取回并渲染D3D11VA解码帧
    * @author: tyy
    * @param[in] avctx 解码器上下文
    * @param[in] frame 解码帧
    * @return 无
    * @note:
    */
    void d3d11va_retrieve_data(AVCodecContext* avctx, AVFrame* frame);

    const char *get_device_detail() const;

private:
    /**
    * @brief 重置D3D11VA成员变量
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void d3d11_zero();

    /**
    * @brief 初始化D3D11窗口渲染资源
    * @author: tyy
    * @param[in] hwnd 窗口句柄
    * @return HRESULT结果
    * @note:
    */
    HRESULT d3d11_init_private(HWND hwnd);

private:
    // D3D11设备
    ID3D11Device* _d3d11_device;
    // D3D11设备上下文
    ID3D11DeviceContext* _d3d11_device_context;
    // D3D11视频设备
    ID3D11VideoDevice* _d3d11_video_device;
    // D3D11视频上下文
    ID3D11VideoContext* _d3d11_video_context;
    // D3D11渲染目标视图
    ID3D11RenderTargetView* _render_target_view;
    // DXGI交换链
    IDXGISwapChain2* _swap_chain2;
    // D3D11视频处理器枚举器
    ID3D11VideoProcessorEnumerator* _d3d11_video_processor_enumerator;
    // D3D11视频处理器
    ID3D11VideoProcessor* _d3d11_video_processor;

    // D3D11 device detail
    char _device_detail[256];

public:
    // 渲染窗口句柄
    HWND _hwnd;
};

}
