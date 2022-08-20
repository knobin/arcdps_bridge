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
    SquadModifyHandler(PlayerContainer& squad)
        : m_squad{squad}
    {}

    // Run callback with the modify lock.
    template<typename Callback>
    void work(Callback cb) const;

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

private:
    PlayerContainer& m_squad;
    mutable std::mutex m_mutex{};
};

template<typename Callback>
void SquadModifyHandler::work(Callback cb) const
{
    std::unique_lock<std::mutex> lock(m_mutex);
    cb();
}

template<typename Sender, typename Updater>
void SquadModifyHandler::addPlayer(const PlayerInfo& player, Sender sender, Updater updater)
{
    std::unique_lock<std::mutex> lock(m_mutex);

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
void SquadModifyHandler::removePlayer(const std::string& accountName, Sender sender)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto entry = m_squad.remove(accountName))
    {
        if (entry->player.self)
            m_squad.clear();

        sender(SquadAction::Remove, *entry);
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

#endif // BRIDGE_SQUADMODIFYHANDLER_HPP
