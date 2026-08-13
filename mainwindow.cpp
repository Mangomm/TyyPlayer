#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QAbstractItemView>
#include <QAction>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSplitter>
#include <QStandardPaths>
#include <QStyle>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include "src/player/tyy_log.h"

static const int SPLIT_SCREEN_COUNTS[] = {1, 2, 4, 9, 16, 32};
static const int SPLIT_SCREEN_COUNT_SIZE = sizeof(SPLIT_SCREEN_COUNTS) / sizeof(SPLIT_SCREEN_COUNTS[0]);
static const int MAX_SPLIT_SCREEN_COUNT = 32;
static const char *SETTINGS_PLAYLIST_KEY = "playlist/files";
static const char *SETTINGS_FILE_NAME = "playlist.ini";

/**
* @brief 获取播放列表配置文件路径
* @author: tyy
* @param 无
* @return 配置文件路径
* @note:
*/
static QString get_playlist_settings_file()
{
    QString config_dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (config_dir.isEmpty())
    {
        config_dir = QDir::homePath() + "/.TyyPlayer";
    }

    QDir dir(config_dir);
    if (!dir.exists())
    {
        dir.mkpath(".");
    }

    return dir.filePath(SETTINGS_FILE_NAME);
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      _video_widget(nullptr),
      _play_widget(nullptr),
      _control_widget(nullptr),
      _playlist_panel(nullptr),
      _video_layout(nullptr),
      _time_label(nullptr),
      _playlist_widget(nullptr),
      _play_button(nullptr),
      _pause_button(nullptr),
      _stop_button(nullptr),
      _add_button(nullptr),
      _remove_button(nullptr),
      _playlist_toggle_button(nullptr),
      _progress_slider(nullptr),
      _volume_slider(nullptr),
      _play_timer(nullptr),
      _overlay_hide_timer(nullptr),
      _duration_seconds(300),
      _split_screen_count(1),
      _selected_screen_index(-1),
      _next_default_screen_index(0),
      _overlay_screen_index(-1),
      _fullscreen_screen_index(-1),
      _fullscreen_saved_split_count(1),
      _is_playing(false),
      _playlist_visible(true),
      _player_handle(nullptr)
{
    ui->setupUi(this);
    init_ui();
    init_connections();
    load_playlist();
}

MainWindow::~MainWindow()
{
    save_playlist();
    release_current_player();
    delete ui;
}

void MainWindow::init_ui()
{
    setWindowTitle("TyyPlayer");
    resize(1200, 720);

    QHBoxLayout *main_layout = new QHBoxLayout(ui->centralwidget);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    _play_widget = new QWidget(ui->centralwidget);
    QVBoxLayout *play_layout = new QVBoxLayout(_play_widget);
    play_layout->setContentsMargins(0, 0, 0, 0);
    play_layout->setSpacing(8);

    init_video_grid(_play_widget);

    _control_widget = new QWidget(_play_widget);
    QVBoxLayout *control_layout = new QVBoxLayout(_control_widget);
    control_layout->setContentsMargins(0, 0, 0, 0);
    control_layout->setSpacing(6);

    _progress_slider = new QSlider(Qt::Horizontal, _control_widget);
    _progress_slider->setRange(0, _duration_seconds);

    QHBoxLayout *button_layout = new QHBoxLayout();
    button_layout->setContentsMargins(0, 0, 0, 0);
    button_layout->setSpacing(6);

    QPushButton *backward_button = new QPushButton(_control_widget);
    backward_button->setIcon(style()->standardIcon(QStyle::SP_MediaSeekBackward));
    backward_button->setToolTip("Backward");

    _play_button = new QPushButton(_control_widget);
    _play_button->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    _play_button->setToolTip("Play");

    _pause_button = new QPushButton(_control_widget);
    _pause_button->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    _pause_button->setToolTip("Pause");
    _pause_button->setEnabled(false);

    _stop_button = new QPushButton(_control_widget);
    _stop_button->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    _stop_button->setToolTip("Stop");

    QPushButton *forward_button = new QPushButton(_control_widget);
    forward_button->setIcon(style()->standardIcon(QStyle::SP_MediaSeekForward));
    forward_button->setToolTip("Forward");

    _time_label = new QLabel(_control_widget);
    _time_label->setMinimumWidth(96);
    _time_label->setText(format_time(0) + " / " + format_time(_duration_seconds));

    QLabel *volume_label = new QLabel("Volume", _control_widget);
    _volume_slider = new QSlider(Qt::Horizontal, _control_widget);
    _volume_slider->setRange(0, 100);
    _volume_slider->setValue(80);
    _volume_slider->setMaximumWidth(160);

    _playlist_toggle_button = new QPushButton(_control_widget);
    _playlist_toggle_button->setFixedSize(32, 28);
    _playlist_toggle_button->setCursor(Qt::PointingHandCursor);
    update_playlist_toggle_button();

    button_layout->addWidget(backward_button);
    button_layout->addWidget(_play_button);
    button_layout->addWidget(_pause_button);
    button_layout->addWidget(_stop_button);
    button_layout->addWidget(forward_button);
    button_layout->addSpacing(12);
    button_layout->addWidget(_time_label);
    button_layout->addStretch();
    button_layout->addWidget(_playlist_toggle_button);
    button_layout->addWidget(volume_label);
    button_layout->addWidget(_volume_slider);

    control_layout->addWidget(_progress_slider);
    control_layout->addLayout(button_layout);

    play_layout->addWidget(_video_widget, 1);
    play_layout->addWidget(_control_widget);

    _playlist_panel = new QWidget(ui->centralwidget);
    _playlist_panel->setMinimumWidth(260);
    QVBoxLayout *playlist_layout = new QVBoxLayout(_playlist_panel);
    playlist_layout->setContentsMargins(0, 0, 0, 0);
    playlist_layout->setSpacing(6);

    QLabel *playlist_title = new QLabel("Playlist", _playlist_panel);
    _playlist_widget = new QListWidget(_playlist_panel);
    _playlist_widget->setSelectionMode(QAbstractItemView::ExtendedSelection);

    QHBoxLayout *playlist_button_layout = new QHBoxLayout();
    playlist_button_layout->setContentsMargins(0, 0, 0, 0);
    playlist_button_layout->setSpacing(6);

    _add_button = new QPushButton("Add", _playlist_panel);
    _remove_button = new QPushButton("Remove", _playlist_panel);
    playlist_button_layout->addWidget(_add_button);
    playlist_button_layout->addWidget(_remove_button);

    playlist_layout->addWidget(playlist_title);
    playlist_layout->addWidget(_playlist_widget, 1);
    playlist_layout->addLayout(playlist_button_layout);

    QSplitter *main_splitter = new QSplitter(Qt::Horizontal, ui->centralwidget);
    main_splitter->addWidget(_play_widget);
    main_splitter->addWidget(_playlist_panel);
    main_splitter->setStretchFactor(0, 1);
    main_splitter->setStretchFactor(1, 0);
    main_splitter->setSizes(QList<int>() << 900 << 280);

    main_layout->addWidget(main_splitter, 1);

    _play_timer = new QTimer(this);
    _play_timer->setInterval(1000);
    _overlay_hide_timer = new QTimer(this);
    _overlay_hide_timer->setSingleShot(true);
    _overlay_hide_timer->setInterval(3000);

    connect(backward_button, SIGNAL(clicked()), this, SLOT(seek_backward()));
    connect(forward_button, SIGNAL(clicked()), this, SLOT(seek_forward()));
}

void MainWindow::init_connections()
{
    connect(_add_button, SIGNAL(clicked()), this, SLOT(add_media_files()));
    connect(_remove_button, SIGNAL(clicked()), this, SLOT(remove_selected_media()));
    connect(_playlist_toggle_button, SIGNAL(clicked()), this, SLOT(toggle_playlist_panel()));
    connect(_play_button, SIGNAL(clicked()), this, SLOT(toggle_play()));
    connect(_pause_button, SIGNAL(clicked()), this, SLOT(toggle_pause()));
    connect(_stop_button, SIGNAL(clicked()), this, SLOT(stop_play()));
    connect(_progress_slider, SIGNAL(sliderMoved(int)), this, SLOT(set_play_position(int)));
    connect(_volume_slider, SIGNAL(valueChanged(int)), this, SLOT(set_volume_value(int)));
    connect(_play_timer, SIGNAL(timeout()), this, SLOT(on_play_timer()));
    connect(_playlist_widget, SIGNAL(currentRowChanged(int)), this, SLOT(on_playlist_row_changed(int)));
    connect(_playlist_widget, SIGNAL(itemDoubleClicked(QListWidgetItem*)), this, SLOT(play_playlist_item(QListWidgetItem*)));
    connect(_video_widget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(show_split_menu(QPoint)));
    connect(_overlay_hide_timer, SIGNAL(timeout()), this, SLOT(hide_video_overlay()));
}

void MainWindow::init_video_grid(QWidget *parent)
{
    _video_widget = new QWidget(parent);
    _video_widget->setMinimumSize(640, 360);
    _video_widget->setContextMenuPolicy(Qt::CustomContextMenu);
    _video_widget->setMouseTracking(true);
    _video_widget->installEventFilter(this);

    _video_layout = new QGridLayout(_video_widget);
    _video_layout->setContentsMargins(0, 0, 0, 0);
    _video_layout->setSpacing(2);

    for (int index = 0; index < MAX_SPLIT_SCREEN_COUNT; ++index)
    {
        QLabel *video_label = new QLabel(_video_widget);
        video_label->setAlignment(Qt::AlignCenter);
        video_label->setText(QString("Screen %1").arg(index + 1));
        video_label->setMouseTracking(true);
        video_label->installEventFilter(this);
        _video_labels.push_back(video_label);
        _player_handles.push_back(nullptr);
        _screen_playing.push_back(false);
        _screen_paused.push_back(false);
        update_video_label_style(index);

        QPushButton *full_screen_button = new QPushButton(_video_widget);
        full_screen_button->setText("⛶");
        full_screen_button->setToolTip("全屏显示该路视频");
        full_screen_button->setFixedSize(34, 30);
        full_screen_button->setCursor(Qt::PointingHandCursor);
        full_screen_button->setMouseTracking(true);
        full_screen_button->hide();
        full_screen_button->installEventFilter(this);
        full_screen_button->setProperty("screen_index", index);
        full_screen_button->setStyleSheet(
            "QPushButton {"
            " background: rgba(20, 20, 20, 120);"
            " color: rgba(255, 255, 255, 220);"
            " border: 1px solid rgba(255, 255, 255, 90);"
            " border-radius: 4px;"
            " font-size: 18px;"
            "}"
            "QPushButton:hover {"
            " background: rgba(47, 128, 255, 190);"
            " color: #ffffff;"
            " border: 1px solid rgba(255, 255, 255, 170);"
            "}"
            "QPushButton:pressed {"
            " background: rgba(31, 95, 191, 220);"
            "}");
        connect(full_screen_button, SIGNAL(clicked()), this, SLOT(toggle_video_screen_full_screen()));
        _video_full_screen_buttons.push_back(full_screen_button);
    }

    update_split_screen(1);
}

void MainWindow::add_media_files()
{
    QStringList file_names = QFileDialog::getOpenFileNames(this,
                                                           "Open Media",
                                                           QString(),
                                                           "Media Files (*.mp4 *.mkv *.avi *.mov *.flv *.mp3 *.aac *.wav);;All Files (*.*)");
    for (const QString &file_name : file_names)
    {
        bool exists = false;
        for (int index = 0; index < _playlist_widget->count(); ++index)
        {
            QListWidgetItem *item = _playlist_widget->item(index);
            if (item != nullptr && item->data(Qt::UserRole).toString() == file_name)
            {
                exists = true;
                break;
            }
        }

        if (exists)
        {
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(QFileInfo(file_name).fileName(), _playlist_widget);
        item->setData(Qt::UserRole, file_name);
    }

    if (_playlist_widget->currentRow() < 0 && _playlist_widget->count() > 0)
    {
        _playlist_widget->setCurrentRow(0);
    }

    save_playlist();
}

void MainWindow::remove_selected_media()
{
    QList<QListWidgetItem *> selected_items = _playlist_widget->selectedItems();
    for (QListWidgetItem *item : selected_items)
    {
        delete _playlist_widget->takeItem(_playlist_widget->row(item));
    }

    if (_playlist_widget->count() == 0)
    {
        release_current_player();
        update_play_state(false);
        _progress_slider->setValue(0);
        update_current_media(-1);
        _time_label->setText(format_time(0) + " / " + format_time(_duration_seconds));
    }

    save_playlist();
}

void MainWindow::toggle_play()
{
    if (_playlist_widget->count() == 0)
    {
        add_media_files();
    }

    if (_playlist_widget->count() == 0)
    {
        return;
    }

    if (_playlist_widget->currentRow() < 0)
    {
        _playlist_widget->setCurrentRow(0);
    }

    int screen_index = get_next_play_screen_index();
    int ret = start_current_media(screen_index);
    if (ret != TYY_PLAYER_ERROR_OK)
    {
        release_screen_player(screen_index);
        QMessageBox::warning(this, "TyyPlayer", QString("Open media failed, error=%1").arg(ret));
        return;
    }

    update_play_state(true);
    int old_screen_index = _selected_screen_index;
    _selected_screen_index = -1;
    update_video_label_style(old_screen_index);
    update_pause_button_state();
}

void MainWindow::toggle_pause()
{
    int screen_index = -1;
    TyyPlayerHandle player_handle = get_current_player_handle(&screen_index);
    if (player_handle == nullptr || screen_index < 0 || screen_index >= _screen_paused.size())
    {
        return;
    }

    bool next_paused = !_screen_paused[screen_index];
    int ret = tyy_player_pause(player_handle, next_paused ? 1 : 0);
    if (ret != TYY_PLAYER_ERROR_OK)
    {
        QMessageBox::warning(this, "TyyPlayer", QString("Pause media failed, error=%1").arg(ret));
        return;
    }

    _screen_paused[screen_index] = next_paused;
    update_pause_button_state();
}

void MainWindow::stop_play()
{
    if (_selected_screen_index >= 0 && _selected_screen_index < _player_handles.size())
    {
        release_screen_player(_selected_screen_index);
    }
    else
    {
        release_current_player();
    }

    bool has_playing_screen = false;
    for (int index = 0; index < _screen_playing.size(); ++index)
    {
        if (_screen_playing[index])
        {
            has_playing_screen = true;
            break;
        }
    }
    update_play_state(has_playing_screen);
    _progress_slider->setValue(0);
    _time_label->setText(format_time(0) + " / " + format_time(_duration_seconds));
    update_current_media(_playlist_widget->currentRow());
}

void MainWindow::seek_backward()
{
    int screen_index = -1;
    TyyPlayerHandle player_handle = get_current_player_handle(&screen_index);
    if (player_handle != nullptr)
    {
        int ret = tyy_player_seek(player_handle, 0, 5);
        if (ret != TYY_PLAYER_ERROR_OK)
        {
            QMessageBox::warning(this, "TyyPlayer", QString("Seek media failed, error=%1").arg(ret));
            return;
        }
    }

    int position = _progress_slider->value() - 5;
    if (position < 0)
    {
        position = 0;
    }
    set_play_position(position);
}

void MainWindow::seek_forward()
{
    int screen_index = -1;
    TyyPlayerHandle player_handle = get_current_player_handle(&screen_index);
    if (player_handle != nullptr)
    {
        int ret = tyy_player_seek(player_handle, 1, 5);
        if (ret != TYY_PLAYER_ERROR_OK)
        {
            QMessageBox::warning(this, "TyyPlayer", QString("Seek media failed, error=%1").arg(ret));
            return;
        }
    }

    int position = _progress_slider->value() + 5;
    if (position > _duration_seconds)
    {
        position = _duration_seconds;
    }
    set_play_position(position);
}

void MainWindow::set_play_position(int position)
{
    _progress_slider->setValue(position);
    _time_label->setText(format_time(position) + " / " + format_time(_duration_seconds));
}

void MainWindow::set_volume_value(int volume)
{
    setWindowTitle(QString("TyyPlayer - Volume %1%").arg(volume));
}

void MainWindow::on_play_timer()
{
    int position = _progress_slider->value() + 1;
    if (position >= _duration_seconds)
    {
        position = _duration_seconds;
        update_play_state(false);
    }

    set_play_position(position);
}

void MainWindow::on_playlist_row_changed(int current_row)
{
    update_current_media(current_row);
}

void MainWindow::play_playlist_item(QListWidgetItem *item)
{
    if (item == nullptr)
    {
        return;
    }

    _playlist_widget->setCurrentItem(item);
    toggle_play();
}

void MainWindow::show_split_menu(const QPoint &position)
{
    QMenu menu(this);
    QMenu *split_menu = menu.addMenu("Split Screen");

    for (int index = 0; index < SPLIT_SCREEN_COUNT_SIZE; ++index)
    {
        int split_count = SPLIT_SCREEN_COUNTS[index];
        if (split_count > _video_labels.size())
        {
            continue;
        }

        QAction *action = split_menu->addAction(QString("%1 Split").arg(split_count));
        action->setData(split_count);
        action->setCheckable(true);
        action->setChecked(split_count == _split_screen_count);
        connect(action, SIGNAL(triggered()), this, SLOT(set_split_screen_count()));
    }

    menu.addSeparator();
    QAction *full_screen_action = menu.addAction(isFullScreen() ? "Exit Full Screen" : "Full Screen");
    full_screen_action->setCheckable(true);
    full_screen_action->setChecked(isFullScreen());
    connect(full_screen_action, SIGNAL(triggered()), this, SLOT(toggle_full_screen()));

    menu.exec(_video_widget->mapToGlobal(position));
}

void MainWindow::set_split_screen_count()
{
    QAction *action = qobject_cast<QAction *>(sender());
    if (action == nullptr)
    {
        return;
    }

    update_split_screen(action->data().toInt());
}

void MainWindow::toggle_full_screen()
{
    if (isFullScreen())
    {
        if (_control_widget != nullptr)
        {
            _control_widget->show();
        }
        if (_playlist_panel != nullptr)
        {
            _playlist_panel->setVisible(_playlist_visible);
        }
        if (ui->centralwidget->layout() != nullptr)
        {
            ui->centralwidget->layout()->setContentsMargins(8, 8, 8, 8);
            ui->centralwidget->layout()->setSpacing(8);
        }
        if (_play_widget != nullptr && _play_widget->layout() != nullptr)
        {
            _play_widget->layout()->setSpacing(8);
        }
        showNormal();
    }
    else
    {
        if (_control_widget != nullptr)
        {
            _control_widget->hide();
        }
        if (_playlist_panel != nullptr)
        {
            _playlist_panel->hide();
        }
        if (ui->centralwidget->layout() != nullptr)
        {
            ui->centralwidget->layout()->setContentsMargins(0, 0, 0, 0);
            ui->centralwidget->layout()->setSpacing(0);
        }
        if (_play_widget != nullptr && _play_widget->layout() != nullptr)
        {
            _play_widget->layout()->setSpacing(0);
        }
        showFullScreen();
    }
}

void MainWindow::toggle_playlist_panel()
{
    _playlist_visible = !_playlist_visible;

    if (_playlist_panel != nullptr)
    {
        _playlist_panel->setVisible(_playlist_visible);
    }

    if (_playlist_toggle_button != nullptr)
    {
        update_playlist_toggle_button();
    }
}

void MainWindow::hide_video_overlay()
{
    for (int index = 0; index < _video_full_screen_buttons.size(); ++index)
    {
        if (_video_full_screen_buttons[index] != nullptr)
        {
            _video_full_screen_buttons[index]->hide();
        }
    }
    _overlay_screen_index = -1;
}

void MainWindow::toggle_video_screen_full_screen()
{
    QPushButton *button = qobject_cast<QPushButton *>(sender());
    if (button == nullptr)
    {
        return;
    }

    int screen_index = button->property("screen_index").toInt();
    if (screen_index < 0 || screen_index >= _screen_playing.size() || !_screen_playing[screen_index])
    {
        hide_video_overlay();
        return;
    }

    if (_fullscreen_screen_index == screen_index)
    {
        exit_video_screen_full_screen();
    }
    else
    {
        enter_video_screen_full_screen(screen_index);
    }
}

void MainWindow::update_playlist_toggle_button()
{
    if (_playlist_toggle_button == nullptr)
    {
        return;
    }

    _playlist_toggle_button->setText("☰");
    _playlist_toggle_button->setToolTip(_playlist_visible ? "隐藏播放列表" : "打开播放列表");
    _playlist_toggle_button->setStyleSheet(
        "QPushButton {"
        " background: #202020;"
        " color: #e6e6e6;"
        " border: 1px solid #3a3a3a;"
        " border-radius: 4px;"
        " font-size: 18px;"
        " font-weight: bold;"
        " padding-bottom: 2px;"
        "}"
        "QPushButton:hover {"
        " background: #2f80ff;"
        " color: #ffffff;"
        " border: 1px solid #5a9cff;"
        "}"
        "QPushButton:pressed {"
        " background: #1f5fbf;"
        "}");
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event != nullptr && (event->type() == QEvent::Enter || event->type() == QEvent::MouseMove))
    {
        for (int index = 0; index < _video_labels.size(); ++index)
        {
            if (watched == _video_labels[index])
            {
                show_video_overlay(index);
                return false;
            }
            if (index < _video_full_screen_buttons.size() && watched == _video_full_screen_buttons[index])
            {
                if (_overlay_hide_timer != nullptr)
                {
                    _overlay_hide_timer->start();
                }
                return false;
            }
        }
    }

    if (event != nullptr && event->type() == QEvent::Leave)
    {
        for (int index = 0; index < _video_labels.size(); ++index)
        {
            if (watched == _video_labels[index])
            {
                if (_overlay_hide_timer != nullptr)
                {
                    _overlay_hide_timer->start();
                }
                return false;
            }
        }
    }

    if (event != nullptr && event->type() == QEvent::Resize)
    {
        for (int index = 0; index < _video_labels.size(); ++index)
        {
            if (watched == _video_labels[index])
            {
                update_video_overlay_geometry(index);
                return false;
            }
        }
    }

    if (event != nullptr && event->type() == QEvent::MouseButtonPress)
    {
        QMouseEvent *mouse_event = static_cast<QMouseEvent *>(event);
        if (mouse_event->button() == Qt::LeftButton)
        {
            for (int index = 0; index < _video_labels.size(); ++index)
            {
                if (watched == _video_labels[index])
                {
                    select_video_screen(index);
                    return false;
                }
            }
        }
    }

    if (event != nullptr && event->type() == QEvent::MouseButtonDblClick)
    {
        if (watched == _video_widget)
        {
            toggle_full_screen();
            return true;
        }

        for (int index = 0; index < _video_labels.size(); ++index)
        {
            if (watched == _video_labels[index])
            {
                if (_fullscreen_screen_index == index)
                {
                    exit_video_screen_full_screen();
                }
                else
                {
                    enter_video_screen_full_screen(index);
                }
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::update_play_state(bool is_playing)
{
    _is_playing = is_playing;

    if (_is_playing)
    {
        _play_button->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        _play_button->setToolTip("Play");
        _pause_button->setEnabled(true);
        _play_timer->start();
    }
    else
    {
        _play_button->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        _play_button->setToolTip("Play");
        _pause_button->setEnabled(false);
        _play_timer->stop();
    }

    update_pause_button_state();
}

void MainWindow::update_pause_button_state()
{
    if (_pause_button == nullptr)
    {
        return;
    }

    int screen_index = -1;
    if (_selected_screen_index >= 0 && _selected_screen_index < _screen_playing.size() &&
        _screen_playing[_selected_screen_index])
    {
        screen_index = _selected_screen_index;
    }
    else if (_player_handle != nullptr)
    {
        for (int index = 0; index < _player_handles.size(); ++index)
        {
            if (_player_handles[index] == _player_handle && index < _screen_playing.size() && _screen_playing[index])
            {
                screen_index = index;
                break;
            }
        }
    }

    bool can_pause = screen_index >= 0 && screen_index < _screen_paused.size();
    _pause_button->setEnabled(can_pause);

    if (can_pause && _screen_paused[screen_index])
    {
        _pause_button->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        _pause_button->setToolTip("Resume");
    }
    else
    {
        _pause_button->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
        _pause_button->setToolTip("Pause");
    }
}

TyyPlayerHandle MainWindow::get_current_player_handle(int *screen_index) const
{
    int current_screen_index = -1;
    TyyPlayerHandle player_handle = _player_handle;
    if (_selected_screen_index >= 0 && _selected_screen_index < _player_handles.size())
    {
        current_screen_index = _selected_screen_index;
        player_handle = _player_handles[_selected_screen_index];
    }
    else
    {
        for (int index = 0; index < _player_handles.size(); ++index)
        {
            if (_player_handles[index] == player_handle)
            {
                current_screen_index = index;
                break;
            }
        }
    }

    if (screen_index != nullptr)
    {
        *screen_index = current_screen_index;
    }

    return player_handle;
}

void MainWindow::update_current_media(int current_row)
{
    QListWidgetItem *item = _playlist_widget->item(current_row);
    if (item == nullptr)
    {
        for (int index = 0; index < _video_labels.size(); ++index)
        {
            if (index >= _player_handles.size() || _player_handles[index] == nullptr)
            {
                _video_labels[index]->setText(QString("Screen %1").arg(index + 1));
            }
        }
        return;
    }

    _progress_slider->setValue(0);
    int screen_index = _selected_screen_index;
    if (screen_index < 0 || screen_index >= _split_screen_count)
    {
        screen_index = _next_default_screen_index;
    }
    if (screen_index >= 0 && screen_index < _video_labels.size() &&
        (screen_index >= _player_handles.size() || _player_handles[screen_index] == nullptr))
    {
        _video_labels[screen_index]->setText(item->text());
    }
    _time_label->setText(format_time(0) + " / " + format_time(_duration_seconds));
}

void MainWindow::update_split_screen(int split_count)
{
    if (split_count <= 0 || split_count > _video_labels.size())
    {
        return;
    }

    while (_video_layout->count() > 0)
    {
        QLayoutItem *layout_item = _video_layout->takeAt(0);
        if (layout_item != nullptr)
        {
            delete layout_item;
        }
    }

    _split_screen_count = split_count;
    int columns = get_split_columns(split_count);
    for (int index = 0; index < _video_labels.size(); ++index)
    {
        QLabel *video_label = _video_labels[index];
        if (index < split_count)
        {
            int row = index / columns;
            int column = index % columns;
            video_label->show();
            _video_layout->addWidget(video_label, row, column);
        }
        else
        {
            video_label->hide();
        }
        update_video_label_style(index);
        update_video_overlay_geometry(index);
    }

    if (_selected_screen_index >= split_count)
    {
        _selected_screen_index = -1;
    }
    if (_next_default_screen_index >= split_count)
    {
        _next_default_screen_index = 0;
    }
}

void MainWindow::select_video_screen(int screen_index)
{
    if (screen_index < 0 || screen_index >= _split_screen_count)
    {
        return;
    }

    int old_screen_index = _selected_screen_index;
    _selected_screen_index = screen_index;
    update_video_label_style(old_screen_index);
    update_video_label_style(_selected_screen_index);
    update_pause_button_state();
}

void MainWindow::update_video_label_style(int screen_index)
{
    if (screen_index < 0 || screen_index >= _video_labels.size())
    {
        return;
    }

    QLabel *video_label = _video_labels[screen_index];
    if (video_label == nullptr)
    {
        return;
    }

    if (screen_index == _selected_screen_index)
    {
        video_label->setStyleSheet("QLabel { background: #101010; color: #d8d8d8; border: 2px solid #2f80ff; }");
    }
    else
    {
        video_label->setStyleSheet("QLabel { background: #101010; color: #d8d8d8; border: 1px solid #303030; }");
    }
}

void MainWindow::show_video_overlay(int screen_index)
{
    if (screen_index < 0 || screen_index >= _video_full_screen_buttons.size())
    {
        return;
    }
    if (screen_index >= _screen_playing.size() || !_screen_playing[screen_index])
    {
        hide_video_overlay();
        return;
    }
    if (screen_index >= _split_screen_count && _fullscreen_screen_index < 0)
    {
        return;
    }

    QPushButton *button = _video_full_screen_buttons[screen_index];
    if (button == nullptr)
    {
        return;
    }

    if (_overlay_screen_index == screen_index && button->isVisible())
    {
        if (_overlay_hide_timer != nullptr)
        {
            _overlay_hide_timer->start();
        }
        return;
    }

    for (int index = 0; index < _video_full_screen_buttons.size(); ++index)
    {
        if (index != screen_index && _video_full_screen_buttons[index] != nullptr)
        {
            _video_full_screen_buttons[index]->hide();
        }
    }

    _overlay_screen_index = screen_index;
    update_video_overlay_geometry(screen_index);
    button->setToolTip(_fullscreen_screen_index == screen_index ? "退出该路全屏" : "全屏显示该路视频");
    button->show();
    button->raise();

    if (_overlay_hide_timer != nullptr)
    {
        _overlay_hide_timer->start();
    }
}

void MainWindow::update_video_overlay_geometry(int screen_index)
{
    if (screen_index < 0 || screen_index >= _video_full_screen_buttons.size() ||
        screen_index >= _video_labels.size())
    {
        return;
    }

    QPushButton *button = _video_full_screen_buttons[screen_index];
    QLabel *video_label = _video_labels[screen_index];
    if (button == nullptr || video_label == nullptr)
    {
        return;
    }

    QRect label_rect = video_label->geometry();
    int margin = 10;
    int x = label_rect.right() - button->width() - margin + 1;
    int y = label_rect.bottom() - button->height() - margin + 1;
    button->move(x, y);
}

void MainWindow::enter_video_screen_full_screen(int screen_index)
{
    if (screen_index < 0 || screen_index >= _video_labels.size())
    {
        return;
    }

    _fullscreen_saved_split_count = _split_screen_count;
    _fullscreen_screen_index = screen_index;
    hide_video_overlay();

    while (_video_layout->count() > 0)
    {
        QLayoutItem *layout_item = _video_layout->takeAt(0);
        if (layout_item != nullptr)
        {
            delete layout_item;
        }
    }
    _video_layout->setSpacing(0);

    for (int index = 0; index < _video_labels.size(); ++index)
    {
        QLabel *video_label = _video_labels[index];
        if (video_label == nullptr)
        {
            continue;
        }

        if (index == screen_index)
        {
            video_label->setMinimumSize(0, 0);
            video_label->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
            video_label->show();
            _video_layout->addWidget(video_label, 0, 0);
        }
        else
        {
            video_label->setMinimumSize(0, 0);
            video_label->setMaximumSize(1, 1);
            video_label->move(-10000, -10000);
            video_label->resize(1, 1);
        }
        if (index < _video_full_screen_buttons.size() && _video_full_screen_buttons[index] != nullptr)
        {
            _video_full_screen_buttons[index]->hide();
        }
    }

    if (_control_widget != nullptr)
    {
        _control_widget->hide();
    }
    if (_playlist_panel != nullptr)
    {
        _playlist_panel->hide();
    }
    if (ui->centralwidget->layout() != nullptr)
    {
        ui->centralwidget->layout()->setContentsMargins(0, 0, 0, 0);
        ui->centralwidget->layout()->setSpacing(0);
    }
    if (_play_widget != nullptr && _play_widget->layout() != nullptr)
    {
        _play_widget->layout()->setSpacing(0);
    }

    showFullScreen();
    _video_layout->activate();
    if (_video_labels[screen_index] != nullptr)
    {
        _video_labels[screen_index]->updateGeometry();
        _video_labels[screen_index]->update();
    }
}

void MainWindow::exit_video_screen_full_screen()
{
    if (_fullscreen_screen_index < 0)
    {
        return;
    }

    _fullscreen_screen_index = -1;
    hide_video_overlay();

    for (int index = 0; index < _video_labels.size(); ++index)
    {
        if (_video_labels[index] != nullptr)
        {
            _video_labels[index]->setMinimumSize(0, 0);
            _video_labels[index]->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        }
    }

    if (_control_widget != nullptr)
    {
        _control_widget->show();
    }
    if (_playlist_panel != nullptr)
    {
        _playlist_panel->setVisible(_playlist_visible);
    }
    if (ui->centralwidget->layout() != nullptr)
    {
        ui->centralwidget->layout()->setContentsMargins(8, 8, 8, 8);
        ui->centralwidget->layout()->setSpacing(8);
    }
    if (_play_widget != nullptr && _play_widget->layout() != nullptr)
    {
        _play_widget->layout()->setSpacing(8);
    }

    showNormal();
    _video_layout->setSpacing(2);
    if (_fullscreen_saved_split_count <= 0 || _fullscreen_saved_split_count > _video_labels.size())
    {
        _fullscreen_saved_split_count = _split_screen_count;
    }
    update_split_screen(_fullscreen_saved_split_count);
}

int MainWindow::get_next_play_screen_index()
{
    if (_selected_screen_index >= 0 && _selected_screen_index < _split_screen_count)
    {
        return _selected_screen_index;
    }

    if (_split_screen_count <= 0)
    {
        return 0;
    }

    int screen_index = _next_default_screen_index;
    if (screen_index < 0 || screen_index >= _split_screen_count)
    {
        screen_index = 0;
    }

    _next_default_screen_index = (screen_index + 1) % _split_screen_count;
    return screen_index;
}

void MainWindow::load_playlist()
{
    QSettings settings(get_playlist_settings_file(), QSettings::IniFormat);
    QStringList file_names = settings.value(SETTINGS_PLAYLIST_KEY).toStringList();
    for (const QString &file_name : file_names)
    {
        if (file_name.isEmpty())
        {
            continue;
        }

        QListWidgetItem *item = new QListWidgetItem(QFileInfo(file_name).fileName(), _playlist_widget);
        item->setData(Qt::UserRole, file_name);
    }

    if (_playlist_widget->count() > 0)
    {
        _playlist_widget->setCurrentRow(0);
    }
}

void MainWindow::save_playlist() const
{
    QStringList file_names;
    for (int index = 0; index < _playlist_widget->count(); ++index)
    {
        QListWidgetItem *item = _playlist_widget->item(index);
        if (item == nullptr)
        {
            continue;
        }

        QString file_name = item->data(Qt::UserRole).toString();
        if (!file_name.isEmpty())
        {
            file_names.append(file_name);
        }
    }

    QSettings settings(get_playlist_settings_file(), QSettings::IniFormat);
    settings.setValue(SETTINGS_PLAYLIST_KEY, file_names);
}

int MainWindow::start_current_media()
{
    return start_current_media(get_next_play_screen_index());
}

int MainWindow::start_current_media(int screen_index)
{
    if (_playlist_widget->currentRow() < 0 || _video_labels.isEmpty())
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }
    if (screen_index < 0 || screen_index >= _video_labels.size() || screen_index >= _split_screen_count)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }

    QListWidgetItem *item = _playlist_widget->item(_playlist_widget->currentRow());
    if (item == nullptr)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    release_screen_player(screen_index);

    TyyPlayerHandle player_handle = tyy_player_create();
    if (player_handle == nullptr)
    {
        return TYY_PLAYER_ERROR_ALLOC_FAILED;
    }

    QLabel *video_label = _video_labels[screen_index];
    video_label->setText(QString());
    video_label->setUpdatesEnabled(false);
    video_label->show();
    unsigned long long win_id = static_cast<unsigned long long>(video_label->winId());
    TYYINFO("qt video label index: {}, win_id: {}", screen_index, win_id);
    int ret = tyy_player_set_window(player_handle, win_id);
    if (ret != TYY_PLAYER_ERROR_OK)
    {
        tyy_player_destroy(player_handle);
        return ret;
    }

    QByteArray file_name = item->data(Qt::UserRole).toString().toLocal8Bit();
    ret = tyy_player_open(player_handle, file_name.constData());
    if (ret != TYY_PLAYER_ERROR_OK)
    {
        tyy_player_destroy(player_handle);
        return ret;
    }

    ret = tyy_player_start(player_handle);
    if (ret != TYY_PLAYER_ERROR_OK)
    {
        tyy_player_destroy(player_handle);
        return ret;
    }

    _player_handles[screen_index] = player_handle;
    _screen_playing[screen_index] = true;
    _screen_paused[screen_index] = false;
    _player_handle = player_handle;
    update_video_label_style(screen_index);
    update_pause_button_state();

    return TYY_PLAYER_ERROR_OK;
}

void MainWindow::release_current_player()
{
    for (int index = 0; index < _player_handles.size(); ++index)
    {
        release_screen_player(index);
    }

    _player_handle = nullptr;
}

void MainWindow::release_screen_player(int screen_index)
{
    if (screen_index < 0 || screen_index >= _player_handles.size())
    {
        return;
    }

    TyyPlayerHandle player_handle = _player_handles[screen_index];
    if (player_handle != nullptr)
    {
        tyy_player_stop(player_handle);
        tyy_player_close(player_handle);
        tyy_player_destroy(player_handle);
        _player_handles[screen_index] = nullptr;
    }

    _screen_playing[screen_index] = false;
    if (screen_index < _screen_paused.size())
    {
        _screen_paused[screen_index] = false;
    }
    if (_overlay_screen_index == screen_index)
    {
        hide_video_overlay();
    }
    if (_player_handle == player_handle)
    {
        _player_handle = nullptr;
    }

    if (screen_index < _video_labels.size())
    {
        _video_labels[screen_index]->setUpdatesEnabled(true);
    }
    update_pause_button_state();
}

int MainWindow::get_split_columns(int split_count) const
{
    if (split_count <= 1)
    {
        return 1;
    }
    if (split_count <= 2)
    {
        return 2;
    }
    if (split_count <= 4)
    {
        return 2;
    }
    if (split_count <= 9)
    {
        return 3;
    }
    if (split_count <= 16)
    {
        return 4;
    }
    return 8;
}

QString MainWindow::format_time(int seconds) const
{
    int minutes = seconds / 60;
    int remain_seconds = seconds % 60;
    return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(remain_seconds, 2, 10, QChar('0'));
}
