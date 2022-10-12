//
//  tests/SquadTest.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-10.
//

// Catch2 Headers
#include "catch2/catch_test_macros.hpp"

// Local Headers
#include "FuzzHelper.hpp"

// Bridge Headers
#include "PlayerContainer.hpp"

// C++ Headers
#include <sstream>
#include <utility>

//
// PLAYER_VALIDATOR_START.
//

// Might break version compatibility if changed.
TEST_CASE("PLAYER_VALIDATOR_START")
{
    REQUIRE(PLAYER_VALIDATOR_START == 1);
}

///////////////////////////////////////////////////////////////////////////////
//                                PlayerInfo                                 //
///////////////////////////////////////////////////////////////////////////////

//
// serial (PlayerInfo).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("PlayerInfo_partial_size")
{
    constexpr std::size_t expected_size =
        sizeof(int64_t) + (2 * sizeof(uint32_t)) + (2 * sizeof(uint8_t)) + (3 * sizeof(uint8_t));

    REQUIRE(PlayerInfo_partial_size == expected_size);
}

TEST_CASE("serial_size(const PlayerInfo& info)")
{
    SECTION("Valid name")
    {
        char a[12] = "AccountName";
        char c[14] = "CharacterName";
        PlayerInfo info{std::string{a}, std::string{c}, 1, 2, 3, 4, 5, false, false, false};

        constexpr std::size_t expected_size = PlayerInfo_partial_size + 12 + 14;
        REQUIRE(serial_size(info) == expected_size);
    }

    SECTION("nullptr name")
    {
        PlayerInfo info{std::string{}, std::string{}, 1, 2, 3, 4, 5, false, false, false};

        constexpr std::size_t expected_size = PlayerInfo_partial_size + 2; // + 2 for null terminators.
        REQUIRE(serial_size(info) == expected_size);
    }
}

static uint8_t* RequirePlayerInfo(const PlayerInfo& info, uint8_t* storage)
{
    uint8_t* location = storage;

    location = RequireStringAtLocation(location, info.accountName.c_str(), info.accountName.size());
    location = RequireStringAtLocation(location, info.characterName.c_str(), info.characterName.size());

    location = RequireAtLocation(location, static_cast<int64_t>(info.joinTime));
    location = RequireAtLocation(location, static_cast<uint32_t>(info.profession));
    location = RequireAtLocation(location, static_cast<uint32_t>(info.elite));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.role));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.subgroup));

    location = RequireAtLocation(location, static_cast<uint8_t>(info.inInstance));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.self));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.readyStatus));

    return location;
}

TEST_CASE("to_serial(const PlayerInfo& info, uint8_t* storage, std::size_t)")
{
    SECTION("Valid name")
    {
        char a[12] = "AccountName";
        char c[14] = "CharacterName";

        PlayerInfo info{std::string{a}, std::string{c}, 1, 2, 3, 4, 5, false, false, false};
        constexpr std::size_t playerinfo_size = PlayerInfo_partial_size + 12 + 14;

        uint8_t storage[playerinfo_size] = {};
        to_serial(info, storage, playerinfo_size);

        auto location = RequirePlayerInfo(info, storage);
        REQUIRE(storage + playerinfo_size == location);
    }

    SECTION("nullptr name")
    {
        PlayerInfo info{std::string{}, std::string{}, 1, 2, 3, 4, 5, true, true, true};
        constexpr std::size_t playerinfo_size = PlayerInfo_partial_size + 1 + 1;

        uint8_t storage[playerinfo_size] = {};
        to_serial(info, storage, playerinfo_size);

        auto location = RequirePlayerInfo(info, storage);
        REQUIRE(storage + playerinfo_size == location);
    }
}

//
// json (PlayerInfo).
//

static std::string PlayerInfoStrJSON(const PlayerInfo& info)
{
    std::ostringstream oss{};

    const std::string c = (!info.characterName.empty()) ? "\"" + info.characterName + "\"" : "null";

    oss << "{"
        << "\"accountName\":\"" << info.accountName << "\","
        << "\"characterName\":" << c << ","
        << "\"elite\":" << info.elite << ","
        << "\"inInstance\":" << ((info.inInstance) ? "true" : "false") << ","
        << "\"joinTime\":" << info.joinTime << ","
        << "\"profession\":" << info.profession << ","
        << "\"readyStatus\":" << ((info.readyStatus) ? "true" : "false") << ","
        << "\"role\":" << static_cast<uint32_t>(info.role) << ","
        << "\"self\":" << ((info.self) ? "true" : "false") << ","
        << "\"subgroup\":" << static_cast<uint32_t>(info.subgroup) << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const PlayerInfo& info)")
{
    SECTION("Valid name")
    {
        char a[12] = "AccountName";
        char c[14] = "CharacterName";
        PlayerInfo info{std::string{a}, std::string{c}, 1, 2, 3, 4, 5, true, false, true};

        nlohmann::json j = info;

        REQUIRE(j.dump() == PlayerInfoStrJSON(info));
    }

    SECTION("Empty name")
    {
        // Account name is assumed to always exist.

        char a[12] = "AccountName";
        PlayerInfo info{std::string{a}, std::string{}, 1, 2, 3, 4, 5, false, true, false};

        nlohmann::json j = info;

        REQUIRE(j.dump() == PlayerInfoStrJSON(info));
    }
}

//
// Budget fuzzing (PlayerInfo).
//

struct PlayerInfoNode : Node
{
    explicit constexpr PlayerInfoNode(const PlayerInfo& info)
        : value{info}
    {}

    PlayerInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override { return RequirePlayerInfo(value, storage); }
    [[nodiscard]] std::size_t count() const override { return serial_size(value); }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == PlayerInfoStrJSON(value));
    }
};

static PlayerInfo RandomPlayerInfo()
{
    std::string a{RandomString()};
    std::string c{RandomString()};

    int64_t jointime = RandomIntegral<int64_t>();
    uint32_t profession = RandomIntegral<uint32_t>();
    uint32_t elite = RandomIntegral<uint32_t>();
    uint8_t role = RandomIntegral<uint8_t>() % 6;
    uint8_t subgroup = RandomIntegral<uint8_t>();

    bool inInstance = static_cast<bool>(RandomIntegral<uint8_t>() % 2);
    bool self = static_cast<bool>(RandomIntegral<uint8_t>() % 2);
    bool ready = static_cast<bool>(RandomIntegral<uint8_t>() % 2);

    return {a, c, jointime, profession, elite, role, subgroup, inInstance, self, ready};
}

static std::unique_ptr<PlayerInfoNode> PlayerInfoNodeCreator()
{
    return std::make_unique<PlayerInfoNode>(RandomPlayerInfo());
}

TEST_CASE("Budget fuzzing (only PlayerInfo)")
{
    BudgetFuzzer<32, 1024, 2>([]() {
        return PlayerInfoNodeCreator();
    });
}

///////////////////////////////////////////////////////////////////////////////
//                              PlayerInfoEntry                              //
///////////////////////////////////////////////////////////////////////////////

//
// serial (PlayerInfoEntry).
//

TEST_CASE("serial_size(const PlayerInfoEntry& entry)")
{
    char a[12] = "AccountName";
    char c[14] = "CharacterName";
    PlayerInfo info{std::string{a}, std::string{c}, 1, 2, 3, 4, 5, false, false, false};
    PlayerInfoEntry entry{info, 1};

    constexpr std::size_t playerinfo_size = PlayerInfo_partial_size + 12 + 14;
    constexpr std::size_t expected_size = playerinfo_size + sizeof(std::size_t);
    REQUIRE(serial_size(entry) == expected_size);
}

static uint8_t* RequirePlayerInfoEntry(const PlayerInfoEntry& entry, uint8_t* storage)
{
    uint8_t* location = storage;

    location = RequirePlayerInfo(entry.player, location);
    location = RequireAtLocation(location, entry.validator);

    return location;
}

TEST_CASE("to_serial(const PlayerInfoEntry& entry, uint8_t* storage, std::size_t)")
{
    char a[12] = "AccountName";
    char c[14] = "CharacterName";

    PlayerInfo info{std::string{a}, std::string{c}, 1, 2, 3, 4, 5, false, false, false};
    constexpr std::size_t playerinfo_size = PlayerInfo_partial_size + 12 + 14;

    PlayerInfoEntry entry{info, 1};
    constexpr std::size_t playerinfoentry_size = playerinfo_size + sizeof(uint64_t);

    uint8_t storage[playerinfoentry_size] = {};
    to_serial(entry, storage, playerinfoentry_size);

    auto location = RequirePlayerInfoEntry(entry, storage);
    REQUIRE(storage + playerinfoentry_size == location);
}

//
// json (PlayerInfoEntry).
//

static std::string PlayerInfoEntryStrJSON(const PlayerInfoEntry& entry)
{
    std::ostringstream oss{};

    oss << "{"
        << "\"player\":" << PlayerInfoStrJSON(entry.player) << ","
        << "\"validator\":" << entry.validator << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const UserInfo& user)")
{
    char a[12] = "AccountName";
    char c[14] = "CharacterName";

    PlayerInfo info{std::string{a}, std::string{c}, 1, 2, 3, 4, 5, false, false, false};
    PlayerInfoEntry entry{info, 1};

    nlohmann::json j = entry;
    REQUIRE(j.dump() == PlayerInfoEntryStrJSON(entry));
}

//
// Budget fuzzing (PlayerInfoEntry).
//

struct PlayerInfoEntryNode : Node
{
    explicit constexpr PlayerInfoEntryNode(const PlayerInfoEntry& entry) noexcept
        : value{entry}
    {}

    PlayerInfoEntry value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override { return RequirePlayerInfoEntry(value, storage); }
    [[nodiscard]] std::size_t count() const override { return serial_size(value); }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == PlayerInfoEntryStrJSON(value));
    }
};

static PlayerInfoEntry RandomPlayerInfoEntry()
{
    return {RandomPlayerInfo(), RandomIntegral<decltype(PlayerInfoEntry::validator)>()};
}

static std::unique_ptr<PlayerInfoEntryNode> PlayerInfoEntryNodeCreator()
{
    return std::make_unique<PlayerInfoEntryNode>(RandomPlayerInfoEntry());
}

TEST_CASE("Budget fuzzing (only PlayerInfoEntryNode)")
{
    BudgetFuzzer<32, 1024, 2>([]() {
        return PlayerInfoEntryNodeCreator();
    });
}

///////////////////////////////////////////////////////////////////////////////
//                            PlayerContainer                                //
///////////////////////////////////////////////////////////////////////////////

//
// TODO: Create tests for Add/Update/Remove functions to ensure that the squad is correct.
//

//
// serial (PlayerContainer).
//

static uint8_t* RequirePlayerContainer(const std::vector<PlayerInfoEntry>& entries, uint8_t* storage,
                                       std::size_t padding = 0)
{
    uint8_t* location = storage + SerialStartPadding + padding; // Squad data includes start padding.

    location = RequireAtLocation(location, static_cast<uint64_t>(entries.size()));

    for (std::size_t i{0}; i < entries.size(); ++i)
        location = RequirePlayerInfoEntry(entries[i], location);

    return location;
}

static std::vector<PlayerInfoEntry> BasicPlayerContainer(PlayerContainer& squad)
{
    char a1[12] = "AccountName";
    char c1[14] = "CharacterName";
    PlayerInfo p1{std::string{a1}, std::string{c1}, 1, 2, 3, 4, 5, false, false, false};

    char a2[15] = "AccountName2nd";
    char c2[17] = "CharacterName2nd";
    PlayerInfo p2{std::string{a2}, std::string{c2}, 1, 2, 3, 4, 5, false, false, false};
    PlayerInfo p2_updated{std::string{a2}, std::string{c2}, 1, 2, 3, 4, 5, true, false, false};

    squad.add(p1);
    squad.add(p2);

    // Update the validator for p2 to be 2.
    if (auto entry = squad.find(p2.accountName))
    {
        entry->player = p2_updated;
        squad.update(*entry);
    }

    return {PlayerInfoEntry{p1, PLAYER_VALIDATOR_START}, PlayerInfoEntry{p2_updated, 2}};
}

TEST_CASE("toSerial(std::size_t startPadding)")
{
    // Get squad data.
    PlayerContainer squad{};
    auto entries = BasicPlayerContainer(squad);

    // No padding.
    SerialData serial = squad.toSerial();
    auto location = RequirePlayerContainer(entries, serial.ptr.get());
    REQUIRE(serial.ptr.get() + serial.count == location);

    // Some padding.
    SerialData serial_padding = squad.toSerial(5);
    location = RequirePlayerContainer(entries, serial_padding.ptr.get(), 5);
    REQUIRE(serial_padding.ptr.get() + serial_padding.count == location);
}

//
// json (PlayerContainer).
//

static std::string PlayerContainerStrJSON(const std::vector<PlayerInfoEntry>& entries)
{
    std::ostringstream oss{};

    oss << "{\"members\":[";
    for (std::size_t i{0}; i < entries.size(); ++i)
        oss << PlayerInfoEntryStrJSON(entries[i]) << ((i < entries.size() - 1) ? "," : "");
    oss << "]}";

    return oss.str();
}

TEST_CASE("toJSON()")
{
    // Get squad data.
    PlayerContainer squad{};
    auto entries = BasicPlayerContainer(squad);

    nlohmann::json j = squad.toJSON();

    REQUIRE(j.dump() == PlayerContainerStrJSON(entries));
}

//
// Budget fuzzing (PlayerContainer).
//

static std::vector<PlayerInfoEntry> RandomPlayerContainer(PlayerContainer& squad)
{
    const std::size_t player_count = RandomIntegral<std::size_t, 0, 50>();
    std::vector<PlayerInfoEntry> entries{};
    entries.reserve(player_count);

    // Create players.
    for (std::size_t j{0}; j < player_count; ++j)
    {
        PlayerInfo player = RandomPlayerInfo();

        auto present = std::find_if(entries.begin(), entries.end(), [&player](const auto& entry) {
            return entry.player.accountName == player.accountName;
        });

        if (present != entries.end())
        {
            // Account Name already exists, update values.
            if (auto entry = squad.find(player.accountName))
            {
                entry->player = player;
                squad.update(*entry);
            }
            present->player = player;
            ++present->validator;
        }
        else
        {
            // New Account Name.
            squad.add(player);
            entries.emplace_back(player, PLAYER_VALIDATOR_START);
        }
    }

    return entries;
}

TEST_CASE("Budget fuzzing (PlayerContainer)")
{
    const std::size_t tests = RandomIntegral<std::size_t, 0, 128>();
    for (std::size_t i{0}; i < tests; ++i)
    {
        // Random squad data.
        PlayerContainer squad{};
        auto entries = RandomPlayerContainer(squad);

        SECTION("Serial")
        {
            SerialData serial = squad.toSerial();
            auto location = RequirePlayerContainer(entries, serial.ptr.get());
            REQUIRE(serial.ptr.get() + serial.count == location);
        }

        SECTION("JSON")
        {
            nlohmann::json j = squad.toJSON();
            REQUIRE(j.dump() == PlayerContainerStrJSON(entries));
        }
    }
}