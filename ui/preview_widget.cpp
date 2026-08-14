#include "preview_widget.h"

#include <QImage>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent),
      _image_label(new QLabel(this)),
      _time_label(new QLabel(this))
{
    setWindowFlags(Qt::ToolTip);
    setFixedSize(180, 128);
    setStyleSheet(
        "PreviewWidget {"
        " background: #202020;"
        " border: 1px solid #606060;"
        " border-radius: 4px;"
        "}"
        "QLabel {"
        " color: #ffffff;"
        "}");

    _image_label->setFixedSize(172, 96);
    _image_label->setAlignment(Qt::AlignCenter);
    _time_label->setAlignment(Qt::AlignCenter);

    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(4, 4, 4, 4);
    main_layout->setSpacing(2);
    main_layout->addWidget(_image_label);
    main_layout->addWidget(_time_label);
}

void PreviewWidget::set_preview(const QImage &image, const QString &time_text)
{
    _image_label->setPixmap(QPixmap::fromImage(image).scaled(
        _image_label->size(),
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation));
    _time_label->setText(time_text);
}
