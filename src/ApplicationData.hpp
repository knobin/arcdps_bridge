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

// C++ Headers
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>

struct BridgeInfo
{
    std::string_view version{"1.0.2"};  // Bridge version.
    std::string extrasVersion{""};      // Unofficial Extras version.
    std::string arcvers{""};            // ArcDPS version.
    bool arcLoaded{false};              // Is ArcDPS loaded (enabled in the bridge).
    bool extrasFound{false};            // Has the Unofficial Extras init callback been called.
    bool extrasLoaded{false};           // Is Unofficial Extras loaded (enabled in the bridge).
};

struct Configs
{
    // General.
    bool enabled{true};            // Should the extension be enabled.
    bool arcDPS{true};             // Should ArcDPS be used.
    bool extras{true};             // Should the Unofficial Extras be used.

    // Server.
    std::size_t maxClients{128};            // Max amount of clinets at one time.
    std::size_t clientTimeoutTimer{300000}; // Check if client disconnected after specified amount of milliseconds.
    std::size_t msgQueueSize{500};          // How many messages can be queued before being dropped.

    void set(const std::string header, const std::string& entry, const std::string& value);
private:
    template<typename T>
    std::optional<T> StringTo(const std::string& str)
    {
        T value;
        std::istringstream iss(str);
        iss >> value;
        if (iss.fail())
            return std::nullopt;
        return value;
    }
};

struct CharacterType
{
    uint32_t profession{};
    uint32_t elite{};
};

struct ApplicationData
{
    PlayerContainer Squad{};
    PlayerContainer::PlayerInfo Self{};

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
std::string BridgeInfoToJSON(const BridgeInfo& info);

enum class MessageType : uint8_t
{
    NONE = 0,
    Combat = 1,
    Extras = 2,
    Squad = 4
};

#endif // BRIDGE_APPLICATIONDATA_HPP
