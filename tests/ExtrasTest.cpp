//
//  tests/ExtrasTest.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-10.
//

// Catch2 Headers
#include "catch2/catch_test_macros.hpp"

// Local Headers
#include "FuzzHelper.hpp"

// Bridge Headers
#include "Extras.hpp"

// C++ Headers
#include <sstream>
#include <utility>

///////////////////////////////////////////////////////////////////////////////
//                                 UserInfo                                  //
///////////////////////////////////////////////////////////////////////////////

//
// serial (UserInfo).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("UserInfo_partial_size")
{
    constexpr std::size_t expected_size = sizeof(int64_t) + sizeof(UserInfo::Role) + sizeof(UserInfo::Subgroup) + 
                                          sizeof(uint8_t);

    REQUIRE(UserInfo_partial_size == expected_size);
}

TEST_CASE("serial_size(const UserInfo& info)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name";
        UserInfo info{n, 1, UserRole::Member, 2, false};

        constexpr std::size_t expected_size = UserInfo_partial_size + 10;
        REQUIRE(serial_size(info) == expected_size);
    }

    SECTION("nullptr name")
    {
        UserInfo info{nullptr, 1, UserRole::Member, 2, false};

        constexpr std::size_t expected_size = UserInfo_partial_size + 1; // + 1 for null terminator.
        REQUIRE(serial_size(info) == expected_size);
    }
}

static uint8_t* RequirePlayerInfo(const UserInfo& info, uint8_t* storage)
{
    uint8_t* location = storage;
    const std::size_t str_count = serial_size(info) - UserInfo_partial_size - 1;

    location = RequireStringAtLocation(location, info.AccountName, str_count);
    location = RequireAtLocation(location, static_cast<int64_t>(info.JoinTime));
    location = RequireAtLocation(location, static_cast<std::underlying_type_t<UserRole>>(info.Role));
    location = RequireAtLocation(location, info.Subgroup);
    location = RequireAtLocation(location, static_cast<uint8_t>(info.ReadyStatus));

    return location;
}

TEST_CASE("to_serial(const UserInfo& info, uint8_t* storage, std::size_t)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name"; // Size includes null terminator.
        UserInfo info{n, 1, UserRole::Member, 2, false};

        constexpr std::size_t userinfo_size = UserInfo_partial_size + 10;

        uint8_t storage[userinfo_size] = {};
        to_serial(info, storage, userinfo_size);

        uint8_t* location = RequirePlayerInfo(info, storage);
        REQUIRE(storage + userinfo_size == location);
    }

    SECTION("Empty name")
    {
        UserInfo info{nullptr, 1, UserRole::Member, 2, false};
        constexpr std::size_t userinfo_size = UserInfo_partial_size + 1; // + 1 for null terminator.

        uint8_t storage[userinfo_size] = {};
        to_serial(info, storage, userinfo_size);

        uint8_t* location = RequirePlayerInfo(info, storage);
        REQUIRE(storage + userinfo_size == location);
    }
}

//
// json (UserInfo).
//

static std::string UserInfoStrJSON(const UserInfo& info)
{
    std::ostringstream oss{};

    const std::string a = (info.AccountName) ? "\"" + std::string{info.AccountName} + "\"" : "null";

    oss << "{"
        << "\"AccountName\":" << a << ","
        << "\"JoinTime\":" << static_cast<int64_t>(info.JoinTime) << ","
        << "\"ReadyStatus\":" << ((info.ReadyStatus) ? "true" : "false") << ","
        << "\"Role\":" << static_cast<uint32_t>(info.Role) << ","
        << "\"Subgroup\":" << static_cast<int64_t>(info.Subgroup) << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const UserInfo& user)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name"; // Size includes null terminator.
        UserInfo info{n, 1, UserRole::Member, 2, false};

        nlohmann::json j = info;
        REQUIRE(j.dump() == UserInfoStrJSON(info));
    }

    SECTION("Empty name")
    {
        UserInfo info{nullptr, 1, UserRole::Member, 2, false};

        nlohmann::json j = info;
        REQUIRE(j.dump() == UserInfoStrJSON(info));
    }
}

//
// Budget fuzzing (UserInfo).
//

struct UserInfoNode : Node
{
    UserInfoNode(std::optional<std::string> user_name, const UserInfo& info)
        : name{std::move(user_name)}, value{info}
    {
        if (name)
            value.AccountName = name->c_str();
    }

    std::optional<std::string> name;
    UserInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override
    {
        return RequirePlayerInfo(value, storage);
    }
    [[nodiscard]] std::size_t count() const override
    {
        return serial_size(value);
    }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == UserInfoStrJSON(value));
    }
};

static UserInfo RandomUserInfo()
{
    int64_t jointime = RandomIntegral<int64_t>();
    auto role = RandomIntegral<std::underlying_type_t<UserRole>>() % 6;
    uint8_t subgroup = RandomIntegral<uint8_t>();
    uint8_t ready = RandomIntegral<uint8_t>() & 2;

    return {nullptr, jointime, static_cast<UserRole>(role), subgroup, static_cast<bool>(ready)};
}

static std::unique_ptr<UserInfoNode> UserInfoNodeCreator()
{
    std::optional<std::string> user_name = OptionalRandomString();
    return std::make_unique<UserInfoNode>(user_name, RandomUserInfo());
}

TEST_CASE("Budget fuzzing (only UserInfo)")
{
    BudgetFuzzer<32, 1024, 2>([]() { 
        return UserInfoNodeCreator(); 
    });
}

///////////////////////////////////////////////////////////////////////////////
//                                 Language                                  //
///////////////////////////////////////////////////////////////////////////////

//
// serial (Language).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("serial_size(Language)")
{
    constexpr std::size_t expected_size = sizeof(int32_t);

    REQUIRE(serial_size(Language{}) == expected_size);
}

static uint8_t* RequireLanguage(Language language, uint8_t* storage)
{
    return RequireAtLocation(storage, static_cast<std::underlying_type_t<Language>>(language));
}

static void ValidateLanguageSerialValue(Language language)
{
    constexpr std::size_t language_size = serial_size(Language{});

    uint8_t storage[language_size] = {};
    to_serial(language, storage, language_size);

    uint8_t* location = RequireLanguage(language, storage);
    REQUIRE(storage + language_size == location);
}

TEST_CASE("to_serial(Language language, uint8_t* storage, std::size_t)")
{
    ValidateLanguageSerialValue(Language::English);
    ValidateLanguageSerialValue(Language::French);
    ValidateLanguageSerialValue(Language::German);
    ValidateLanguageSerialValue(Language::Spanish);
    ValidateLanguageSerialValue(Language::Chinese);
}

//
// json (Language).
//

static std::string LanguageStrJSON(Language language)
{
    std::ostringstream oss{};

    oss << "{"
        << "\"Language\":" << static_cast<std::underlying_type_t<Language>>(language)
        << "}";

    return oss.str();
}

static void ValidateLanguageJSON(Language language)
{
    nlohmann::json j = language;
    REQUIRE(j.dump() == LanguageStrJSON(language));
}

TEST_CASE("to_json(nlohmann::json& j, Language language)")
{
    ValidateLanguageJSON(Language::English);
    ValidateLanguageJSON(Language::French);
    ValidateLanguageJSON(Language::German);
    ValidateLanguageJSON(Language::Spanish);
    ValidateLanguageJSON(Language::Chinese);
}

///////////////////////////////////////////////////////////////////////////////
//                                 KeyBind                                   //
///////////////////////////////////////////////////////////////////////////////

//
// serial (KeyBind).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("serial_size(KeyBinds::KeyBindChanged)")
{
    constexpr std::size_t expected_size = sizeof(int32_t) + sizeof(uint32_t) + (3 * sizeof(int32_t));

    REQUIRE(serial_size(KeyBinds::KeyBindChanged{}) == expected_size);
}

static uint8_t* RequireKeyBindChanged(const KeyBinds::KeyBindChanged& keybind, uint8_t* storage)
{
    uint8_t* location = storage;

    const auto keyControl = static_cast<std::underlying_type_t<KeyBinds::KeyControl>>(keybind.KeyControl);
    location = RequireAtLocation(location, keyControl);
    location = RequireAtLocation(location, keybind.KeyIndex);

    const auto deviceType = static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(keybind.SingleKey.DeviceType);
    location = RequireAtLocation(location, deviceType);
    location = RequireAtLocation(location, keybind.SingleKey.Code);
    location = RequireAtLocation(location, keybind.SingleKey.Modifier);

    return location;
}

TEST_CASE("to_serial(const KeyBinds::KeyBindChanged&, uint8_t*, std::size_t)")
{
    KeyBinds::KeyBindChanged keyChanged{
        KeyBinds::KeyControl::Movement_MoveForward, 3, 
        {KeyBinds::DeviceType::Keyboard, 4, 1}
    };

    constexpr std::size_t keybind_size = serial_size(KeyBinds::KeyBindChanged{});
    uint8_t storage[keybind_size] = {};
    to_serial(keyChanged, storage, keybind_size);

    uint8_t* location = RequireKeyBindChanged(keyChanged, storage);
    REQUIRE(storage + keybind_size == location);
}

//
// json (KeyBind).
//

static std::string KeyBindChangedStrJSON(const KeyBinds::KeyBindChanged& keyChanged)
{
    std::ostringstream oss{};

    oss << "{"
        << "\"KeyControl\":" << static_cast<std::underlying_type_t<KeyBinds::KeyControl>>(keyChanged.KeyControl) << ","
        << "\"KeyIndex\":" << keyChanged.KeyIndex << ","
        << "\"SingleKey\":" << "{" 
        << "\"Code\":" << keyChanged.SingleKey.Code << ","
        << "\"DeviceType\":" << static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(keyChanged.SingleKey.DeviceType) << ","
        << "\"Modifier\":" << keyChanged.SingleKey.Modifier
        << "}}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json&, const KeyBinds::KeyBindChanged&)")
{
    KeyBinds::KeyBindChanged keyChanged{
        KeyBinds::KeyControl::Movement_MoveForward, 3, 
        {KeyBinds::DeviceType::Keyboard, 4, 1}
    };

    nlohmann::json j;
    to_json(j, keyChanged);
    REQUIRE(j.dump() == KeyBindChangedStrJSON(keyChanged));
}

///////////////////////////////////////////////////////////////////////////////
//                             ChatMessageInfo                               //
///////////////////////////////////////////////////////////////////////////////

//
// serial (ChatMessageInfo).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("ChatMessageInfo_partial_size")
{
    constexpr std::size_t expected_size = sizeof(uint32_t) + (3 * sizeof(uint8_t));

    REQUIRE(ChatMessageInfo_partial_size == expected_size);
}

// It's important this value does not change (breaks version compatibility).
TEST_CASE("serial_size(const ChatMessageInfo&)")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    constexpr std::size_t expected_size = ChatMessageInfo_partial_size + 25 + 19 + 20 + 16;

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    REQUIRE(serial_size(chatMsgInfo) == expected_size);
}

static uint8_t* RequireChatMessageInfo(const ChatMessageInfo& chatMsgInfo, uint8_t* storage)
{
    uint8_t* location = storage;

    location = RequireAtLocation(location, chatMsgInfo.ChannelId);
    location = RequireAtLocation(location, static_cast<std::underlying_type_t<ChannelType>>(chatMsgInfo.Type));
    location = RequireAtLocation(location, chatMsgInfo.Subgroup);
    location = RequireAtLocation(location, chatMsgInfo.IsBroadcast);

    location = RequireStringAtLocation(location, chatMsgInfo.Timestamp, chatMsgInfo.TimestampLength);
    location = RequireStringAtLocation(location, chatMsgInfo.AccountName, chatMsgInfo.AccountNameLength);
    location = RequireStringAtLocation(location, chatMsgInfo.CharacterName, chatMsgInfo.CharacterNameLength);
    location = RequireStringAtLocation(location, chatMsgInfo.Text, chatMsgInfo.TextLength);

    return location;
}

TEST_CASE("to_serial(const ChatMessageInfo&, uint8_t* storage, std::size_t)")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    constexpr std::size_t chat_msg_info_size = ChatMessageInfo_partial_size + 25 + 19 + 20 + 16;

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    uint8_t storage[chat_msg_info_size] = {};
    to_serial(chatMsgInfo, storage, chat_msg_info_size);

    uint8_t* location = RequireChatMessageInfo(chatMsgInfo, storage);
    REQUIRE(storage + chat_msg_info_size == location);
}

//
// json (ChatMessageInfo).
//

static std::string ChatMessageInfoStrJSON(const ChatMessageInfo& chatMsgInfo)
{
    std::ostringstream oss{};

    const std::string timestamp = (chatMsgInfo.Timestamp) ? "\"" + std::string{chatMsgInfo.Timestamp} + "\"" : "null";
    const std::string accName = (chatMsgInfo.AccountName) ? "\"" + std::string{chatMsgInfo.AccountName} + "\"" : "null";
    const std::string charName = (chatMsgInfo.CharacterName) ? "\"" + std::string{chatMsgInfo.CharacterName} + "\"" : "null";
    const std::string text = (chatMsgInfo.Text) ? "\"" + std::string{chatMsgInfo.Text} + "\"" : "null";
    const uint32_t type = static_cast<uint32_t>(static_cast<std::underlying_type_t<ChannelType>>(chatMsgInfo.Type));

    oss << "{"
        << "\"AccountName\":" << accName << ","
        << "\"ChannelId\":" << chatMsgInfo.ChannelId << ","
        << "\"CharacterName\":" << charName << ","
        << "\"IsBroadcast\":" << static_cast<uint32_t>(chatMsgInfo.IsBroadcast) << ","
        << "\"Subgroup\":" << static_cast<uint32_t>(chatMsgInfo.Subgroup) << ","
        << "\"Text\":" << text << ","
        << "\"Timestamp\":" << timestamp << ","
        << "\"Type\":" << type
        << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const ChatMessageInfo&)")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    nlohmann::json j = chatMsgInfo;
    REQUIRE(j.dump() == ChatMessageInfoStrJSON(chatMsgInfo));
}

//
// Budget fuzzing (ChatMessageInfo).
//

struct ChatMessageInfoNode : Node
{
    ChatMessageInfoNode(std::optional<std::string> at, std::optional<std::string> acc,
                        std::optional<std::string> ch, std::optional<std::string> str, ChatMessageInfo& info)
        : timestamp{std::move(at)}, accountName{std::move(acc)}, charName{std::move(ch)}, text{std::move(str)}, value{info}
    {
        if (timestamp)
        {
            value.Timestamp = timestamp->c_str();
            value.TimestampLength = timestamp->size();
        }
        
        if (accountName)
        {
            value.AccountName = accountName->c_str();
            value.AccountNameLength = accountName->size();
        }
        
        if (charName)
        {
            value.CharacterName = charName->c_str();
            value.CharacterNameLength = charName->size();
        }

        if (text)
        {
            value.Text = text->c_str();
            value.TextLength = text->size();
        }
    }

    std::optional<std::string> timestamp;
    std::optional<std::string> accountName;
    std::optional<std::string> charName;
    std::optional<std::string> text;
    ChatMessageInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override
    {
        return RequireChatMessageInfo(value, storage);
    }
    [[nodiscard]] std::size_t count() const override
    {
        return serial_size(value);
    }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == ChatMessageInfoStrJSON(value));
    }
};

static ChatMessageInfo RandomChatMessageInfo()
{
    uint32_t channelId = RandomIntegral<uint32_t>();
    auto type = RandomIntegral<std::underlying_type_t<ChannelType>>() % 4;
    uint8_t subgroup = RandomIntegral<uint8_t>();
    uint8_t isBroadcast = RandomIntegral<uint8_t>() & 2;

    return {channelId, static_cast<ChannelType>(type), subgroup, isBroadcast, 0, nullptr, 0, nullptr, 0, nullptr, 0, 
            nullptr, 0};
}

static std::unique_ptr<ChatMessageInfoNode> ChatMessageInfoNodeCreator()
{
    std::optional<std::string> at = OptionalRandomString();
    std::optional<std::string> acc = OptionalRandomString();
    std::optional<std::string> ch = OptionalRandomString();
    std::optional<std::string> str = OptionalRandomString();
    auto chatMsgInfo = RandomChatMessageInfo();
    return std::make_unique<ChatMessageInfoNode>(at, acc, ch, str, chatMsgInfo);
}

TEST_CASE("Budget fuzzing (only ChatMessageInfo)")
{
    BudgetFuzzer<32, 1024, 2>([]() {
        return ChatMessageInfoNodeCreator(); 
    });
}
