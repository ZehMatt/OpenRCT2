/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
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

#include <mutex>
#include <thread>

struct TileElement;
enum class ViewportInteractionItem : uint8_t;

#pragma pack(push, 1)
/* size 0x12 */
struct attached_paint_struct
{
    uint32_t image_id; // 0x00
    union
    {
        uint32_t tertiary_colour;
        // If masked image_id is masked_id
        uint32_t colour_image_id;
    };
    int16_t x;     // 0x08
    int16_t y;     // 0x0A
    uint8_t flags; // 0x0C
    uint8_t pad_0D;
    attached_paint_struct* next; // 0x0E
};
#ifdef PLATFORM_32BIT
// TODO: drop packing from this when all rendering is done.
assert_struct_size(attached_paint_struct, 0x12);
#endif

enum PAINT_QUADRANT_FLAGS
{
    PAINT_QUADRANT_FLAG_IDENTICAL = (1 << 0),
    PAINT_QUADRANT_FLAG_BIGGER = (1 << 7),
    PAINT_QUADRANT_FLAG_NEXT = (1 << 1),
};

struct paint_struct_bound_box
{
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint16_t x_end;
    uint16_t y_end;
    uint16_t z_end;
};

/* size 0x34 */
struct paint_struct
{
    uint32_t image_id; // 0x00
    union
    {
        uint32_t tertiary_colour; // 0x04
        // If masked image_id is masked_id
        uint32_t colour_image_id; // 0x04
    };
    paint_struct_bound_box bounds; // 0x08
    int16_t x;                     // 0x14
    int16_t y;                     // 0x16
    uint16_t quadrant_index;
    uint8_t flags;
    uint8_t quadrant_flags;
    attached_paint_struct* attached_ps; // 0x1C
    paint_struct* children;
    paint_struct* next_quadrant_ps;      // 0x24
    ViewportInteractionItem sprite_type; // 0x28
    uint8_t var_29;
    uint16_t pad_2A;
    uint16_t map_x;           // 0x2C
    uint16_t map_y;           // 0x2E
    TileElement* tileElement; // 0x30 (or sprite pointer)
};
#ifdef PLATFORM_32BIT
// TODO: drop packing from this when all rendering is done.
assert_struct_size(paint_struct, 0x34);
#endif

struct paint_string_struct
{
    rct_string_id string_id;   // 0x00
    paint_string_struct* next; // 0x02
    int32_t x;                 // 0x06
    int32_t y;                 // 0x08
    uint32_t args[4];          // 0x0A
    uint8_t* y_offsets;        // 0x1A
};
#pragma pack(pop)

union paint_entry
{
    paint_struct basic;
    attached_paint_struct attached;
    paint_string_struct string;
};

struct sprite_bb
{
    uint32_t sprite_id;
    CoordsXYZ offset;
    CoordsXYZ bb_offset;
    CoordsXYZ bb_size;
};

enum PAINT_STRUCT_FLAGS
{
    PAINT_STRUCT_FLAG_IS_MASKED = (1 << 0)
};

struct support_height
{
    uint16_t height;
    uint8_t slope;
    uint8_t pad;
};

struct tunnel_entry
{
    uint8_t height;
    uint8_t type;
};

#define MAX_PAINT_QUADRANTS 512
#define TUNNEL_MAX_COUNT 65

/**
 * A pool of paint_entry instances that can be rented out.
 * The internal implementation uses an unrolled linked list so that each
 * paint session can quickly allocate a new paint entry until it requires
 * another node / block of paint entries. Only the node allocation needs to
 * be thread safe.
 */
class PaintEntryPool
{
    static constexpr size_t NodeSize = 512;

public:
    struct Node
    {
        Node* Next{};
        size_t Count{};
        paint_entry PaintStructs[NodeSize]{};
    };

    struct Chain
    {
        PaintEntryPool* Pool{};
        Node* Head{};
        Node* Current{};

        Chain() = default;
        Chain(PaintEntryPool* pool);
        Chain(Chain&& chain);
        ~Chain();

        Chain& operator=(Chain&& chain) noexcept;

        paint_entry* Allocate();
        void Clear();
        size_t GetCount() const;
    };

private:
    std::vector<Node*> _available;
    std::mutex _mutex;

    Node* AllocateNode();

public:
    ~PaintEntryPool();

    Chain Create();
    void FreeNodes(Node* head);
};

struct PaintSessionCore
{
    paint_struct* Quadrants[MAX_PAINT_QUADRANTS];
    paint_struct* LastPS;
    paint_string_struct* PSStringHead;
    paint_string_struct* LastPSString;
    attached_paint_struct* LastAttachedPS;
    const TileElement* SurfaceElement;
    const void* CurrentlyDrawnItem;
    TileElement* PathElementOnSameHeight;
    TileElement* TrackElementOnSameHeight;
    paint_struct PaintHead;
    uint32_t ViewFlags;
    uint32_t QuadrantBackIndex;
    uint32_t QuadrantFrontIndex;
    CoordsXY SpritePosition;
    ViewportInteractionItem InteractionType;
    uint8_t CurrentRotation;
    support_height SupportSegments[9];
    support_height Support;
    paint_struct* WoodenSupportsPrependTo;
    CoordsXY MapPosition;
    tunnel_entry LeftTunnels[TUNNEL_MAX_COUNT];
    uint8_t LeftTunnelCount;
    tunnel_entry RightTunnels[TUNNEL_MAX_COUNT];
    uint8_t RightTunnelCount;
    uint8_t VerticalTunnelHeight;
    bool DidPassSurface;
    uint8_t Unk141E9DB;
    uint16_t WaterHeight;
    uint32_t TrackColours[4];
};

struct paint_session : public PaintSessionCore
{
    rct_drawpixelinfo DPI;
    PaintEntryPool::Chain PaintEntryChain;

    paint_struct* AllocateNormalPaintEntry() noexcept
    {
        auto* entry = PaintEntryChain.Allocate();
        if (entry != nullptr)
        {
            LastPS = &entry->basic;
            return LastPS;
        }
        return nullptr;
    }

    attached_paint_struct* AllocateAttachedPaintEntry() noexcept
    {
        auto* entry = PaintEntryChain.Allocate();
        if (entry != nullptr)
        {
            LastAttachedPS = &entry->attached;
            return LastAttachedPS;
        }
        return nullptr;
    }

    paint_string_struct* AllocateStringPaintEntry() noexcept
    {
        auto* entry = PaintEntryChain.Allocate();
        if (entry != nullptr)
        {
            auto* string = &entry->string;
            if (LastPSString == nullptr)
            {
                PSStringHead = string;
            }
            else
            {
                LastPSString->next = string;
            }
            LastPSString = string;
            return LastPSString;
        }
        return nullptr;
    }
};

struct RecordedPaintSession
{
    PaintSessionCore Session;
    std::vector<paint_entry> Entries;
};

extern paint_session gPaintSession;

// Globals for paint clipping
extern uint8_t gClipHeight;
extern CoordsXY gClipSelectionA;
extern CoordsXY gClipSelectionB;

/** rct2: 0x00993CC4. The white ghost that indicates not-yet-built elements. */
#define CONSTRUCTION_MARKER (COLOUR_DARK_GREEN << 19 | COLOUR_GREY << 24 | IMAGE_TYPE_REMAP)
extern bool gShowDirtyVisuals;
extern bool gPaintBoundingBoxes;
extern bool gPaintBlockedTiles;
extern bool gPaintWidePathsAsGhost;

paint_struct* PaintAddImageAsParent(
    paint_session* session, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset);
paint_struct* PaintAddImageAsParent(
    paint_session* session, uint32_t image_id, const CoordsXYZ& offset, const CoordsXYZ& boundBoxSize);
paint_struct* PaintAddImageAsParent(
    paint_session* session, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset, int16_t bound_box_offset_x,
    int16_t bound_box_offset_y, int16_t bound_box_offset_z);
paint_struct* PaintAddImageAsParent(
    paint_session* session, uint32_t image_id, const CoordsXYZ& offset, const CoordsXYZ& boundBoxSize,
    const CoordsXYZ& boundBoxOffset);
[[nodiscard]] paint_struct* PaintAddImageAsOrphan(
    paint_session* session, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset, int16_t bound_box_offset_x,
    int16_t bound_box_offset_y, int16_t bound_box_offset_z);
paint_struct* PaintAddImageAsChild(
    paint_session* session, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset, int16_t bound_box_offset_x,
    int16_t bound_box_offset_y, int16_t bound_box_offset_z);
paint_struct* PaintAddImageAsChild(
    paint_session* session, uint32_t image_id, const CoordsXYZ& offset, const CoordsXYZ& boundBoxLength,
    const CoordsXYZ& boundBoxOffset);

paint_struct* PaintAddImageAsParentRotated(
    paint_session* session, uint8_t direction, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset);
paint_struct* PaintAddImageAsParentRotated(
    paint_session* session, uint8_t direction, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset, int16_t bound_box_offset_x,
    int16_t bound_box_offset_y, int16_t bound_box_offset_z);
paint_struct* PaintAddImageAsChildRotated(
    paint_session* session, uint8_t direction, uint32_t image_id, int8_t x_offset, int8_t y_offset, int16_t bound_box_length_x,
    int16_t bound_box_length_y, int8_t bound_box_length_z, int16_t z_offset, int16_t bound_box_offset_x,
    int16_t bound_box_offset_y, int16_t bound_box_offset_z);

void paint_util_push_tunnel_rotated(paint_session* session, uint8_t direction, uint16_t height, uint8_t type);

bool PaintAttachToPreviousAttach(paint_session* session, uint32_t image_id, int16_t x, int16_t y);
bool PaintAttachToPreviousPS(paint_session* session, uint32_t image_id, int16_t x, int16_t y);
void PaintFloatingMoneyEffect(
    paint_session* session, money64 amount, rct_string_id string_id, int16_t y, int16_t z, int8_t y_offsets[], int16_t offset_x,
    uint32_t rotation);

paint_session* PaintSessionAlloc(rct_drawpixelinfo* dpi, uint32_t viewFlags);
void PaintSessionFree(paint_session* session);
void PaintSessionGenerate(paint_session* session);
void PaintSessionArrange(PaintSessionCore* session);
void PaintDrawStructs(paint_session* session);
void PaintDrawMoneyStructs(rct_drawpixelinfo* dpi, paint_string_struct* ps);

// TESTING
#ifdef __TESTPAINT__
void testpaint_clear_ignore();
void testpaint_ignore(uint8_t direction, uint8_t trackSequence);
void testpaint_ignore_all();
bool testpaint_is_ignored(uint8_t direction, uint8_t trackSequence);

#    define TESTPAINT_IGNORE(direction, trackSequence) testpaint_ignore(direction, trackSequence)
#    define TESTPAINT_IGNORE_ALL() testpaint_ignore_all()
#else
#    define TESTPAINT_IGNORE(direction, trackSequence)
#    define TESTPAINT_IGNORE_ALL()
#endif
