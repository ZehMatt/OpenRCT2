#pragma once

#define PEEP_CONTROLLER

#if defined(PEEP_CONTROLLER)
#include "../common.h"
#include <string>

void peep_controller_add_avatar(const std::string& name, sint32 playerId);

#endif