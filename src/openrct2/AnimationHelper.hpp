#pragma once

#include <cstdint>

namespace OpenRCT2
{
    template<uint32_t TFrameCount, uint32_t TFrameTimeMs> class AnimationHelper
    {
        static constexpr float _frameTime = static_cast<float>(TFrameTimeMs) / 1000.0f;

        uint32_t _frame{};
        float _accumulator{};

    public:
        constexpr void Reset() noexcept
        {
            _frame = 0;
            _accumulator = 0.0f;
        }

        constexpr void Update(float frameTime) noexcept
        {
            _accumulator += frameTime;
            // In order to keep the frame time consistent, we need to keep track of the
            // time that has passed since the last frame. If we don't do this, the animation
            // will run faster or slower depending on the frame rate.
            while (_accumulator >= _frameTime)
            {
                _accumulator -= _frameTime;
                _frame = (_frame + 1) % TFrameCount;
            }
        }

        constexpr uint32_t GetFrame() const noexcept
        {
            return _frame;
        }
    };

} // namespace OpenRCT2
