#pragma once

#include <cstdint>

struct PaintSession;

namespace OpenRCT2::Weather
{
    void Reset();
    void Update();
    void Paint(PaintSession& session, int32_t imageDirection);

} // namespace OpenRCT2::Rain
