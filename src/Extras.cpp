//
//  src/Extras.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-18.
//

// Local Headers
#include "Extras.hpp"

// C++ Headers
#include <type_traits>

//
// Extras Squad Callback.
//

std::size_t serial_size(const UserInfo& info)
{
    std::size_t str_count{1};
    if (info.AccountName != nullptr)
        str_count += std::strlen(info.AccountName);

    return str_count + UserInfo_partial_size;
}

void to_serial(const UserInfo& info, uint8_t* storage, std::size_t count)
{
    const std::size_t str_count = count - UserInfo_partial_size - 1;

    uint8_t* location = serial_w_string(storage, info.AccountName, str_count);
    location = serial_w_integral(location, static_cast<int64_t>(info.JoinTime));
    location = serial_w_integral(location, static_cast<std::underlying_type_t<UserRole>>(info.Role));
    location = serial_w_integral(location, info.Subgroup);
    location = serial_w_integral(location, static_cast<uint8_t>(info.ReadyStatus));
}

void to_json(nlohmann::json& j, const UserInfo& user)
{
    j =  nlohmann::json{
        {"AccountName", nullptr},
        {"Role", static_cast<std::underlying_type_t<UserRole>>(user.Role)},
        {"Subgroup", static_cast<uint32_t>(user.Subgroup)}, 
        {"JoinTime", static_cast<int64_t>(user.JoinTime)},
        {"ReadyStatus", user.ReadyStatus}
    };

    if (user.AccountName)
        j["AccountName"] = user.AccountName;
}
