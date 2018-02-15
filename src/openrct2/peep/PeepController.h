#pragma once

#define PEEP_CONTROLLER

#if defined(PEEP_CONTROLLER)
#include "../common.h"
#include <string>

struct rct_peep;

void peep_controller_update();
void peep_controller_add_avatar(const std::string& name, sint32 playerId);
void peep_controller_set_avatar(rct_peep *peep);

#endif