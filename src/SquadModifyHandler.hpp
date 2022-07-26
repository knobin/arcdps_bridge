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
#include <mutex>

class SquadModifyHandler
{
public:
    SquadModifyHandler() = delete;
    SquadModifyHandler(PlayerContainer& squad)
        : m_squad{squad}
    {}

    template<typename SuccessCallback, typename FailedCallback>
    void addPlayer(const PlayerInfo& player, SuccessCallback scb, FailedCallback fcb);

    template<typename UpdateCallback, typename SuccessCallback>
    void updatePlayer(const PlayerInfoEntry& existing, UpdateCallback ucb, SuccessCallback scb);

    template<typename SuccessCallback>
    void removePlayer(const std::string& accountName, SuccessCallback scb);

    void clear()
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        m_squad.clear();
    }

private:
    PlayerContainer& m_squad;
    std::mutex m_mutex{};
};

template<typename SuccessCallback, typename FailedCallback>
void SquadModifyHandler::addPlayer(const PlayerInfo& player, SuccessCallback scb, FailedCallback fcb)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_squad.add(player) == PlayerContainer::Status::Success)
        scb(PlayerInfoEntry{player, PLAYER_VALIDATOR_START});
    else
        fcb();
}

template<typename UpdateCallback, typename SuccessCallback>
void SquadModifyHandler::updatePlayer(const PlayerInfoEntry& existing, UpdateCallback ucb, SuccessCallback scb)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    PlayerContainer::PlayerInfoUpdate update = {existing, PlayerContainer::Status::ValidatorError};

    while (update.entry && update.status == PlayerContainer::Status::ValidatorError)
    {
        ucb(update.entry->player);
        update = m_squad.update(*update.entry);
    }
    
    if (update.entry && update.status == PlayerContainer::Status::Success)
        scb(*update.entry);
}

template<typename SuccessCallback>
void SquadModifyHandler::removePlayer(const std::string& accountName, SuccessCallback scb)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (auto entry = m_squad.remove(accountName))
        scb(*entry);  
}


#endif // BRIDGE_SQUADMODIFYHANDLER_HPP
