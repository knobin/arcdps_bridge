//
//  src/PlayerContainer.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PLAYERCONTAINER_HPP
#define BRIDGE_PLAYERCONTAINER_HPP

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

// C++ Headers
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

#define PLAYER_VALIDATOR_START 1

struct PlayerInfo
{
    std::string accountName{};
    std::string characterName{};
    long long joinTime{};
    uint32_t profession{};
    uint32_t elite{};
    uint8_t role{static_cast<uint8_t>(UserRole::None)};
    uint8_t subgroup{};
    bool inInstance{false};

    std::string toJSON() const;
};

struct PlayerInfoEntry
{
    PlayerInfo player{};
    std::size_t validator{};
};

class PlayerContainer
{
public:
    enum class Status : int
    {
        Invalid = 0,    // Operation can never be successful.
        ValidatorError, // Invalid validator.
        ExistsError,    // Operation could not be completed because item already exists.
        Equal,          // New value to update with is the same is the old value. Increases validator.
        Success         // Operation was successful. Increases validator.
    };

    struct PlayerInfoUpdate
    {
        std::optional<PlayerInfoEntry> entry;
        Status status{Status::Invalid};
    };

public:
    Status add(const PlayerInfo& player);
    PlayerInfoUpdate update(const PlayerInfoEntry& playerEntry);
    std::optional<PlayerInfoEntry> remove(const std::string& accountName);
    std::optional<PlayerInfoEntry> find(const std::string& accountName) const;

    template<typename UnaryPredicate>
    std::optional<PlayerInfoEntry> find_if(UnaryPredicate p)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        auto it = std::find_if(m_squad.cbegin(), m_squad.cend(), p);

        if (it != m_squad.cend())
            return it->second;

        return std::nullopt;
    }

    void clear();

    std::string toJSON() const;

private:
    std::array<std::pair<bool, PlayerInfoEntry>, 50> m_squad{};
    mutable std::mutex m_mutex;
};

bool operator==(const PlayerInfo& lhs, const PlayerInfo& rhs);
bool operator!=(const PlayerInfo& lhs, const PlayerInfo& rhs);

#endif // BRIDGE_PLAYERCONTAINER_HPP