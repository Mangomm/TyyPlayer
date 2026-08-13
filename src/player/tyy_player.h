#ifndef TYY_PLAYER_H
#define TYY_PLAYER_H

#include "tyy_player_api.h"
#include "tyy_properties.h"

namespace TyyPlayer {
class TyyVideoState;
}

class TyyPlayerCore
{
public:
    /**
    * @brief 创建播放器对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    TyyPlayerCore();

    /**
    * @brief 销毁播放器对象
    * @author: tyy
    * @param 无
    * @return 无
    * @note:
    */
    ~TyyPlayerCore();

    /**
    * @brief 设置播放器渲染窗口
    * @author: tyy
    * @param[in] win_id Qt控件窗口句柄
    * @return 错误码
    * @note:
    */
    int set_window(unsigned long long win_id);

    /**
    * @brief 设置播放器事件回调
    * @author: tyy
    * @param[in] callback 事件回调函数
    * @param[in] user_data 用户自定义数据
    * @return 错误码
    * @note:
    */
    int set_event_callback(TyyPlayerEventCallback callback, void *user_data);

    /**
    * @brief 打开媒体资源
    * @author: tyy
    * @param[in] url 媒体路径或URL
    * @return 错误码
    * @note:
    */
    int open(const char *url);

    /**
    * @brief 启动播放
    * @author: tyy
    * @param 无
    * @return 错误码
    * @note:
    */
    int start();

    /**
    * @brief 暂停或恢复
    * @author: tyy
    * @param[in] pause 是否暂停
    * @return 错误码
    * @note:
    */
    int pause(int pause);

    /**
    * @brief 快进或快退
    * @author: tyy
    * @param[in] forward 是否前进
    * @param[in] seek_interval 跳转间隔，单位秒
    * @return 错误码
    * @note:
    */
    int seek(int forward, int seek_interval);

    /**
    * @brief 单步播放下一帧
    * @author: tyy
    * @param 无
    * @return 错误码
    * @note:
    */
    int step_to_next_frame();

    /**
    * @brief 停止播放
    * @author: tyy
    * @param 无
    * @return 错误码
    * @note:
    */
    int stop();

    /**
    * @brief 关闭媒体资源
    * @author: tyy
    * @param 无
    * @return 错误码
    * @note:
    */
    int close();

private:
    // ffplay风格播放器对象
    TyyPlayer::TyyVideoState *_video_state;

    // 播放器配置参数
    TyyPlayer::Properties _properties;

    // 播放器事件回调函数
    TyyPlayerEventCallback _event_callback;

    // 播放器事件回调用户数据
    void *_event_user_data;

    // 是否已经打开媒体资源
    bool _is_opened;

    // 是否已经启动播放
    bool _is_started;

    // 是否处于暂停状态
    bool _is_paused;
};

#endif
