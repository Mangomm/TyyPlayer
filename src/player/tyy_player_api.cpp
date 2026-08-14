#include "tyy_player_api.h"

#include "tyy_player.h"

TyyPlayerHandle tyy_player_create()
{
    return new TyyPlayerCore();
}

void tyy_player_destroy(TyyPlayerHandle handle)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    delete player;
}

int tyy_player_set_window(TyyPlayerHandle handle, unsigned long long win_id)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->set_window(win_id);
}

int tyy_player_set_event_callback(TyyPlayerHandle handle, TyyPlayerEventCallback callback, void *user_data)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->set_event_callback(callback, user_data);
}

int tyy_player_set_decoder_type(TyyPlayerHandle handle, int decoder_type)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->set_decoder_type(decoder_type);
}

int tyy_player_get_statistics(TyyPlayerHandle handle, TyyPlayerStatistics *statistics)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->get_statistics(statistics);
}

int tyy_player_open(TyyPlayerHandle handle, const char *url)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->open(url);
}

int tyy_player_start(TyyPlayerHandle handle)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->start();
}

int tyy_player_pause(TyyPlayerHandle handle, int pause)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->pause(pause);
}

int tyy_player_seek(TyyPlayerHandle handle, int forward, int seek_interval)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->seek(forward, seek_interval);
}

int tyy_player_seek_to(TyyPlayerHandle handle, int position_seconds, int duration_seconds)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->seek_to(position_seconds, duration_seconds);
}

int tyy_player_step_to_next_frame(TyyPlayerHandle handle)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->step_to_next_frame();
}

int tyy_player_set_volume(TyyPlayerHandle handle, int volume)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->set_volume(volume);
}

int tyy_player_set_speed(TyyPlayerHandle handle, float speed)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->set_speed(speed);
}

int tyy_player_stop(TyyPlayerHandle handle)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->stop();
}

int tyy_player_close(TyyPlayerHandle handle)
{
    TyyPlayerCore *player = static_cast<TyyPlayerCore *>(handle);
    if (player == nullptr)
    {
        return TYY_PLAYER_ERROR_INVALID_PARAM;
    }
    return player->close();
}
