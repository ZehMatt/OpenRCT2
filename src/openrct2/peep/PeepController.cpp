#include "PeepController.h"
#if defined(PEEP_CONTROLLER)
#include "Peep.h"
#include "../world/Park.h"
#include "../Input.h"
#include "../interface/Viewport.h"
#include "../actions/ParkCreateAvatar.hpp"
#include "../actions/PeepMove.hpp"

static sint32 _assignedPeepIndex = -1;

// Called on server, sent to all clients
void peep_controller_add_avatar(const std::string& name, sint32 playerId)
{
    auto action = ParkCreateAvatar(name, playerId);
    auto res = GameActions::Execute(&action);
}

// Called on client, keeps track of what peep we can control.
void peep_controller_set_avatar(rct_peep *peep)
{
    if(!peep)
        return;

    _assignedPeepIndex = peep->sprite_index;

    auto intent = Intent(WC_PEEP);
    intent.putExtra(INTENT_EXTRA_PEEP, peep);
    context_open_intent(&intent);

    rct_window *mainWindow = window_get_main();
    if (mainWindow)
    {
        window_follow_sprite(mainWindow, peep->sprite_index);
    }
}

rct_peep* peep_controller_get_peep()
{
    if(_assignedPeepIndex == -1)
        return nullptr;

    return GET_PEEP(_assignedPeepIndex);
}

#pragma warning(disable:4005)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

void peep_controller_update_client()
{
    rct_peep *peep = peep_controller_get_peep();
    if(!peep)
        return;

    if (peep->state != PEEP_STATE_WALKING)
    {
        uint8 dir = 4;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)
        {
            dir = 0;
        }
        else if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
        {
            dir = 2;
        }
        else if (GetAsyncKeyState(VK_UP) & 0x8000)
        {
            dir = 3;
        }
        else if (GetAsyncKeyState(VK_DOWN) & 0x8000)
        {
            dir = 1;
        }
        if (dir != 4)
        {
            auto action = PeepMoveAction(peep->sprite_index, dir);
            auto res = GameActions::Execute(&action);
        }
    }
}

// Called per client/server frame.
void peep_controller_update()
{
    if(network_get_mode() == NETWORK_MODE_CLIENT)
        peep_controller_update_client();
}

void peep_controller_walk_direction(rct_peep *peep, uint8 dir)
{
    if (peep->state == PEEP_STATE_WALKING)
    {
        // Already busy walking.
        return;
    }
    peep->state = PEEP_STATE_WALKING;

}

#endif
