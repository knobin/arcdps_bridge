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
// BridgeInfo Serialization.
//

static void RequireBridgeInfo(const BridgeInfo& info, uint8_t* location)
{
    location = RequireAtLocation(location, info.validator);

    const auto evIndex{ReadIntegral<uint64_t>(location)};
    uint8_t *evLocation{location + evIndex};
    location += sizeof(uint64_t);
    location = RequireAtLocation(location, static_cast<uint32_t>(info.extrasVersion.size() + ((evIndex) ? 1 : 0)));
    if (evIndex > 0)
        RequireStringAtLocation(evLocation, info.extrasVersion.data(), static_cast<uint32_t>(info.extrasVersion.size()));

    const auto avIndex{ReadIntegral<uint64_t>(location)};
    uint8_t *avLocation{location + avIndex};
    location += sizeof(uint64_t);
    location = RequireAtLocation(location, static_cast<uint32_t>(info.arcVersion.size() + ((avIndex) ? 1 : 0)));
    if (avIndex > 0)
        RequireStringAtLocation(avLocation, info.arcVersion.data(), static_cast<uint32_t>(info.arcVersion.size()));

    location = RequireAtLocation(location, info.extrasInfoVersion);

    location = RequireAtLocation(location, static_cast<uint8_t>(info.arcLoaded));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.extrasFound));
    location = RequireAtLocation(location, static_cast<uint8_t>(info.extrasLoaded));
}

TEST_CASE("BridgeInfo Serialization")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcVersion = "arc version string";

    constexpr std::size_t evLength = 22; // Including null terminator.
    constexpr std::size_t avLength = 19; // Including null terminator.

    // It's important this value does not change (breaks version compatibility).
    SECTION("Fixed Size")
    {
        constexpr std::size_t variables = sizeof(uint64_t) + sizeof(uint32_t) + (3 * sizeof(uint8_t));
        constexpr std::size_t ev = sizeof(uint64_t) + sizeof(uint32_t); // Pointer + size.
        constexpr std::size_t av = sizeof(uint64_t) + sizeof(uint32_t); // Pointer + size.

        REQUIRE(BridgeInfoSerializer::fixedSize() == variables + ev + av);
    }

    SECTION("Dynamic Size")
    {
        BridgeInfoSerializer serializer{info};
        REQUIRE(serializer.dynamicSize() == evLength + avLength);
    }

    SECTION("Full Size")
    {
        BridgeInfoSerializer serializer{info};
        REQUIRE(serializer.size() == BridgeInfoSerializer::fixedSize() + serializer.dynamicSize());
    }

    SECTION("Write")
    {
        constexpr auto totalSize = BridgeInfoSerializer::fixedSize() + evLength + avLength;
        uint8_t storage[totalSize] = {};

        BridgeInfoSerializer serializer{info};
        MemoryLocation fixed{storage};
        MemoryLocation dynamic{storage + BridgeInfoSerializer::fixedSize()};

        serializer.writeToBuffers(fixed, dynamic);
        RequireBridgeInfo(info, storage);
    }
}

//
// BridgeInfo JSON.
//

static std::string BridgeInfoStrJSON(const BridgeInfo& info)
{
    std::ostringstream oss{};

    const std::string eV = (!info.extrasVersion.empty()) ? "\"" + info.extrasVersion + "\"" : "null";
    const std::string aV = (!info.arcVersion.empty()) ? "\"" + info.arcVersion + "\"" : "null";

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

TEST_CASE("ToJSON(const BridgeInfo& info)")
{
    BridgeInfo info{};
    info.extrasVersion = "extras version string";
    info.arcVersion = "arc version string";

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
        value.arcVersion = arcVersion;
        value.validator = validator;
        value.extrasInfoVersion = infoVersion;
        value.arcLoaded = arcL;
        value.extrasFound = extrasF;
        value.extrasLoaded = extrasL;
    }

    BridgeInfo value;

    uint8_t* write(uint8_t* storage) override
    {
        BridgeInfoSerializer serializer{value};
        MemoryLocation fixed{storage};
        MemoryLocation dynamic{storage + BridgeInfoSerializer::fixedSize()};
        serializer.writeToBuffers(fixed, dynamic);
        return storage + serializer.size();
    }
    uint8_t* require(uint8_t* storage) override
    {
        RequireBridgeInfo(value, storage);
        BridgeInfoSerializer serializer{value};
        return storage + serializer.size();
    }
    std::size_t count() const override
    {
        BridgeInfoSerializer serializer{value};
        return serializer.size();
    }
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
