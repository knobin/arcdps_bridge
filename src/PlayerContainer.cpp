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

std::optional<PlayerContainer::PlayerInfoEntry> PlayerContainer::find(const std::string& accountName) const
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_squad.cbegin(), m_squad.cend(),
                           [&accountName](const auto& p) { return p.first && accountName == p.second.player.accountName; });

    if (it != m_squad.cend())
        return it->second;

    return std::nullopt;
}

PlayerContainer::PlayerInfoUpdate PlayerContainer::update(const PlayerInfoEntry& playerEntry)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Get player if exists already.
    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&playerEntry](const auto& p) { return p.first && playerEntry.player.accountName == p.second.player.accountName; });

    if (it != m_squad.end())
    {
        auto& member = it->second;
        if (member.validator == playerEntry.validator)
        {
            member = playerEntry;
            ++member.validator;
            BRIDGE_INFO("Updated \"", member.player.accountName, "\" ", "in squad.");
            return {std::nullopt, Status::Success};
        }
        else
        {
            BRIDGE_INFO("Could not update player with \"", member.player.accountName, "\" due to validators not matching,  ", member.validator, " != ", playerEntry.validator, ".");
            return {it->second, Status::ValidatorError};
        }
    }

    BRIDGE_INFO("Could not update player with \"", playerEntry.player.accountName, "\" due to not being found.");
    return {std::nullopt, Status::Invalid};
}

PlayerContainer::Status PlayerContainer::add(const PlayerInfo& player)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Get player if exists already.
    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&player](const auto& p) { return p.first && player.accountName == p.second.player.accountName; });

    // Player exists already.
    if (it != m_squad.end())
    {
        BRIDGE_INFO("Player \"", player.accountName, "\" already exist!");
        return Status::ExistsError;
    }

    // Finds an empty slot to use.
    it = std::find_if(m_squad.begin(), m_squad.end(), [](const auto& p) { return !p.first; });

    // Add.
    if (it != m_squad.end())
    {
        BRIDGE_INFO("Added ", " \"", player.accountName, "\" to squad.");
        it->second = {player, 0};
        it->first = true;
        return Status::Success;
    }

    BRIDGE_INFO("Exceeding squad limit of 50 players trying to add \"", player.accountName, "\".");
    return Status::Invalid;
}

std::optional<PlayerContainer::PlayerInfo> PlayerContainer::remove(const std::string& accountName)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&accountName](const auto& p) { return (accountName == p.second.player.accountName); });
    if (it != m_squad.end())
    {
        BRIDGE_INFO("Removing \"", accountName, "\" from squad.");
        it->first = false;
        PlayerInfo copy = it->second.player;
        it->second = {{}, 0};
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
        p.second = {{}, 0};
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
            ss << ((added++ > 0) ? "," : "") << m_squad[i].second.player.toJSON();

    ss << "]";
    return ss.str();
}
