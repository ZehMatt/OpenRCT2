#include "GameActionsFuzzer.h"

#include "../actions/GameAction.h"
#include "../core/DataSerialiser.h"
#include "../core/MemoryStream.h"

#include <random>

static bool _fuzzerEnabled = false;
static uint32_t _nextActionId = 0;
static MemoryStream _inputStream;
static std::mt19937_64 _prng;

typedef void (*fnSerializeRandomData)(DataSerialiser&, std::mt19937_64&);

static void SerialiseRandomInteger8(DataSerialiser& ds, std::mt19937_64& prng)
{
    std::uniform_int_distribution<uint16_t> dist(0, std::numeric_limits<uint8_t>::max());
    ds << static_cast<uint8_t>(dist(prng));
}

static void SerialiseRandomInteger16(DataSerialiser& ds, std::mt19937_64& prng)
{
    std::uniform_int_distribution<uint16_t> dist(0, std::numeric_limits<uint16_t>::max());
    ds << dist(prng);
}

static void SerialiseRandomInteger32(DataSerialiser& ds, std::mt19937_64& prng)
{
    std::uniform_int_distribution<uint32_t> dist(0, std::numeric_limits<uint32_t>::max());
    ds << dist(prng);
}

static void SerialiseRandomString(DataSerialiser& ds, std::mt19937_64& prng)
{
    std::uniform_int_distribution<uint16_t> dist(0, std::numeric_limits<uint8_t>::max());
    std::string str;
    for (size_t i = 0; i < 1 + (prng() % 16); i++)
    {
        str += static_cast<char>(dist(prng));
    }
    ds << str;
}

static fnSerializeRandomData _RandomSerialisers[] = {
    SerialiseRandomInteger8,
    SerialiseRandomInteger16,
    SerialiseRandomInteger32,
    SerialiseRandomString,
};

static void UpdateRandomInput()
{
    _inputStream.SetPosition(0);
    DataSerialiser ds(true, _inputStream);

    for (size_t i = 0; i < 1 + (rand() % 8); i++)
    {
        size_t idx = rand() % std::size(_RandomSerialisers);
        fnSerializeRandomData fn = _RandomSerialisers[idx];
        fn(ds, _prng);
    }
}

void GameActionsFuzzer::Initialize()
{
    _fuzzerEnabled = true;
    _nextActionId = 0;
    _prng.seed(time(nullptr));
    UpdateRandomInput();

    GameActions::Initialize();
}

void GameActionsFuzzer::Shutdown()
{
    _fuzzerEnabled = false;
}

static void ExecuteRandomAction(uint32_t actionId)
{
    if (GameActions::IsValidId(actionId) == false)
        return;

    // Ignore certain actions.
    if (actionId == GAME_COMMAND_TOGGLE_PAUSE)
        return;

    _inputStream.SetPosition(0);
    DataSerialiser ds(false, _inputStream);

    auto action = GameActions::Create(actionId);
    action->Serialise(ds);

    auto res = GameActions::Execute(action.get());
    if (res->Error == GA_ERROR::OK)
    {
        log_info("Game Action '%s' Passed.", action->GetName());
    }
    else
    {
        log_info("Game Action '%s' Failed.", action->GetName());
    }
}

void GameActionsFuzzer::Update()
{
    if (_fuzzerEnabled == false)
        return;

    uint32_t curId = _nextActionId;

    ExecuteRandomAction(curId);

    _nextActionId++;
    if (_nextActionId == GAME_COMMAND_COUNT)
    {
        // Update input each cycle.
        UpdateRandomInput();

        // Start over with new inputs.
        _nextActionId = 0;
    }
}
