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
    * @brief 释放当前播放器对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    void release_current_player();

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

    // 播放时间显示标签
    QLabel *_time_label;

    // 播放列表控件
    QListWidget *_playlist_widget;

    // 播放和暂停按钮
    QPushButton *_play_button;

    // 停止播放按钮
    QPushButton *_stop_button;

    // 添加媒体文件按钮
    QPushButton *_add_button;

    // 删除播放列表选中项按钮
    QPushButton *_remove_button;

    // 播放进度条
    QSlider *_progress_slider;

    // 音量调节滑块
    QSlider *_volume_slider;

    // 模拟播放进度的定时器
    QTimer *_play_timer;

    // 当前模拟媒体总时长，单位秒
    int _duration_seconds;

    // 当前分屏数量
    int _split_screen_count;

    // 当前是否处于播放状态
    bool _is_playing;

    // 当前播放使用的底层播放器句柄
    TyyPlayerHandle _player_handle;
};

#endif // MAINWINDOW_H
