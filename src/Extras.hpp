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
constexpr std::size_t UserInfo_partial_size =
    sizeof(int64_t) + sizeof(UserInfo::Role) + sizeof(UserInfo::Subgroup) + sizeof(uint8_t);
std::size_t serial_size(const UserInfo& info);
void to_serial(const UserInfo& info, uint8_t* storage, std::size_t count);
void to_json(nlohmann::json& j, const UserInfo& user);

//
// Extras Language Callback.
//

constexpr std::size_t serial_size(Language)
{
    return sizeof(std::underlying_type_t<Language>);
}
void to_serial(Language language, uint8_t* storage, std::size_t count);
void to_json(nlohmann::json& j, Language language);

//
// Extras KeyBind Callback.
//

constexpr std::size_t serial_size(KeyBinds::KeyBindChanged)
{
    constexpr std::size_t key_control_size = sizeof(std::underlying_type_t<KeyBinds::KeyControl>);
    constexpr std::size_t key_index_size = sizeof(uint32_t);
    constexpr std::size_t key_size =
        sizeof(std::underlying_type_t<KeyBinds::DeviceType>) + sizeof(int32_t) + sizeof(KeyBinds::Modifier);

    return key_control_size + key_index_size + key_size;
}
void to_serial(const KeyBinds::KeyBindChanged& pChangedKeyBind, uint8_t* storage, std::size_t count);
void to_json(nlohmann::json& j, const KeyBinds::KeyBindChanged& pChangedKeyBind);

//
// Extras Chat Message Callback.
//

constexpr std::size_t ChatMessageInfo_partial_size = sizeof(ChatMessageInfo::ChannelId) +
                                                     sizeof(std::underlying_type_t<ChannelType>) + sizeof(uint8_t) +
                                                     sizeof(uint8_t);
std::size_t serial_size(const ChatMessageInfo& chatMsgInfo);
void to_serial(const ChatMessageInfo& chatMsgInfo, uint8_t* storage, std::size_t count);
void to_json(nlohmann::json& j, const ChatMessageInfo& chatMsgInfo);

#endif // BRIDGE_EXTRAS_HPP
