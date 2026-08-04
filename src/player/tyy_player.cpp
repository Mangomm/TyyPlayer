#include "tyy_player.h"

#include "tyy_video_state.h"

#include <stdint.h>

static const char *TYY_PLAYER_PROPERTY_URL = "url";
static const char *TYY_PLAYER_PROPERTY_WIN_ID = "win_id";

TyyPlayerCore::TyyPlayerCore()
    : _video_state(new TyyPlayer::TyyVideoState()),
      _event_callback(nullptr),
      _event_user_data(nullptr),
      _is_opened(false),
      _is_started(false),
      _is_paused(false)
{
}

TyyPlayerCore::~TyyPlayerCore()
{
    close();
    delete _video_state;
    _video_state = nullptr;
}

int TyyPlayerCore::set_window(unsigned long long win_id)
{
    if (win_id == 0)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }

    _properties.set_property(TYY_PLAYER_PROPERTY_WIN_ID, static_cast<uint64_t>(win_id));
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::set_event_callback(TyyPlayerEventCallback callback, void *user_data)
{
    _event_callback = callback;
    _event_user_data = user_data;
    _video_state->set_event_callback(callback, user_data);
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::open(const char *url)
{
    if (url == nullptr || url[0] == '\0')
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }

    _properties.set_property(TYY_PLAYER_PROPERTY_URL, url);
    _is_opened = true;
    _is_paused = false;
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::start()
{
    if (_is_started)
    {
        return TYY_PLAYER_ERROR_OK;
    }

    std::string url = _properties.get_property(TYY_PLAYER_PROPERTY_URL, std::string());
    uint64_t win_id = _properties.get_property(TYY_PLAYER_PROPERTY_WIN_ID, static_cast<uint64_t>(0));
    if (!_is_opened || url.empty() || win_id == 0)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    int return_value = 0;
    _video_state->set_event_callback(_event_callback, _event_user_data);
    if (!_video_state->play(_properties, return_value))
    {
        _is_started = false;
        _is_paused = false;
        return return_value;
    }

    _is_started = true;
    _is_paused = false;
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::pause(int pause)
{
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    bool want_pause = pause != 0;
    if (_is_paused != want_pause)
    {
        _video_state->toggle_pause();
        _is_paused = want_pause;
    }

    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::stop()
{
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_OK;
    }

    bool ret = _video_state->close();
    _is_started = false;
    _is_paused = false;
    return ret ? TYY_PLAYER_ERROR_OK : TYY_PLAYER_ERROR_STATE_FAILED;
}

int TyyPlayerCore::close()
{
    if (_video_state == nullptr)
    {
        return TYY_PLAYER_ERROR_OK;
    }
    stop();
    _properties.set_property(TYY_PLAYER_PROPERTY_URL, "");
    _is_opened = false;
    _is_paused = false;
    return TYY_PLAYER_ERROR_OK;
}
