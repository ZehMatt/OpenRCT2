/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include "../common.h"
#include "../core/FixedVector.h"
#include "../drawing/Drawing.h"
#include "../interface/Colour.h"
#include "../world/Location.hpp"
#include "../world/Map.h"
#include "Boundbox.h"

#include <mutex>
#include <thread>
#include <variant>
#include <vector>

struct EntityBase;
struct TileElement;

enum class RailingEntrySupportType : uint8_t;
enum class ViewportInteractionItem : uint8_t;

struct PaintStructBoundBox
{
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t x_end;
    int32_t y_end;
    int32_t z_end;
};

enum class PaintNodeId : uint32_t
{
    Invalid = 0xFFFFFFFF,
};

struct PaintNodeBase
{
    PaintNodeId Id;
    PaintNodeId Next;
};

struct AttachedPaintStruct : PaintNodeBase
{
    ImageId image_id;
    ImageId ColourImageId;
    int32_t x;
    int32_t y;
    bool IsMasked;
};

struct PaintStruct : PaintNodeBase
{
    PaintStructBoundBox bounds;
    PaintNodeId AttachedNode;
    PaintNodeId NextChildNode;
    PaintNodeId NextQuadrantNode;
    ImageId image_id;
    int32_t x;
    int32_t y;
    int32_t map_x;
    int32_t map_y;
    uint16_t quadrant_index;
    uint8_t SortFlags;
    ViewportInteractionItem sprite_type;
    TileElement* tileElement;
    EntityBase* entity;
};

struct PaintStringStruct : PaintNodeBase
{
    StringId string_id;
    int32_t x;
    int32_t y;
    uint32_t args[4];
    uint8_t* y_offsets;
};

union PaintNode
{
    PaintStruct _PaintStruct;
    AttachedPaintStruct _AttachedPaintStruct;
    PaintStringStruct _PaintStringStruct;

    template<typename T> T* get()
    {
        if constexpr (std::is_same_v<T, PaintStruct>)
        {
            return &_PaintStruct;
        }
        else if constexpr (std::is_same_v<T, AttachedPaintStruct>)
        {
            return &_AttachedPaintStruct;
        }
        else if constexpr (std::is_same_v<T, PaintStringStruct>)
        {
            return &_PaintStringStruct;
        }
    }
};

struct SpriteBb
{
    uint32_t sprite_id;
    CoordsXYZ offset;
    CoordsXYZ bb_offset;
    CoordsXYZ bb_size;
};

struct SupportHeight
{
    uint16_t height;
    uint8_t slope;
    uint8_t pad;
};

struct TunnelEntry
{
    uint8_t height;
    uint8_t type;
};

// The maximum size must be MAXIMUM_MAP_SIZE_TECHNICAL multiplied by 2 because
// the quadrant index is based on the x and y components combined.
static constexpr int32_t MaxPaintQuadrants = MAXIMUM_MAP_SIZE_TECHNICAL * 2;

#define TUNNEL_MAX_COUNT 65

struct PaintSessionCore
{
    std::vector<PaintNode> PaintEntries;
    PaintStruct PaintHead;
    PaintNodeId Quadrants[MaxPaintQuadrants];
    PaintNodeId LastPS;
    PaintNodeId PSStringHead;
    PaintNodeId LastPSString;
    PaintNodeId LastAttachedPS;
    const TileElement* SurfaceElement;
    EntityBase* CurrentlyDrawnEntity;
    TileElement* CurrentlyDrawnTileElement;
    const TileElement* PathElementOnSameHeight;
    const TileElement* TrackElementOnSameHeight;
    PaintNodeId WoodenSupportsPrependTo;
    CoordsXY SpritePosition;
    CoordsXY MapPosition;
    uint32_t ViewFlags;
    uint32_t QuadrantBackIndex;
    uint32_t QuadrantFrontIndex;
    ImageId TrackColours[4];
    SupportHeight SupportSegments[9];
    SupportHeight Support;
    uint16_t WaterHeight;
    TunnelEntry LeftTunnels[TUNNEL_MAX_COUNT];
    TunnelEntry RightTunnels[TUNNEL_MAX_COUNT];
    uint8_t LeftTunnelCount;
    uint8_t RightTunnelCount;
    uint8_t VerticalTunnelHeight;
    uint8_t CurrentRotation;
    uint8_t Flags;
    ViewportInteractionItem InteractionType;
};

struct PaintSession : public PaintSessionCore
{
    DrawPixelInfo DPI;

    template<typename T> T* AllocateNode()
    {
        const auto index = PaintEntries.size();
        auto& entry = PaintEntries.emplace_back(PaintNode{});
        auto* node = entry.get<T>();
        node->Id = static_cast<PaintNodeId>(index);
        node->Next = PaintNodeId::Invalid;
        return node;
    }

    PaintStruct* AllocateNormalPaintEntry() noexcept
    {
        auto* node = AllocateNode<PaintStruct>();
        LastPS = node->Id;
        return node;
    }

    AttachedPaintStruct* AllocateAttachedPaintEntry() noexcept
    {
        auto* node = AllocateNode<AttachedPaintStruct>();
        LastAttachedPS = node->Id;
        return node;
    }

    PaintStringStruct* AllocateStringPaintEntry() noexcept
    {
        auto* string = AllocateNode<PaintStringStruct>();
        auto* last = GetNode<PaintStringStruct>(LastPSString);
        if (last == nullptr)
        {
            PSStringHead = string->Id;
        }
        else
        {
            last->Next = string->Id;
        }

        LastPSString = string->Id;
        return string;
    }

    template<typename T> T* GetNode(PaintNodeId id) noexcept
    {
        if (id == PaintNodeId::Invalid)
            return nullptr;
        const auto index = static_cast<size_t>(id);
        /*
        if (index >= PaintEntries.size())
            return nullptr;
        */
        return PaintEntries[index].get<T>();
    }
};

struct FootpathPaintInfo
{
    uint32_t SurfaceImageId{};
    uint32_t BridgeImageId{};
    uint32_t RailingsImageId{};
    uint32_t SurfaceFlags{};
    uint32_t RailingFlags{};
    uint8_t ScrollingMode{};
    RailingEntrySupportType SupportType{};
    colour_t SupportColour = 255;
};

struct RecordedPaintSession
{
    PaintSession Session;
    std::vector<PaintNode> Entries;
};

extern PaintSession gPaintSession;

// Globals for paint clipping
extern uint8_t gClipHeight;
extern CoordsXY gClipSelectionA;
extern CoordsXY gClipSelectionB;

/** rct2: 0x00993CC4. The white ghost that indicates not-yet-built elements. */
constexpr const ImageId ConstructionMarker = ImageId(0).WithRemap(FilterPaletteID::Palette44);
constexpr const ImageId HighlightMarker = ImageId(0).WithRemap(FilterPaletteID::Palette44);
constexpr const ImageId TrackGhost = ImageId(0, FilterPaletteID::PaletteNull);

extern bool gShowDirtyVisuals;
extern bool gPaintBoundingBoxes;
extern bool gPaintBlockedTiles;
extern bool gPaintWidePathsAsGhost;

PaintNodeId PaintAddImageAsParent(
    PaintSession& session, const ImageId image_id, const CoordsXYZ& offset, const BoundBoxXYZ& boundBox);
/**
 *  rct2: 0x006861AC, 0x00686337, 0x006864D0, 0x0068666B, 0x0098196C
 *
 * @param image_id (ebx)
 * @param x_offset (al)
 * @param y_offset (cl)
 * @param bound_box_length_x (di)
 * @param bound_box_length_y (si)
 * @param bound_box_length_z (ah)
 * @param z_offset (dx)
 * @return (ebp) PaintStruct on success (CF == 0), nullptr on failure (CF == 1)
 */
inline PaintNodeId PaintAddImageAsParent(
    PaintSession& session, const ImageId image_id, const CoordsXYZ& offset, const CoordsXYZ& boundBoxSize)
{
    return PaintAddImageAsParent(session, image_id, offset, { offset, boundBoxSize });
}

[[nodiscard]] PaintNodeId PaintAddImageAsOrphan(
    PaintSession& session, const ImageId image_id, const CoordsXYZ& offset, const BoundBoxXYZ& boundBox);

PaintNodeId PaintAddImageAsChild(
    PaintSession& session, const ImageId image_id, const CoordsXYZ& offset, const BoundBoxXYZ& boundBox);

PaintNodeId PaintAddImageAsChildRotated(
    PaintSession& session, const uint8_t direction, const ImageId image_id, const CoordsXYZ& offset,
    const BoundBoxXYZ& boundBox);

PaintNodeId PaintAddImageAsParentRotated(
    PaintSession& session, const uint8_t direction, const ImageId imageId, const CoordsXYZ& offset,
    const BoundBoxXYZ& boundBox);

inline PaintNodeId PaintAddImageAsParentRotated(
    PaintSession& session, const uint8_t direction, const ImageId imageId, const CoordsXYZ& offset,
    const CoordsXYZ& boundBoxSize)
{
    return PaintAddImageAsParentRotated(session, direction, imageId, offset, { offset, boundBoxSize });
}

void PaintUtilPushTunnelRotated(PaintSession& session, uint8_t direction, uint16_t height, uint8_t type);

bool PaintAttachToPreviousAttach(PaintSession& session, const ImageId imageId, int32_t x, int32_t y);
bool PaintAttachToPreviousPS(PaintSession& session, const ImageId image_id, int32_t x, int32_t y);
void PaintFloatingMoneyEffect(
    PaintSession& session, money64 amount, StringId string_id, int32_t y, int32_t z, int8_t y_offsets[], int32_t offset_x,
    uint32_t rotation);

PaintSession* PaintSessionAlloc(DrawPixelInfo* dpi, uint32_t viewFlags);
void PaintSessionFree(PaintSession* session);
void PaintSessionGenerate(PaintSession& session);
void PaintSessionArrange(PaintSession& session);
void PaintDrawStructs(PaintSession& session);
void PaintDrawMoneyStructs(PaintSession& session);
