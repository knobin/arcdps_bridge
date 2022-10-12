//
//  tests/AppDataTest.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-21.
//

// Catch2 Headers
#include "catch2/catch_test_macros.hpp"

// Local Headers
#include "FuzzHelper.hpp"

// Bridge Headers
#include "ApplicationData.hpp"

// C++ Headers
#include <sstream>

///////////////////////////////////////////////////////////////////////////////
//                                BridgeInfo                                 //
///////////////////////////////////////////////////////////////////////////////

//
// serial (BridgeInfo).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("BridgeInfo Minimal Size")
{
    BridgeInfo info{};

    constexpr std::size_t partial_size = sizeof(uint64_t) + (3 * sizeof(uint32_t)) + (3 * sizeof(uint8_t));
    constexpr std::string_view version{BRIDGE_VERSION_STR};
    constexpr std::size_t version_size = version.size() + 1; // Including null terminator.
    constexpr std::size_t extrasVersion_size = 1;            // Only null terminator.
    constexpr std::size_t arcvers_size = 1;                  // Only null terminator.

    constexpr std::size_t expected_size = partial_size + version_size + extrasVersion_size + arcvers_size;
    REQUIRE(serial_size(info) == expected_size);
}

TEST_CASE("serial_size(const BridgeInfo& info)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcvers = "arc version string";

    constexpr std::size_t partial_size = sizeof(uint64_t) + (3 * sizeof(uint32_t)) + (3 * sizeof(uint8_t));
    constexpr std::string_view version{BRIDGE_VERSION_STR};
    constexpr std::size_t version_size = version.size() + 1; // Including null terminator.
    constexpr std::size_t extrasVersion_size = 22;           // Including null terminator.
    constexpr std::size_t arcvers_size = 19;                 // Including null terminator.

    constexpr std::size_t expected_size = partial_size + version_size + extrasVersion_size + arcvers_size;
    REQUIRE(serial_size(info) == expected_size);
}

static uint8_t* RequireBridgeInfo(const BridgeInfo& info, uint8_t* storage)
{
    uint8_t* location = storage;

    location = RequireAtLocation(location, info.majorApiVersion);
    location = RequireAtLocation(location, info.minorApiVersion);

    location = RequireAtLocation(location, info.validator);

    location = RequireStringAtLocation(location, info.version.data(), info.version.size());
    location = RequireStringAtLocation(location, info.extrasVersion.data(), info.extrasVersion.size());
    location = RequireStringAtLocation(location, info.arcvers.data(), info.arcvers.size());

    location = RequireAtLocation(location, info.extrasInfoVersion);

    location = RequireAtLocation(location, static_cast<uint8_t>(info.arcLoaded));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.extrasFound));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.extrasLoaded));

    return location;
}

TEST_CASE("to_serial(const BridgeInfo& info, uint8_t* storage, std::size_t)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcvers = "arc version string";

    constexpr std::size_t partial_size = sizeof(uint64_t) + (3 * sizeof(uint32_t)) + (3 * sizeof(uint8_t));
    constexpr std::string_view version{BRIDGE_VERSION_STR};
    constexpr std::size_t version_size = version.size() + 1; // Including null terminator.
    constexpr std::size_t extrasVersion_size = 22;           // Including null terminator.
    constexpr std::size_t arcvers_size = 19;                 // Including null terminator.

    constexpr std::size_t bridgeinfo_size = partial_size + version_size + extrasVersion_size + arcvers_size;

    uint8_t storage[bridgeinfo_size] = {};
    to_serial(info, storage, bridgeinfo_size);

    uint8_t* location = RequireBridgeInfo(info, storage);
    REQUIRE(storage + bridgeinfo_size == location);
}

//
// json (BridgeInfo).
//

static std::string BridgeInfoStrJSON(const BridgeInfo& info)
{
    std::ostringstream oss{};

    const std::string eV = (!info.extrasVersion.empty()) ? "\"" + info.extrasVersion + "\"" : "null";
    const std::string aV = (!info.arcvers.empty()) ? "\"" + info.arcvers + "\"" : "null";
    const std::string version = "\"" + std::string{info.version} + "\"";

    oss << "{"
        << "\"arcLoaded\":" << ((info.arcLoaded) ? "true" : "false") << ","
        << "\"arcVersion\":" << aV << ","
        << "\"extrasFound\":" << ((info.extrasFound) ? "true" : "false") << ","
        << "\"extrasInfoVersion\":" << info.extrasInfoVersion << ","
        << "\"extrasLoaded\":" << ((info.extrasLoaded) ? "true" : "false") << ","
        << "\"extrasVersion\":" << eV << ","
        << "\"majorApiVersion\":" << info.majorApiVersion << ","
        << "\"minorApiVersion\":" << info.minorApiVersion << ","
        << "\"validator\":" << info.validator << ","
        << "\"version\":" << version << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const UserInfo& user)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcvers = "arc version string";

    nlohmann::json j = info;
    REQUIRE(j.dump() == BridgeInfoStrJSON(info));
}

//
// Budget fuzzing (BridgeInfo).
//

struct BridgeInfoNode : Node
{
    BridgeInfoNode(const std::string& extraVersion, const std::string& arcVersion, uint64_t validator, uint32_t major,
                   uint32_t minor, uint32_t infoVersion, bool arcL, bool extrasF, bool extrasL)
        : value{}
    {
        value.extrasVersion = extraVersion;
        value.arcvers = arcVersion;
        value.validator = validator;
        value.majorApiVersion = major;
        value.minorApiVersion = minor;
        value.extrasInfoVersion = infoVersion;
        value.arcLoaded = arcL;
        value.extrasFound = extrasF;
        value.extrasLoaded = extrasL;
    }

    BridgeInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override { return RequireBridgeInfo(value, storage); }
    std::size_t count() const override { return serial_size(value); }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == BridgeInfoStrJSON(value));
    }
};

static std::unique_ptr<BridgeInfoNode> BridgeInfoNodeCreator()
{
    std::string eV = RandomString();
    std::string aV = RandomString();

    uint64_t va = RandomIntegral<uint64_t>();
    uint32_t major = RandomIntegral<uint32_t>();
    uint32_t minor = RandomIntegral<uint32_t>();
    uint32_t infoVersion = RandomIntegral<uint32_t>();

    bool aL = static_cast<bool>(RandomIntegral<uint32_t>() & 2);
    bool eF = static_cast<bool>(RandomIntegral<uint32_t>() & 2);
    bool eL = static_cast<bool>(RandomIntegral<uint32_t>() & 2);

    std::string user_name = RandomString();
    return std::make_unique<BridgeInfoNode>(eV, aV, va, major, minor, infoVersion, aL, eF, eL);
}

TEST_CASE("Budget fuzzing (only BridgeInfo)")
{
    BudgetFuzzer<16, 512, 2>([]() {
        return BridgeInfoNodeCreator();
    });
}
