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

public:
    std::optional<PlayerInfo> find(const std::string& accountName) const;
    std::optional<PlayerContainer::PlayerInfo> update(const PlayerInfo& player);
    std::optional<PlayerContainer::PlayerInfo> add(const PlayerInfo& player);
    std::optional<PlayerContainer::PlayerInfo> remove(const std::string& accountName);

    void clear();

    std::string toJSON() const;

private:
    std::array<std::pair<bool, PlayerInfo>, 50> m_squad{};
    mutable std::mutex m_mutex{};
};

#endif // BRIDGE_PLAYERCONTAINER_HPP