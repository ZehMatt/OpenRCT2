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
#include "../peep/PeepController.h"
#include "../network/network.h"

struct PeepMoveAction : public GameActionBase<GAME_COMMAND_PEEP_MOVE, GameActionResult>
{
private:
    sint32 _peepIndex = -1;
    uint8 _direction = 0;

public:
    PeepMoveAction() {}
    PeepMoveAction(sint32 peepIndex, uint8 direction)
        : _peepIndex(peepIndex),
        _direction(direction)
    {
    }

    uint16 GetActionFlags() const override
    {
        return GameAction::GetActionFlags();
    }

    void Serialise(DataSerialiser& stream) override
    {
        GameAction::Serialise(stream);

        stream << _peepIndex << _direction;
    }

    GameActionResult::Ptr Query() const override
    {
        rct_peep *peep = GET_PEEP(_peepIndex);
        if (!peep)
        {
            log_info("Invalid peep.");
            return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_NONE);
        }
        if (peep->player_controlled != 1 || peep->player_id != (sint32)GetPlayer())
        {
            log_info("Player trying to move peep who doesn't belong to him.");
            return std::make_unique<GameActionResult>(GA_ERROR::DISALLOWED, STR_NONE);
        }
        return std::make_unique<GameActionResult>();
    }

    GameActionResult::Ptr Execute() const override
    {
        static const LocationXY16 directionDelta[] = {
            { -1,   0 },
            { 0, +1 },
            { +1,   0 },
            { 0, -1 },
            { -1, +1 },
            { +1, +1 },
            { +1, -1 },
            { -1, -1 }
        };

        auto res = std::make_unique<ParkCreateAvatarGameActionResult>();

        rct_peep *peep = GET_PEEP(_peepIndex);

        // NOTE: We need the direction.
        const LocationXY16& delta = directionDelta[_direction];

        peep->destination_x = peep->x + delta.x;
        peep->destination_y = peep->y + delta.y;
        peep->destination_tolerance = 2;

        peep->state = PEEP_STATE_WALKING;
        peep->sub_state = 0;

        res->spriteIndex = peep->sprite_index;
        res->Position.x = peep->x;
        res->Position.y = peep->y;
        res->Position.z = peep->z;

        return res;
    }
};
