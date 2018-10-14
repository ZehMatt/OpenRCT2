/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#ifndef DISABLE_NETWORK
#    include "../common.h"
#    include "NetworkKey.h"
#    include "NetworkPacket.h"
#    include "NetworkTypes.h"

#    include <list>
#    include <memory>
#    include <vector>
#include <functional>

interface ITcpSocket;
class NetworkPlayer;
class NetworkPacketDispatcher;
struct ObjectRepositoryItem;

class NetworkConnection final
{
public:
    ITcpSocket *Socket;
    int AuthStatus;
    NetworkPlayer* Player;

    std::unique_ptr<ITcpSocket> Sock;

    NetworkConnection();
    ~NetworkConnection();

    int32_t ReadPacket(NetworkPacketDispatcher& dispatcher);

    void QueuePacket(std::unique_ptr<NetworkPacket> packet, bool front = false);
    void SendQueuedPackets();
    void ResetLastPacketTime();
    bool ReceivedPacketRecently();

    const utf8* GetLastDisconnectReason() const;
    void SetLastDisconnectReason(const utf8* src);
    void SetLastDisconnectReason(const rct_string_id string_id, void* args = nullptr);

    template<typename TPacket> void EnqueuePacket(const TPacket& packet)
    {
        MemoryStream data;

        TPacket& pck = const_cast<TPacket&>(packet);

        DataSerialiser serialiser(true, data);
        pck.SerialiseHead(serialiser);
        pck.Serialise(serialiser);

        _packetsOutQueue.emplace_back(data);
    }

private:
    bool SendPacket(NetworkPacket& packet);
    bool SendPacket2(MemoryStream& data);

private:
    // std::list<std::unique_ptr<NetworkPacket>> _outboundPackets;
#    pragma pack(push, 1)
    struct NetworkPacketHeader
    {
        uint16_t length;
        uint32_t tick;
    };
#    pragma pack(pop)

    struct NetworkPacketBuffer
    {
        NetworkPacketHeader header;
        std::vector<uint8_t> data;
        size_t bytesRead;
    };

    struct NetworkPacketHandler
    {
        int32_t type;
        std::function<void(NetworkConnection&, MemoryStream&)> fn;
    };

    std::vector<MemoryStream> _packetsOutQueue;
    std::vector<NetworkPacketHandler> _handlers;
    NetworkPacketBuffer _buffer;

    uint32_t _lastPacketTime = 0;
    utf8* _lastDisconnectReason = nullptr;
};

#endif // DISABLE_NETWORK
