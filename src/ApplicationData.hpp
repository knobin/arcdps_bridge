//
//  src/ApplicationData.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_APPLICATIONDATA_HPP
#define BRIDGE_APPLICATIONDATA_HPP

// Local Headers
#include "PlayerContainer.hpp"

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// C++ Headers
#include <cstddef>
#include <optional>
#include <mutex>
#include <string>
#include <unordered_map>

#ifdef BRIDGE_BUILD_DEBUG
    #define BRIDGE_BUILD_STR "DEBUG"
#else
    #define BRIDGE_BUILD_STR "RELEASE"
#endif

struct BridgeInfo
{
    std::string_view version{BRIDGE_VERSION_STR};  // Bridge version.
    std::string extrasVersion{""};      // Unofficial Extras version.
    std::string arcvers{""};            // ArcDPS version.

    uint64_t validator{1};              // Version of the BridgeInfo, if any value changes this will be incremented.
    
    bool arcLoaded{false};              // Is ArcDPS loaded (enabled in the bridge).
    bool extrasFound{false};            // Has the Unofficial Extras init callback been called.
    bool extrasLoaded{false};           // Is Unofficial Extras loaded (enabled in the bridge).

    mutable std::mutex mutex;
};

void to_json(nlohmann::json& j, const BridgeInfo& info);

inline std::size_t serial_size(const BridgeInfo& info)
{
    return info.version.size() + info.extrasVersion.size() + info.arcvers.size() + 3 + (3 * sizeof(uint8_t)) +
           sizeof(info.validator);
}

inline void to_serial(const BridgeInfo& info, uint8_t* storage, std::size_t)
{
    uint8_t* location = storage;

    std::memcpy(location, &info.version[0], info.version.size());
    location[info.version.size()] = '\0';
    location += info.version.size() + 1;

    std::memcpy(location, &info.extrasVersion[0], info.extrasVersion.size());
    location[info.extrasVersion.size()] = '\0';
    location += info.extrasVersion.size() + 1;

    std::memcpy(location, &info.arcvers[0], info.arcvers.size());
    location[info.arcvers.size()] = '\0';
    location += info.arcvers.size() + 1;

    location = serial_w_integral(location, info.validator);
    
    location[0] = static_cast<uint8_t>(info.arcLoaded);
    location[1] = static_cast<uint8_t>(info.arcLoaded);
    location[2] = static_cast<uint8_t>(info.arcLoaded);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

struct Configs
{
    // General.
    bool enabled{true};            // Should the extension be enabled.
    bool arcDPS{true};             // Should ArcDPS be used.
    bool extras{true};             // Should the Unofficial Extras be used.

    // Server.
    std::size_t maxClients{32};             // Max amount of clinets at one time.
    std::size_t clientTimeoutTimer{120000}; // Check if client disconnected after specified amount of milliseconds.
    std::size_t msgQueueSize{64};           // How many messages can be queued before being dropped.

    void set(const std::string header, const std::string& entry, const std::string& value);
private:
    template<typename T>
    std::optional<T> StringTo(const std::string& str)
    {
        T value{};
        std::istringstream iss(str);
        iss >> value;
        if (iss.fail())
            return std::nullopt;
        return value;
    }
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct CharacterType
{
    uint32_t profession{};
    uint32_t elite{};
};

///////////////////////////////////////////////////////////////////////////////////////////////////

struct ApplicationData
{
    PlayerContainer Squad{};
    PlayerInfo Self{};

    std::unordered_map<std::string, CharacterType> CharacterTypeCache{};

    Configs Config{};
    BridgeInfo Info{};

    std::string_view ConfigFile{"arcdps_bridge.ini"};
    std::string_view LogFile{"arcdps_bridge.log"};
    std::string_view PipeName{"\\\\.\\pipe\\arcdps-bridge"};
};

Configs InitConfigs(const std::string& filepath);
void CreateConfigFile(const std::string& filepath);
Configs LoadConfigFile(const std::string& filepath);

#endif // BRIDGE_APPLICATIONDATA_HPP
