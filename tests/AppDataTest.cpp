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
//                                 AppData                                   //
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("AppData Request ID")
{
    ApplicationData appData{};
    REQUIRE(appData.requestID() == 1);
    REQUIRE(appData.requestID() == 2);
}

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

    constexpr std::size_t partial_size = sizeof(uint64_t) + sizeof(uint32_t) + (3 * sizeof(uint8_t));
    constexpr std::size_t extrasVersion_size = 1;            // Only null terminator.
    constexpr std::size_t arcVersion_size = 1;                  // Only null terminator.

    constexpr std::size_t expected_size = partial_size + extrasVersion_size + arcVersion_size;
    REQUIRE(SerialSize(info) == expected_size);
}

TEST_CASE("SerialSize(const BridgeInfo&)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcvers = "arc version string";

    constexpr std::size_t partial_size = sizeof(uint64_t) + sizeof(uint32_t) + (3 * sizeof(uint8_t));
    constexpr std::size_t extrasVersion_size = 22;  // Including null terminator.
    constexpr std::size_t arcVersion_size = 19;     // Including null terminator.

    constexpr std::size_t expected_size = partial_size + extrasVersion_size + arcVersion_size;
    REQUIRE(SerialSize(info) == expected_size);
}

static uint8_t* RequireBridgeInfo(const BridgeInfo& info, uint8_t* storage)
{
    uint8_t* location = storage;

    location = RequireAtLocation(location, info.validator);

    location = RequireStringAtLocation(location, info.extrasVersion.data(), info.extrasVersion.size());
    location = RequireStringAtLocation(location, info.arcvers.data(), info.arcvers.size());

    location = RequireAtLocation(location, info.extrasInfoVersion);

    location = RequireAtLocation(location, static_cast<uint8_t>(info.arcLoaded));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.extrasFound));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.extrasLoaded));

    return location;
}

TEST_CASE("ToSerial(const BridgeInfo&, uint8_t*, std::size_t)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcvers = "arc version string";

    constexpr std::size_t partial_size = sizeof(uint64_t) + sizeof(uint32_t) + (3 * sizeof(uint8_t));
    constexpr std::size_t extrasVersion_size = 22;  // Including null terminator.
    constexpr std::size_t arcVersion_size = 19;     // Including null terminator.

    constexpr std::size_t bridgeInfo_size = partial_size + extrasVersion_size + arcVersion_size;

    uint8_t storage[bridgeInfo_size] = {};
    ToSerial(info, storage, bridgeInfo_size);

    uint8_t* location = RequireBridgeInfo(info, storage);
    REQUIRE(storage + bridgeInfo_size == location);
}

//
// json (BridgeInfo).
//

static std::string BridgeInfoStrJSON(const BridgeInfo& info)
{
    std::ostringstream oss{};

    const std::string eV = (!info.extrasVersion.empty()) ? "\"" + info.extrasVersion + "\"" : "null";
    const std::string aV = (!info.arcvers.empty()) ? "\"" + info.arcvers + "\"" : "null";

    oss << "{"
        << "\"arcLoaded\":" << ((info.arcLoaded) ? "true" : "false") << ","
        << "\"arcVersion\":" << aV << ","
        << "\"extrasFound\":" << ((info.extrasFound) ? "true" : "false") << ","
        << "\"extrasInfoVersion\":" << info.extrasInfoVersion << ","
        << "\"extrasLoaded\":" << ((info.extrasLoaded) ? "true" : "false") << ","
        << "\"extrasVersion\":" << eV << ","
        << "\"validator\":" << info.validator << "}";

    return oss.str();
}

TEST_CASE("ToJSON(const UserInfo& user)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcvers = "arc version string";

    nlohmann::json j = ToJSON(info);
    REQUIRE(j.dump() == BridgeInfoStrJSON(info));
}

//
// Budget fuzzing (BridgeInfo).
//

struct BridgeInfoNode : Node
{
    BridgeInfoNode(const std::string& extraVersion, const std::string& arcVersion, uint64_t validator, uint32_t infoVersion, bool arcL, bool extrasF, bool extrasL)
        : value{}
    {
        value.extrasVersion = extraVersion;
        value.arcvers = arcVersion;
        value.validator = validator;
        value.extrasInfoVersion = infoVersion;
        value.arcLoaded = arcL;
        value.extrasFound = extrasF;
        value.extrasLoaded = extrasL;
    }

    BridgeInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = SerialSize(value);
        ToSerial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override { return RequireBridgeInfo(value, storage); }
    std::size_t count() const override { return SerialSize(value); }
    void json_require() override
    {
        nlohmann::json j = ToJSON(value);
        REQUIRE(j.dump() == BridgeInfoStrJSON(value));
    }
};

static std::unique_ptr<BridgeInfoNode> BridgeInfoNodeCreator()
{
    const std::string eV = RandomString();
    const std::string aV = RandomString();

    const auto va = RandomIntegral<uint64_t>();
    const auto infoVersion = RandomIntegral<uint32_t>();

    const bool aL = static_cast<bool>(RandomIntegral<uint32_t>() & 2);
    const bool eF = static_cast<bool>(RandomIntegral<uint32_t>() & 2);
    const bool eL = static_cast<bool>(RandomIntegral<uint32_t>() & 2);

    return std::make_unique<BridgeInfoNode>(eV, aV, va, infoVersion, aL, eF, eL);
}

TEST_CASE("Budget fuzzing (only BridgeInfo)")
{
    BudgetFuzzer<16, 512, 2>([]() {
        return BridgeInfoNodeCreator();
    });
}
