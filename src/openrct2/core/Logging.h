/*****************************************************************************
 * Copyright (c) 2014-2019 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#pragma once

#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace Logging
{
    enum class Level : size_t
    {
        None = 0,
        Basic,
        Minimal,
        Verbose
    };

    class Group
    {
    public:
        Group() = delete;
        Group(const char* name, Level defaultLevel = Level::Minimal)
            : _name(name)
        {
            initialize(defaultLevel);
        }

        const std::string& Name() const
        {
            return _name;
        }

        size_t Index() const
        {
            return _index;
        }

    private:
        void initialize(Level defaultLevel);

    private:
        std::string _name;
        size_t _index;
    };

    class Instance
    {
        friend class Group;

    public:
        bool open(const char* file, bool append = false)
        {
            close();
#ifdef _WIN32
            if (fopen_s(&_fp, file, append ? "at" : "wt") != 0)
            {
                _fp = nullptr;
            }
#else
            _fp = fopen(file, append ? "at" : "wt");
#endif
            return _fp != nullptr;
        }

        void close()
        {
            if (_fp)
                fclose(_fp);
            _fp = nullptr;
        }

        void setLevel(const Group& group, Level level)
        {
            assert(group.Index() < _levels.size());
            _levels[group.Index()] = level;
        }

        const std::vector<std::string>& getGroups() const
        {
            return _groups;
        }

        const std::vector<Level>& getLevels() const
        {
            return _levels;
        }

        template<typename... Args> void log(const Group& group, const char* fmt, Args... args) const
        {
            assert(group.Index() < _levels.size());

            const Level groupLevel = _levels[group.Index()];
            if (groupLevel == Level::None)
                return;

            logToHandle(stdout, fmt, std::forward<Args>(args)...);
            if (_fp != nullptr)
            {
                logToHandle(_fp, fmt, std::forward<Args>(args)...);
            }
        }

        template<typename... Args> void log(const Group& group, Level level, const char* fmt, Args... args) const
        {
            assert(group.Index() < _levels.size());

            const Level groupLevel = _levels[group.Index()];
            if (level > groupLevel)
                return;

            logToHandle(stdout, fmt, std::forward<Args>(args)...);
            if (_fp != nullptr)
            {
                logToHandle(_fp, fmt, std::forward<Args>(args)...);
            }
        }

        template<typename... Args> void logToHandle(FILE* h, const char* fmt, Args... args) const
        {
            if (h == nullptr)
                return;
            fprintf(h, fmt, std::forward<Args>(args)...);
        }

    private:
        std::vector<std::string> _groups;
        std::vector<Level> _levels;
        FILE* _fp = nullptr;
    };

    extern Instance* gLogInstance;

    void initialize();
    void shutdown();

    inline bool open(const char* file, bool append = false)
    {
        return gLogInstance->open(file, append);
    }

    inline const std::vector<std::string>& getGroups()
    {
        return gLogInstance->getGroups();
    }

    inline const std::vector<Level>& getLevels()
    {
        return gLogInstance->getLevels();
    }

    inline void setLevel(const Group& group, Level level)
    {
        gLogInstance->setLevel(group, level);
    }

    template<typename... Args> inline void log(const Group& group, const char* fmt, Args... args)
    {
        gLogInstance->log(group, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args> inline void logMinimal(const Group& group, const char* fmt, Args... args)
    {
        gLogInstance->log(group, Level::Minimal, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args> inline void logVerbose(const Group& group, const char* fmt, Args... args)
    {
        gLogInstance->log(group, Level::Verbose, fmt, std::forward<Args>(args)...);
    }

} // namespace Logging
