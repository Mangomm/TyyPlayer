#include "tyy_ffmpeg_hw.h"
using namespace TyyPlayer;

TFFMPEG_HW::TFFMPEG_HW():
	_hw_device_ctx (NULL) {
}

TFFMPEG_HW::~TFFMPEG_HW() {

}

int TFFMPEG_HW::hw_decoder_init(AVCodecContext* ctx, const enum AVHWDeviceType type, HWND hwnd)
{
	AVDictionary* opts = NULL;
	char* device = NULL;
	int err = 0;

	if (type == AV_HWDEVICE_TYPE_D3D11VA) {
		device = "0";
	}

	if ((err = av_hwdevice_ctx_create(&_hw_device_ctx, type,
		device, opts, 0)) < 0) {
		goto error;
	}
	// _hw_device_ctx ref count +1, save in AVCodecContext
	ctx->hw_device_ctx = av_buffer_ref(_hw_device_ctx);

error:
	av_dict_free(&opts);
	return err;
}

void TFFMPEG_HW::hw_decoder_uninit() {
	if (_hw_device_ctx) {
		av_buffer_unref(&_hw_device_ctx);
		_hw_device_ctx = NULL;
	}
}

AVBufferRef* TFFMPEG_HW::get_hw_device_ctx() {
	return _hw_device_ctx;
}
