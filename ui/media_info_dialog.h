#ifndef MEDIA_INFO_DIALOG_H
#define MEDIA_INFO_DIALOG_H

#include <QDialog>
#include <QList>

#include "tyy_player_api.h"

class QTreeWidget;
class QTreeWidgetItem;

class MediaInfoDialog : public QDialog
{
    Q_OBJECT

public:
    /**
    * @brief Create media information dialog
    * @author: tyy
    * @param[in] parent parent widget
    * @return none
    * @note:
    */
    explicit MediaInfoDialog(QWidget *parent = nullptr);

    /**
    * @brief Load and show media information
    * @author: tyy
    * @param[in] file_name media file path
    * @return true if media information loaded
    * @note:
    */
    bool set_media_file(const QString &file_name);

    /**
    * @brief Set realtime player statistics
    * @author: tyy
    * @param[in] statistics player statistics
    * @return none
    * @note:
    */
    void set_statistics(const TyyPlayerStatistics &statistics);

    /**
    * @brief Enable realtime statistics refresh
    * @author: tyy
    * @param[in] player_handle player handle
    * @return none
    * @note:
    */
    void set_player_handle(TyyPlayerHandle player_handle);

protected:
    /**
    * @brief Timer event
    * @author: tyy
    * @param[in] event timer event
    * @return none
    * @note:
    */
    void timerEvent(QTimerEvent *event);

private:
    /**
    * @brief Add one information row
    * @author: tyy
    * @param[in] parent parent tree node
    * @param[in] name row name
    * @param[in] value row value
    * @return none
    * @note:
    */
    void add_item(QTreeWidgetItem *parent, const QString &name, const QString &value);

    /**
    * @brief Set one information row value
    * @author: tyy
    * @param[in] item tree item
    * @param[in] value row value
    * @return none
    * @note:
    */
    void set_item_value(QTreeWidgetItem *item, const QString &value);

    /**
    * @brief Refresh realtime statistics
    * @author: tyy
    * @param none
    * @return none
    * @note:
    */
    void refresh_statistics();

    /**
    * @brief Format duration in microseconds
    * @author: tyy
    * @param[in] duration duration value
    * @return formatted duration
    * @note:
    */
    QString format_duration(qint64 duration) const;

    /**
    * @brief Format bit rate value
    * @author: tyy
    * @param[in] bit_rate bit rate value
    * @return formatted bit rate
    * @note:
    */
    QString format_bit_rate(qint64 bit_rate) const;

private:
    // Media information tree view
    QTreeWidget *_tree_widget;

    // Runtime statistics root item
    QTreeWidgetItem *_statistics_item;

    // Runtime statistics row items
    QList<QTreeWidgetItem *> _statistics_items;

    // Player handle used for realtime statistics
    TyyPlayerHandle _player_handle;

    // Runtime statistics refresh timer id
    int _statistics_timer_id;
};

#endif
