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
    UserInfoNode(const std::string& user_name, const UserInfo& info) 
        : name{user_name}, value{info}
    {
        value.AccountName = name.c_str();
    }

    std::string name;
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
    std::size_t count() const override
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
    std::string user_name = RandomString();
    return std::make_unique<UserInfoNode>(user_name, RandomUserInfo());
}

TEST_CASE("Budget fuzzing (only UserInfo)")
{
    BudgetFuzzer<32, 1024, 2>([]() { 
        return UserInfoNodeCreator(); 
    });
}
