#ifndef PREVIEW_WIDGET_H
#define PREVIEW_WIDGET_H

#include <QWidget>

class QLabel;

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    /**
    * @brief Create preview widget
    * @author: tyy
    * @param[in] parent parent widget
    * @return none
    * @note:
    */
    explicit PreviewWidget(QWidget *parent = nullptr);

    /**
    * @brief Set preview image and timestamp text
    * @author: tyy
    * @param[in] image preview image
    * @param[in] time_text timestamp text
    * @return none
    * @note:
    */
    void set_preview(const QImage &image, const QString &time_text);

private:
    // Preview image label
    QLabel *_image_label;

    // Preview time label
    QLabel *_time_label;
};

#endif
