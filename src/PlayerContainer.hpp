//
//  src/PlayerContainer.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PLAYERCONTAINER_HPP
#define BRIDGE_PLAYERCONTAINER_HPP

// Local Headers
#include "Message.hpp"

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
    int64_t joinTime{};
    uint32_t profession{};
    uint32_t elite{};
    uint8_t role{static_cast<uint8_t>(UserRole::None)};
    uint8_t subgroup{};
    bool inInstance{false};
    bool self{false};
    bool readyStatus{false};
};

struct PlayerInfoEntry
{
    PlayerInfo player{};
    uint64_t validator{};
};

class PlayerContainer
{
public:
    enum class Status : uint8_t
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

        auto it = std::find_if(m_squad.cbegin(), m_squad.cend(), [&p](const auto& entry) { 
            return entry.first && p(entry.second.player);
        });

        if (it != m_squad.cend())
            return it->second;

        return std::nullopt;
    }

    void clear();

    nlohmann::json toJSON() const;
    SerialData toSerial(std::size_t startPadding = 0) const;

private:
    std::array<std::pair<bool, PlayerInfoEntry>, 65> m_squad{};
    mutable std::mutex m_mutex;
};

constexpr std::size_t PlayerInfo_partial_size = sizeof(PlayerInfo::joinTime) + sizeof(PlayerInfo::profession) +
                                                sizeof(PlayerInfo::elite) + sizeof(PlayerInfo::role) +
                                                sizeof(PlayerInfo::subgroup) + (3 * sizeof(uint8_t));

inline std::size_t serial_size(const PlayerInfo& player)
{
    const std::size_t acc_str_count{1 + player.accountName.size()};
    const std::size_t char_str_count{1 + player.characterName.size()};

    return acc_str_count + char_str_count + PlayerInfo_partial_size;
}

inline std::size_t serial_size(const PlayerInfoEntry& entry)
{
    return sizeof(entry.validator) + serial_size(entry.player);
}

void to_serial(const PlayerInfo& player, uint8_t* storage, std::size_t count);
void to_serial(const PlayerInfoEntry& entry, uint8_t* storage, std::size_t count);

void to_json(nlohmann::json& j, const PlayerInfo& player);
void to_json(nlohmann::json& j, const PlayerInfoEntry& entry);

bool operator==(const PlayerInfo& lhs, const PlayerInfo& rhs);
bool operator!=(const PlayerInfo& lhs, const PlayerInfo& rhs);

#endif // BRIDGE_PLAYERCONTAINER_HPP