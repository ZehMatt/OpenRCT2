#include "PeepController.h"
#if defined(PEEP_CONTROLLER)
#include "Peep.h"
#include "../world/Park.h"
#include "../interface/Viewport.h"
#include "../actions/ParkCreateAvatar.hpp"

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

void peep_controller_update_client()
{
    rct_peep *peep = peep_controller_get_peep();
    if(!peep)
        return;
}

// Called per client/server frame.
void peep_controller_update()
{

    if(network_get_mode() == NETWORK_MODE_CLIENT)
        peep_controller_update_client();
}

#endif
