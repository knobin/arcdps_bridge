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

class SquadModifyHandler
{
public:
    SquadModifyHandler() = delete;
    SquadModifyHandler(PlayerContainer& squad)
        : m_squad{squad}
    {
        for (std::size_t i{0}; i < m_addQueue.size(); ++i)
            m_addQueue[i] = {false, {}};
    }

    template<typename Sender, typename Updater>
    void addPlayer(const PlayerInfo& player, Sender sender, Updater updater);

    template<typename Sender, typename Updater>
    void updatePlayer(const std::string& accountName, Sender sender, Updater updater);

    template<typename UnaryPredicate, typename Sender, typename Updater>
    void updatePlayer(UnaryPredicate p, Sender sender, Updater updater);

    template<typename Sender>
    void removePlayer(const std::string& accountName, Sender sender);

private:
    template<typename Sender, typename Updater>
    void addPlayerToSquad(const PlayerInfo& player, Sender sender, Updater updater);
    
    template<typename Sender, typename Updater>
    void updatePlayerInSquad(const PlayerInfoEntry& existing, Sender sender, Updater updater);

    // Add Queue functions.
    template<typename Sender, typename Updater>
    void queuePlayer(const PlayerInfo& player, Sender sender, Updater updater);
    
    template<typename UnaryPredicate, typename Updater>
    void updateQueuedPlayer(UnaryPredicate p, Updater updater);
    
    void dequeuePlayer(const std::string& accountName);

private:
    struct QueuedPlayer
    {
        PlayerInfo player;
        std::function<void(const std::string&, const PlayerInfoEntry&)> sender;
    };

private:
    PlayerContainer& m_squad;
    std::array<std::pair<bool, QueuedPlayer>, 50> m_addQueue{};
    std::mutex m_mutex{};
    bool m_selfInSquad{false};
};

template<typename Sender, typename Updater>
void SquadModifyHandler::addPlayer(const PlayerInfo& player, Sender sender, Updater updater)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Safe to add member to Squad.
    if (m_selfInSquad || player.self)
    {
        if (auto existing = m_squad.find(player.accountName))
            updatePlayerInSquad(*existing, sender, updater);
        else
            addPlayerToSquad(player, sender, updater);
    }

    // If self is being added, send other added players as well. 
    if (player.self)
    {
        m_selfInSquad = true;

        for (std::size_t i{0}; i < m_addQueue.size(); ++i)
        {
            if (m_addQueue[i].first) // QueuedPlayer is valid.
            {
                QueuedPlayer& member = m_addQueue[i].second;

                // The add here should always succeed, otherwise it is an error.
                if (m_squad.add(member.player) == PlayerContainer::Status::Success)
                    member.sender("add", PlayerInfoEntry{member.player, PLAYER_VALIDATOR_START});

                m_addQueue[i] = {false, {}};
            }
        }      
    }

    // If self is not in squad, queue player.
    if (!m_selfInSquad)
        queuePlayer(player, sender, updater);
}

template<typename Sender, typename Updater>
void SquadModifyHandler::updatePlayer(const std::string& accountName, Sender sender, Updater updater)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto existing = m_squad.find(accountName))
        updatePlayerInSquad(*existing, sender, updater);
    else if (!m_selfInSquad)
    {
        auto p = [&accountName](const PlayerInfo& player){ return player.accountName == accountName; };
        updateQueuedPlayer(p, updater);
    }
}

template<typename UnaryPredicate, typename Sender, typename Updater>
void SquadModifyHandler::updatePlayer(UnaryPredicate p, Sender sender, Updater updater)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto existing = m_squad.find_if(p))
        updatePlayerInSquad(*existing, sender, updater);
    else if (!m_selfInSquad)
        updateQueuedPlayer(p, updater);
}

template<typename Sender>
void SquadModifyHandler::removePlayer(const std::string& accountName, Sender sender)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto entry = m_squad.remove(accountName))
    {
        if (entry->player.self)
        {
            m_selfInSquad = false;
            m_squad.clear();
        }

        sender(*entry);
    }
    else
    {
        // If player is not in squad, dequeue if queued.
        // Player should not exist in both places at the same time.
        dequeuePlayer(accountName);
    }
}

template<typename Sender, typename Updater>
void SquadModifyHandler::addPlayerToSquad(const PlayerInfo& player, Sender sender, Updater updater)
{
    if (m_squad.add(player) == PlayerContainer::Status::Success)
    {
        sender("add", PlayerInfoEntry{player, PLAYER_VALIDATOR_START});
    }
    else
    {
        if (auto existing = m_squad.find(player.accountName))
        {
            updatePlayerInSquad(*existing, sender, updater);
        }
    }
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
        sender("update", *update.entry);
}

template<typename Sender, typename Updater>
void SquadModifyHandler::queuePlayer(const PlayerInfo& player, Sender sender, Updater updater)
{
    // Get player if exists already.
    auto it = std::find_if(m_addQueue.begin(), m_addQueue.end(), [&player](const auto& p) { 
        return p.first && player.accountName == p.second.player.accountName;
    });

    if (it != m_addQueue.end())
    {
        // Player exists already, update information.
        updater(it->second.player);
    }
    else
    {
        // Finds an empty slot to use.
        it = std::find_if(m_addQueue.begin(), m_addQueue.end(), [](const auto& p) { return !p.first; });

        // Add player.
        if (it != m_addQueue.end())
        {
            it->second = {player, sender};
            it->first = true;
        }
    }
}

template<typename UnaryPredicate, typename Updater>
void SquadModifyHandler::updateQueuedPlayer(UnaryPredicate p, Updater updater)
{
    // Get player if exists already.
    auto it = std::find_if(m_addQueue.begin(), m_addQueue.end(), [&p](const auto& entry) { 
        return entry.first && p(entry.second.player);
    });

    if (it != m_addQueue.end())
    {
        // Update information.
        updater(it->second.player);
    }
}

void SquadModifyHandler::dequeuePlayer(const std::string& accountName)
{
    // Get player if exists already.
    auto it = std::find_if(m_addQueue.begin(), m_addQueue.end(), [&accountName](const auto& p) { 
        return p.first && accountName == p.second.player.accountName;
    });

    if (it != m_addQueue.end())
    {
        it->first = false;
        it->second = {};
    }
}

#endif // BRIDGE_SQUADMODIFYHANDLER_HPP
