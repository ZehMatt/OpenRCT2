#pragma once

#include <cstdint>

struct PaintSession;

namespace OpenRCT2::Weather::Clouds
{

    void Update();
    void Reset();
    void Paint(PaintSession& session, int32_t imageDirection);

} // namespace OpenRCT2::Weather::Clouds
