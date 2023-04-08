#include "WeatherSystem.hpp"

#include "../../paint/Paint.h"
#include "Clouds.hpp"
#include "Rain.hpp"

#include <array>
#include <random>
#include <vector>

#pragma optimize("", off)

namespace OpenRCT2::Weather
{
    void Reset()
    {
        Rain::Reset();
        Clouds::Reset();
    }

    void Update()
    {
        Rain::Update();
        Clouds::Update();
    }

    void Paint(PaintSession& session, int32_t imageDirection)
    {
        Rain::Paint(session, imageDirection);
        //Clouds::Paint(session, imageDirection);
    }

} // namespace OpenRCT2::Weather
