#include "video_frame_extractor.h"

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include "libswscale/swscale.h"
}

VideoFrameExtractor::VideoFrameExtractor()
    : _format_context(nullptr),
      _codec_context(nullptr),
      _frame(nullptr),
      _rgb_frame(nullptr),
      _sws_context(nullptr),
      _video_stream_index(-1)
{
}

VideoFrameExtractor::~VideoFrameExtractor()
{
    close();
}

bool VideoFrameExtractor::set_file(const QString &file_name)
{
    if (_file_name == file_name && _format_context != nullptr && _codec_context != nullptr)
    {
        return true;
    }

    close();
    _file_name = file_name;
    if (_file_name.isEmpty())
    {
        return false;
    }

    QByteArray local_file_name = _file_name.toLocal8Bit();
    if (avformat_open_input(&_format_context, local_file_name.constData(), nullptr, nullptr) < 0)
    {
        close();
        return false;
    }

    if (avformat_find_stream_info(_format_context, nullptr) < 0)
    {
        close();
        return false;
    }

    return open_decoder();
}

bool VideoFrameExtractor::extract_frame(double seconds, QImage *image)
{
    if (image == nullptr || _format_context == nullptr || _codec_context == nullptr || _video_stream_index < 0)
    {
        return false;
    }

    AVStream *stream = _format_context->streams[_video_stream_index];
    int64_t timestamp = static_cast<int64_t>(seconds / av_q2d(stream->time_base));
    if (av_seek_frame(_format_context, _video_stream_index, timestamp, AVSEEK_FLAG_BACKWARD) < 0)
    {
        return false;
    }
    avcodec_flush_buffers(_codec_context);

    AVPacket packet;
    av_init_packet(&packet);
    bool got_frame = false;
    while (av_read_frame(_format_context, &packet) >= 0)
    {
        if (packet.stream_index == _video_stream_index)
        {
            int ret = avcodec_send_packet(_codec_context, &packet);
            av_packet_unref(&packet);
            if (ret < 0)
            {
                continue;
            }

            ret = avcodec_receive_frame(_codec_context, _frame);
            if (ret == 0)
            {
                got_frame = true;
                break;
            }
        }
        else
        {
            av_packet_unref(&packet);
        }
    }

    if (!got_frame)
    {
        return false;
    }

    int width = _codec_context->width;
    int height = _codec_context->height;
    QImage preview_image(width, height, QImage::Format_RGB32);
    uint8_t *dst_data[4] = { preview_image.bits(), nullptr, nullptr, nullptr };
    int dst_linesize[4] = { preview_image.bytesPerLine(), 0, 0, 0 };

    _sws_context = sws_getCachedContext(
        _sws_context,
        width,
        height,
        _codec_context->pix_fmt,
        width,
        height,
        AV_PIX_FMT_BGRA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (_sws_context == nullptr)
    {
        return false;
    }

    sws_scale(_sws_context, _frame->data, _frame->linesize, 0, height, dst_data, dst_linesize);
    *image = preview_image;
    return true;
}

void VideoFrameExtractor::close()
{
    if (_sws_context != nullptr)
    {
        sws_freeContext(_sws_context);
        _sws_context = nullptr;
    }
    if (_frame != nullptr)
    {
        av_frame_free(&_frame);
    }
    if (_rgb_frame != nullptr)
    {
        av_frame_free(&_rgb_frame);
    }
    if (_codec_context != nullptr)
    {
        avcodec_free_context(&_codec_context);
    }
    if (_format_context != nullptr)
    {
        avformat_close_input(&_format_context);
    }

    _file_name.clear();
    _video_stream_index = -1;
}

bool VideoFrameExtractor::open_decoder()
{
    AVCodec *codec = nullptr;
    _video_stream_index = av_find_best_stream(_format_context, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (_video_stream_index < 0 || codec == nullptr)
    {
        return false;
    }

    _codec_context = avcodec_alloc_context3(codec);
    if (_codec_context == nullptr)
    {
        return false;
    }

    if (avcodec_parameters_to_context(_codec_context, _format_context->streams[_video_stream_index]->codecpar) < 0)
    {
        return false;
    }

    if (avcodec_open2(_codec_context, codec, nullptr) < 0)
    {
        return false;
    }

    _frame = av_frame_alloc();
    _rgb_frame = av_frame_alloc();
    return _frame != nullptr && _rgb_frame != nullptr;
}
