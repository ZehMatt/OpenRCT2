/*****************************************************************************
 * Copyright (c) 2014-2018 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Fuzzing.hpp"
#include "TestData.h"

#include <gtest/gtest.h>
#include <openrct2/Context.h>
#include <openrct2/Game.h>
#include <openrct2/GameState.h>
#include <openrct2/OpenRCT2.h>
#include <openrct2/ParkImporter.h>
#include <openrct2/actions/GameAction.h>
#include <openrct2/audio/AudioContext.h>
#include <openrct2/core/File.h>
#include <openrct2/core/Path.hpp>
#include <openrct2/core/String.hpp>
#include <openrct2/network/NetworkBase.h>
#include <openrct2/network/NetworkPacket.h>
#include <openrct2/network/network.h>
#include <openrct2/platform/platform.h>
#include <openrct2/ride/Ride.h>
#include <random>
#include <string>
#include <windows.h>

using namespace OpenRCT2;

// If this is defined it wont spawn a local server on its own.
//#define NO_CHILD

enum class StateType
{
    Init = 0,
    Connect,
    AwaitConnection,
    FuzzerLoop,
    Cleanup,
};

struct ClientState
{
    std::mt19937_64 prng;
    StateType state{};
    Fuzzing::ChildProcess* child;
    Fuzzing::InputState input;
    std::unique_ptr<IContext> context{};
    uint64_t nextRetry{};
    uint64_t nextInput{};
    int retryCount{};
    uint64_t nextReport{};
    size_t totalPackets{};
};

static void HandleInit(ClientState& state)
{
    state.prng.seed(1);

#ifndef NO_CHILD
    state.child = Fuzzing::SpawnDebugChild(
        "host testdata/parks/bpb.sv6 --password=fuzz --port=11753 --user-data-path=.\\fuzzer_server");
    ASSERT_NE(state.child, nullptr);
#endif

    gOpenRCT2Headless = true;
    gOpenRCT2NoGraphics = true;

    utf8 absolutePath[MAX_PATH]{};
    Path::GetAbsolute(absolutePath, std::size(absolutePath), ".\\fuzzer_client");
    String::Set(gCustomUserDataPath, std::size(gCustomUserDataPath), absolutePath);

    String::Set(gCustomPassword, std::size(gCustomPassword), "fuzz");

    state.context = CreateContext();
    bool initialised = state.context->Initialise();
    ASSERT_TRUE(initialised);

    state.state = StateType::Connect;
}

static void HandleConnect(ClientState& state)
{
    if (state.nextRetry != 0 && state.nextRetry < GetTickCount64())
        return;

    network_set_password("fuzz");
    ASSERT_TRUE(network_begin_client("127.0.0.1", 11753));
    ASSERT_EQ(network_get_mode(), NETWORK_MODE_CLIENT);
    ASSERT_EQ(network_get_status(), NETWORK_STATUS_CONNECTING);

    state.state = StateType::AwaitConnection;
}

static void HandleAwaitConnection(ClientState& state)
{
    auto status = network_get_status();
    if (status == NETWORK_STATUS_CONNECTING)
    {
        return;
    }
    else if (status == NETWORK_STATUS_CONNECTED)
    {
        state.state = StateType::FuzzerLoop;
        printf("Connected to child\n");
    }
    else
    {
        printf("Failed to connect\n");
        if (state.retryCount < 10)
        {
            printf("Retrying in 1 second...\n");
            state.retryCount++;
            state.nextRetry = GetTickCount64() + 1000;
            state.state = StateType::Connect;
        }
        else
        {
            ASSERT_TRUE(state.retryCount < 10);
            state.state = StateType::Cleanup;
        }
    }
}

static void HandleFuzzerLoop(ClientState& state)
{
    auto* connection = gNetwork.GetServerConnection();
    if (connection == nullptr)
    {
        printf("Lost connection.\n");
        state.state = StateType::Cleanup;
        return;
    }

    // if (connection->PendingPacketCount() > 100)
    // return;

    GameCommand cmdType;

    for (;;)
    {
        Fuzzing::MutateInputRandom(state.input, state.prng);

        cmdType = static_cast<GameCommand>(state.input.raw[0] % static_cast<size_t>(GameCommand::Count));
        if (cmdType == GameCommand::LoadOrQuit)
            continue;

        break;
    }

    NetworkPacket packet;
    packet.Write(state.input.raw.data(), state.input.raw.size());

    connection->QueuePacket(packet);
    state.totalPackets++;

    if (GetTickCount64() >= state.nextReport)
    {
        printf(
            "Input size: %zu bits (%zu bytes), Total Packets: %zu\n", state.input.raw.size() * 8, state.input.raw.size(),
            state.totalPackets);

        state.nextReport = GetTickCount64() + 10000;
    }
}

TEST(NetworkFuzzer, all)
{
    std::string path = TestData::GetParkPath("bpb.sv6");

    core_init();

    ClientState client;

    bool exitLoop = false;
    while (!exitLoop)
    {
        network_update();

        switch (client.state)
        {
            case StateType::Init:
                HandleInit(client);
                break;
            case StateType::Connect:
                HandleConnect(client);
                break;
            case StateType::AwaitConnection:
                HandleAwaitConnection(client);
                break;
            case StateType::FuzzerLoop:
                HandleFuzzerLoop(client);
                break;
            case StateType::Cleanup:
                exitLoop = true;
                continue;
        }

        if (client.context)
        {
            auto* gs = client.context->GetGameState();
            if (gs)
                gs->UpdateLogic();
        }

        network_flush();

        if (client.child != nullptr)
        {
            bool res = Fuzzing::UpdateChild(
                client.child, [](uintptr_t addresss, Fuzzing::ExceptionType exType, uintptr_t memAddress) -> void {
                    printf("Child crashed at %p\n", (void*)addresss);
                });

            if (!res)
                break;
        }
    }

    SUCCEED();
}
