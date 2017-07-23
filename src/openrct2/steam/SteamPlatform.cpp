#include "../platform/platform.h"
#include "SteamPlatform.h"
#include <windows.h>

SteamPlatform _steamPlatform;
SteamPlatform *gSteamPlatform = &_steamPlatform;

SteamPlatform::SteamPlatform()
{
    log_info("%s\n", __FUNCTION__);
}

bool SteamPlatform::Initialize()
{
    log_info("%s\n", __FUNCTION__);

    if (!SteamAPI_Init())
    {
        log_info("SteamAPI_Init failed\n");
        return false;
    }
   
    _steamAvailable = true;
    return true;
}

void SteamPlatform::Shutdown()
{
    //SteamAPI_Shutdown();

    _steamAvailable = false;
}

void SteamPlatform::Update()
{
    SteamAPI_RunCallbacks();
}

bool SteamPlatform::IsAvailable() const
{
    return _steamAvailable;
}

extern "C" 
{
    bool steamplatform_init()
    {
        return _steamPlatform.Initialize();
    }

    void steamplatform_update()
    {
        _steamPlatform.Update();
    }

    void steamplatform_shutdown()
    {
        _steamPlatform.Shutdown();
    }

    bool steamplatform_available()
    {
        return _steamPlatform.IsAvailable();
    }
}
