//
//  src/PlayerContainer.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "PlayerContainer.hpp"
#include "Log.hpp"

// C++ Headers
#include <sstream>

std::string PlayerContainer::PlayerInfo::toJSON() const
{
    std::ostringstream ss{};
    ss << "{"
       << "\"accountName\":\"" << accountName << "\","
       << "\"characterName\":" << ((!characterName.empty()) ? "\"" + characterName + "\"" : "null") << ","
       << "\"joinTime\":" << joinTime << ","
       << "\"profession\":" << profession << ","
       << "\"elite\":" << elite << ","
       << "\"role\":" << static_cast<int>(role) << ","
       << "\"subgroup\":" << static_cast<int>(subgroup)<< ","
       << "\"inInstance\":" << ((inInstance) ? "true" : "false") << "}";
    return ss.str();
}

std::optional<PlayerContainer::PlayerInfo> PlayerContainer::find(const std::string& accountName) const
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_squad.cbegin(), m_squad.cend(),
                           [&accountName](const auto& p) { return p.first && accountName == p.second.accountName; });

    if (it != m_squad.cend())
        return it->second;

    return std::nullopt;
}

std::optional<PlayerContainer::PlayerInfo> PlayerContainer::update(const PlayerInfo& player)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Get player if exists already.
    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&player](const auto& p) { return p.first && player.accountName == p.second.accountName; });

    if (it != m_squad.end())
    {
        auto& member = it->second;
        BRIDGE_INFO("Updated \"", member.accountName, "\" ", "in squad.");
        member = player;
        return member;
    }

    BRIDGE_INFO("Could not update player with \"", player.accountName, "\" due to not being found.");
    return std::nullopt;
}

std::optional<PlayerContainer::PlayerInfo> PlayerContainer::add(const PlayerInfo& player)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Get player if exists already.
    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&player](const auto& p) { return p.first && player.accountName == p.second.accountName; });

    // Add or Update status.
    const bool found = it != m_squad.end();

    // Player does not exist.
    if (!found)
    {
        // Finds an empty slot to use.
        it = std::find_if(m_squad.begin(), m_squad.end(), [](const auto& p) { return !p.first; });
    }

    // Add or Update player.
    if (it != m_squad.end())
    {
        BRIDGE_INFO(((found) ? "Updated" : "Added"), " \"", player.accountName, "\" ", ((found) ? "in" : "to"),
                    " squad.");
        it->second = player;
        it->first = true;
        return it->second;
    }

    BRIDGE_INFO("Exceeding squad limit of 50 players trying to add \"", player.accountName, "\".");
    return std::nullopt;
}

std::optional<PlayerContainer::PlayerInfo> PlayerContainer::remove(const std::string& accountName)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&accountName](const auto& p) { return (accountName == p.second.accountName); });
    if (it != m_squad.end())
    {
        BRIDGE_INFO("Removing \"", accountName, "\" from squad.");
        it->first = false;
        PlayerInfo copy = it->second;
        it->second = PlayerInfo{};
        return copy;
    }
    return std::nullopt;
}

void PlayerContainer::clear()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    for (auto& p : m_squad)
    {
        p.first = false;
        p.second = PlayerInfo{};
    }

    BRIDGE_INFO("Cleared squad.");
}

std::string PlayerContainer::toJSON() const
{
    std::unique_lock<std::mutex> lock(m_mutex);

    std::ostringstream ss{};
    ss << "[";

    uint8_t added{0};
    for (std::size_t i{0}; i < m_squad.size(); ++i)
        if (m_squad[i].first)
            ss << ((added++ > 0) ? "," : "") << m_squad[i].second.toJSON();

    ss << "]";
    return ss.str();
}
