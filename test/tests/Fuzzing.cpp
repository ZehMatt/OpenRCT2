#include "Fuzzing.hpp"
#include <windows.h>
#include <cstdio>

namespace OpenRCT2::Fuzzing
{
    struct ChildProcess
    {
        STARTUPINFOA si{};
        PROCESS_INFORMATION pi{};
        bool hitSystemBP{};
    };

    bool MutateInput(InputState& input, std::mt19937_64& prng)
    {
        enum class MutationType
        {
            BitFlip,
            ByteRand,
            InsertByte,
            RemoveByte,
        };

        if (input.raw.empty())
        {
            input.raw.push_back(static_cast<uint8_t>(prng() % 0xFF));
            return true;
        }

        std::discrete_distribution<> d({ 1000, 500, 1, 1 });
        auto mutationType = static_cast<MutationType>(d(prng));

        if (mutationType == MutationType::RemoveByte && input.raw.size() <= 1)
            mutationType = MutationType::BitFlip;
        if (mutationType == MutationType::InsertByte && input.raw.size() >= 1024)
            mutationType = MutationType::BitFlip;

        if (mutationType == MutationType::BitFlip)
        {
            auto byteIndex = prng() % input.raw.size();
            auto bitIndex = prng() % 8;

            input.raw[byteIndex] ^= static_cast<uint8_t>(1 << bitIndex);
        }
        else if (mutationType == MutationType::ByteRand)
        {
            auto byteIndex = prng() % input.raw.size();

            input.raw[byteIndex] = static_cast<uint8_t>(prng() % 0xFF);
        }
        else if (mutationType == MutationType::InsertByte)
        {
            auto byteIndex = prng() % input.raw.size();

            input.raw.insert(input.raw.begin() + byteIndex, static_cast<uint8_t>(prng() % 0xFF));
        }
        else if (mutationType == MutationType::RemoveByte)
        {
            auto byteIndex = prng() % input.raw.size();

            input.raw.erase(input.raw.begin() + byteIndex);
        }

        return true;
    }

    ChildProcess* SpawnDebugChild(const char* arguments)
    {
        ChildProcess* child = new ChildProcess();
        child->si.cb = sizeof(child->si);

        char commandLine[512];
        sprintf_s(commandLine, "openrct2 %s", arguments);

        if (CreateProcessA(nullptr, commandLine, nullptr, nullptr, FALSE, DEBUG_PROCESS, nullptr, nullptr, &child->si, &child->pi) == FALSE)
        {
            delete child;
            return nullptr;
        }

        return child;
    }

    enum DebugContinueType
    {
        Resume,
        Terminate,
        Unhandled,
    };

    static DebugContinueType HandleException(ChildProcess* process, DEBUG_EVENT& ev, const CrashHandler& handler)
    {
        auto& exInfo = ev.u.Exception;
        auto& exRecord = exInfo.ExceptionRecord;

        if (exInfo.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
        {
            if (process->hitSystemBP != false)
            {
                handler(reinterpret_cast<uintptr_t>(exRecord.ExceptionAddress), ExceptionType::Breakpoint, 0);
                return DebugContinueType::Unhandled;
            }
            else
            {
                process->hitSystemBP = true;
                return DebugContinueType::Resume;
            }
        }
        else if (exInfo.ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        {
            const uintptr_t memAddress = exRecord.ExceptionInformation[1];
            handler(reinterpret_cast<uintptr_t>(exRecord.ExceptionAddress), ExceptionType::AccessViolation, memAddress);

            if (exInfo.dwFirstChance == 0)
                return DebugContinueType::Terminate;
            else
                return DebugContinueType::Unhandled;
        }

        return DebugContinueType::Resume;
    }

    bool UpdateChild(ChildProcess* process, const CrashHandler& handler)
    {
        DEBUG_EVENT ev{};

        if (WaitForDebugEvent(&ev, 0) == FALSE)
            return true;

        DebugContinueType continueType = DebugContinueType::Resume;

        switch (ev.dwDebugEventCode)
        {
        case EXIT_PROCESS_DEBUG_EVENT:
            continueType = DebugContinueType::Terminate;
            break;
        case EXCEPTION_DEBUG_EVENT:
            continueType = HandleException(process, ev, handler);
            break;
        }

        if (continueType == DebugContinueType::Terminate)
        {
            // Terminated.
            return false;
        }

        auto resumeStatus = DBG_CONTINUE;

        if (continueType == DebugContinueType::Unhandled)
            resumeStatus = DBG_EXCEPTION_NOT_HANDLED; // Pass to application
        else if (continueType == DebugContinueType::Resume)
            resumeStatus = DBG_CONTINUE; // Resume normally.

        if (ContinueDebugEvent(ev.dwProcessId, ev.dwThreadId, resumeStatus) == FALSE)
        {
            return false;
        }

        return true;
    }

    bool FinishProcess(ChildProcess* process)
    {
        CloseHandle(process->pi.hThread);
        CloseHandle(process->pi.hProcess);

        return false;
    }
}
