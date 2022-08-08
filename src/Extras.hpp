//
//  src/Extras.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-08.
//

#ifndef BRIDGE_EXTRAS_HPP
#define BRIDGE_EXTRAS_HPP

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

inline void to_json(nlohmann::json& j, const UserInfo& user)
{
    j =  nlohmann::json{
        {"accountName", std::string{user.AccountName}},
        {"role", static_cast<int>(static_cast<uint8_t>(user.Role))},
        {"subgroup", static_cast<int>(user.Subgroup + 1)}, 
        {"joinTime", user.JoinTime},
        {"readyStatus", user.ReadyStatus}
    };
}

#endif // BRIDGE_EXTRAS_HPP
