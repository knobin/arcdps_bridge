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
    serial_w_integral(location, static_cast<uint8_t>(info.ReadyStatus));
}

void to_json(nlohmann::json& j, const UserInfo& user)
{
    j = nlohmann::json{{"AccountName", nullptr},
                       {"Role", static_cast<std::underlying_type_t<UserRole>>(user.Role)},
                       {"Subgroup", static_cast<uint32_t>(user.Subgroup)},
                       {"JoinTime", static_cast<int64_t>(user.JoinTime)},
                       {"ReadyStatus", user.ReadyStatus}};

    if (user.AccountName)
        j["AccountName"] = user.AccountName;
}

//
// Extras Language Callback.
//

void to_serial(Language language, uint8_t* storage, std::size_t)
{
    serial_w_integral(storage, static_cast<std::underlying_type_t<Language>>(language));
}

void to_json(nlohmann::json& j, Language language)
{
    j = nlohmann::json{{"Language", static_cast<std::underlying_type_t<Language>>(language)}};
}

//
// Extras KeyBind Callback.
//

void to_serial(const KeyBinds::KeyBindChanged& pChangedKeyBind, uint8_t* storage, std::size_t)
{
    uint8_t* location = storage;

    const auto keyControl = static_cast<std::underlying_type_t<KeyBinds::KeyControl>>(pChangedKeyBind.KeyControl);
    location = serial_w_integral(location, keyControl);
    location = serial_w_integral(location, pChangedKeyBind.KeyIndex);

    const auto deviceType =
        static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(pChangedKeyBind.SingleKey.DeviceType);
    location = serial_w_integral(location, deviceType);
    location = serial_w_integral(location, pChangedKeyBind.SingleKey.Code);
    serial_w_integral(location, pChangedKeyBind.SingleKey.Modifier);
}

void to_json(nlohmann::json& j, const KeyBinds::KeyBindChanged& pChangedKeyBind)
{
    j = nlohmann::json{
        {"KeyControl", static_cast<std::underlying_type_t<KeyBinds::KeyControl>>(pChangedKeyBind.KeyControl)},
        {"KeyIndex", pChangedKeyBind.KeyIndex},
        {"SingleKey",
         {{"DeviceType",
           static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(pChangedKeyBind.SingleKey.DeviceType)},
          {"Code", pChangedKeyBind.SingleKey.Code},
          {"Modifier", pChangedKeyBind.SingleKey.Modifier}}},
    };
}

//
// Extras Chat Message Callback.
//

std::size_t serial_size(const ChatMessageInfo& chatMsgInfo)
{
    const std::size_t timestampCount = chatMsgInfo.TimestampLength + 1;
    const std::size_t accNameCount = chatMsgInfo.AccountNameLength + 1;
    const std::size_t charNameCount = chatMsgInfo.CharacterNameLength + 1;
    const std::size_t textCount = chatMsgInfo.TextLength + 1;

    const std::size_t total_string_size = timestampCount + accNameCount + charNameCount + textCount;
    return ChatMessageInfo_partial_size + total_string_size;
}

void to_serial(const ChatMessageInfo& chatMsgInfo, uint8_t* storage, std::size_t)
{
    uint8_t* location = storage;

    location = serial_w_integral(location, chatMsgInfo.ChannelId);
    location = serial_w_integral(location, static_cast<std::underlying_type_t<ChannelType>>(chatMsgInfo.Type));
    location = serial_w_integral(location, chatMsgInfo.Subgroup);
    location = serial_w_integral(location, chatMsgInfo.IsBroadcast);

    location = serial_w_string(location, chatMsgInfo.Timestamp, chatMsgInfo.TimestampLength);
    location = serial_w_string(location, chatMsgInfo.AccountName, chatMsgInfo.AccountNameLength);
    location = serial_w_string(location, chatMsgInfo.CharacterName, chatMsgInfo.CharacterNameLength);
    serial_w_string(location, chatMsgInfo.Text, chatMsgInfo.TextLength);
}

void to_json(nlohmann::json& j, const ChatMessageInfo& chatMsgInfo)
{
    j = nlohmann::json{{"ChannelId", chatMsgInfo.ChannelId},
                       {"Type", static_cast<std::underlying_type_t<ChannelType>>(chatMsgInfo.Type)},
                       {"Subgroup", static_cast<uint32_t>(chatMsgInfo.Subgroup)},
                       {"IsBroadcast", static_cast<uint32_t>(chatMsgInfo.IsBroadcast)},
                       {"Timestamp", nullptr},
                       {"AccountName", nullptr},
                       {"CharacterName", nullptr},
                       {"Text", nullptr}};

    if (chatMsgInfo.Timestamp)
        j["Timestamp"] = chatMsgInfo.Timestamp;

    if (chatMsgInfo.AccountName)
        j["AccountName"] = chatMsgInfo.AccountName;

    if (chatMsgInfo.CharacterName)
        j["CharacterName"] = chatMsgInfo.CharacterName;

    if (chatMsgInfo.Text)
        j["Text"] = chatMsgInfo.Text;
}
