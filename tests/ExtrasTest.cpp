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
//                                 Helpers                                   //
///////////////////////////////////////////////////////////////////////////////

template <typename T>
struct GeneratorSpaceHelper
{
    using Type = T;
    static auto SerialSize(const T& value) { return Extras::SerialSize(value); }
    static auto ToSerial(const T& value, uint8_t* storage, std::size_t count)
    {
        return Extras::ToSerial(value, storage, count);
    }
    static auto ToJSON(const T& value) { return Extras::ToJSON(value); }
};

///////////////////////////////////////////////////////////////////////////////
//                                 UserInfo                                  //
///////////////////////////////////////////////////////////////////////////////

//
// serial (UserInfo).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("UserInfoPartialSize")
{
    constexpr std::size_t expected_size =
        sizeof(int64_t) + sizeof(UserInfo::Role) + sizeof(UserInfo::Subgroup) + sizeof(uint8_t);

    REQUIRE(Extras::UserInfoPartialSize == expected_size);
}

TEST_CASE("SerialSize(const UserInfo& info)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name";
        UserInfo info{n, 1, UserRole::Member, 2, false};

        constexpr std::size_t expected_size = Extras::UserInfoPartialSize + 10;
        REQUIRE(Extras::SerialSize(info) == expected_size);
    }

    SECTION("nullptr name")
    {
        UserInfo info{nullptr, 1, UserRole::Member, 2, false};

        constexpr std::size_t expected_size = Extras::UserInfoPartialSize + 1; // + 1 for null terminator.
        REQUIRE(Extras::SerialSize(info) == expected_size);
    }
}

static uint8_t* RequirePlayerInfo(const UserInfo& info, uint8_t* storage)
{
    uint8_t* location = storage;
    const std::size_t str_count = Extras::SerialSize(info) - Extras::UserInfoPartialSize - 1;

    location = RequireStringAtLocation(location, info.AccountName, str_count);
    location = RequireAtLocation(location, static_cast<int64_t>(info.JoinTime));
    location = RequireAtLocation(location, static_cast<std::underlying_type_t<UserRole>>(info.Role));
    location = RequireAtLocation(location, info.Subgroup);
    location = RequireAtLocation(location, static_cast<uint8_t>(info.ReadyStatus));

    return location;
}

TEST_CASE("ToSerial(const UserInfo& info, uint8_t* storage, std::size_t)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name"; // Size includes null terminator.
        UserInfo info{n, 1, UserRole::Member, 2, false};

        constexpr std::size_t userinfo_size = Extras::UserInfoPartialSize + 10;

        uint8_t storage[userinfo_size] = {};
        Extras::ToSerial(info, storage, userinfo_size);

        uint8_t* location = RequirePlayerInfo(info, storage);
        REQUIRE(storage + userinfo_size == location);
    }

    SECTION("Empty name")
    {
        UserInfo info{nullptr, 1, UserRole::Member, 2, false};
        constexpr std::size_t userinfo_size = Extras::UserInfoPartialSize + 1; // + 1 for null terminator.

        uint8_t storage[userinfo_size] = {};
        Extras::ToSerial(info, storage, userinfo_size);

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

TEST_CASE("ToJSON(nlohmann::json& j, const UserInfo& user)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name"; // Size includes null terminator.
        UserInfo info{n, 1, UserRole::Member, 2, false};

        const nlohmann::json j = Extras::ToJSON(info);
        REQUIRE(j.dump() == UserInfoStrJSON(info));
    }

    SECTION("Empty name")
    {
        UserInfo info{nullptr, 1, UserRole::Member, 2, false};

        const nlohmann::json j = Extras::ToJSON(info);
        REQUIRE(j.dump() == UserInfoStrJSON(info));
    }
}

//
// Generator (UserInfo).
//

TEST_CASE("Extras::SquadMessageGenerator")
{
    char n[10] = "Test Name"; // Size includes null terminator.
    UserInfo info{n, 1, UserRole::Member, 2, false};

    const uint64_t id = 10;
    const uint64_t timestamp = 12;

    RequireMessageGenerator<GeneratorSpaceHelper<UserInfo>, MessageCategory::Extras,
                            MessageType::ExtrasSquadUpdate>(id, timestamp, info, Extras::SquadMessageGenerator);
}

//
// Budget fuzzing (UserInfo).
//

struct UserInfoNode : Node
{
    UserInfoNode(std::optional<std::string> user_name, const UserInfo& info)
        : name{std::move(user_name)},
          value{info}
    {
        if (name)
            value.AccountName = name->c_str();
    }

    std::optional<std::string> name;
    UserInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = Extras::SerialSize(value);
        Extras::ToSerial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override { return RequirePlayerInfo(value, storage); }
    [[nodiscard]] std::size_t count() const override { return Extras::SerialSize(value); }
    void json_require() override
    {
        const nlohmann::json j = Extras::ToJSON(value);
        REQUIRE(j.dump() == UserInfoStrJSON(value));
    }
    void other() override
    {
        const uint64_t id = RandomIntegral<uint64_t>();
        const uint64_t timestamp = RandomIntegral<uint64_t>();

        RequireMessageGenerator<GeneratorSpaceHelper<UserInfo>, MessageCategory::Extras,
                                MessageType::ExtrasSquadUpdate>(id, timestamp, value, Extras::SquadMessageGenerator);
    }
};

static UserInfo RandomUserInfo()
{
    const int64_t jointime = RandomIntegral<int64_t>();
    auto role = RandomIntegral<std::underlying_type_t<UserRole>>() % 6;
    const uint8_t subgroup = RandomIntegral<uint8_t>();
    const uint8_t ready = RandomIntegral<uint8_t>() & 2;

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
TEST_CASE("SerialSize(Language)")
{
    constexpr std::size_t expected_size = sizeof(int32_t);

    REQUIRE(Extras::SerialSize(Language{}) == expected_size);
}

static uint8_t* RequireLanguage(Language language, uint8_t* storage)
{
    return RequireAtLocation(storage, static_cast<std::underlying_type_t<Language>>(language));
}

static void ValidateLanguageSerialValue(Language language)
{
    constexpr std::size_t language_size = Extras::SerialSize(Language{});

    uint8_t storage[language_size] = {};
    Extras::ToSerial(language, storage, language_size);

    uint8_t* location = RequireLanguage(language, storage);
    REQUIRE(storage + language_size == location);
}

TEST_CASE("ToSerial(Language language, uint8_t* storage, std::size_t)")
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
        << "\"Language\":" << static_cast<std::underlying_type_t<Language>>(language) << "}";

    return oss.str();
}

static void ValidateLanguageJSON(Language language)
{
    const nlohmann::json j = Extras::ToJSON(language);
    REQUIRE(j.dump() == LanguageStrJSON(language));
}

TEST_CASE("ToJSON(nlohmann::json& j, Language language)")
{
    ValidateLanguageJSON(Language::English);
    ValidateLanguageJSON(Language::French);
    ValidateLanguageJSON(Language::German);
    ValidateLanguageJSON(Language::Spanish);
    ValidateLanguageJSON(Language::Chinese);
}

//
// Generator (Language).
//

TEST_CASE("Extras::LanguageMessageGenerator")
{
    const uint64_t id = RandomIntegral<uint64_t>();
    const uint64_t timestamp = RandomIntegral<uint64_t>();

    RequireMessageGenerator<GeneratorSpaceHelper<Language>, MessageCategory::Extras,
                            MessageType::ExtrasLanguageChanged>(id, timestamp, Language::English, Extras::LanguageMessageGenerator);
}

///////////////////////////////////////////////////////////////////////////////
//                                 KeyBind                                   //
///////////////////////////////////////////////////////////////////////////////

//
// serial (KeyBind).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("SerialSize(KeyBinds::KeyBindChanged)")
{
    constexpr std::size_t expected_size = sizeof(int32_t) + sizeof(uint32_t) + (3 * sizeof(int32_t));

    REQUIRE(Extras::SerialSize(KeyBinds::KeyBindChanged{}) == expected_size);
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

TEST_CASE("ToSerial(const KeyBinds::KeyBindChanged&, uint8_t*, std::size_t)")
{
    KeyBinds::KeyBindChanged keyChanged{KeyBinds::KeyControl::Movement_MoveForward,
                                        3,
                                        {KeyBinds::DeviceType::Keyboard, 4, 1}};

    constexpr std::size_t keybind_size = Extras::SerialSize(KeyBinds::KeyBindChanged{});
    uint8_t storage[keybind_size] = {};
    Extras::ToSerial(keyChanged, storage, keybind_size);

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
        << "\"SingleKey\":"
        << "{"
        << "\"Code\":" << keyChanged.SingleKey.Code << ","
        << "\"DeviceType\":"
        << static_cast<std::underlying_type_t<KeyBinds::DeviceType>>(keyChanged.SingleKey.DeviceType) << ","
        << "\"Modifier\":" << keyChanged.SingleKey.Modifier << "}}";

    return oss.str();
}

TEST_CASE("ToJSON(nlohmann::json&, const KeyBinds::KeyBindChanged&)")
{
    KeyBinds::KeyBindChanged keyChanged{KeyBinds::KeyControl::Movement_MoveForward,
                                        3,
                                        {KeyBinds::DeviceType::Keyboard, 4, 1}};

    const nlohmann::json j = Extras::ToJSON(keyChanged);
    REQUIRE(j.dump() == KeyBindChangedStrJSON(keyChanged));
}

//
// Generator (KeyBind).
//

TEST_CASE("Extras::KeyBindMessageGenerator")
{
    KeyBinds::KeyBindChanged keyChanged{KeyBinds::KeyControl::Movement_MoveForward,
                                        3,
                                        {KeyBinds::DeviceType::Keyboard, 4, 1}};

    const uint64_t id = RandomIntegral<uint64_t>();
    const uint64_t timestamp = RandomIntegral<uint64_t>();

    RequireMessageGenerator<GeneratorSpaceHelper<KeyBinds::KeyBindChanged>, MessageCategory::Extras,
                            MessageType::ExtrasKeyBindChanged>(id, timestamp, keyChanged, Extras::KeyBindMessageGenerator);
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

    REQUIRE(Extras::ChatMessageInfoPartialSize == expected_size);
}

// It's important this value does not change (breaks version compatibility).
TEST_CASE("SerialSize(const ChatMessageInfo&)")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    constexpr std::size_t expected_size = Extras::ChatMessageInfoPartialSize + 25 + 19 + 20 + 16;

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    REQUIRE(Extras::SerialSize(chatMsgInfo) == expected_size);
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

TEST_CASE("ToSerial(const ChatMessageInfo&, uint8_t* storage, std::size_t)")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    constexpr std::size_t chat_msg_info_size = Extras::ChatMessageInfoPartialSize + 25 + 19 + 20 + 16;

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    uint8_t storage[chat_msg_info_size] = {};
    Extras::ToSerial(chatMsgInfo, storage, chat_msg_info_size);

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
    const std::string charName =
        (chatMsgInfo.CharacterName) ? "\"" + std::string{chatMsgInfo.CharacterName} + "\"" : "null";
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
        << "\"Type\":" << type << "}";

    return oss.str();
}

TEST_CASE("ToJSON(nlohmann::json& j, const ChatMessageInfo&)")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    const nlohmann::json j = Extras::ToJSON(chatMsgInfo);
    REQUIRE(j.dump() == ChatMessageInfoStrJSON(chatMsgInfo));
}

//
// Generator (ChatMessageInfo).
//

TEST_CASE("Extras::ChatMessageGenerator")
{
    char timestamp[25] = "2022-09-04T00:02:16.606Z";
    char accountName[19] = ":Test account name";
    char characterName[20] = "Test character name";
    char text[16] = "Test text input";

    ChatMessageInfo chatMsgInfo{4,  ChannelType::Invalid, 2,  1,    0, timestamp, 24, accountName,
                                18, characterName,        19, text, 15};

    const uint64_t id = RandomIntegral<uint64_t>();
    const uint64_t epochTimestamp = RandomIntegral<uint64_t>();

    RequireMessageGenerator<GeneratorSpaceHelper<ChatMessageInfo>, MessageCategory::Extras,
                            MessageType::ExtrasChatMessage>(id, epochTimestamp, chatMsgInfo, Extras::ChatMessageGenerator);
}

//
// Budget fuzzing (ChatMessageInfo).
//

struct ChatMessageInfoNode : Node
{
    ChatMessageInfoNode(std::optional<std::string> at, std::optional<std::string> acc, std::optional<std::string> ch,
                        std::optional<std::string> str, ChatMessageInfo& info)
        : timestamp{std::move(at)},
          accountName{std::move(acc)},
          charName{std::move(ch)},
          text{std::move(str)},
          value{info}
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
        const std::size_t count = Extras::SerialSize(value);
        Extras::ToSerial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override { return RequireChatMessageInfo(value, storage); }
    [[nodiscard]] std::size_t count() const override { return Extras::SerialSize(value); }
    void json_require() override
    {
        const nlohmann::json j = Extras::ToJSON(value);
        REQUIRE(j.dump() == ChatMessageInfoStrJSON(value));
    }
    void other() override
    {
        const uint64_t id = RandomIntegral<uint64_t>();
        const uint64_t timestamp = RandomIntegral<uint64_t>();

        RequireMessageGenerator<GeneratorSpaceHelper<ChatMessageInfo>, MessageCategory::Extras,
                                MessageType::ExtrasChatMessage>(id, timestamp, value, Extras::ChatMessageGenerator);
    }
};

static ChatMessageInfo RandomChatMessageInfo()
{
    uint32_t channelId = RandomIntegral<uint32_t>();
    auto type = RandomIntegral<std::underlying_type_t<ChannelType>>() % 4;
    uint8_t subgroup = RandomIntegral<uint8_t>();
    uint8_t isBroadcast = RandomIntegral<uint8_t>() & 2;

    return {channelId, static_cast<ChannelType>(type),
            subgroup,  isBroadcast,
            0,         nullptr,
            0,         nullptr,
            0,         nullptr,
            0,         nullptr,
            0};
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
