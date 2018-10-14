/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#ifndef DISABLE_NETWORK

#include "NetworkConnection.h"

#include "../core/String.hpp"
#include "../localisation/Localisation.h"
#include "../platform/platform.h"
#include "network.h"
#include "NetworkBase.h"

#    pragma optimize("g", off)

constexpr size_t NETWORK_DISCONNECT_REASON_BUFFER_SIZE = 256;

NetworkConnection::NetworkConnection()
{
    ResetLastPacketTime();

    _buffer.header.length = 0;
    _buffer.header.tick = 0;
    _buffer.bytesRead = 0;
}

NetworkConnection::~NetworkConnection()
{
}

int32_t NetworkConnection::ReadPacket(NetworkPacketDispatcher& dispatcher)
{
    size_t bytesRead = 0;

    // Read header first.
    if (_buffer.bytesRead < sizeof(_buffer.header))
    {
        size_t missingBytes = sizeof(_buffer.header) - _buffer.bytesRead;

        uint8_t* buf = (uint8_t*)(&_buffer.header) + _buffer.bytesRead;

        NETWORK_READPACKET status = Sock->ReceiveData(buf, missingBytes, &bytesRead);
        if (status != NETWORK_READPACKET_SUCCESS)
        {
            return status;
        }

        _buffer.bytesRead += bytesRead;

        if (_buffer.bytesRead < sizeof(_buffer.header))
        {
            return NETWORK_READPACKET_MORE_DATA;
        }

        // Decode header.
        _buffer.header.length = Convert::NetworkToHost(_buffer.header.length);
        _buffer.header.tick = Convert::NetworkToHost(_buffer.header.tick);

        _buffer.data.resize(_buffer.header.length - sizeof(_buffer.header));
    }

    size_t missingBytes = _buffer.header.length - _buffer.bytesRead;

    uint8_t* buf = (uint8_t*)_buffer.data.data() + _buffer.bytesRead - sizeof(NetworkPacketHeader);

    NETWORK_READPACKET status = Sock->ReceiveData(buf, missingBytes, &bytesRead);
    if (status != NETWORK_READPACKET_SUCCESS)
    {
        return status;
    }

    _buffer.bytesRead += bytesRead;

    if (_buffer.bytesRead == _buffer.header.length)
    {
        // Full packet received.
        MemoryStream stream(_buffer.data.data(), _buffer.data.size());

        dispatcher.Dispatch(*this, stream);

        _buffer.bytesRead = 0;

        return NETWORK_READPACKET_SUCCESS;
    }

    return NETWORK_READPACKET_MORE_DATA;
}

bool NetworkConnection::SendPacket(NetworkPacket& packet)
{
    uint16_t sizen = Convert::HostToNetwork(packet.Size);

    std::vector<uint8_t> tosend;
    tosend.reserve(sizeof(sizen) + packet.Size);
    tosend.insert(tosend.end(), (uint8_t*)&sizen, (uint8_t*)&sizen + sizeof(sizen));
    tosend.insert(tosend.end(), packet.Data->begin(), packet.Data->end());

    const void* buffer = &tosend[packet.BytesTransferred];
    size_t bufferSize = tosend.size() - packet.BytesTransferred;
    size_t sent = Sock->SendData(buffer, bufferSize);
    if (sent > 0)
    {
        packet.BytesTransferred += sent;
    }
    return packet.BytesTransferred == tosend.size();
}

bool NetworkConnection::SendPacket2(MemoryStream& data)
{
    MemoryStream packet;

    NetworkPacketHeader header;
    header.length = Convert::HostToNetwork(data.GetLength() + sizeof(NetworkPacketHeader));
    header.tick = Convert::HostToNetwork(gCurrentTicks);

    packet.Write(header);

    // Data.
    packet.Write(data.GetData(), data.GetLength());

    // Write data.
    if (Sock->SendData(packet.GetData(), packet.GetLength()) != packet.GetLength())
    {
        return false;
    }

    return true;
}

void NetworkConnection::QueuePacket(std::unique_ptr<NetworkPacket> packet, bool front)
{
}

void NetworkConnection::SendQueuedPackets()
{
    for (auto& buf : _packetsOutQueue)
    {
        SendPacket2(buf);
    }
    _packetsOutQueue.clear();
}

void NetworkConnection::ResetLastPacketTime()
{
    _lastPacketTime = platform_get_ticks();
}

bool NetworkConnection::ReceivedPacketRecently()
{
#ifndef DEBUG
    if (platform_get_ticks() > _lastPacketTime + 7000)
    {
        return false;
    }
#endif
    return true;
}

const utf8* NetworkConnection::GetLastDisconnectReason() const
{
    return this->_lastDisconnectReason;
}

void NetworkConnection::SetLastDisconnectReason(const utf8* src)
{
    if (src == nullptr)
    {
        delete[] _lastDisconnectReason;
        _lastDisconnectReason = nullptr;
        return;
    }

    if (_lastDisconnectReason == nullptr)
    {
        _lastDisconnectReason = new utf8[NETWORK_DISCONNECT_REASON_BUFFER_SIZE];
    }
    String::Set(_lastDisconnectReason, NETWORK_DISCONNECT_REASON_BUFFER_SIZE, src);
}

void NetworkConnection::SetLastDisconnectReason(const rct_string_id string_id, void* args)
{
    char buffer[NETWORK_DISCONNECT_REASON_BUFFER_SIZE];
    format_string(buffer, NETWORK_DISCONNECT_REASON_BUFFER_SIZE, string_id, args);
    SetLastDisconnectReason(buffer);
}

#endif

#pragma optimize("g", on)
