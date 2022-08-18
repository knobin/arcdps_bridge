//
//  src/Extras.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-08.
//

#ifndef BRIDGE_EXTRAS_HPP
#define BRIDGE_EXTRAS_HPP

// Local Headers
#include "Message.hpp"

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

//
// Extras Squad Callback.
//

// __time64_t is treated as int64_t here.
// bool is treated as 1 byte.
constexpr std::size_t UserInfo_partial_size = sizeof(int64_t) + sizeof(UserInfo::Role) + sizeof(UserInfo::Subgroup) + 
                                              sizeof(uint8_t);
std::size_t serial_size(const UserInfo& info);
void to_serial(const UserInfo& info, uint8_t* storage, std::size_t count);
void to_json(nlohmann::json& j, const UserInfo& user);



#endif // BRIDGE_EXTRAS_HPP
