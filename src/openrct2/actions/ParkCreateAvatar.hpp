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

#include "../core/MemoryStream.h"
#include "GameAction.h"

#include "../Context.h"
#include "../windows/Intent.h"
#include "../interface/Window.h"
#include "../world/Park.h"
#include "../network/network.h"

class ParkCreateAvatarGameActionResult final : public GameActionResult
{
public:
    ParkCreateAvatarGameActionResult() : GameActionResult(GA_ERROR::OK, 0) {}
    ParkCreateAvatarGameActionResult(GA_ERROR error, rct_string_id message) : GameActionResult(error, message) {}

    sint32 spriteIndex = -1;
};

struct ParkCreateAvatar : public GameActionBase<GAME_COMMAND_CREATE_GUEST, GameActionResult>
{
private:
    std::string _name;
    sint32 _playerId;

public:
    ParkCreateAvatar() {}
    ParkCreateAvatar(const std::string& name, sint32 playerId)
        : _name(name),
        _playerId(playerId)
    {
    }

    uint16 GetActionFlags() const override
    {
        return GameAction::GetActionFlags() | GA_FLAGS::ALLOW_WHILE_PAUSED;
    }

    void Serialise(DataSerialiser& stream) override
    {
        GameAction::Serialise(stream);

        stream << _name << _playerId;
    }

    GameActionResult::Ptr Query() const override
    {
        return std::make_unique<GameActionResult>();
    }

    GameActionResult::Ptr Execute() const override
    {
        auto res = std::make_unique<ParkCreateAvatarGameActionResult>();

        rct_peep *peep = park_generate_new_guest();
        peep->player_controlled = 1;
        peep->player_id = _playerId;
        if(_name.empty())
            guest_set_name(peep->sprite_index, "Host");
        else
            guest_set_name(peep->sprite_index, _name.c_str());

        res->spriteIndex = peep->sprite_index;
        res->Position.x = peep->x;
        res->Position.y = peep->y;
        res->Position.z = peep->z;

        if (_playerId == network_get_current_player_id())
        {
            auto intent = Intent(WC_PEEP);
            intent.putExtra(INTENT_EXTRA_PEEP, peep);
            context_open_intent(&intent);
        }

        return res;
    }
};
