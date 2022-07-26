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

std::string PlayerInfo::toJSON() const
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

#if BRIDGE_DEBUG_LEVEL > 0 // 1 and above.
static std::string PrintPlayerInfoDiff(const PlayerInfo& p1, const PlayerInfo& p2)
{
    std::ostringstream ss{};
    if (p1.accountName != p2.accountName)
        ss << "accountName: \"" << p1.accountName << "\" => \"" << p2.accountName << "\"";
    if (p1.characterName != p2.characterName)
        ss << ",characterName: \"" << p1.characterName << "\" => \"" << p2.characterName << "\"";
    if (p1.joinTime != p2.joinTime)
        ss << ",joinTime: \"" << p1.joinTime << "\" => \"" << p2.joinTime << "\"";
    if (p1.profession != p2.profession)
        ss << ",profession: \"" << p1.profession << "\" => \"" << p2.profession << "\"";
    if (p1.elite != p2.elite)
        ss << ",elite: \"" << p1.elite << "\" => \"" << p2.elite << "\"";
    if (p1.role != p2.role)
        ss << ",role: \"" << static_cast<int>(p1.role) << "\" => \"" << static_cast<int>(p2.role) << "\"";
    if (p1.subgroup != p2.subgroup)
        ss << ",role: \"" << static_cast<int>(p1.subgroup) << "\" => \"" << static_cast<int>(p2.subgroup) << "\"";
    if (p1.inInstance != p2.inInstance)
        ss << ",inInstance: \"" << p1.inInstance << "\" => \"" << p2.inInstance << "\"";

    std::string changes = ss.str();

    if (!changes.empty() && changes.front() == ',')
        changes.erase(0,1);

    return "{" + changes + "}";
}
#endif

std::optional<PlayerInfoEntry> PlayerContainer::find(const std::string& accountName) const
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
            if (member.player != playerEntry.player)
            {
                BRIDGE_INFO("Updated \"{}\" in squad, with: {}", member.player.accountName, PrintPlayerInfoDiff(member.player, playerEntry.player));
                member = playerEntry;
                ++member.validator;
                return {member, Status::Success};
            }
            else
            {
                BRIDGE_DEBUG("Tried to update \"{}\" in squad with the same information.", member.player.accountName);
                return {std::nullopt, Status::Equal};
            }
        }
        else
        {
            BRIDGE_WARN("Could not update player with \"{}\" due to validators not matching, {} != {}.", member.player.accountName, member.validator, playerEntry.validator);
            return {it->second, Status::ValidatorError};
        }
    }

    BRIDGE_ERROR("Could not update player with \"{}\" due to not being found.", playerEntry.player.accountName);
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
        BRIDGE_WARN("Player \"{}\" already exist!", player.accountName);
        return Status::ExistsError;
    }

    // Finds an empty slot to use.
    it = std::find_if(m_squad.begin(), m_squad.end(), [](const auto& p) { return !p.first; });

    // Add.
    if (it != m_squad.end())
    {
        BRIDGE_INFO("Added \"{}\" to squad.", player.accountName);
        it->second = {player, PLAYER_VALIDATOR_START};
        it->first = true;
        return Status::Success;
    }

    BRIDGE_ERROR("Exceeding squad limit of 50 players trying to add \"{}\".", player.accountName);
    return Status::Invalid;
}

std::optional<PlayerInfoEntry> PlayerContainer::remove(const std::string& accountName)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    auto it = std::find_if(m_squad.begin(), m_squad.end(),
                           [&accountName](const auto& p) { return (accountName == p.second.player.accountName); });
    if (it != m_squad.end())
    {
        BRIDGE_INFO("Removing \"{}\" from squad.", accountName);
        it->first = false;
        PlayerInfoEntry copy = it->second;
        it->second = {{}, PLAYER_VALIDATOR_START};
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
        p.second = {{}, PLAYER_VALIDATOR_START};
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

bool operator==(const PlayerInfo& lhs, const PlayerInfo& rhs)
{
    return ((lhs.accountName == rhs.accountName) && (lhs.characterName == rhs.characterName) &&
            (lhs.joinTime == rhs.joinTime) && (lhs.profession == rhs.profession) &&
            (lhs.elite == rhs.elite) && (lhs.role == rhs.role) &&
            (lhs.subgroup == rhs.subgroup) && (lhs.inInstance == rhs.inInstance));
}

bool operator!=(const PlayerInfo& lhs, const PlayerInfo& rhs)
{
    return !(lhs == rhs);
}