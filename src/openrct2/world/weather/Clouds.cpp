#include "Clouds.hpp"

#include "../../Game.h"
#include "../../paint/Paint.h"
#include "../../sprites.h"
#include "../../util/PerlinNoise.hpp"
#include "../Location.hpp"
#include "../Map.h"

#include <array>
#include <cmath>
#include <random>
#include <vector>

//#pragma optimize("", off)

namespace OpenRCT2::Weather::Clouds
{
    FastNoiseLite _noise(1444);

    constexpr auto GridSize = 256;
    static std::array<float, GridSize * GridSize> _grid{};

    void Reset()
    {
        _noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        _noise.SetFractalType(FastNoiseLite::FractalType_FBm);
        _noise.SetFrequency(0.00015f);
        _noise.SetFractalOctaves(5);
        _noise.SetFractalLacunarity(2.0f);
        _noise.SetFractalGain(0.50f);
    }

    void Update()
    {
        const auto tick = gCurrentTicks;
        float scale = 10.5f;
        for (int32_t x = 0; x < GridSize; x++)
        {
            for (int32_t y = 0; y < GridSize; y++)
            {
                const double noise = _noise.GetNoise((tick + x) * scale, (tick + y) * scale);
                _grid[y * GridSize + x] = noise;
            }
        }
    }

    int32_t RemapValue(int32_t value, int32_t valueMin, int32_t valueMax, int32_t newMin, int32_t newMax)
    {
        return newMin + (value - valueMin) * (newMax - newMin) / (valueMax - valueMin);
    }

    static constexpr bool ImageWithinDPI(const ScreenCoordsXY& imagePos, const DrawPixelInfo& dpi)
    {
        int32_t left = imagePos.x;
        int32_t bottom = imagePos.y;
        int32_t right = left;
        int32_t top = bottom;

        if (right <= dpi.x)
            return false;
        if (top <= dpi.y)
            return false;
        if (left >= dpi.x + dpi.width)
            return false;
        if (bottom >= dpi.y + dpi.height)
            return false;
        return true;
    }

    void Paint(PaintSession& session, int32_t imageDirection)
    {
        int32_t cloudZ = 900;

        auto mapSize = gMapSize.ToCoordsXY();
        auto stepSize = 10;
        const auto tick = gCurrentTicks;
        float scale = 10.5f;

        for (int32_t x = 0; x < mapSize.x; x += stepSize)
        {
            for (int32_t y = 0; y < mapSize.y; y += stepSize)
            {
                auto pos = CoordsXYZ{ x, y, cloudZ };
                const auto imagePos = Translate3DTo2DWithZ(session.CurrentRotation, pos);

                if (!ImageWithinDPI(imagePos, session.DPI))
                {
                    continue;
                }

                const double noise = _noise.GetNoise((tick + x) * scale, (tick + y) * scale);
                if (noise < 0.0f)
                {
                    y += 16;
                    continue;
                }

                int32_t frame = (int)(noise * 14.0f);
                y += frame;

                session.CurrentlyDrawnEntity = nullptr;
                session.SpritePosition = pos;
                session.InteractionType = ViewportInteractionItem::None;

                uint32_t imageId = 22637 + frame;
                PaintAddImageAsParent(session, ImageId(imageId), { 0, 0, pos.z }, { 10, 10, 10 });
            }
        }
    }

} // namespace OpenRCT2::Weather::Clouds
