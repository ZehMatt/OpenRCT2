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

class NetworkPlayer;
struct ObjectRepositoryItem;

class NetworkConnection
{
public:
    ITcpSocket *                                Socket          = nullptr;
    NetworkPacket                               InboundPacket;
    NETWORK_AUTH                                AuthStatus      = NETWORK_AUTH_NONE;
    NetworkPlayer *                             Player          = nullptr;
    uint32                                      PingTime        = 0;
    NetworkKey                                  Key;
    std::vector<uint8>                          Challenge;
    std::vector<const ObjectRepositoryItem *>   RequestedObjects;

    NetworkConnection();
    virtual ~NetworkConnection();

    virtual sint32 ReadPacket();
    virtual void QueuePacket(std::unique_ptr<NetworkPacket> packet, bool front = false);
    virtual void SendQueuedPackets();
    virtual void ResetLastPacketTime();
    virtual bool ReceivedPacketRecently();

    virtual const utf8 * GetLastDisconnectReason() const;
    virtual void SetLastDisconnectReason(const utf8 * src);
    virtual void SetLastDisconnectReason(const rct_string_id string_id, void * args = nullptr);

    virtual bool IsP2P() const { return false; }

protected:
    uint32                                      _lastPacketTime;

private:
    utf8 *                                      _lastDisconnectReason = nullptr;
    std::list<std::unique_ptr<NetworkPacket>>   _outboundPackets;

protected:
    virtual bool SendPacket(NetworkPacket &packet);
};
