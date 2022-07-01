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
#include <string>
#include <unordered_map>

struct BridgeInfo
{
    std::string_view version{"1.0.1"}; // Bridge version.
    std::string extrasVersion{""};      // Unofficial Extras version.
    std::string arcvers{""};           // ArcDPS version.
    bool arcLoaded{false};             // Is ArcDPS loaded.
    bool extrasLoaded{false};           // Is Unofficial Extras loaded.
};

struct Configs
{
    bool enabled{true};            // Should the extension be enabled.
    bool arcDPS{true};             // Should ArcDPS be used.
    bool extras{true};             // Should the Unofficial Extras be used.
    std::size_t msgQueueSize{500}; // How many messages can be queued before being dropped.
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
