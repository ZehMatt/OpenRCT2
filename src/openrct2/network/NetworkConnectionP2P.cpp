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

#ifndef DISABLE_NETWORK

#include "network.h"
#include "NetworkConnectionP2P.h"
#include "../core/String.hpp"
#include "../steam/SteamPlatform.h"

extern "C"
{
#include "../localisation/localisation.h"
#include "../platform/platform.h"
}

NetworkConnectionP2P::NetworkConnectionP2P()
{
    ResetLastPacketTime();
}

NetworkConnectionP2P::~NetworkConnectionP2P()
{
    delete Socket;
}

void NetworkConnectionP2P::AppendData(const void *data, size_t len)
{
    size_t curSize = _dataBuffer.size();
    _dataBuffer.resize(curSize + len);
    memcpy(_dataBuffer.data() + curSize, data, len);
}

NETWORK_READPACKET NetworkConnectionP2P::ReadData(void *buf, size_t maxBuf, size_t& bytesRead)
{
    if (_dataBuffer.empty())
        return NETWORK_READPACKET_NO_DATA;

    if (maxBuf > _dataBuffer.size())
        maxBuf = _dataBuffer.size();

    memcpy(buf, _dataBuffer.data(), maxBuf);
    _dataBuffer.erase(_dataBuffer.begin(), _dataBuffer.begin() + maxBuf);
    bytesRead = maxBuf;

    return NETWORK_READPACKET_SUCCESS;
}

sint32 NetworkConnectionP2P::ReadPacket()
{
//     P2PSessionState_t state = { 0 };
//     if (SteamNetworking()->GetP2PSessionState(_remoteId, &state))
//     {
//         if (!state.m_bConnectionActive)
//             return NETWORK_READPACKET_DISCONNECTED;
//     }

    if (InboundPacket.BytesTransferred < sizeof(InboundPacket.Size))
    {
        // read packet size
        void * buffer = &((char*)&InboundPacket.Size)[InboundPacket.BytesTransferred];
        size_t bufferLength = sizeof(InboundPacket.Size) - InboundPacket.BytesTransferred;
        size_t readBytes;

        NETWORK_READPACKET status = ReadData(buffer, bufferLength, readBytes);
        if (status != NETWORK_READPACKET_SUCCESS)
        {
            return status;
        }

        InboundPacket.BytesTransferred += readBytes;
        if (InboundPacket.BytesTransferred == sizeof(InboundPacket.Size))
        {
            InboundPacket.Size = Convert::NetworkToHost(InboundPacket.Size);
            if (InboundPacket.Size == 0) // Can't have a size 0 packet
            {
                return NETWORK_READPACKET_DISCONNECTED;
            }
            InboundPacket.Data->resize(InboundPacket.Size);
        }
    }
    else
    {
        // read packet data
        if (InboundPacket.Data->capacity() > 0)
        {
            void * buffer = &InboundPacket.GetData()[InboundPacket.BytesTransferred - sizeof(InboundPacket.Size)];
            size_t bufferLength = sizeof(InboundPacket.Size) + InboundPacket.Size - InboundPacket.BytesTransferred;
            size_t readBytes;
            NETWORK_READPACKET status = ReadData(buffer, bufferLength, readBytes);
            if (status != NETWORK_READPACKET_SUCCESS)
            {
                return status;
            }

            InboundPacket.BytesTransferred += readBytes;
        }
        if (InboundPacket.BytesTransferred == sizeof(InboundPacket.Size) + InboundPacket.Size)
        {
            _lastPacketTime = platform_get_ticks();
            return NETWORK_READPACKET_SUCCESS;
        }
    }
    return NETWORK_READPACKET_MORE_DATA;
}

bool NetworkConnectionP2P::SendPacket(NetworkPacket& packet)
{
    uint16 sizen = Convert::HostToNetwork(packet.Size);
    std::vector<uint8> tosend;
    tosend.reserve(sizeof(sizen) + packet.Size);
    tosend.insert(tosend.end(), (uint8*)&sizen, (uint8*)&sizen + sizeof(sizen));
    tosend.insert(tosend.end(), packet.Data->begin(), packet.Data->end());

    const void * buffer = &tosend[packet.BytesTransferred];
    size_t bufferSize = tosend.size() - packet.BytesTransferred;

    bool sent = SteamNetworking()->SendP2PPacket(_remoteId, buffer, (uint32)bufferSize, k_EP2PSendReliable);

    if (sent)
    {
        packet.BytesTransferred += bufferSize;
    }
    if (packet.BytesTransferred == tosend.size())
    {
        return true;
    }

    return false;
}

#endif
