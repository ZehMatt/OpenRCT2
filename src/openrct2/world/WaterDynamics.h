#pragma once

#include "Location.hpp"

class WaterDynamics
{
private:
    TileCoordsXY _iterator{};

public:
    static WaterDynamics& Get();

    void Reset();

    void Tick();
};
