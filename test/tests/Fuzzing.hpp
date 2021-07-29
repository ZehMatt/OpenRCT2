/*****************************************************************************
 * Copyright (c) 2014-2021 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <cstdint>
#include <random>
#include <vector>

namespace OpenRCT2::Fuzzing
{
    enum class ExceptionType
    {
        Breakpoint,
        AccessViolation,
    };

    using CrashHandler = void (*)(uintptr_t Address, ExceptionType exType, uintptr_t MemoryAddress);

    struct InputState
    {
        size_t numBits = 0;
        size_t bitIndex = 0;
        std::vector<uint8_t> raw;
    };

    bool MutateInput(InputState& input, std::mt19937_64& prng);

    struct ChildProcess;

    ChildProcess* SpawnDebugChild(const char* args);

    bool UpdateChild(ChildProcess* process, const CrashHandler& handler);

    bool FinishProcess(ChildProcess* process);

} // namespace OpenRCT2::Fuzzing
