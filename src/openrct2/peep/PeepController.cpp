#include "PeepController.h"
#if defined(PEEP_CONTROLLER)
#include "Peep.h"
#include "../world/Park.h"
#include "../actions/ParkCreateAvatar.hpp"

// Called on server, sent to all clients
void peep_controller_add_avatar(const std::string& name, sint32 playerId)
{
    auto action = ParkCreateAvatar(name, playerId);
    auto res = GameActions::Execute(&action);
}

#endif