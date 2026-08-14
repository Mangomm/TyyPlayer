#ifndef VIDEO_FRAME_EXTRACTOR_H
#define VIDEO_FRAME_EXTRACTOR_H

#include <QImage>
#include <QString>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
struct SwsContext;

class VideoFrameExtractor
{
public:
    /**
    * @brief Create video frame extractor
    * @author: tyy
    * @param none
    * @return none
    * @note:
    */
    VideoFrameExtractor();

    /**
    * @brief Destroy video frame extractor
    * @author: tyy
    * @param none
    * @return none
    * @note:
    */
    ~VideoFrameExtractor();

    /**
    * @brief Set media file used for preview extraction
    * @author: tyy
    * @param[in] file_name media file path
    * @return true if file opened
    * @note:
    */
    bool set_file(const QString &file_name);

    /**
    * @brief Extract one video frame at timestamp
    * @author: tyy
    * @param[in] seconds preview timestamp
    * @param[out] image extracted image
    * @return true if frame extracted
    * @note:
    */
    bool extract_frame(double seconds, QImage *image);

    /**
    * @brief Close current media file
    * @author: tyy
    * @param none
    * @return none
    * @note:
    */
    void close();

private:
    /**
    * @brief Open decoder for video stream
    * @author: tyy
    * @param none
    * @return true if decoder opened
    * @note:
    */
    bool open_decoder();

private:
    // Current media file path
    QString _file_name;

    // FFmpeg format context
    AVFormatContext *_format_context;

    // FFmpeg decoder context
    AVCodecContext *_codec_context;

    // Decoded frame
    AVFrame *_frame;

    // RGB convert frame
    AVFrame *_rgb_frame;

    // Pixel format converter
    SwsContext *_sws_context;

    // Video stream index
    int _video_stream_index;
};

#endif
