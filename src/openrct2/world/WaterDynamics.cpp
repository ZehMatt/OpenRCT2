#include "WaterDynamics.h"

#include "Map.h"

//#pragma optimize("", off)

static WaterDynamics _waterDynamics;

WaterDynamics& WaterDynamics::Get()
{
    return _waterDynamics;
}

void WaterDynamics::Reset()
{
    _iterator = { 0, 0 };
}

static void applyFlood(SurfaceElement* sourceTile, const TileCoordsXY& coords)
{
    if (!MapIsLocationValid(coords.ToCoordsXY()))
        return;

    auto* surfaceElement = MapGetSurfaceElementAt(coords.ToCoordsXY());
    if (surfaceElement == nullptr)
        return;

    const auto sourceWaterHeight = sourceTile->GetWaterHeight();
    if (sourceWaterHeight == 0)
        return;

    const auto curHeight = surfaceElement->GetBaseZ();
    if (curHeight > sourceWaterHeight)
        return;

    const auto curWaterHeight = std::max(surfaceElement->GetWaterHeight(), surfaceElement->GetBaseZ());
    if (curWaterHeight < sourceWaterHeight)
    {
        sourceTile->SetWaterHeight(sourceWaterHeight - 1);

        auto newHeight = std::max(curHeight, curWaterHeight + 1);
        surfaceElement->SetWaterHeight(newHeight);

        MapInvalidateTileFull(coords.ToCoordsXY());
    }
}

void WaterDynamics::Tick()
{
    for (int i = 0; i < 1024 * 1000; i++)
    {
        if (_iterator.x >= MAXIMUM_MAP_SIZE_TECHNICAL)
        {
            _iterator.x = 0;
            _iterator.y++;
        }

        if (_iterator.y >= MAXIMUM_MAP_SIZE_TECHNICAL)
        {
            _iterator.y = 0;
        }

        auto* surfaceElement = MapGetSurfaceElementAt(_iterator.ToCoordsXY());
        if (surfaceElement != nullptr)
        {
            if (surfaceElement->GetWaterHeight() > surfaceElement->BaseHeight)
            {
                applyFlood(surfaceElement, _iterator + TileCoordsXY{ 1, 0 });
                applyFlood(surfaceElement, _iterator + TileCoordsXY{ 0, 1 });
                applyFlood(surfaceElement, _iterator + TileCoordsXY{ -1, 0 });
                applyFlood(surfaceElement, _iterator + TileCoordsXY{ 0, -1 });

                MapInvalidateTileFull(_iterator.ToCoordsXY());
            }
        }

        _iterator.x++;
    }
}
