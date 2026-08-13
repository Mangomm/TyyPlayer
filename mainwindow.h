#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>

#include "tyy_player_api.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class QLabel;
class QGridLayout;
class QListWidget;
class QListWidgetItem;
class QPoint;
class QPushButton;
class QSlider;
class QTimer;
class QWidget;
class QEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    /**
    * @brief 创建主窗口并初始化播放器界面
    * @author: tyy
    * @param[in] parent 父窗口指针
    * @return 无
    * @note:
    */
    MainWindow(QWidget *parent = nullptr);

    /**
    * @brief 销毁主窗口并释放界面资源
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~MainWindow();

protected:
    /**
    * @brief Filter video area mouse double click events
    * @author: tyy
    * @param[in] watched Event source object
    * @param[in] event Qt event object
    * @return true if event handled
    * @note:
    */
    bool eventFilter(QObject *watched, QEvent *event);

private slots:
    /**
    * @brief 添加媒体文件到播放列表
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void add_media_files();

    /**
    * @brief 删除播放列表中选中的媒体文件
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void remove_selected_media();

    /**
    * @brief 切换播放和暂停状态
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void toggle_play();

    /**
    * @brief 切换暂停和恢复状态
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void toggle_pause();

    /**
    * @brief 停止当前播放的视频
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void stop_play();

    /**
    * @brief 向后跳转播放进度
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void seek_backward();

    /**
    * @brief 向前跳转播放进度
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void seek_forward();

    /**
    * @brief 设置播放进度位置
    * @author: tyy
    * @param[in] position 播放进度，单位秒
    * @return 无
    * @note:
    */
    void set_play_position(int position);

    /**
    * @brief 设置音量显示值
    * @author: tyy
    * @param[in] volume 音量值，范围0到100
    * @return 无
    * @note: 当前只更新窗口标题，暂不接入底层音频控制
    */
    void set_volume_value(int volume);

    /**
    * @brief 播放定时器回调
    * @author: tyy
    * @param 无
    * @return 无
    * @note: 当前用于模拟播放进度递增
    */
    void on_play_timer();

    /**
    * @brief 播放列表当前行变化回调
    * @author: tyy
    * @param[in] current_row 当前播放列表行号
    * @return 无
    * @note:
    */
    void on_playlist_row_changed(int current_row);

    /**
    * @brief 双击播放列表文件后直接播放
    * @author: tyy
    * @param[in] item 双击的播放列表项
    * @return 无
    * @note:
    */
    void play_playlist_item(QListWidgetItem *item);

    /**
    * @brief 显示视频区域右键菜单
    * @author: tyy
    * @param[in] position 鼠标右键菜单位置
    * @return 无
    * @note: 菜单中包含分屏设置，后续可扩展全屏和宽高比选项
    */
    void show_split_menu(const QPoint &position);

    /**
    * @brief 设置当前分屏数量
    * @author: tyy
    * @param 无
    * @return 无
    * @note: 分屏数量来自右键菜单动作的data数据
    */
    void set_split_screen_count();

    /**
    * @brief Toggle main window full screen state
    * @author: tyy
    * @param none
    * @return none
    * @note:
    */
    void toggle_full_screen();

    /**
    * @brief 显示或隐藏播放列表区域
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void toggle_playlist_panel();

    /**
    * @brief 隐藏视频悬浮全屏按钮
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void hide_video_overlay();

    /**
    * @brief 切换某一路视频独立全屏
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void toggle_video_screen_full_screen();

private:
    /**
    * @brief 初始化主界面控件布局
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void init_ui();

    /**
    * @brief 初始化界面信号槽连接
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void init_connections();

    /**
    * @brief 初始化视频分屏网格
    * @author: tyy
    * @param[in] parent 父控件指针
    * @return 无
    * @note:
    */
    void init_video_grid(QWidget *parent);

    /**
    * @brief 更新播放按钮和定时器状态
    * @author: tyy
    * @param[in] is_playing 是否处于播放状态
    * @return 无
    * @note:
    */
    void update_play_state(bool is_playing);

    /**
    * @brief 更新暂停和恢复按钮状态
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void update_pause_button_state();

    /**
    * @brief 更新当前媒体在视频区域的显示
    * @author: tyy
    * @param[in] current_row 当前播放列表行号
    * @return 无
    * @note:
    */
    void update_current_media(int current_row);

    /**
    * @brief 更新视频区域分屏布局
    * @author: tyy
    * @param[in] split_count 分屏数量
    * @return 无
    * @note: 当前支持1、2、4、9、16、32分屏
    */
    void update_split_screen(int split_count);

    /**
    * @brief 更新指定分屏的选中状态
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 无
    * @note:
    */
    void select_video_screen(int screen_index);

    /**
    * @brief 更新分屏显示控件样式
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 无
    * @note:
    */
    void update_video_label_style(int screen_index);

    /**
    * @brief 显示指定分屏的悬浮全屏按钮
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 无
    * @note:
    */
    void show_video_overlay(int screen_index);

    /**
    * @brief 更新指定分屏悬浮按钮位置
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 无
    * @note:
    */
    void update_video_overlay_geometry(int screen_index);

    /**
    * @brief 进入指定分屏独立全屏
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 无
    * @note:
    */
    void enter_video_screen_full_screen(int screen_index);

    /**
    * @brief 退出分屏独立全屏
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void exit_video_screen_full_screen();

    /**
    * @brief 更新播放列表显示按钮状态
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void update_playlist_toggle_button();

    /**
    * @brief 获取下一次播放使用的分屏索引
    * @author: tyy
    * @param 无
    * @return 分屏索引
    * @note:
    */
    int get_next_play_screen_index();

    /**
    * @brief 加载上次保存的播放列表
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void load_playlist();

    /**
    * @brief 保存当前播放列表
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void save_playlist() const;

    /**
    * @brief 打开并启动当前选中的媒体文件
    * @author: tyy
    * @param 无
    * @return 错误码
    * @note: 当前先使用第一个分屏窗口播放
    */
    int start_current_media();

    /**
    * @brief 在指定分屏打开并启动当前选中的媒体文件
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 错误码
    * @note:
    */
    int start_current_media(int screen_index);

    /**
    * @brief 释放当前播放器对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void release_current_player();

    /**
    * @brief 释放指定分屏的播放器对象
    * @author: tyy
    * @param[in] screen_index 分屏索引
    * @return 无
    * @note:
    */
    void release_screen_player(int screen_index);

    /**
    * @brief 获取指定分屏数量对应的列数
    * @author: tyy
    * @param[in] split_count 分屏数量
    * @return 网格列数
    * @note:
    */
    int get_split_columns(int split_count) const;

    /**
    * @brief 格式化播放时间
    * @author: tyy
    * @param[in] seconds 时间，单位秒
    * @return mm:ss格式字符串
    * @note:
    */
    QString format_time(int seconds) const;

private:
    // Qt Designer生成的主窗口界面对象
    Ui::MainWindow *ui;

    // 视频显示区域的父控件
    QWidget *_video_widget;

    // 播放区域父控件
    QWidget *_play_widget;

    // 播放控制区域控件
    QWidget *_control_widget;

    // 播放列表区域控件
    QWidget *_playlist_panel;

    // 视频分屏网格布局
    QGridLayout *_video_layout;

    // 视频分屏显示控件列表
    QVector<QLabel *> _video_labels;

    // 每个分屏对应的底层播放器句柄
    QVector<TyyPlayerHandle> _player_handles;

    // 每个分屏当前是否处于播放状态
    QVector<bool> _screen_playing;

    // 每个分屏当前是否处于暂停状态
    QVector<bool> _screen_paused;

    // 每个分屏对应的悬浮全屏按钮
    QVector<QPushButton *> _video_full_screen_buttons;

    // 播放时间显示标签
    QLabel *_time_label;

    // 播放列表控件
    QListWidget *_playlist_widget;

    // 播放和暂停按钮
    QPushButton *_play_button;

    // 暂停和恢复按钮
    QPushButton *_pause_button;

    // 停止播放按钮
    QPushButton *_stop_button;

    // 添加媒体文件按钮
    QPushButton *_add_button;

    // 删除播放列表选中项按钮
    QPushButton *_remove_button;

    // 显示或隐藏播放列表按钮
    QPushButton *_playlist_toggle_button;

    // 播放进度条
    QSlider *_progress_slider;

    // 音量调节滑块
    QSlider *_volume_slider;

    // 模拟播放进度的定时器
    QTimer *_play_timer;

    // 视频悬浮按钮自动隐藏定时器
    QTimer *_overlay_hide_timer;

    // 当前模拟媒体总时长，单位秒
    int _duration_seconds;

    // 当前分屏数量
    int _split_screen_count;

    // 当前手动选择的分屏索引
    int _selected_screen_index;

    // 没有手动选择时，下一次默认播放使用的分屏索引
    int _next_default_screen_index;

    // 当前显示悬浮按钮的分屏索引
    int _overlay_screen_index;

    // 当前独立全屏的分屏索引
    int _fullscreen_screen_index;

    // 独立全屏前的分屏数量
    int _fullscreen_saved_split_count;

    // 当前是否处于播放状态
    bool _is_playing;

    // 播放列表当前是否显示
    bool _playlist_visible;

    // 当前播放使用的底层播放器句柄
    TyyPlayerHandle _player_handle;
};

#endif // MAINWINDOW_H
