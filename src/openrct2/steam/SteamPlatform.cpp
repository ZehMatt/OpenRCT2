extern "C" {
#include "../platform/platform.h"
}
#include "../OpenRCT2.h"
#include "../network/network.h"
#include "SteamPlatform.h"
#include "core/steam_gameserver.h"
#include <windows.h>

SteamPlatform _steamPlatform;
SteamPlatform *gSteamPlatform = &_steamPlatform;

void* platform_load_library(const char *lib)
{
#ifdef _WIN32 
    return (void*)LoadLibraryA(lib);
#else 
    // TODO: Implement me.
#endif
}

void* platform_library_getproc(void *mod, const char *proc)
{
#ifdef _WIN32 
    return GetProcAddress((HMODULE)mod, proc);
#else 
    // TODO: Implement me.
#endif
}

SteamPlatform::SteamPlatform()
{
    log_info("%s\n", __FUNCTION__);
}

bool SteamPlatform::Initialize()
{
    log_info("%s\n", __FUNCTION__);

    if (gNetworkStart == NETWORK_MODE_SERVER && gOpenRCT2Headless)
    {
        _gameServer = true;
    }
    else
    {
        if (!SteamAPI_Init())
        {
            log_info("SteamAPI_Init failed\n");
            return false;
        }
        _gameServer = false;
    }

    _steamAvailable = true;
    return true;
}

// We only want this with headless versions or being able to test p2p without the need of two accounts.
bool SteamPlatform::InitializeServer(const char *host, uint16 port)
{
    if (!SteamGameServer_Init(0 /* INADDR_ANY */, 3333, port + 1, port + 2, eServerModeNoAuthentication, OPENRCT2_VERSION))
    {
        log_info("SteamGameServer_Init failed\n");
        return false;
    }

    ISteamGameServer *server = SteamGameServer();

    if (_gameServer == true)
    {

        log_info("Logging on steam anonymous...\n");
        server->LogOnAnonymous();

        uint32 ticksStart = platform_get_ticks();
        while (server->BLoggedOn() == false)
        {
            if (platform_get_ticks() - ticksStart >= 10000)
            {
                log_info("Unable to logon\n");
                return false;
            }
        }

        log_info("Logged on");
    }

    CSteamID steamId = gSteamPlatform->SteamID();
    log_info("P2P address: %llu\n", steamId.ConvertToUint64());

    return true;
}

void SteamPlatform::Shutdown()
{
    //SteamAPI_Shutdown();

    _steamAvailable = false;
}

void SteamPlatform::Update()
{
    if(_gameServer)
        SteamGameServer_RunCallbacks();
    else 
        SteamAPI_RunCallbacks();
}

bool SteamPlatform::IsAvailable() const
{
    return _steamAvailable;
}

ISteamUser* SteamPlatform::User() const
{
    return SteamUser();
}

ISteamNetworking* SteamPlatform::Networking() const
{
    if (_gameServer)
        return SteamGameServerNetworking();
    return SteamNetworking();
}

CSteamID SteamPlatform::SteamID() const
{
    if (_gameServer)
        return SteamGameServer()->GetSteamID();
    return SteamUser()->GetSteamID();
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
