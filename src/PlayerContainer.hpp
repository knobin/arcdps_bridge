//
//  src/PlayerContainer.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PLAYERCONTAINER_HPP
#define BRIDGE_PLAYERCONTAINER_HPP

// C++ Headers
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

class PlayerContainer
{
public:
    struct PlayerInfo
    {
        std::string accountName{};
        std::string characterName{};
        long long joinTime{};
        uint32_t profession{};
        uint32_t elite{};
        uint8_t role{};
        uint8_t subgroup{};
        bool inInstance{false};

        std::string toJSON() const;
    };

    struct PlayerInfoEntry
    {
        PlayerInfo player{};
        std::size_t validator{};
    };

    enum class Status : int
    {
        Invalid = 0,    // Operation can never be successful.
        ValidatorError, // Invalid validator.
        ExistsError,    // Operation could not be completed because item already exists.
        Success         // Operation was successful.
    };

    struct PlayerInfoUpdate
    {
        std::optional<PlayerInfoEntry> entry;
        Status status{Status::Invalid};
    };

public:
    Status add(const PlayerInfo& player);
    PlayerInfoUpdate update(const PlayerInfoEntry& playerEntry);
    std::optional<PlayerContainer::PlayerInfo> remove(const std::string& accountName);
    std::optional<PlayerInfoEntry> find(const std::string& accountName) const;

    void clear();

    std::string toJSON() const;

private:
    std::array<std::pair<bool, PlayerInfoEntry>, 50> m_squad{};
    mutable std::mutex m_mutex{};
};

#endif // BRIDGE_PLAYERCONTAINER_HPP