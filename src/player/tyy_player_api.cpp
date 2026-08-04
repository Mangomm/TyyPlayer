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
