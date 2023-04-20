/*****************************************************************************
 * Copyright (c) 2014-2023 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "Drawing.h"

template<DrawBlendOp TBlendOp, int8_t TZoom>
static void FASTCALL DrawBMPSpriteMagnify(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    constexpr auto zoomLevel = ZoomLevel{ TZoom };
    constexpr auto zoom = zoomLevel.ApplyInversedTo(1);

    auto& g1 = args.SourceImage;
    auto src = g1.offset + ((static_cast<size_t>(g1.width) * args.SrcY) + args.SrcX);
    auto dst = args.DestinationBits;
    auto& paletteMap = args.PalMap;
    size_t srcLineWidth = g1.width;
    size_t dstLineWidth = zoomLevel.ApplyInversedTo(dpi.width) + dpi.pitch;
    auto width = zoomLevel.ApplyInversedTo(args.Width);
    auto height = zoomLevel.ApplyInversedTo(args.Height);
    for (; height > 0; height -= zoom)
    {
        auto nextSrc = src + srcLineWidth;
        auto nextDst = dst + (dstLineWidth * zoom);
        for (int32_t widthRemaining = width; widthRemaining > 0; widthRemaining -= zoom, src++, dst += zoom)
        {
            // Copy src to a block of zoom * zoom on dst
            BlitPixels<TBlendOp, zoom>(src, dst, paletteMap, dstLineWidth);
        }
        src = nextSrc;
        dst = nextDst;
    }
}

template<DrawBlendOp TBlendOp, int8_t TZoom>
static void FASTCALL DrawBMPSpriteMinify(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    constexpr auto zoomLevel = ZoomLevel{ TZoom };
    constexpr auto zoom = zoomLevel.ApplyTo(1);

    auto& g1 = args.SourceImage;
    auto src = g1.offset + ((static_cast<size_t>(g1.width) * args.SrcY) + args.SrcX);
    auto dst = args.DestinationBits;
    auto& paletteMap = args.PalMap;
    auto width = args.Width;
    auto height = args.Height;
    size_t srcLineWidth = zoomLevel.ApplyTo(g1.width);
    size_t dstLineWidth = zoomLevel.ApplyInversedTo(static_cast<size_t>(dpi.width)) + dpi.pitch;
    for (; height > 0; height -= zoom)
    {
        auto nextSrc = src + srcLineWidth;
        auto nextDst = dst + dstLineWidth;
        for (int32_t widthRemaining = width; widthRemaining > 0; widthRemaining -= zoom, src += zoom, dst++)
        {
            BlitPixel<TBlendOp>(src, dst, paletteMap);
        }
        src = nextSrc;
        dst = nextDst;
    }
}

template<DrawBlendOp TBlendOp, int8_t TZoom> static void FASTCALL DrawBMPSprite(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    if constexpr (ZoomLevel{ TZoom } < ZoomLevel{ 0 })
    {
        DrawBMPSpriteMagnify<TBlendOp, TZoom>(dpi, args);
    }
    else
    {
        DrawBMPSpriteMinify<TBlendOp, TZoom>(dpi, args);
    }
}

template<int8_t TZoom>
void GfxBmpSpriteToBufferImpl(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    auto imageId = args.Image;

    // Image uses the palette pointer to remap the colours of the image
    if (imageId.HasPrimary())
    {
        if (imageId.IsBlended())
        {
            // Copy non-transparent bitmap data but blend src and dst pixel using the palette map.
            DrawBMPSprite<BLEND_TRANSPARENT | BLEND_SRC | BLEND_DST, TZoom>(dpi, args);
        }
        else
        {
            // Copy non-transparent bitmap data but re-colour using the palette map.
            DrawBMPSprite<BLEND_TRANSPARENT | BLEND_SRC, TZoom>(dpi, args);
        }
    }
    else if (imageId.IsBlended())
    {
        // Image is only a transparency mask. Just colour the pixels using the palette map.
        // Used for glass.
        DrawBMPSprite<BLEND_TRANSPARENT | BLEND_DST, TZoom>(dpi, args);
    }
    else if (!(args.SourceImage.flags & G1_FLAG_HAS_TRANSPARENCY))
    {
        // Copy raw bitmap data to target
        DrawBMPSprite<BLEND_NONE, TZoom>(dpi, args);
    }
    else
    {
        // Copy raw bitmap data to target but exclude transparent pixels
        DrawBMPSprite<BLEND_TRANSPARENT, TZoom>(dpi, args);
    }
}

/**
 * Copies a sprite onto the buffer. There is no compression used on the sprite
 * image.
 *  rct2: 0x0067A690
 * @param imageId Only flags are used.
 */
void FASTCALL GfxBmpSpriteToBuffer(DrawPixelInfo& dpi, const DrawSpriteArgs& args)
{
    switch (static_cast<int8_t>(dpi.zoom_level))
    {
        case -2:
            return GfxBmpSpriteToBufferImpl<-2>(dpi, args);
        case -1:
            return GfxBmpSpriteToBufferImpl<-1>(dpi, args);
        case 0:
            return GfxBmpSpriteToBufferImpl<0>(dpi, args);
        case 1:
            return GfxBmpSpriteToBufferImpl<1>(dpi, args);
        case 2:
            return GfxBmpSpriteToBufferImpl<2>(dpi, args);
        case 3:
            return GfxBmpSpriteToBufferImpl<3>(dpi, args);
    }
}
