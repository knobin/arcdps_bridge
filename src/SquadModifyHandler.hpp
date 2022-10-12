//
//  src/SquadModifyHandler.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-07-26.
//

#ifndef BRIDGE_SQUADMODIFYHANDLER_HPP
#define BRIDGE_SQUADMODIFYHANDLER_HPP

// Local Headers
#include "PlayerContainer.hpp"

// C++ Headers
#include <functional>
#include <mutex>

enum class SquadAction : uint8_t
{
    Add     = 1,
    Remove  = 2,
    Update  = 4
};

class SquadModifyHandler
{
public:
    SquadModifyHandler() = delete;
    explicit SquadModifyHandler(PlayerContainer& squad)
        : m_squad{squad}
    {}

    static constexpr uint8_t CombatBit = 1;
    static constexpr uint8_t ExtrasBit = 2;

    // Run callback with the modify lock.
    template<typename Callback>
    void work(Callback cb) const;

    template<typename Sender, typename Updater>
    void addPlayer(const PlayerInfo& player, Sender sender, Updater updater, uint8_t bits);

    template<typename Sender, typename Updater>
    void updatePlayer(const std::string& accountName, Sender sender, Updater updater);

    template<typename UnaryPredicate, typename Sender, typename Updater>
    void updatePlayer(UnaryPredicate p, Sender sender, Updater updater);

    template<typename Sender>
    void removePlayer(const std::string& accountName, Sender sender, uint8_t bits);

private:
    template<typename Sender, typename Updater>
    void addPlayerToSquad(const PlayerInfo& player, Sender sender, Updater updater);
    
    template<typename Sender, typename Updater>
    void updatePlayerInSquad(const PlayerInfoEntry& existing, Sender sender, Updater updater);

    struct PlayerCache
    {
        std::string accountName{};
        uint8_t bits{0};
    };

    [[nodiscard]] int findCachedPlayer(const std::string& accountName);
    [[nodiscard]] int createCachedPlayer(const std::string& accountName);
    void clearCachedPlayer();

private:
    PlayerContainer& m_squad;
    std::array<PlayerCache, 100> m_addCache{};
    mutable std::mutex m_mutex{};
};

template<typename Callback>
void SquadModifyHandler::work(Callback cb) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    cb();
}

template<typename Sender, typename Updater>
void SquadModifyHandler::addPlayer(const PlayerInfo& player, Sender sender, Updater updater, uint8_t bits)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    int index = createCachedPlayer(player.accountName);
    if (index > -1)
        m_addCache[index].bits |= bits;

    if (auto existing = m_squad.find(player.accountName))
        updatePlayerInSquad(*existing, sender, updater);
    else
        addPlayerToSquad(player, sender, updater);
}

template<typename Sender, typename Updater>
void SquadModifyHandler::updatePlayer(const std::string& accountName, Sender sender, Updater updater)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto existing = m_squad.find(accountName))
        updatePlayerInSquad(*existing, sender, updater);
}

template<typename UnaryPredicate, typename Sender, typename Updater>
void SquadModifyHandler::updatePlayer(UnaryPredicate p, Sender sender, Updater updater)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto existing = m_squad.find_if(p))
        updatePlayerInSquad(*existing, sender, updater);
}

template<typename Sender>
void SquadModifyHandler::removePlayer(const std::string& accountName, Sender sender, uint8_t bits)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    int index = findCachedPlayer(accountName);
    bool shouldRemove = (index < 0); // Default to remove if no index found.

    if (index > -1)
    {
        // Valid index exists.

        m_addCache[index].bits &= ~bits;
        if (m_addCache[index].bits == 0)
        {
            shouldRemove = true;
            m_addCache[index] = PlayerCache{};
        }
    }

    if (shouldRemove)
    {
        if (auto entry = m_squad.remove(accountName))
        {
            if (entry->player.self)
            {
                m_squad.clear();

                // Will remove cache of players only added from Extras.
                // When self is removed, Extras will not send events for the other members in the squad.
                // Combat adds will be removed from combat events.
                clearCachedPlayer(); 
            }

            sender(SquadAction::Remove, *entry);
        }
    }
}

template<typename Sender, typename Updater>
void SquadModifyHandler::addPlayerToSquad(const PlayerInfo& player, Sender sender, Updater updater)
{
    if (m_squad.add(player) == PlayerContainer::Status::Success)
        sender(SquadAction::Add, PlayerInfoEntry{player, PLAYER_VALIDATOR_START});
    else if (auto existing = m_squad.find(player.accountName))
        updatePlayerInSquad(*existing, sender, updater);
}

template<typename Sender, typename Updater>
void SquadModifyHandler::updatePlayerInSquad(const PlayerInfoEntry& existing, Sender sender, Updater updater)
{
    PlayerContainer::PlayerInfoUpdate update = {existing, PlayerContainer::Status::ValidatorError};

    while (update.entry && update.status == PlayerContainer::Status::ValidatorError)
    {
        updater(update.entry->player);
        update = m_squad.update(*update.entry);
    }
    
    if (update.entry && update.status == PlayerContainer::Status::Success)
        sender(SquadAction::Update, *update.entry);
}

inline int SquadModifyHandler::findCachedPlayer(const std::string& accountName)
{
    for (int i{0}; i < m_addCache.size(); ++i)
        if (m_addCache[i].accountName == accountName)
            return i;

    return -1;
}

inline int SquadModifyHandler::createCachedPlayer(const std::string& accountName)
{
    int index = findCachedPlayer(accountName);

    if (index == -1)
    {
        // Find free slot.
        for (int i{0}; i < m_addCache.size(); ++i)
        {
            if (m_addCache[i].accountName.empty())
            {
                index = i;
                m_addCache[i].accountName = accountName;
                break;
            }
        }
    }

    return index;
}

inline void SquadModifyHandler::clearCachedPlayer()
{
    for (int i{0}; i < m_addCache.size(); ++i)
        if (m_addCache[i].bits == ExtrasBit)
            m_addCache[i] = PlayerCache{};
}

#endif // BRIDGE_SQUADMODIFYHANDLER_HPP
