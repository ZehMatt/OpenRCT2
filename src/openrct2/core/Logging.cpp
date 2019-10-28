#include "Logging.h"

#include <algorithm>

namespace Logging
{
    Instance* gLogInstance = nullptr;

    void initialize()
    {
        if (gLogInstance)
            return;
        gLogInstance = new Instance();
    }

    void shutdown()
    {
        if (!gLogInstance)
            return;

        delete gLogInstance;
        gLogInstance = nullptr;
    }

    void Group::initialize(Level defaultLevel)
    {
        Logging::initialize();

        auto it = std::find(gLogInstance->_groups.begin(), gLogInstance->_groups.end(), _name);

        size_t groupIndex;
        if (it == gLogInstance->_groups.end())
        {
            // Allocate group.
            groupIndex = gLogInstance->_groups.size();

            gLogInstance->_groups.resize(groupIndex + 1);
            gLogInstance->_levels.resize(groupIndex + 1);

            gLogInstance->_groups[groupIndex] = _name;
            gLogInstance->_levels[groupIndex] = defaultLevel;
        }
        else
        {
            groupIndex = std::distance(gLogInstance->_groups.begin(), it);
        }

        _index = groupIndex;
    }

} // namespace Logging
