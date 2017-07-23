#pragma region Copyright (c) 2014-2017 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#pragma once

#include <list>
#include <memory>
#include <vector>

#include "../common.h"

#include "NetworkTypes.h"
#include "NetworkKey.h"
#include "NetworkPacket.h"
#include "TcpSocket.h"
#include "NetworkConnection.h"
#include "../steam/core/steamclientpublic.h"

class NetworkPlayer;
struct ObjectRepositoryItem;

class NetworkConnectionP2P : public NetworkConnection
{
public:
    CSteamID _remoteId;
    std::vector<uint8_t> _dataBuffer;
    bool _fullyConnected;

public:
    NetworkConnectionP2P();
    virtual ~NetworkConnectionP2P();

    void AppendData(const void *data, size_t len);
 
    virtual sint32 ReadPacket() override;

    virtual bool IsP2P() const override { return true; }

private:
    NETWORK_READPACKET ReadData(void *buf, size_t maxBuf, size_t& res);

protected:
    virtual bool SendPacket(NetworkPacket &packet) override;
};
