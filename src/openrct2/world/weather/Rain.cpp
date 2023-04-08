#include "Rain.hpp"

#include "../../Game.h"
#include "../../paint/Paint.h"
#include "../../sprites.h"
#include "../../util/PerlinNoise.hpp"
#include "../Climate.h"
#include "../Location.hpp"
#include "../Map.h"
#include "../TileElementsView.h"

#include <vector>

#pragma optimize("", off)

namespace OpenRCT2::Weather::Rain
{
    struct RainParticle
    {
        uint32_t Id;
        CoordsXYZ Pos;
    };

    struct RainSurfaceHit
    {
        uint32_t Id;
        CoordsXYZ Pos;
        int32_t Ticks;
    };

    static FastNoiseLite _noise(1444);

    static std::vector<RainParticle> _rainParticles;
    static uint32_t _nextRainId = 0;

    static std::vector<RainSurfaceHit> _rainSurfaceHits;
    static uint32_t _nextSurfaceHitId = 0;

    static std::array<int, 128 * 128> _rainMap{};

    static constexpr size_t kMaxParticles = 20000;
    static constexpr size_t kMaxSurfaceHits = 6000;

    static constexpr uint32_t kRainRateChange = 10;

    static constexpr int32_t kSpawnHeight = 1000;

    static constexpr uint32_t kDropAnimateTime = 10;
    static constexpr uint32_t kDropFrames = 5;
    static constexpr uint32_t kDropCycleTime = kDropAnimateTime / kDropFrames;

    static constexpr CoordsXYZ kRainFallVectors[] = {
        { 0, 0, -26 },
        { 0, 0, -27 },
        { 0, 0, -28 },
        { 0, 0, -29 },
    };

    static int32_t _rainRate = 0;
    static int32_t _rainRateTarget = 0;
    static int32_t _rainTimer = 0;

    void Reset()
    {
        _noise.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
        _noise.SetFractalType(FastNoiseLite::FractalType_DomainWarpProgressive);

        // Cellular
        _noise.SetCellularDistanceFunction(FastNoiseLite::CellularDistanceFunction_Hybrid);
        _noise.SetCellularReturnType(FastNoiseLite::CellularReturnType_CellValue);
        _noise.SetCellularJitter(1.0f);
        _noise.SetFrequency(0.005f);

        // Domain Warp.
        _noise.SetDomainWarpType(FastNoiseLite::DomainWarpType_BasicGrid);
        _noise.SetDomainWarpAmp(30.0);

        // Domain warp fractal.
        _noise.SetFractalOctaves(5);
        _noise.SetFractalLacunarity(2.0f);
        _noise.SetFractalGain(0.60f);

        _rainParticles.clear();
    }

    static int32_t UtilRandMinMax(int32_t min, int32_t max)
    {
        // Calculate the range of values to generate
        int32_t range = max - min + 1;

        // Generate a random value within the range
        uint32_t rand_val = (uint32_t)rand();
        int32_t result = (int32_t)(rand_val % range) + min;

        return result;
    }

    static bool StopRainParticle(const RainParticle& particle, const CoordsXY& maxMapSize)
    {
        const auto& pos = particle.Pos;
        if (pos.x < 0)
            return true;
        if (pos.y < 0)
            return true;
        if (pos.z < 0)
            return true;
        if (pos.x > maxMapSize.x)
            return true;
        if (pos.y > maxMapSize.y)
            return true;
        return false;
    }

    static std::optional<CoordsXYZ> GetTileHitPosition(const RainParticle& particle)
    {
        const auto tilePos = TileCoordsXYZ(particle.Pos);
        if (tilePos.z > 100)
            return std::nullopt;

        // Re-align height, base is multiplied by COORDS_Z_STEP
        const auto coords = tilePos.ToCoordsXYZ();

        int32_t highestBase = 0;
        TileElement* highestElement = nullptr;

        for (TileElement* tile : TileElementsView<TileElement>(tilePos))
        {
            // Ignore surface element.
            if (tile->GetType() == TileElementType::Wall)
                continue;

            auto height = std::max(tile->GetClearanceZ(), tile->GetBaseZ());
            if (tile->GetType() == TileElementType::Surface)
            {
                height = std::max(height, tile->AsSurface()->GetWaterHeight());
            }

            if (height >= highestBase)
            {
                highestBase = height;
                highestElement = tile;
            }
        }

        if (highestElement == nullptr)
            return std::nullopt;

        if (particle.Pos.z > highestBase)
            return std::nullopt;

        // Hit surface.
        return CoordsXYZ{ tilePos.ToCoordsXY(), highestBase };
    }

    static void InvalidatePos(const CoordsXYZ& pos)
    {
        const auto screenPos = Translate3DTo2DWithZ(GetCurrentRotation(), pos);
        const auto spriteRect = ScreenRect{ screenPos.x - 4, screenPos.y - 24, screenPos.x + 4, screenPos.y + 24 };
        ViewportsInvalidate(spriteRect, ZoomLevel::max());
    }

    static void UpdateRainRate()
    {
        if (gClimateCurrent.Weather == WeatherType::Rain || gClimateCurrent.Weather == WeatherType::HeavyRain)
        {
            if (gClimateCurrent.Level == WeatherLevel::Heavy)
            {
                _rainRateTarget = 150;
            }
            else if (gClimateCurrent.Level == WeatherLevel::Light)
            {
                _rainRateTarget = 20;
            }
            else
            {
                _rainRateTarget = 0;
            }
        }
        else
        {
            _rainRateTarget = 0;
        }
    }

    static void UpdateRainMap()
    {
        float offset = (gCurrentTicks * 0.2);
        float scale = 2.5f;
        for (int x = 0; x < 128; x++)
        {
            for (int y = 0; y < 128; y++)
            {
                float val = _noise.GetNoise(offset + (x * scale), offset + (y * scale));
                _rainMap[y * 128 + x] = (int)(val * 100.0f);
            }
        }
    }

    int32_t RemapValue(int32_t value, int32_t valueMin, int32_t valueMax, int32_t newMin, int32_t newMax)
    {
        return newMin + (value - valueMin) * (newMax - newMin) / (valueMax - valueMin);
    }

    void Update()
    {
        UpdateRainRate();
        UpdateRainMap();

        // Approach target
        if (_rainTimer++ >= kRainRateChange)
        {
            if (_rainRate > _rainRateTarget)
                _rainRate = _rainRate - std::min(std::abs(_rainRateTarget - _rainRate), 3);
            else if (_rainRate < _rainRateTarget)
                _rainRate += 2;

            _rainTimer = 0;
        }

        const auto mapSize = gMapSize.ToCoordsXY();

        // Update existing drops.
        for (size_t i = 0; i < _rainParticles.size();)
        {
            auto& particle = _rainParticles[i];
            auto& pos = particle.Pos;

            // Invalidate old location.
            InvalidatePos(pos);

            bool remove = false;

            const auto hitPos = GetTileHitPosition(particle);
            if (hitPos.has_value())
            {
                remove = true;
                if (_rainSurfaceHits.size() < kMaxSurfaceHits)
                {
                    _rainSurfaceHits.push_back(RainSurfaceHit{ _nextSurfaceHitId, hitPos.value(), 0 });
                    _nextSurfaceHitId++;
                }
            }
            else
            {
                if (StopRainParticle(particle, mapSize))
                {
                    remove = true;
                }
                else
                {
                    const auto fallVector = kRainFallVectors[i % std::size(kRainFallVectors)];
                    pos = pos + fallVector;

                    // Invalidate new location
                    InvalidatePos(pos);
                }
            }

            if (remove)
            {
                std::swap(particle, _rainParticles[_rainParticles.size() - 1]);
                _rainParticles.pop_back();
            }
            else
            {
                i++;
            }
        }

        // Update drops
        for (size_t i = 0; i < _rainSurfaceHits.size();)
        {
            auto& drop = _rainSurfaceHits[i];
            InvalidatePos(drop.Pos);

            if (drop.Ticks++ > kDropAnimateTime)
            {
                std::swap(drop, _rainSurfaceHits[_rainSurfaceHits.size() - 1]);
                _rainSurfaceHits.pop_back();
            }
            else
            {
                i++;
            }
        }

        if (_rainRate <= 0)
            return;

        // Spawn new particles
        size_t remainingCount = std::min<size_t>(_rainRate, kMaxParticles - _rainParticles.size());
        for (size_t n = 0; n < remainingCount; n++)
        {
            if (UtilRandMinMax(0, 99) > _rainRate)
                continue;

            const int32_t posX = UtilRandMinMax(0, mapSize.x);
            const int32_t posY = UtilRandMinMax(0, mapSize.y);

            //const auto x1 = RemapValue(posX, 0, mapSize.x, 0, 128);
            //const auto y1 = RemapValue(posY, 0, mapSize.y, 0, 128);

            //const auto density = _rainMap[y1 * 128 + x1];
            //if (UtilRandMinMax(0, 99) > density)
                //continue;

            const int32_t spawnHeight = 1000;

            auto particle = RainParticle{ _nextRainId, { posX, posY, spawnHeight } };
            _nextRainId++;

            _rainParticles.push_back(particle);
        }
    }

    static int32_t GetRainTrailImage(PaintSession& session)
    {
        switch (static_cast<int8_t>(session.DPI.zoom_level))
        {
            case 0:
                return SPR_G2_EFFECT_RAIN_TRAIL_00;
            case 1:
                return SPR_G2_EFFECT_RAIN_TRAIL_01;
            case 2:
                return SPR_G2_EFFECT_RAIN_TRAIL_02;
            case 3:
                return SPR_G2_EFFECT_RAIN_TRAIL_03;
            default:
                break;
        }
        return SPR_G2_EFFECT_RAIN_TRAIL_00;
    }

    static int32_t GetSkipCount(PaintSession& session)
    {
        switch (static_cast<int8_t>(session.DPI.zoom_level))
        {
            case 0:
                return 1;
            case 1:
                return 2;
            case 2:
                return 3;
            case 3:
                return 4;
            default:
                break;
        }
        return 1;
    }

    static void PaintRainTrails(PaintSession& session, int32_t imageDirection)
    {
        const auto baseImage = GetRainTrailImage(session);
        const auto imageId = ImageId(baseImage).WithTransparency(FilterPaletteID::PaletteTranslucentWhite);

        const auto skipCount = GetSkipCount(session);
        for (size_t i = 0; i < _rainParticles.size(); i++)
        {
            const auto& particle = _rainParticles[i];
            if (particle.Id % skipCount != 0)
                continue;

            const auto& pos = particle.Pos;

            session.CurrentlyDrawnEntity = nullptr;
            session.SpritePosition = pos;
            session.InteractionType = ViewportInteractionItem::None;

            PaintAddImageAsParent(session, imageId, { 0, 0, pos.z }, { 4, 4, 24 });
        }
    }

    static void PaintRainDrops(PaintSession& session, int32_t imageDirection)
    {
        const auto baseImage = ImageId(SPR_G2_EFFECT_RAIN_HIT_SURFACE_00)
                                   .WithTransparency(FilterPaletteID::PaletteTranslucentWhite);

        const auto skipCount = GetSkipCount(session);
        for (size_t i = 0; i < _rainSurfaceHits.size(); i++)
        {
            const auto& particle = _rainSurfaceHits[i];
            if (particle.Id % skipCount != 0)
                continue;

            const auto cycleIndex = particle.Ticks / kDropCycleTime;
            const auto& pos = particle.Pos;

            const auto imageId = baseImage.WithIndex(SPR_G2_EFFECT_RAIN_HIT_SURFACE_00 + cycleIndex);

            session.CurrentlyDrawnEntity = nullptr;
            session.SpritePosition = pos;
            session.InteractionType = ViewportInteractionItem::None;

            PaintAddImageAsParent(session, imageId, { 0, 0, pos.z + 1 }, { 6, 6, 1 });
        }
    }

    void PaintRainMap(PaintSession& session, int32_t imageDirection)
    {
        for (auto x = 0; x < gMapSize.x; x++)
        {
            for (auto y = 0; y < gMapSize.y; y++)
            {
                const auto tilePos = TileCoordsXY{ x, y };

                const auto x1 = RemapValue(tilePos.x, 0, gMapSize.x, 0, 128);
                const auto y1 = RemapValue(tilePos.y, 0, gMapSize.y, 0, 128);

                const auto density = _rainMap[y1 * 128 + x1];
                if (density <= 30)
                    continue;

                auto imageColourFlats = ImageId(SPR_G2_SURFACE_GLASSY_RECOLOURABLE, FilterPaletteID::PaletteWater)
                                            .WithBlended(true);

                session.CurrentlyDrawnEntity = nullptr;
                session.SpritePosition = tilePos.ToCoordsXY();
                session.InteractionType = ViewportInteractionItem::None;

                PaintAddImageAsParent(session, imageColourFlats, { 0, 0, kSpawnHeight }, { 6, 6, 1 });
            }
        }
    }

    void Paint(PaintSession& session, int32_t imageDirection)
    {
        PaintRainTrails(session, imageDirection);
        PaintRainDrops(session, imageDirection);
        // PaintRainMap(session, imageDirection);
    }

} // namespace OpenRCT2::Weather::Rain
