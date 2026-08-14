#include "tyy_player.h"

#include "tyy_video_state.h"

#include <stdint.h>

static const char *TYY_PLAYER_PROPERTY_URL = "url";
static const char *TYY_PLAYER_PROPERTY_WIN_ID = "win_id";
static const char *TYY_PLAYER_PROPERTY_HWACCEL_TYPE = "hwaccel_type";

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

int TyyPlayerCore::set_decoder_type(int decoder_type)
{
    if (decoder_type != TYY_PLAYER_DECODER_TYPE_SOFTWARE &&
        decoder_type != TYY_PLAYER_DECODER_TYPE_AUTO &&
        decoder_type != TYY_PLAYER_DECODER_TYPE_D3D11VA)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }

    _properties.set_property(TYY_PLAYER_PROPERTY_HWACCEL_TYPE, decoder_type);
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::get_statistics(TyyPlayerStatistics *statistics)
{
    if (statistics == nullptr || _video_state == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    return _video_state->get_statistics(statistics) ? TYY_PLAYER_ERROR_OK : TYY_PLAYER_ERROR_STATE_FAILED;
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

int TyyPlayerCore::seek(int forward, int seek_interval)
{
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    _video_state->on_user_fb_seek(forward ? 2 : 1, seek_interval);
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::step_to_next_frame()
{
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    _video_state->step_next_frame();
    _is_paused = true;
    return TYY_PLAYER_ERROR_OK;
}

int TyyPlayerCore::set_volume(int volume)
{
    if (volume < 0 || volume > 100)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    return _video_state->set_audio_volume(volume) ? TYY_PLAYER_ERROR_OK : TYY_PLAYER_ERROR_INVALID_PARAM;
}

int TyyPlayerCore::set_speed(float speed)
{
    bool is_supported_speed =
        speed == 0.5f ||
        speed == 0.75f ||
        speed == 1.0f ||
        speed == 1.5f ||
        speed == 1.75f ||
        speed == 2.0f;
    if (!is_supported_speed)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    if (!_is_started)
    {
        return TYY_PLAYER_ERROR_STATE_FAILED;
    }

    _video_state->change_speed(speed);
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
