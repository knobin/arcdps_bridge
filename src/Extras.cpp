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

namespace Extras
{
    //
    // Extras Squad Callback.
    //

    std::size_t SerialSize(const UserInfo& info)
    {
        std::size_t str_count{1};
        if (info.AccountName != nullptr)
            str_count += std::strlen(info.AccountName);

        return str_count + UserInfoPartialSize;
    }

    void ToSerial(const UserInfo& info, uint8_t* storage, std::size_t count)
    {
        const std::size_t str_count = count - UserInfoPartialSize - 1;

        uint8_t* location = serial_w_string(storage, info.AccountName, str_count);
        location = serial_w_integral(location, static_cast<int64_t>(info.JoinTime));
        location = serial_w_integral(location, static_cast<std::underlying_type_t<UserRole>>(info.Role));
        location = serial_w_integral(location, info.Subgroup);
        serial_w_integral(location, static_cast<uint8_t>(info.ReadyStatus));
    }

    nlohmann::json ToJSON(const UserInfo& user)
    {
        nlohmann::json j{{"AccountName", nullptr},
                         {"Role", static_cast<std::underlying_type_t<UserRole>>(user.Role)},
                         {"Subgroup", static_cast<uint32_t>(user.Subgroup)},
                         {"JoinTime", static_cast<int64_t>(user.JoinTime)},
                         {"ReadyStatus", user.ReadyStatus}};

        if (user.AccountName)
            j["AccountName"] = user.AccountName;

        return j;
    }

    Message SquadMessageGenerator(const UserInfo& info, std::underlying_type_t<MessageProtocol> protocols)
    {
        const auto protocolSerial = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::Serial);
        const auto protocolJSON = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::JSON);

        SerialData serial{};

        if (protocols & protocolSerial)
        {
            const std::size_t uinfo_count = SerialSize(info);
            serial = CreateSerialData(uinfo_count);
            ToSerial(info, &serial.ptr[SerialStartPadding], uinfo_count);

            if (protocols == protocolSerial)
                return ExtrasMessage<MessageType::ExtrasSquadUpdate>(serial);
        }

        nlohmann::json json{};

        if (protocols & protocolJSON)
        {
            json = ToJSON(info);

            if (protocols == protocolJSON)
                return ExtrasMessage<MessageType::ExtrasSquadUpdate>(json);
        }

        return ExtrasMessage<MessageType::ExtrasSquadUpdate>(serial, json);
    }

    //
    // Extras Language Callback.
    //

    void ToSerial(Language language, uint8_t* storage, std::size_t)
    {
        serial_w_integral(storage, static_cast<std::underlying_type_t<Language>>(language));
    }

    nlohmann::json ToJSON(Language language)
    {
        return nlohmann::json{{"Language", static_cast<std::underlying_type_t<Language>>(language)}};
    }

    Message LanguageMessageGenerator(Language language, std::underlying_type_t<MessageProtocol> protocols)
    {
        const auto protocolSerial = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::Serial);
        const auto protocolJSON = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::JSON);

        SerialData serial{};

        if (protocols & protocolSerial)
        {
            constexpr std::size_t lang_count = SerialSize(Language{});
            serial = CreateSerialData(lang_count);
            ToSerial(language, &serial.ptr[SerialStartPadding], lang_count);

            if (protocols == protocolSerial)
                return ExtrasMessage<MessageType::ExtrasLanguageChanged>(serial);
        }

        nlohmann::json json{};

        if (protocols & protocolJSON)
        {
            json = ToJSON(language);

            if (protocols == protocolJSON)
                return ExtrasMessage<MessageType::ExtrasLanguageChanged>(json);
        }

        return ExtrasMessage<MessageType::ExtrasLanguageChanged>(serial, json);
    }

    //
    // Extras KeyBind Callback.
    //

    void ToSerial(const KeyBinds::KeyBindChanged& pChangedKeyBind, uint8_t* storage, std::size_t)
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

    nlohmann::json ToJSON(const KeyBinds::KeyBindChanged& pChangedKeyBind)
    {
        nlohmann::json j{
            {"KeyControl", static_cast<std::underlying_type_t<KeyBinds::KeyControl>>(pChangedKeyBind.KeyControl)},
            {"KeyIndex", pChangedKeyBind.KeyIndex},
            {"SingleKey",
             {{"DeviceType",
               static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(pChangedKeyBind.SingleKey.DeviceType)},
              {"Code", pChangedKeyBind.SingleKey.Code},
              {"Modifier", pChangedKeyBind.SingleKey.Modifier}}},
        };

        return j;
    }

    Message KeyBindMessageGenerator(const KeyBinds::KeyBindChanged& pChangedKeyBind,
                                    std::underlying_type_t<MessageProtocol> protocols)
    {
        const auto protocolSerial = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::Serial);
        const auto protocolJSON = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::JSON);

        SerialData serial{};

        if (protocols & protocolSerial)
        {
            constexpr std::size_t keybind_count = SerialSize(KeyBinds::KeyBindChanged{});
            serial = CreateSerialData(keybind_count);
            ToSerial(pChangedKeyBind, &serial.ptr[SerialStartPadding], keybind_count);

            if (protocols == protocolSerial)
                return ExtrasMessage<MessageType::ExtrasKeyBindChanged>(serial);
        }

        nlohmann::json json{};

        if (protocols & protocolJSON)
        {
            json = ToJSON(pChangedKeyBind);
            if (protocols == protocolJSON)
                return ExtrasMessage<MessageType::ExtrasKeyBindChanged>(json);
        }

        return ExtrasMessage<MessageType::ExtrasKeyBindChanged>(serial, json);
    }

    //
    // Extras Chat Message Callback.
    //

    std::size_t SerialSize(const ChatMessageInfo& chatMsgInfo)
    {
        const std::size_t timestampCount = chatMsgInfo.TimestampLength + 1;
        const std::size_t accNameCount = chatMsgInfo.AccountNameLength + 1;
        const std::size_t charNameCount = chatMsgInfo.CharacterNameLength + 1;
        const std::size_t textCount = chatMsgInfo.TextLength + 1;

        const std::size_t total_string_size = timestampCount + accNameCount + charNameCount + textCount;
        return ChatMessageInfoPartialSize + total_string_size;
    }

    void ToSerial(const ChatMessageInfo& chatMsgInfo, uint8_t* storage, std::size_t)
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

    nlohmann::json ToJSON(const ChatMessageInfo& chatMsgInfo)
    {
        nlohmann::json j{{"ChannelId", chatMsgInfo.ChannelId},
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

        return j;
    }

    Message ChatMessageGenerator(const ChatMessageInfo& pChatMessage, std::underlying_type_t<MessageProtocol> protocols)
    {
        const auto protocolSerial = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::Serial);
        const auto protocolJSON = static_cast<std::underlying_type_t<MessageProtocol>>(MessageProtocol::JSON);

        SerialData serial{};

        if (protocols & protocolSerial)
        {
            const std::size_t chat_msg_count = SerialSize(pChatMessage);
            serial = CreateSerialData(chat_msg_count);
            ToSerial(pChatMessage, &serial.ptr[SerialStartPadding], chat_msg_count);

            if (protocols == protocolSerial)
                return ExtrasMessage<MessageType::ExtrasChatMessage>(serial);
        }

        nlohmann::json json{};

        if (protocols & protocolJSON)
        {
            json = ToJSON(pChatMessage);

            if (protocols == protocolJSON)
                return ExtrasMessage<MessageType::ExtrasChatMessage>(json);
        }

        return ExtrasMessage<MessageType::ExtrasChatMessage>(serial, json);
    }
} // namespace Extras