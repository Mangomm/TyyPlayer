#include "media_info_dialog.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QTimerEvent>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/pixdesc.h"
}

MediaInfoDialog::MediaInfoDialog(QWidget *parent)
    : QDialog(parent),
      _tree_widget(new QTreeWidget(this)),
      _statistics_item(nullptr),
      _player_handle(nullptr),
      _statistics_timer_id(0)
{
    setWindowTitle("Media Info");
    resize(680, 460);

    _tree_widget->setColumnCount(2);
    _tree_widget->setHeaderLabels(QStringList() << "Name" << "Value");
    _tree_widget->header()->setStretchLastSection(true);
    _tree_widget->setAlternatingRowColors(true);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->addWidget(_tree_widget);
}

bool MediaInfoDialog::set_media_file(const QString &file_name)
{
    _tree_widget->clear();

    AVFormatContext *format_context = nullptr;
    QByteArray local_file_name = file_name.toLocal8Bit();
    int ret = avformat_open_input(&format_context, local_file_name.constData(), nullptr, nullptr);
    if (ret < 0)
    {
        return false;
    }

    ret = avformat_find_stream_info(format_context, nullptr);
    if (ret < 0)
    {
        avformat_close_input(&format_context);
        return false;
    }

    QFileInfo file_info(file_name);
    QTreeWidgetItem *base_item = new QTreeWidgetItem(_tree_widget, QStringList() << "Base" << QString());
    add_item(base_item, "File Path", file_name);
    add_item(base_item, "File Name", file_info.fileName());
    add_item(base_item, "Format", QString::fromUtf8(format_context->iformat->long_name));
    add_item(base_item, "Duration", format_duration(format_context->duration));
    add_item(base_item, "Bit Rate", format_bit_rate(format_context->bit_rate));

    for (unsigned int index = 0; index < format_context->nb_streams; ++index)
    {
        AVStream *stream = format_context->streams[index];
        if (stream == nullptr || stream->codecpar == nullptr)
        {
            continue;
        }

        AVCodecParameters *codecpar = stream->codecpar;
        QString stream_title;
        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            stream_title = QString("Video Stream %1").arg(index);
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            stream_title = QString("Audio Stream %1").arg(index);
        }
        else
        {
            continue;
        }

        QTreeWidgetItem *stream_item = new QTreeWidgetItem(_tree_widget, QStringList() << stream_title << QString());
        add_item(stream_item, "Codec", QString::fromUtf8(avcodec_get_name(codecpar->codec_id)));
        add_item(stream_item, "Bit Rate", format_bit_rate(codecpar->bit_rate));

        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            add_item(stream_item, "Resolution", QString("%1 x %2").arg(codecpar->width).arg(codecpar->height));
            add_item(stream_item, "Pixel Format", QString::fromUtf8(av_get_pix_fmt_name(static_cast<AVPixelFormat>(codecpar->format))));
            AVRational frame_rate = stream->avg_frame_rate.num != 0 ? stream->avg_frame_rate : stream->r_frame_rate;
            double fps = frame_rate.den != 0 ? av_q2d(frame_rate) : 0.0;
            add_item(stream_item, "Frame Rate", fps > 0.0 ? QString::number(fps, 'f', 3) + " fps" : "N/A");
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            add_item(stream_item, "Sample Rate", codecpar->sample_rate > 0 ? QString::number(codecpar->sample_rate) + " Hz" : "N/A");
            add_item(stream_item, "Channels", codecpar->channels > 0 ? QString::number(codecpar->channels) : "N/A");
        }
    }

    _tree_widget->expandAll();
    _tree_widget->resizeColumnToContents(0);
    avformat_close_input(&format_context);
    return true;
}

void MediaInfoDialog::set_statistics(const TyyPlayerStatistics &statistics)
{
    if (!statistics.valid)
    {
        return;
    }

    if (_statistics_item == nullptr)
    {
        _statistics_item = new QTreeWidgetItem(_tree_widget, QStringList() << "Runtime Statistics" << QString());
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Display FPS" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Stream FPS" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Master Clock" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "A/V Diff" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Audio Queue" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Video Queue" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Subtitle Queue" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Frame Drops" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Faulty DTS" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Faulty PTS" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Video Decoder" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Video Decoder Detail" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Audio Decoder" << QString()));
        _statistics_items.append(new QTreeWidgetItem(_statistics_item, QStringList() << "Audio Decoder Detail" << QString()));
    }

    set_item_value(_statistics_items.value(0), statistics.display_fps > 0.0 ? QString::number(statistics.display_fps, 'f', 2) + " fps" : "N/A");
    set_item_value(_statistics_items.value(1), statistics.stream_fps > 0.0 ? QString::number(statistics.stream_fps, 'f', 2) + " fps" : "N/A");
    set_item_value(_statistics_items.value(2), QString::number(statistics.master_clock, 'f', 3));
    set_item_value(_statistics_items.value(3), QString::number(statistics.av_diff, 'f', 3));
    set_item_value(_statistics_items.value(4), QString::number(statistics.audio_queue_size) + " KB");
    set_item_value(_statistics_items.value(5), QString::number(statistics.video_queue_size) + " KB");
    set_item_value(_statistics_items.value(6), QString::number(statistics.subtitle_queue_size) + " B");
    set_item_value(_statistics_items.value(7), QString::number(statistics.frame_drops));
    set_item_value(_statistics_items.value(8), QString::number(statistics.faulty_dts));
    set_item_value(_statistics_items.value(9), QString::number(statistics.faulty_pts));
    set_item_value(_statistics_items.value(10), QString::fromLocal8Bit(statistics.video_decoder));
    set_item_value(_statistics_items.value(11), QString::fromLocal8Bit(statistics.video_decoder_detail));
    set_item_value(_statistics_items.value(12), QString::fromLocal8Bit(statistics.audio_decoder));
    set_item_value(_statistics_items.value(13), QString::fromLocal8Bit(statistics.audio_decoder_detail));

    _tree_widget->expandAll();
    _tree_widget->resizeColumnToContents(0);
}

void MediaInfoDialog::set_player_handle(TyyPlayerHandle player_handle)
{
    _player_handle = player_handle;
    refresh_statistics();
    if (_player_handle != nullptr && _statistics_timer_id == 0)
    {
        _statistics_timer_id = startTimer(1000);
    }
}

void MediaInfoDialog::timerEvent(QTimerEvent *event)
{
    if (event != nullptr && event->timerId() == _statistics_timer_id)
    {
        refresh_statistics();
        return;
    }

    QDialog::timerEvent(event);
}

void MediaInfoDialog::add_item(QTreeWidgetItem *parent, const QString &name, const QString &value)
{
    if (parent == nullptr)
    {
        return;
    }

    new QTreeWidgetItem(parent, QStringList() << name << (value.isEmpty() ? "N/A" : value));
}

void MediaInfoDialog::set_item_value(QTreeWidgetItem *item, const QString &value)
{
    if (item == nullptr)
    {
        return;
    }

    item->setText(1, value.isEmpty() ? "N/A" : value);
}

void MediaInfoDialog::refresh_statistics()
{
    if (_player_handle == nullptr)
    {
        return;
    }

    TyyPlayerStatistics statistics;
    if (tyy_player_get_statistics(_player_handle, &statistics) == TYY_PLAYER_ERROR_OK)
    {
        set_statistics(statistics);
    }
}

QString MediaInfoDialog::format_duration(qint64 duration) const
{
    if (duration <= 0)
    {
        return "N/A";
    }

    qint64 total_seconds = duration / AV_TIME_BASE;
    qint64 hours = total_seconds / 3600;
    qint64 minutes = (total_seconds % 3600) / 60;
    qint64 seconds = total_seconds % 60;
    return QString("%1:%2:%3")
        .arg(hours, 2, 10, QLatin1Char('0'))
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString MediaInfoDialog::format_bit_rate(qint64 bit_rate) const
{
    if (bit_rate <= 0)
    {
        return "N/A";
    }

    if (bit_rate >= 1000000)
    {
        return QString::number(bit_rate / 1000000.0, 'f', 2) + " Mbps";
    }
    return QString::number(bit_rate / 1000.0, 'f', 2) + " Kbps";
}
