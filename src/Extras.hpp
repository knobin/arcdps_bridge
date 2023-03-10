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

namespace Extras
{
    //
    // Extras Squad Callback.
    //

    // __time64_t is treated as int64_t here.
    // bool is treated as 1 byte.
    constexpr std::size_t UserInfoPartialSize{sizeof(int64_t) + sizeof(UserInfo::Role) + sizeof(UserInfo::Subgroup) +
                                              sizeof(uint8_t)};
    [[nodiscard]] std::size_t SerialSize(const UserInfo& info);
    void ToSerial(const UserInfo& info, uint8_t* storage, std::size_t count);
    [[nodiscard]] nlohmann::json ToJSON(const UserInfo& user);

    //
    // Extras Language Callback.
    //

    [[nodiscard]] constexpr std::size_t SerialSize(Language)
    {
        return sizeof(std::underlying_type_t<Language>);
    }
    void ToSerial(Language language, uint8_t* storage, std::size_t count);
    [[nodiscard]] nlohmann::json ToJSON(Language language);

    //
    // Extras KeyBind Callback.
    //

    [[nodiscard]] constexpr std::size_t SerialSize(KeyBinds::KeyBindChanged)
    {
        constexpr std::size_t key_control_size = sizeof(std::underlying_type_t<KeyBinds::KeyControl>);
        constexpr std::size_t key_index_size = sizeof(uint32_t);
        constexpr std::size_t key_size =
            sizeof(std::underlying_type_t<KeyBinds::DeviceType>) + sizeof(int32_t) + sizeof(KeyBinds::Modifier);

        return key_control_size + key_index_size + key_size;
    }
    void ToSerial(const KeyBinds::KeyBindChanged& pChangedKeyBind, uint8_t* storage, std::size_t count);
    [[nodiscard]] nlohmann::json ToJSON(const KeyBinds::KeyBindChanged& pChangedKeyBind);

    //
    // Extras Chat Message Callback.
    //

    constexpr std::size_t ChatMessageInfoPartialSize = sizeof(ChatMessageInfo::ChannelId) +
                                                       sizeof(std::underlying_type_t<ChannelType>) + sizeof(uint8_t) +
                                                       sizeof(uint8_t);
    [[nodiscard]] std::size_t SerialSize(const ChatMessageInfo& chatMsgInfo);
    void ToSerial(const ChatMessageInfo& chatMsgInfo, uint8_t* storage, std::size_t count);
    [[nodiscard]] nlohmann::json ToJSON(const ChatMessageInfo& chatMsgInfo);

} // namespace Extras

#endif // BRIDGE_EXTRAS_HPP
