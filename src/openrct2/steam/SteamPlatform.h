#pragma once 

#include "core/steam_api.h"

#ifdef __cplusplus 

class SteamPlatform
{
private:
    bool _steamAvailable = false;

public:
    SteamPlatform();

    bool Initialize();
    void Shutdown();
    void Update();
    bool IsAvailable() const;
};

extern SteamPlatform *gSteamPlatform;

#endif 

extern "C" 
{
    bool steamplatform_init();
    void steamplatform_update();
    void steamplatform_shutdown();
    bool steamplatform_available();
}