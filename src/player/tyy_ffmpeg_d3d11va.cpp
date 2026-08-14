#include <d3d11.h>
#include "tyy_ffmpeg_d3d11va.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif
//#include "config.h"
#define CONFIG_D3D11VA 1

#if CONFIG_D3D11VA
#include "libavutil/hwcontext_d3d11va.h"
#endif
#ifdef __cplusplus
}
#endif
using namespace TyyPlayer;

#define SAFE_RELEASE(X)                                   \
    if ((X)) {                                            \
        (X)->Release();									  \
        X = NULL;                                         \
    }


TD3D11VA_Decoder::TD3D11VA_Decoder() {
	d3d11_zero();
}

TD3D11VA_Decoder::~TD3D11VA_Decoder() {
	d3d11_uninit();
}

void TD3D11VA_Decoder::d3d11_zero() {
	_d3d11_device = NULL;
	_d3d11_device_context = NULL;
	_d3d11_video_device = NULL;
	_d3d11_video_context = NULL;
	_render_target_view = NULL;

	_swap_chain2 = NULL;
	_d3d11_video_processor_enumerator = NULL;
	_d3d11_video_processor = NULL;
	memset(_device_detail, 0, sizeof(_device_detail));
}

int TD3D11VA_Decoder::d3d11_init(AVCodecContext* ctx, const enum AVHWDeviceType type) {
#if CONFIG_D3D11VA
	if (type == AV_HWDEVICE_TYPE_D3D11VA) {
		// set d3d from ffmpeg hw_device_ctx
		AVHWDeviceContext* hw_device_ctx = (AVHWDeviceContext*)ctx->hw_device_ctx->data;
		AVD3D11VADeviceContext* d3d11_device_ctx = (AVD3D11VADeviceContext*)hw_device_ctx->hwctx;

		_d3d11_device = d3d11_device_ctx->device;
		_d3d11_device_context = d3d11_device_ctx->device_context;
		_d3d11_video_device = d3d11_device_ctx->video_device;
		_d3d11_video_context = d3d11_device_ctx->video_context;

		// d3d11 init
		if (FAILED(d3d11_init_private(_hwnd)))
			return -1;
	}
#endif
	return 0;
}

enum AVPixelFormat TD3D11VA_Decoder::d3d11_get_hw_format(AVCodecContext* ctx,
	const enum AVPixelFormat* pix_fmts)
{
	const enum AVPixelFormat* p;

	for (p = pix_fmts; *p != -1; p++) {
		if (*p == AV_PIX_FMT_D3D11)
			return *p;
	}

	return AV_PIX_FMT_NONE;
}

HRESULT TD3D11VA_Decoder::d3d11_init_private(HWND hWnd) {
	HRESULT hr = S_OK;
	IDXGIDevice1* pDXGIdevice = NULL;
	IDXGIAdapter* pDXGIAdapter = NULL;
	IDXGIFactory2* pIDXGIFactory3 = NULL;
	IDXGIOutput* pDXGIOutput = NULL;
	IDXGISwapChain1* pSwapChain1 = NULL;
	ID3D11RenderTargetView* pRenderTargetView = NULL;
	ID3D11Texture2D* pBackBuffer = NULL;
	RECT rect;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc;
	D3D11_VIEWPORT VP;

	hr = _d3d11_device->QueryInterface(__uuidof(IDXGIDevice1), (void**)&pDXGIdevice);
	if (FAILED(hr)) 
		goto done;

	hr = pDXGIdevice->GetParent(__uuidof(IDXGIAdapter), (void**)&pDXGIAdapter);
	if (FAILED(hr)) 
		goto done;

	DXGI_ADAPTER_DESC adapter_desc;
	if (SUCCEEDED(pDXGIAdapter->GetDesc(&adapter_desc))) {
		char adapter_name[128] = { 0 };
		wcstombs(adapter_name, adapter_desc.Description, sizeof(adapter_name) - 1);
		snprintf(_device_detail, sizeof(_device_detail),
			"D3D11 Video Acceleration (%s vendor %u)",
			adapter_name,
			adapter_desc.VendorId);
	}

	hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&pIDXGIFactory3);
	if (FAILED(hr)) 
		goto done;

	hr = pDXGIAdapter->EnumOutputs(0, &pDXGIOutput);
	if (FAILED(hr)) 
		goto done;

	if (GetClientRect(hWnd, &rect) < 0) 
		goto done;

	ZeroMemory(&swapChainDesc, sizeof(swapChainDesc));
	swapChainDesc.Width = rect.right - rect.left;
	swapChainDesc.Height = rect.bottom - rect.top;
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = FALSE;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	//swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	//swapChainDesc.AlphaMode = DXGI_ALPHA_MODE::DXGI_ALPHA_MODE_IGNORE;
	//swapChainDesc.Flags = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	swapChainDesc.Flags = 0;

	hr = pIDXGIFactory3->CreateSwapChainForHwnd(_d3d11_device, hWnd, &swapChainDesc, NULL, NULL, &pSwapChain1);
	if (FAILED(hr)) 
		goto done;

	_swap_chain2 = (IDXGISwapChain2*)pSwapChain1;

	//DXGI_SWAP_CHAIN_DESC1 swapChainDesc2;
	//pSwapChain1->GetDesc1(&swapChainDesc2);

	_swap_chain2->SetMaximumFrameLatency(1);

	//	IF_FAILED_THROW(m_swapchain2->SetFullscreenState(TRUE, NULL));//full screen
	//ResizeSwapChain();

	hr = _swap_chain2->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&pBackBuffer));
	if (FAILED(hr)) 
		goto done;

	// Create a render target view
	hr = _d3d11_device->CreateRenderTargetView(pBackBuffer, nullptr, &pRenderTargetView);
	if (FAILED(hr)) 
		goto done;
	_render_target_view = pRenderTargetView;// tyycode:save render to flush last frame

	// Set new render target
	_d3d11_device_context->OMSetRenderTargets(1, &pRenderTargetView, nullptr);

	VP.Width = swapChainDesc.Width;
	VP.Height = swapChainDesc.Height;
	VP.MinDepth = 0.0f;
	VP.MaxDepth = 1.0f;
	VP.TopLeftX = 0;
	VP.TopLeftY = 0;
	_d3d11_device_context->RSSetViewports(1, &VP);

done:
	if (FAILED(hr))
		SAFE_RELEASE(pSwapChain1);

	SAFE_RELEASE(pBackBuffer);
	//SAFE_RELEASE(pRenderTargetView);
	SAFE_RELEASE(pDXGIOutput);
	SAFE_RELEASE(pIDXGIFactory3);
	SAFE_RELEASE(pDXGIAdapter);
	SAFE_RELEASE(pDXGIdevice);

	return hr;
}

const char *TD3D11VA_Decoder::get_device_detail() const
{
	return _device_detail;
}

void TD3D11VA_Decoder::d3d11_uninit()
{
	// tyy FIXME: flush last frame
	if (_render_target_view) {
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		_d3d11_device_context->ClearRenderTargetView(_render_target_view, clearColor);
		SAFE_RELEASE(_render_target_view);
		if (_swap_chain2 != NULL) {
			_swap_chain2->Present(0, 0);
		}
	}
	
	SAFE_RELEASE(_d3d11_video_processor_enumerator);
	SAFE_RELEASE(_d3d11_video_processor);
	SAFE_RELEASE(_swap_chain2);
}

void TD3D11VA_Decoder::d3d11va_retrieve_data(AVCodecContext* avctx, AVFrame* frame)
{
	HRESULT hr = S_OK;
	D3D11_TEXTURE2D_DESC texture_desc;
	D3D11_TEXTURE2D_DESC bktexture_desc;
	ID3D11RenderTargetView* pRenderTargetView = NULL;
	ID3D11VideoProcessorInputView* pD3D11VideoProcessorInputViewIn = NULL;
	ID3D11VideoProcessorOutputView* pD3D11VideoProcessorOutputView = NULL;
	ID3D11Texture2D* pDXGIBackBuffer = NULL;
	RECT rect;
	int w, h;
	D3D11_VIEWPORT VP;
	int index;
	ID3D11Texture2D* hwTexture;

	if (frame == NULL || frame->format != AV_PIX_FMT_D3D11)
		return;

	index = (intptr_t)frame->data[1];
	hwTexture = (ID3D11Texture2D*)frame->data[0];

	hwTexture->GetDesc(&texture_desc);

	hr = _swap_chain2->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pDXGIBackBuffer);
	if (FAILED(hr)) 
		return;

	pDXGIBackBuffer->GetDesc(&bktexture_desc);

#if 1
	GetClientRect(_hwnd, &rect);

	w = rect.right - rect.left;
	h = rect.bottom - rect.top;

	if (w != bktexture_desc.Width || h != bktexture_desc.Height) {

		//ResizeSwapChain();
		hr = _swap_chain2->ResizeBuffers(
			2,
			w,
			h,
			bktexture_desc.Format,
			0
		);
		if (SUCCEEDED(hr))
		{
#if 1
			pDXGIBackBuffer->GetDesc(&bktexture_desc);
#else
			bktexture_desc.Width = w;
			bktexture_desc.Height = h;
#endif
			// Create a render target view
			hr = _d3d11_device->CreateRenderTargetView(pDXGIBackBuffer, nullptr, &pRenderTargetView);
			if (FAILED(hr)) 
				goto done;
			if (_render_target_view) {
				SAFE_RELEASE(_render_target_view);
			}
			_render_target_view = pRenderTargetView;

			// Set new render target
			_d3d11_device_context->OMSetRenderTargets(1, &pRenderTargetView, nullptr);

			ZeroMemory(&VP, sizeof(VP));
			VP.Width = bktexture_desc.Width;
			VP.Height = bktexture_desc.Height;
			VP.MinDepth = 0.0f;
			VP.MaxDepth = 1.0f;
			VP.TopLeftX = 0;
			VP.TopLeftY = 0;
			_d3d11_device_context->RSSetViewports(1, &VP);

			//SAFE_RELEASE(pRenderTargetView);
			SAFE_RELEASE(_d3d11_video_processor_enumerator);
			SAFE_RELEASE(_d3d11_video_processor);
		}
	}
#endif

	if (!_d3d11_video_processor_enumerator || !_d3d11_video_processor)
	{
		SAFE_RELEASE(_d3d11_video_processor_enumerator);
		SAFE_RELEASE(_d3d11_video_processor);

		D3D11_VIDEO_PROCESSOR_CONTENT_DESC ContentDesc;
		ZeroMemory(&ContentDesc, sizeof(ContentDesc));
		ContentDesc.InputFrameFormat = D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE;
		ContentDesc.InputWidth = texture_desc.Width;
		ContentDesc.InputHeight = texture_desc.Height;
		ContentDesc.OutputWidth = bktexture_desc.Width;
		ContentDesc.OutputHeight = bktexture_desc.Height;
		ContentDesc.Usage = D3D11_VIDEO_USAGE_PLAYBACK_NORMAL;
		hr = _d3d11_video_device->CreateVideoProcessorEnumerator(&ContentDesc, &_d3d11_video_processor_enumerator);
		if (FAILED(hr)) 
			return;

		UINT uiFlags;
		DXGI_FORMAT VP_Output_Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		hr = _d3d11_video_processor_enumerator->CheckVideoProcessorFormat(VP_Output_Format, &uiFlags);
		if (FAILED(hr)) 
			return;

		DXGI_FORMAT VP_input_Format = texture_desc.Format;
		hr = _d3d11_video_processor_enumerator->CheckVideoProcessorFormat(VP_input_Format, &uiFlags);
		if (FAILED(hr)) 
			return;

		//  NV12 surface to RGB backbuffer
		RECT srcrc = { 0, 0, (LONG)texture_desc.Width, (LONG)texture_desc.Height };
		RECT destcrc = { 0, 0, (LONG)bktexture_desc.Width, (LONG)bktexture_desc.Height };

		hr = _d3d11_video_device->CreateVideoProcessor(_d3d11_video_processor_enumerator, 0, &_d3d11_video_processor);
		if (FAILED(hr)) 
			return;

		_d3d11_video_context->VideoProcessorSetStreamFrameFormat(_d3d11_video_processor, 0, D3D11_VIDEO_FRAME_FORMAT_PROGRESSIVE);
		_d3d11_video_context->VideoProcessorSetStreamOutputRate(_d3d11_video_processor, 0, D3D11_VIDEO_PROCESSOR_OUTPUT_RATE_NORMAL, TRUE, NULL);

		_d3d11_video_context->VideoProcessorSetStreamSourceRect(_d3d11_video_processor, 0, TRUE, &srcrc);
		_d3d11_video_context->VideoProcessorSetStreamDestRect(_d3d11_video_processor, 0, TRUE, &destcrc);
		_d3d11_video_context->VideoProcessorSetOutputTargetRect(_d3d11_video_processor, TRUE, &destcrc);


		D3D11_VIDEO_COLOR color;
		color.YCbCr = { 0.0625f, 0.5f, 0.5f, 0.5f }; // black color
		_d3d11_video_context->VideoProcessorSetOutputBackgroundColor(_d3d11_video_processor, TRUE, &color);

	}

	//        IF_FAILED_THROW(_d3d11_device->CreateRenderTargetView(m_pDXGIBackBuffer, nullptr, &_rtv));
	//        D3D11_VIEWPORT VP;
	//        ZeroMemory(&VP, sizeof(VP));
	//        VP.Width = bktexture_desc.Width;
	//        VP.Height = bktexture_desc.Height;
	//        VP.MinDepth = 0.0f;
	//        VP.MaxDepth = 1.0f;
	//        VP.TopLeftX = 0;
	//        VP.TopLeftY = 0;
	//        _d3d11_device_context->RSSetViewports(1, &VP);

	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC pInDesc;
	ZeroMemory(&pInDesc, sizeof(pInDesc));
	D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC InputViewDesc;

	pInDesc.FourCC = 0;
	pInDesc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
	pInDesc.Texture2D.MipSlice = 0;
	pInDesc.Texture2D.ArraySlice = index;

	hr = _d3d11_video_device->CreateVideoProcessorInputView(hwTexture, _d3d11_video_processor_enumerator, &pInDesc, &pD3D11VideoProcessorInputViewIn);
	if (FAILED(hr)) 
		goto done;

	D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC pOutDesc;
	ZeroMemory(&pOutDesc, sizeof(pOutDesc));

	pOutDesc.ViewDimension = D3D11_VPOV_DIMENSION_TEXTURE2D;
	pOutDesc.Texture2D.MipSlice = 0;

	hr = _d3d11_video_device->CreateVideoProcessorOutputView(pDXGIBackBuffer, _d3d11_video_processor_enumerator, &pOutDesc, &pD3D11VideoProcessorOutputView);
	if (FAILED(hr)) 
		goto done;

	D3D11_VIDEO_PROCESSOR_STREAM StreamData;
	ZeroMemory(&StreamData, sizeof(StreamData));
	StreamData.Enable = TRUE;
	StreamData.OutputIndex = 0;
	StreamData.InputFrameOrField = 0;
	StreamData.PastFrames = 0;
	StreamData.FutureFrames = 0;
	StreamData.ppPastSurfaces = NULL;
	StreamData.ppFutureSurfaces = NULL;
	StreamData.pInputSurface = pD3D11VideoProcessorInputViewIn;
	StreamData.ppPastSurfacesRight = NULL;
	StreamData.ppFutureSurfacesRight = NULL;

	hr = _d3d11_video_context->VideoProcessorBlt(_d3d11_video_processor, pD3D11VideoProcessorOutputView, 0, 1, &StreamData);
	if (FAILED(hr)) 
		goto done;

	DXGI_PRESENT_PARAMETERS parameters;
	ZeroMemory(&parameters, sizeof(parameters));

	if (_swap_chain2 != NULL)
	{
		//hr = m_swapchain2->Present1(0, DXGI_PRESENT_ALLOW_TEARING, &parameters);
		_swap_chain2->Present1(0, DXGI_PRESENT_DO_NOT_WAIT, &parameters);
	}

done:
	//SAFE_RELEASE(hwTexture);
	SAFE_RELEASE(pD3D11VideoProcessorOutputView);
	SAFE_RELEASE(pD3D11VideoProcessorInputViewIn);
	SAFE_RELEASE(pDXGIBackBuffer);
}
