/*****************************************************************************
 * Copyright (c) 2014-2020 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

#include "ObjectManager.h"

#include "../Context.h"
#include "../ParkImporter.h"
#include "../core/Console.hpp"
#include "../core/Memory.hpp"
#include "../localisation/StringIds.h"
#include "../util/Util.h"
#include "FootpathItemObject.h"
#include "LargeSceneryObject.h"
#include "Object.h"
#include "ObjectList.h"
#include "ObjectRepository.h"
#include "RideObject.h"
#include "SceneryGroupObject.h"
#include "SmallSceneryObject.h"
#include "WallObject.h"

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>

class ObjectStorage
{
    ObjectType _type;
    std::vector<std::unique_ptr<Object>> _objects;

public:
    ObjectStorage(ObjectType type, size_t maxSize)
        : _objects(maxSize)
        , _type(type)
    {
    }

    Object* Get(ObjectEntryIndex index) const noexcept
    {
        if (index >= _objects.size())
            return nullptr;

        return _objects[index].get();
    }

    bool AddObject(ObjectEntryIndex index, std::unique_ptr<Object>&& object)
    {
        if (index >= _objects.size())
            _objects.resize(index + 1);

        _objects[index] = std::move(object);
        return true;
    }

    bool AddObject(std::unique_ptr<Object>&& object)
    {
        auto index = GetSpareEntry();
        if (!index)
        {
            _objects.push_back(std::move(object));
            return true;
        }

        _objects[*index] = std::move(object);
        return true;
    }

    std::vector<std::unique_ptr<Object>>& GetAll()
    {
        return _objects;
    }

    std::optional<ObjectEntryIndex> GetSpareEntry() const
    {
        for (size_t i = 0; i < _objects.size(); i++)
        {
            if (_objects[i] == nullptr)
                return static_cast<ObjectEntryIndex>(i);
        }
        return std::nullopt;
    }

    ObjectEntryIndex GetObjectEntryIndex(const Object* object)
    {
        auto it = std::find_if(_objects.begin(), _objects.end(), [object](auto& obj) { return obj.get() == object; });
        if (it == _objects.end())
            return OBJECT_ENTRY_INDEX_NULL;
        return std::distance(_objects.begin(), it);
    }

    void Remove(const Object* object)
    {
        // Because it's possible to have the same loaded object for multiple
        // slots, we have to make sure find and set all of them to nullptr
        for (auto& entry : _objects)
        {
            if (entry.get() != object)
                continue;

            entry.reset();
        }
    }
};

class ObjectManager final : public IObjectManager
{
private:
    IObjectRepository& _objectRepository;
    // std::vector<std::unique_ptr<Object>> _loadedObjects;
    std::array<std::vector<ObjectEntryIndex>, RIDE_TYPE_COUNT> _rideTypeToObjectMap;

    std::array<ObjectStorage, EnumValue(ObjectType::Count)> _storage = {
        ObjectStorage(ObjectType::Ride, MAX_RIDE_OBJECTS),
        ObjectStorage(ObjectType::SmallScenery, MAX_SMALL_SCENERY_OBJECTS),
        ObjectStorage(ObjectType::LargeScenery, MAX_LARGE_SCENERY_OBJECTS),
        ObjectStorage(ObjectType::Walls, MAX_WALL_SCENERY_OBJECTS),
        ObjectStorage(ObjectType::Banners, MAX_BANNER_OBJECTS),
        ObjectStorage(ObjectType::Paths, MAX_PATH_OBJECTS),
        ObjectStorage(ObjectType::PathBits, MAX_PATH_ADDITION_OBJECTS),
        ObjectStorage(ObjectType::SceneryGroup, MAX_SCENERY_GROUP_OBJECTS),
        ObjectStorage(ObjectType::ParkEntrance, MAX_PARK_ENTRANCE_OBJECTS),
        ObjectStorage(ObjectType::Water, MAX_WATER_OBJECTS),
        ObjectStorage(ObjectType::ScenarioText, MAX_SCENARIO_TEXT_OBJECTS),
        ObjectStorage(ObjectType::TerrainSurface, MAX_TERRAIN_SURFACE_OBJECTS),
        ObjectStorage(ObjectType::TerrainEdge, MAX_TERRAIN_EDGE_OBJECTS),
        ObjectStorage(ObjectType::Station, MAX_STATION_OBJECTS),
        ObjectStorage(ObjectType::Music, MAX_MUSIC_OBJECTS),
        ObjectStorage(ObjectType::FootpathSurface, 0),
        ObjectStorage(ObjectType::FootpathRailings, 0),
    };

    // Used to return a safe empty vector back from GetAllRideEntries, can be removed when std::span is available
    std::vector<ObjectEntryIndex> _nullRideTypeEntries;

public:
    explicit ObjectManager(IObjectRepository& objectRepository)
        : _objectRepository(objectRepository)
    {
        //_loadedObjects.resize(OBJECT_ENTRY_COUNT);

        UpdateSceneryGroupIndexes();
        ResetTypeToRideEntryIndexMap();
    }

    ~ObjectManager() override
    {
        UnloadAll();
    }

    Object* GetLoadedObject(ObjectType objectType, ObjectEntryIndex index) override
    {
        auto& storage = GetObjectStorage(objectType);
        return storage.Get(index);
    }

    Object* GetLoadedObject(const ObjectEntryDescriptor& entry) override
    {
        Object* loadedObject = nullptr;
        const ObjectRepositoryItem* ori = _objectRepository.FindObject(entry);
        if (ori != nullptr)
        {
            loadedObject = ori->LoadedObject;
        }
        return loadedObject;
    }

    ObjectEntryIndex GetLoadedObjectEntryIndex(const Object* object) override
    {
        if (object == nullptr)
            return OBJECT_ENTRY_INDEX_NULL;

        auto& storage = GetObjectStorage(object->GetObjectType());
        return storage.GetObjectEntryIndex(object);
    }

    Object* LoadObject(std::string_view identifier) override
    {
        const ObjectRepositoryItem* ori = _objectRepository.FindObject(identifier);
        return RepositoryItemToObject(ori);
    }

    Object* LoadObject(const rct_object_entry* entry) override
    {
        const ObjectRepositoryItem* ori = _objectRepository.FindObject(entry);
        return RepositoryItemToObject(ori);
    }

    void LoadObjects(const rct_object_entry* entries, size_t count) override
    {
        // Find all the required objects
        auto requiredObjects = GetRequiredObjects(entries, count);

        // Unload all the things that are not in the new list.
        // UnloadUnusedObjects(requiredObjects);

        // Load the required objects
        size_t numNewLoadedObjects = 0;
        UnloadAll();
        LoadDefaultObjects();
        LoadObjects(requiredObjects, &numNewLoadedObjects);
        LoadDefaultObjects();
        UpdateSceneryGroupIndexes();
        ResetTypeToRideEntryIndexMap();
        log_verbose("%u / %u new objects loaded", numNewLoadedObjects, requiredObjects.size());
    }

    void UnloadObjects(const std::vector<const ObjectRepositoryItem*>& requiredObjects)
    {
    }

    void UnloadObjects(const std::vector<rct_object_entry>& entries) override
    {
        // TODO there are two performance issues here:
        //        - FindObject for every entry which is a dictionary lookup
        //        - GetLoadedObjectIndex for every entry which enumerates _loadedList

        size_t numObjectsUnloaded = 0;
        for (const auto& entry : entries)
        {
            const ObjectRepositoryItem* ori = _objectRepository.FindObject(&entry);
            if (ori != nullptr)
            {
                Object* loadedObject = ori->LoadedObject;
                if (loadedObject != nullptr)
                {
                    UnloadObject(loadedObject);
                    numObjectsUnloaded++;
                }
            }
        }

        if (numObjectsUnloaded > 0)
        {
            UpdateSceneryGroupIndexes();
            ResetTypeToRideEntryIndexMap();
        }
    }

    void UnloadAll() override
    {
        for (auto& storage : _storage)
        {
            for (auto& object : storage.GetAll())
            {
                UnloadObject(object.get());
            }
        }

        UpdateSceneryGroupIndexes();
        ResetTypeToRideEntryIndexMap();
    }

    void ResetObjects() override
    {
        for (auto& storage : _storage)
        {
            for (auto& object : storage.GetAll())
            {
                object->Unload();
                object->Load();
            }
        }
        UpdateSceneryGroupIndexes();
        ResetTypeToRideEntryIndexMap();
    }

    std::vector<const ObjectRepositoryItem*> GetPackableObjects() override
    {
        std::vector<const ObjectRepositoryItem*> objects;
        size_t numObjects = _objectRepository.GetNumObjects();
        for (size_t i = 0; i < numObjects; i++)
        {
            const ObjectRepositoryItem* item = &_objectRepository.GetObjects()[i];
            if (item->LoadedObject != nullptr && IsObjectCustom(item) && item->LoadedObject->GetLegacyData() != nullptr
                && !item->LoadedObject->IsJsonObject())
            {
                objects.push_back(item);
            }
        }
        return objects;
    }

    void LoadDefaultObjects() override
    {
        // We currently will load new object types here that apply to all
        // loaded RCT1 and RCT2 save files.

        // Surfaces
        LoadObject("rct2.surface.grass");
        LoadObject("rct2.surface.sand");
        LoadObject("rct2.surface.dirt");
        LoadObject("rct2.surface.rock");
        LoadObject("rct2.surface.martian");
        LoadObject("rct2.surface.chequerboard");
        LoadObject("rct2.surface.grassclumps");
        LoadObject("rct2.surface.ice");
        LoadObject("rct2.surface.gridred");
        LoadObject("rct2.surface.gridyellow");
        LoadObject("rct2.surface.gridpurple");
        LoadObject("rct2.surface.gridgreen");
        LoadObject("rct2.surface.sandred");
        LoadObject("rct2.surface.sandbrown");
        LoadObject("rct1.aa.surface.roofred");
        LoadObject("rct1.ll.surface.roofgrey");
        LoadObject("rct1.ll.surface.rust");
        LoadObject("rct1.ll.surface.wood");

        // Edges
        LoadObject("rct2.edge.rock");
        LoadObject("rct2.edge.woodred");
        LoadObject("rct2.edge.woodblack");
        LoadObject("rct2.edge.ice");
        LoadObject("rct1.edge.brick");
        LoadObject("rct1.edge.iron");
        LoadObject("rct1.aa.edge.grey");
        LoadObject("rct1.aa.edge.yellow");
        LoadObject("rct1.aa.edge.red");
        LoadObject("rct1.ll.edge.purple");
        LoadObject("rct1.ll.edge.green");
        LoadObject("rct1.ll.edge.stonebrown");
        LoadObject("rct1.ll.edge.stonegrey");
        LoadObject("rct1.ll.edge.skyscrapera");
        LoadObject("rct1.ll.edge.skyscraperb");

        // Stations
        LoadObject("rct2.station.plain");
        LoadObject("rct2.station.wooden");
        LoadObject("rct2.station.canvastent");
        LoadObject("rct2.station.castlegrey");
        LoadObject("rct2.station.castlebrown");
        LoadObject("rct2.station.jungle");
        LoadObject("rct2.station.log");
        LoadObject("rct2.station.classical");
        LoadObject("rct2.station.abstract");
        LoadObject("rct2.station.snow");
        LoadObject("rct2.station.pagoda");
        LoadObject("rct2.station.space");
        LoadObject("openrct2.station.noentrance");

        // Music
        LoadObject(MUSIC_STYLE_DODGEMS_BEAT, "rct2.music.dodgems");
        LoadObject(MUSIC_STYLE_FAIRGROUND_ORGAN, "rct2.music.fairground");
        LoadObject(MUSIC_STYLE_ROMAN_FANFARE, "rct2.music.roman");
        LoadObject(MUSIC_STYLE_ORIENTAL, "rct2.music.oriental");
        LoadObject(MUSIC_STYLE_MARTIAN, "rct2.music.martian");
        LoadObject(MUSIC_STYLE_JUNGLE_DRUMS, "rct2.music.jungle");
        LoadObject(MUSIC_STYLE_EGYPTIAN, "rct2.music.egyptian");
        LoadObject(MUSIC_STYLE_TOYLAND, "rct2.music.toyland");
        LoadObject(MUSIC_STYLE_SPACE, "rct2.music.space");
        LoadObject(MUSIC_STYLE_HORROR, "rct2.music.horror");
        LoadObject(MUSIC_STYLE_TECHNO, "rct2.music.techno");
        LoadObject(MUSIC_STYLE_GENTLE, "rct2.music.gentle");
        LoadObject(MUSIC_STYLE_SUMMER, "rct2.music.summer");
        LoadObject(MUSIC_STYLE_WATER, "rct2.music.water");
        LoadObject(MUSIC_STYLE_WILD_WEST, "rct2.music.wildwest");
        LoadObject(MUSIC_STYLE_JURASSIC, "rct2.music.jurassic");
        LoadObject(MUSIC_STYLE_ROCK, "rct2.music.rock1");
        LoadObject(MUSIC_STYLE_RAGTIME, "rct2.music.ragtime");
        LoadObject(MUSIC_STYLE_FANTASY, "rct2.music.fantasy");
        LoadObject(MUSIC_STYLE_ROCK_STYLE_2, "rct2.music.rock2");
        LoadObject(MUSIC_STYLE_ICE, "rct2.music.ice");
        LoadObject(MUSIC_STYLE_SNOW, "rct2.music.snow");
        LoadObject(MUSIC_STYLE_CUSTOM_MUSIC_1, "rct2.music.custom1");
        LoadObject(MUSIC_STYLE_CUSTOM_MUSIC_2, "rct2.music.custom2");
        LoadObject(MUSIC_STYLE_MEDIEVAL, "rct2.music.medieval");
        LoadObject(MUSIC_STYLE_URBAN, "rct2.music.urban");
        LoadObject(MUSIC_STYLE_ORGAN, "rct2.music.organ");
        LoadObject(MUSIC_STYLE_MECHANICAL, "rct2.music.mechanical");
        LoadObject(MUSIC_STYLE_MODERN, "rct2.music.modern");
        LoadObject(MUSIC_STYLE_PIRATES, "rct2.music.pirate");
        LoadObject(MUSIC_STYLE_ROCK_STYLE_3, "rct2.music.rock3");
        LoadObject(MUSIC_STYLE_CANDY_STYLE, "rct2.music.candy");
    }

    static rct_string_id GetObjectSourceGameString(const ObjectSourceGame sourceGame)
    {
        switch (sourceGame)
        {
            case ObjectSourceGame::RCT1:
                return STR_SCENARIO_CATEGORY_RCT1;
            case ObjectSourceGame::AddedAttractions:
                return STR_SCENARIO_CATEGORY_RCT1_AA;
            case ObjectSourceGame::LoopyLandscapes:
                return STR_SCENARIO_CATEGORY_RCT1_LL;
            case ObjectSourceGame::RCT2:
                return STR_ROLLERCOASTER_TYCOON_2_DROPDOWN;
            case ObjectSourceGame::WackyWorlds:
                return STR_OBJECT_FILTER_WW;
            case ObjectSourceGame::TimeTwister:
                return STR_OBJECT_FILTER_TT;
            case ObjectSourceGame::OpenRCT2Official:
                return STR_OBJECT_FILTER_OPENRCT2_OFFICIAL;
            default:
                return STR_OBJECT_FILTER_CUSTOM;
        }
    }

    const std::vector<ObjectEntryIndex>& GetAllRideEntries(uint8_t rideType) override
    {
        if (rideType >= RIDE_TYPE_COUNT)
        {
            // Return an empty vector
            return _nullRideTypeEntries;
        }
        return _rideTypeToObjectMap[rideType];
    }

private:
    ObjectStorage& GetObjectStorage(ObjectType type)
    {
        Guard::Assert(EnumValue(type) < _storage.size());
        return _storage[EnumValue(type)];
    }

    Object* LoadObject(ObjectEntryIndex slot, std::string_view identifier)
    {
        const ObjectRepositoryItem* ori = _objectRepository.FindObject(identifier);
        return RepositoryItemToObject(ori, slot);
    }

    Object* RepositoryItemToObject(const ObjectRepositoryItem* ori, std::optional<ObjectEntryIndex> slot = {})
    {
        if (ori == nullptr)
            return nullptr;

        Object* loadedObject = ori->LoadedObject;
        if (loadedObject == nullptr)
        {
            ObjectType objectType = ori->ObjectEntry.GetType();

            auto& storage = GetObjectStorage(objectType);

            if (slot)
            {
                if (storage.Get(*slot) != nullptr)
                {
                    // Slot already taken
                    return nullptr;
                }
            }
            else
            {
                slot = storage.GetSpareEntry();
            }
            if (slot)
            {
                auto object = GetOrLoadObject(ori);
                if (object != nullptr)
                {
                    loadedObject = object.get();
                    storage.AddObject(*slot, std::move(object));

                    UpdateSceneryGroupIndexes();
                    ResetTypeToRideEntryIndexMap();
                }
            }
        }

        return loadedObject;
    }

    void UnloadObject(Object* object)
    {
        if (object == nullptr)
            return;

        const auto objectType = object->GetObjectType();
        object->Unload();

        // TODO try to prevent doing a repository search
        const ObjectRepositoryItem* ori = _objectRepository.FindObject(object->GetObjectEntry());
        if (ori != nullptr)
        {
            _objectRepository.UnregisterLoadedObject(ori, object);
        }

        auto& storage = GetObjectStorage(objectType);
        storage.Remove(object);
    }

    template<ObjectType TObjectType, typename T> void UpdateSceneryGroupIndices()
    {
        auto& storage = GetObjectStorage(TObjectType);
        for (auto& object : storage.GetAll())
        {
            if (object == nullptr)
                continue;

            auto* sceneryEntry = static_cast<T*>(object->GetLegacyData());
            sceneryEntry->scenery_tab_id = GetPrimarySceneryGroupEntryIndex(object.get());
        }
    }

    void UpdateSceneryGroupIndexes()
    {
        UpdateSceneryGroupIndices<ObjectType::SmallScenery, SmallSceneryEntry>();
        UpdateSceneryGroupIndices<ObjectType::LargeScenery, LargeSceneryEntry>();
        UpdateSceneryGroupIndices<ObjectType::Walls, WallSceneryEntry>();
        UpdateSceneryGroupIndices<ObjectType::Banners, BannerSceneryEntry>();
        UpdateSceneryGroupIndices<ObjectType::PathBits, PathBitEntry>();
        // UpdateSceneryGroupIndices<ObjectType::SceneryGroup, SceneryGroupObject>();

        auto& storage = GetObjectStorage(ObjectType::SceneryGroup);
        for (auto& object : storage.GetAll())
        {
            if (object == nullptr)
                continue;

            auto sgObject = dynamic_cast<SceneryGroupObject*>(object.get());
            sgObject->UpdateEntryIndexes();
        }

        // HACK Scenery window will lose its tabs after changing the scenery group indexing
        //      for now just close it, but it will be better to later tell it to invalidate the tabs
        window_close_by_class(WC_SCENERY);
    }

    ObjectEntryIndex GetPrimarySceneryGroupEntryIndex(Object* loadedObject)
    {
        auto sceneryObject = dynamic_cast<SceneryObject*>(loadedObject);
        const auto& primarySGEntry = sceneryObject->GetPrimarySceneryGroup();
        Object* sgObject = GetLoadedObject(primarySGEntry);

        auto entryIndex = OBJECT_ENTRY_INDEX_NULL;
        if (sgObject != nullptr)
        {
            entryIndex = GetLoadedObjectEntryIndex(sgObject);
        }
        return entryIndex;
    }

    rct_object_entry* DuplicateObjectEntry(const rct_object_entry* original)
    {
        rct_object_entry* duplicate = Memory::Allocate<rct_object_entry>(sizeof(rct_object_entry));
        duplicate->checksum = original->checksum;
        strncpy(duplicate->name, original->name, 8);
        duplicate->flags = original->flags;
        return duplicate;
    }

    std::vector<rct_object_entry> GetInvalidObjects(const rct_object_entry* entries) override
    {
        std::vector<rct_object_entry> invalidEntries;
        invalidEntries.reserve(OBJECT_ENTRY_COUNT);
        for (int32_t i = 0; i < OBJECT_ENTRY_COUNT; i++)
        {
            auto entry = entries[i];
            const ObjectRepositoryItem* ori = nullptr;
            if (object_entry_is_empty(&entry))
            {
                entry = {};
                continue;
            }

            ori = _objectRepository.FindObject(&entry);
            if (ori == nullptr)
            {
                if (entry.GetType() != ObjectType::ScenarioText)
                {
                    invalidEntries.push_back(entry);
                    ReportMissingObject(&entry);
                }
                else
                {
                    entry = {};
                    continue;
                }
            }
            else
            {
                auto loadedObject = ori->LoadedObject;
                if (loadedObject == nullptr)
                {
                    auto object = _objectRepository.LoadObject(ori);
                    if (object == nullptr)
                    {
                        invalidEntries.push_back(entry);
                        ReportObjectLoadProblem(&entry);
                    }
                }
            }
        }
        return invalidEntries;
    }

    std::vector<const ObjectRepositoryItem*> GetRequiredObjects(const rct_object_entry* entries, size_t count)
    {
        std::vector<const ObjectRepositoryItem*> requiredObjects;
        std::vector<rct_object_entry> missingObjects;

        for (size_t i = 0; i < count; i++)
        {
            const rct_object_entry* entry = &entries[i];
            const ObjectRepositoryItem* ori = nullptr;
            if (!object_entry_is_empty(entry))
            {
                ori = _objectRepository.FindObject(entry);
                if (ori == nullptr && entry->GetType() != ObjectType::ScenarioText)
                {
                    missingObjects.push_back(*entry);
                    ReportMissingObject(entry);
                }
            }
            requiredObjects.push_back(ori);
        }

        if (!missingObjects.empty())
        {
            throw ObjectLoadException(std::move(missingObjects));
        }

        return requiredObjects;
    }

    template<typename T, typename TFunc> static void ParallelFor(const std::vector<T>& items, TFunc func)
    {
        auto partitions = std::thread::hardware_concurrency();
        auto partitionSize = (items.size() + (partitions - 1)) / partitions;
        std::vector<std::thread> threads;
        for (size_t n = 0; n < partitions; n++)
        {
            auto begin = n * partitionSize;
            auto end = std::min(items.size(), begin + partitionSize);
            threads.emplace_back(
                [func](size_t pbegin, size_t pend) {
                    for (size_t i = pbegin; i < pend; i++)
                    {
                        func(i);
                    }
                },
                begin, end);
        }
        for (auto& t : threads)
        {
            t.join();
        }
    }

    bool LoadObjects(std::vector<const ObjectRepositoryItem*>& requiredObjects, size_t* outNewObjectsLoaded)
    {
        std::vector<Object*> loadedObjects;
        loadedObjects.reserve(OBJECT_ENTRY_COUNT);

        std::vector<rct_object_entry> badObjects;

        // Read objects
        for (size_t i = 0; i < requiredObjects.size(); i++)
        {
            auto requiredObject = requiredObjects[i];
            std::unique_ptr<Object> object;
            if (requiredObject != nullptr)
            {
                auto loadedObject = requiredObject->LoadedObject;
                if (loadedObject != nullptr)
                    continue;

                if (loadedObject == nullptr)
                {
                    // Object requires to be loaded, if the object successfully loads it will register it
                    // as a loaded object otherwise placed into the badObjects list.
                    object = _objectRepository.LoadObject(requiredObject);
                    if (object == nullptr)
                    {
                        badObjects.push_back(requiredObject->ObjectEntry);
                        ReportObjectLoadProblem(&requiredObject->ObjectEntry);
                    }
                    else
                    {
                        loadedObjects.push_back(object.get());

                        // Connect the ori to the registered object
                        _objectRepository.RegisterLoadedObject(requiredObject, object.get());

                        auto& storage = GetObjectStorage(object->GetObjectType());
                        storage.AddObject(std::move(object));
                    }
                }
            }
        }

        // Load objects
        for (auto obj : loadedObjects)
        {
            obj->Load();
        }

        if (!badObjects.empty())
        {
            // Unload all the new objects we loaded
            for (auto object : loadedObjects)
            {
                UnloadObject(object);
            }
            throw ObjectLoadException(std::move(badObjects));
        }

        if (outNewObjectsLoaded != nullptr)
        {
            *outNewObjectsLoaded = loadedObjects.size();
        }

        return true;
    }

    std::unique_ptr<Object> GetOrLoadObject(const ObjectRepositoryItem* ori)
    {
        std::unique_ptr<Object> object;
        auto loadedObject = ori->LoadedObject;
        if (loadedObject == nullptr)
        {
            // Try to load object
            object = _objectRepository.LoadObject(ori);
            if (object != nullptr)
            {
                object->Load();

                // Connect the ori to the registered object
                _objectRepository.RegisterLoadedObject(ori, object.get());
            }
        }
        return object;
    }

    void ResetTypeToRideEntryIndexMap()
    {
        // Clear all ride objects
        for (auto& v : _rideTypeToObjectMap)
        {
            v.clear();
        }

        // Build object lists
        auto maxRideObjects = static_cast<size_t>(object_entry_group_counts[EnumValue(ObjectType::Ride)]);
        for (uint16_t i = 0; i < maxRideObjects; i++)
        {
            auto rideObject = static_cast<RideObject*>(GetLoadedObject(ObjectType::Ride, i));
            if (rideObject != nullptr)
            {
                const auto entry = static_cast<rct_ride_entry*>(rideObject->GetLegacyData());
                if (entry != nullptr)
                {
                    for (auto rideType : entry->ride_type)
                    {
                        if (rideType < _rideTypeToObjectMap.size())
                        {
                            auto& v = _rideTypeToObjectMap[rideType];
                            v.push_back(static_cast<ObjectEntryIndex>(i));
                        }
                    }
                }
            }
        }
    }

    static void ReportMissingObject(const rct_object_entry* entry)
    {
        utf8 objName[DAT_NAME_LENGTH + 1] = { 0 };
        std::copy_n(entry->name, DAT_NAME_LENGTH, objName);
        Console::Error::WriteLine("[%s] Object not found.", objName);
    }

    void ReportObjectLoadProblem(const rct_object_entry* entry)
    {
        utf8 objName[DAT_NAME_LENGTH + 1] = { 0 };
        std::copy_n(entry->name, DAT_NAME_LENGTH, objName);
        Console::Error::WriteLine("[%s] Object could not be loaded.", objName);
    }

    static ObjectHandle GetObjectHandleByType(ObjectType objectType, ObjectEntryIndex entryIndex)
    {
        auto result = static_cast<std::underlying_type_t<ObjectHandle>>(0);
        for (uint32_t i = 0; i < EnumValue(objectType); i++)
        {
            result += object_entry_group_counts[i];
        }
        result += static_cast<std::underlying_type_t<ObjectHandle>>(entryIndex);
        return static_cast<ObjectHandle>(result);
    }
};

std::unique_ptr<IObjectManager> CreateObjectManager(IObjectRepository& objectRepository)
{
    return std::make_unique<ObjectManager>(objectRepository);
}

Object* object_manager_get_loaded_object(const ObjectEntryDescriptor& entry)
{
    auto& objectManager = OpenRCT2::GetContext()->GetObjectManager();
    Object* loadedObject = objectManager.GetLoadedObject(entry);
    return loadedObject;
}

ObjectEntryIndex object_manager_get_loaded_object_entry_index(const Object* loadedObject)
{
    auto& objectManager = OpenRCT2::GetContext()->GetObjectManager();
    auto entryIndex = objectManager.GetLoadedObjectEntryIndex(loadedObject);
    return entryIndex;
}

ObjectEntryIndex object_manager_get_loaded_object_entry_index(const ObjectEntryDescriptor& entry)
{
    return object_manager_get_loaded_object_entry_index(object_manager_get_loaded_object(entry));
}

Object* object_manager_load_object(const rct_object_entry* entry)
{
    auto& objectManager = OpenRCT2::GetContext()->GetObjectManager();
    Object* loadedObject = objectManager.LoadObject(entry);
    return loadedObject;
}

void object_manager_unload_objects(const std::vector<rct_object_entry>& entries)
{
    auto& objectManager = OpenRCT2::GetContext()->GetObjectManager();
    objectManager.UnloadObjects(entries);
}

void object_manager_unload_all_objects()
{
    auto& objectManager = OpenRCT2::GetContext()->GetObjectManager();
    objectManager.UnloadAll();
}

rct_string_id object_manager_get_source_game_string(const ObjectSourceGame sourceGame)
{
    return ObjectManager::GetObjectSourceGameString(sourceGame);
}
