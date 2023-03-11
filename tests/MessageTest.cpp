//
//  tests/MessageTest.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-10.
//

// Catch2 Headers
#include "catch2/catch_test_macros.hpp"

// Local Headers
#include "FuzzHelper.hpp"

// Bridge Headers
#include "Message.hpp"

// C++ Headers
#include <functional>
#include <string_view>
#include <type_traits>

///////////////////////////////////////////////////////////////////////////////
//                             MessageCategory                               //
///////////////////////////////////////////////////////////////////////////////

// It's important this value does not change (breaks version compatibility).
TEST_CASE("MessageCategory values")
{
    using utype = std::underlying_type_t<MessageCategory>;

    REQUIRE(static_cast<utype>(MessageCategory::Info) == 1);
    REQUIRE(static_cast<utype>(MessageCategory::Combat) == 2);
    REQUIRE(static_cast<utype>(MessageCategory::Extras) == 4);
    REQUIRE(static_cast<utype>(MessageCategory::Squad) == 8);
}

TEST_CASE("MessageCategoryToStr")
{
    REQUIRE(MessageCategoryToStr(MessageCategory::Info) == "Info");
    REQUIRE(MessageCategoryToStr(MessageCategory::Combat) == "Combat");
    REQUIRE(MessageCategoryToStr(MessageCategory::Extras) == "Extras");
    REQUIRE(MessageCategoryToStr(MessageCategory::Squad) == "Squad");
}

///////////////////////////////////////////////////////////////////////////////
//                              MessageType                                  //
///////////////////////////////////////////////////////////////////////////////

// It's important this value does not change (breaks version compatibility).
TEST_CASE("MessageType values")
{
    using utype = std::underlying_type_t<MessageType>;

    SECTION("Info types")
    {
        REQUIRE(static_cast<utype>(MessageType::ConnectionStatus) == 1);
        REQUIRE(static_cast<utype>(MessageType::BridgeInfo) == 2);
        REQUIRE(static_cast<utype>(MessageType::Status) == 3);
        REQUIRE(static_cast<utype>(MessageType::Closing) == 4);
    }

    SECTION("ArcDPS combat api types")
    {
        REQUIRE(static_cast<utype>(MessageType::CombatEvent) == 5);
    }

    SECTION("Extras event types")
    {
        REQUIRE(static_cast<utype>(MessageType::ExtrasSquadUpdate) == 6);
        REQUIRE(static_cast<utype>(MessageType::ExtrasLanguageChanged) == 7);
        REQUIRE(static_cast<utype>(MessageType::ExtrasKeyBindChanged) == 8);
        REQUIRE(static_cast<utype>(MessageType::ExtrasChatMessage) == 9);
    }

    SECTION("Squad event types")
    {
        REQUIRE(static_cast<utype>(MessageType::SquadStatus) == 10);
        REQUIRE(static_cast<utype>(MessageType::SquadAdd) == 11);
        REQUIRE(static_cast<utype>(MessageType::SquadUpdate) == 12);
        REQUIRE(static_cast<utype>(MessageType::SquadRemove) == 13);
    }
}

TEST_CASE("MessageTypeToStr")
{
    REQUIRE(MessageTypeToStr(MessageType::ConnectionStatus) == "ConnectionStatus");
    REQUIRE(MessageTypeToStr(MessageType::BridgeInfo) == "BridgeInfo");
    REQUIRE(MessageTypeToStr(MessageType::Status) == "Status");
    REQUIRE(MessageTypeToStr(MessageType::Closing) == "Closing");
    REQUIRE(MessageTypeToStr(MessageType::CombatEvent) == "CombatEvent");
    REQUIRE(MessageTypeToStr(MessageType::ExtrasSquadUpdate) == "ExtrasSquadUpdate");
    REQUIRE(MessageTypeToStr(MessageType::ExtrasLanguageChanged) == "ExtrasLanguageChanged");
    REQUIRE(MessageTypeToStr(MessageType::ExtrasKeyBindChanged) == "ExtrasKeyBindChanged");
    REQUIRE(MessageTypeToStr(MessageType::ExtrasChatMessage) == "ExtrasChatMessage");
    REQUIRE(MessageTypeToStr(MessageType::SquadStatus) == "SquadStatus");
    REQUIRE(MessageTypeToStr(MessageType::SquadAdd) == "SquadAdd");
    REQUIRE(MessageTypeToStr(MessageType::SquadUpdate) == "SquadUpdate");
    REQUIRE(MessageTypeToStr(MessageType::SquadRemove) == "SquadRemove");
}

TEST_CASE("MessageTypeCount")
{
    REQUIRE(MessageTypeCount == 14);
}

TEST_CASE("MessageTypeStrings")
{
    REQUIRE(MessageTypeStrings[0] == "ConnectionStatus");
    REQUIRE(MessageTypeStrings[1] == "BridgeInfo");
    REQUIRE(MessageTypeStrings[2] == "Status");
    REQUIRE(MessageTypeStrings[3] == "Closing");
    REQUIRE(MessageTypeStrings[4] == "CombatEvent");
    REQUIRE(MessageTypeStrings[5] == "ExtrasSquadUpdate");
    REQUIRE(MessageTypeStrings[6] == "ExtrasLanguageChanged");
    REQUIRE(MessageTypeStrings[7] == "ExtrasKeyBindChanged");
    REQUIRE(MessageTypeStrings[8] == "ExtrasChatMessage");
    REQUIRE(MessageTypeStrings[9] == "SquadStatus");
    REQUIRE(MessageTypeStrings[10] == "SquadAdd");
    REQUIRE(MessageTypeStrings[11] == "SquadUpdate");
    REQUIRE(MessageTypeStrings[12] == "SquadRemove");
}

///////////////////////////////////////////////////////////////////////////////
//                           Specialized Matchers                            //
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("MatchCategoryAndType")
{
    using MC = MessageCategory;
    using MT = MessageType;

    // Should write a template struct or function to handle this.

    SECTION("Info types")
    {
        REQUIRE(MatchCategoryAndType<MC::Info, MT::ConnectionStatus>::value);
        REQUIRE(MatchCategoryAndType<MC::Info, MT::BridgeInfo>::value);
        REQUIRE(MatchCategoryAndType<MC::Info, MT::Status>::value);
        REQUIRE(MatchCategoryAndType<MC::Info, MT::Closing>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::CombatEvent>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::ExtrasSquadUpdate>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::ExtrasLanguageChanged>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::ExtrasKeyBindChanged>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::ExtrasChatMessage>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::SquadStatus>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::SquadAdd>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::SquadUpdate>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Info, MT::SquadRemove>::value);
    }

    SECTION("ArcDPS combat api types")
    {
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ConnectionStatus>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::BridgeInfo>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::Status>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::Closing>::value);

        REQUIRE(MatchCategoryAndType<MC::Combat, MT::CombatEvent>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ExtrasSquadUpdate>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ExtrasLanguageChanged>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ExtrasKeyBindChanged>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ExtrasChatMessage>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::SquadStatus>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::SquadAdd>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::SquadUpdate>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::SquadRemove>::value);
    }

    SECTION("Extras event types")
    {
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ConnectionStatus>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::BridgeInfo>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::Status>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::Closing>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::CombatEvent>::value);

        REQUIRE(MatchCategoryAndType<MC::Extras, MT::ExtrasSquadUpdate>::value);
        REQUIRE(MatchCategoryAndType<MC::Extras, MT::ExtrasLanguageChanged>::value);
        REQUIRE(MatchCategoryAndType<MC::Extras, MT::ExtrasKeyBindChanged>::value);
        REQUIRE(MatchCategoryAndType<MC::Extras, MT::ExtrasChatMessage>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::SquadStatus>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::SquadAdd>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::SquadUpdate>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Extras, MT::SquadRemove>::value);
    }

    SECTION("Squad event types")
    {
        REQUIRE_FALSE(MatchCategoryAndType<MC::Combat, MT::ConnectionStatus>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::BridgeInfo>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::Status>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::Closing>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::CombatEvent>::value);

        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::ExtrasSquadUpdate>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::ExtrasLanguageChanged>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::ExtrasKeyBindChanged>::value);
        REQUIRE_FALSE(MatchCategoryAndType<MC::Squad, MT::ExtrasChatMessage>::value);

        REQUIRE(MatchCategoryAndType<MC::Squad, MT::SquadStatus>::value);
        REQUIRE(MatchCategoryAndType<MC::Squad, MT::SquadAdd>::value);
        REQUIRE(MatchCategoryAndType<MC::Squad, MT::SquadUpdate>::value);
        REQUIRE(MatchCategoryAndType<MC::Squad, MT::SquadRemove>::value);
    }
}

///////////////////////////////////////////////////////////////////////////////
//                            MessageProtocol                                //
///////////////////////////////////////////////////////////////////////////////

// It's important this value does not change (breaks version compatibility).
TEST_CASE("MessageProtocol values")
{
    using utype = std::underlying_type_t<MessageProtocol>;

    REQUIRE(static_cast<utype>(MessageProtocol::Serial) == 1);
    REQUIRE(static_cast<utype>(MessageProtocol::JSON) == 2);
}

TEST_CASE("MessageProtocolToStr")
{
    REQUIRE(MessageProtocolToStr(MessageProtocol::Serial) == "Serial");
    REQUIRE(MessageProtocolToStr(MessageProtocol::JSON) == "JSON");
}

TEST_CASE("IsProtocolBitSet")
{
    using utype = std::underlying_type_t<MessageProtocol>;
    constexpr auto serial = static_cast<utype>(MessageProtocol::Serial);
    constexpr auto json = static_cast<utype>(MessageProtocol::JSON);

    REQUIRE(IsProtocolBitSet<MessageProtocol::Serial>(serial));
    REQUIRE(IsProtocolBitSet<MessageProtocol::JSON>(json));

    REQUIRE_FALSE(IsProtocolBitSet<MessageProtocol::Serial>(json));
    REQUIRE_FALSE(IsProtocolBitSet<MessageProtocol::JSON>(serial));

    constexpr auto all = serial + json;
    REQUIRE(IsProtocolBitSet<MessageProtocol::Serial>(all));
    REQUIRE(IsProtocolBitSet<MessageProtocol::JSON>(all));
}

///////////////////////////////////////////////////////////////////////////////
//                               SerialData                                  //
///////////////////////////////////////////////////////////////////////////////

template <typename T, std::size_t Count>
void RequireSerialIntegralWrite()
{
    static_assert(std::is_integral<T>::value, "Integral required.");
    static_assert(!(Count % sizeof(T)), "Count needs to be divisible by sizeof(T).");
    std::size_t write_count = Count / sizeof(T);

    std::array<uint8_t, Count> storage = {};
    uint8_t* location = &storage[0];

    for (std::size_t i{0}; i < write_count; ++i)
        location = serial_w_integral(location, static_cast<T>(i));

    location = &storage[0];
    for (std::size_t i{0}; i < write_count; ++i)
        location = RequireAtLocation(location, static_cast<T>(i));
}

struct VariousIntegrals
{
    int64_t i64;
    uint64_t ui64;
    int32_t i32;
    uint32_t ui32;
    int16_t i16;
    uint16_t ui16;
    int8_t i8;
    uint8_t ui8;
};

TEST_CASE("serial_w_integral")
{
    SECTION("unsigned integers")
    {
        RequireSerialIntegralWrite<uint8_t, 256>();
        RequireSerialIntegralWrite<uint16_t, 256>();
        RequireSerialIntegralWrite<uint32_t, 256>();
        RequireSerialIntegralWrite<uint64_t, 256>();
    }

    SECTION("integers")
    {
        RequireSerialIntegralWrite<int8_t, 256>();
        RequireSerialIntegralWrite<int16_t, 256>();
        RequireSerialIntegralWrite<int32_t, 256>();
        RequireSerialIntegralWrite<int64_t, 256>();
    }

    SECTION("large container (stack)")
    {
        RequireSerialIntegralWrite<int8_t, 12288>();
        RequireSerialIntegralWrite<int16_t, 12288>();
        RequireSerialIntegralWrite<int32_t, 12288>();
        RequireSerialIntegralWrite<int64_t, 12288>();
    }

    SECTION("VariousIntegrals")
    {
        // Struct padding doesnt matter here as we only need the minimum amount for all members.
        std::array<uint8_t, sizeof(VariousIntegrals)> storage = {};
        uint8_t* location = &storage[0];

        // Data.
        VariousIntegrals vi{};
        vi.i64 = 0x6FFFAFFAFFAFFAFF;
        vi.ui64 = 0xFFAFFFBFFF2FFF3F;
        vi.i32 = 0x7F2FF2F4;
        vi.ui32 = 0xF4FFFAF2;
        vi.i16 = 0x7AFA;
        vi.ui16 = 0xEF2F;
        vi.i8 = 0x7A;
        vi.ui8 = 0xF2;

        // Set.
        location = serial_w_integral(location, vi.i64);
        location = serial_w_integral(location, vi.ui64);
        location = serial_w_integral(location, vi.i32);
        location = serial_w_integral(location, vi.ui32);
        location = serial_w_integral(location, vi.i16);
        location = serial_w_integral(location, vi.ui16);
        location = serial_w_integral(location, vi.i8);
        location = serial_w_integral(location, vi.ui8);

        // Read.
        location = &storage[0];
        location = RequireAtLocation(location, vi.i64);
        location = RequireAtLocation(location, vi.ui64);
        location = RequireAtLocation(location, vi.i32);
        location = RequireAtLocation(location, vi.ui32);
        location = RequireAtLocation(location, vi.i16);
        location = RequireAtLocation(location, vi.ui16);
        location = RequireAtLocation(location, vi.i8);
        location = RequireAtLocation(location, vi.ui8);
    }

    SECTION("Budget fuzzing")
    {
        BudgetFuzzer<64, 2048, 2>([]() {
            const std::size_t creator_index = RandomIntegral<std::size_t>() % IntegralCreators.size();
            return IntegralCreators[creator_index]();
        });
    }
}

TEST_CASE("serial_w_string")
{
    SECTION("single string")
    {
        constexpr std::string_view str = "serial_w_string";
        std::array<uint8_t, str.size() + 1> storage{};
        serial_w_string(&storage[0], str.data(), str.size());
        RequireStringAtLocation(&storage[0], str.data(), str.size());
    }

    SECTION("multiple strings")
    {
        constexpr std::string_view str1 = "serial";
        constexpr std::string_view str2 = "_w_";
        constexpr std::string_view str3 = "string";
        std::array<uint8_t, str1.size() + str2.size() + str3.size() + 3> storage{};

        uint8_t* location = serial_w_string(&storage[0], str1.data(), str1.size());
        location = serial_w_string(location, str2.data(), str2.size());
        serial_w_string(location, str3.data(), str3.size());

        location = RequireStringAtLocation(&storage[0], str1.data(), str1.size());
        location = RequireStringAtLocation(location, str2.data(), str2.size());
        RequireStringAtLocation(location, str3.data(), str3.size());
    }

    SECTION("Budget fuzzing")
    {
        BudgetFuzzer<32, 1024, 2>([]() {
            return StringNodeCreator();
        });
    }
}

std::unique_ptr<Node> RandomNodeCreator()
{
    const std::size_t creator_index = RandomIntegral<std::size_t>() % (IntegralCreators.size() + 1);

    if (creator_index == IntegralCreators.size())
        return StringNodeCreator();

    return IntegralCreators[creator_index]();
}

TEST_CASE("Budget fuzzing: Serial (all types)")
{
    BudgetFuzzer<32, 1024, 2>([]() {
        return RandomNodeCreator();
    });
}

///////////////////////////////////////////////////////////////////////////////
//                              Message class                                //
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("Message reserved header byte count")
{
    // Very important this does not change.
    // If this does change, its a major API version bump.

    REQUIRE(Message::HeaderByteCount() == 18);
}

TEST_CASE("Message Constructors")
{
    using MC = MessageCategory;
    using MT = MessageType;

    using U_MC = std::underlying_type_t<MC>;
    using U_MT = std::underlying_type_t<MT>;

    SECTION("Message()")
    {
        Message msg{};

        REQUIRE(msg.valid() == false);

        REQUIRE(msg.id() == 0);
        REQUIRE(msg.timestamp() == 0);

        REQUIRE(msg.data() == nullptr);
        REQUIRE(msg.count() == 0);
    }

    SECTION("Message(protocol, category, type, id, timestamp)")
    {
        Message msg{MessageProtocol::JSON, MessageCategory::Info, MessageType::BridgeInfo, 1, 2};

        REQUIRE(msg.valid() == true);

        REQUIRE(msg.id() == 1);
        REQUIRE(msg.timestamp() == 2);

        REQUIRE(msg.data() == nullptr);
        REQUIRE(msg.count() == 0);

        REQUIRE(msg.category() == MessageCategory::Info);
        REQUIRE(msg.type() == MessageType::BridgeInfo);
    }
}

///////////////////////////////////////////////////////////////////////////////
//                          Serial Message class                             //
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("MessageSerial")
{
    SECTION("MessageSerial(category, type, id, timestamp)")
    {
        MessageSerial msg{MessageCategory::Squad, MessageType::SquadAdd, 5, 6};

        REQUIRE(msg.valid() == true);
        REQUIRE(msg.protocol() == MessageProtocol::Serial);

        REQUIRE(msg.id() == 5);
        REQUIRE(msg.timestamp() == 6);

        REQUIRE(msg.category() == MessageCategory::Squad);
        REQUIRE(msg.type() == MessageType::SquadAdd);

        constexpr auto category = static_cast<std::underlying_type_t<MessageCategory>>(MessageCategory::Squad);
        constexpr auto type = static_cast<std::underlying_type_t<MessageType>>(MessageType::SquadAdd);

        // Require serial header.
        REQUIRE(msg.data()[0] == category);
        REQUIRE(msg.data()[1] == type);
        REQUIRE(*reinterpret_cast<const decltype(msg.id())*>(&msg.data()[2]) == 5);
        constexpr auto id_size = sizeof(decltype(msg.id()));
        REQUIRE(*reinterpret_cast<const decltype(msg.timestamp())*>(&msg.data()[2 + id_size]) == 6);
        REQUIRE(msg.count() == static_cast<std::size_t>(Message::HeaderByteCount()));
    }

    SECTION("MessageSerial(category, type, id, timestamp, data)")
    {
        SerialData data{};
        data.count = Message::HeaderByteCount() + sizeof(uint32_t);
        data.ptr = std::make_shared<uint8_t[]>(data.count);
        serial_w_integral(&data.ptr[Message::HeaderByteCount()], uint32_t{128});

        MessageSerial msg{MessageCategory::Squad, MessageType::SquadAdd, 5, 6, data};

        REQUIRE(msg.valid() == true);
        REQUIRE(msg.protocol() == MessageProtocol::Serial);

        REQUIRE(msg.id() == 5);
        REQUIRE(msg.timestamp() == 6);

        REQUIRE(msg.data() == &data.ptr[0]);
        REQUIRE(msg.count() == data.count);

        REQUIRE(msg.category() == MessageCategory::Squad);
        REQUIRE(msg.type() == MessageType::SquadAdd);

        constexpr auto category = static_cast<std::underlying_type_t<MessageCategory>>(MessageCategory::Squad);
        constexpr auto type = static_cast<std::underlying_type_t<MessageType>>(MessageType::SquadAdd);

        // Require serial header.
        uint8_t* location = RequireAtLocation(&data.ptr[0], category);
        location = RequireAtLocation(location, type);
        location = RequireAtLocation(location, static_cast<decltype(msg.id())>(5));
        location = RequireAtLocation(location, static_cast<decltype(msg.timestamp())>(6));

        // Require Serial data.
        location = RequireAtLocation(location, uint32_t{128});
    }
}

///////////////////////////////////////////////////////////////////////////////
//                           JSON Message class                              //
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("MessageJSON")
{
    SECTION("MessageJSON(category, type, id, timestamp)")
    {
        MessageJSON msg{MessageCategory::Squad, MessageType::SquadAdd, 5, 6};

        REQUIRE(msg.valid() == true);
        REQUIRE(msg.protocol() == MessageProtocol::JSON);

        REQUIRE(msg.id() == 5);
        REQUIRE(msg.timestamp() == 6);

        REQUIRE(msg.category() == MessageCategory::Squad);
        REQUIRE(msg.type() == MessageType::SquadAdd);

        auto j = nlohmann::json{
            {"category", MessageCategoryToStr(MessageCategory::Squad)},
            {"type", MessageTypeToStr(MessageType::SquadAdd)},
            {"id", 5},
            {"timestamp",
             6}}.dump();

        REQUIRE(msg.count() == j.size());
        REQUIRE(std::memcmp(msg.data(), j.data(), msg.count()) == 0);
    }

    SECTION("MessageJSON(category, type, id, timestamp, data)")
    {
        auto data = nlohmann::json{"test", 128};

        MessageJSON msg{MessageCategory::Extras, MessageType::ExtrasSquadUpdate, 7, 8, data};

        REQUIRE(msg.valid() == true);
        REQUIRE(msg.protocol() == MessageProtocol::JSON);

        REQUIRE(msg.id() == 7);
        REQUIRE(msg.timestamp() == 8);

        REQUIRE(msg.category() == MessageCategory::Extras);
        REQUIRE(msg.type() == MessageType::ExtrasSquadUpdate);

        auto j = nlohmann::json{
            {"category", MessageCategoryToStr(MessageCategory::Extras)},
            {"type", MessageTypeToStr(MessageType::ExtrasSquadUpdate)},
            {"id", 7},
            {"timestamp", 8},
            {"data",
             data}}.dump();

        REQUIRE(msg.count() == j.size());
        REQUIRE(std::memcmp(msg.data(), j.data(), msg.count()) == 0);
    }
}

///////////////////////////////////////////////////////////////////////////////
//                            GetMessageClass                                //
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("GetMessageClass")
{
    REQUIRE_FALSE(GetMessageClass<static_cast<MessageProtocol>(0)>::value);

    SECTION("Serial")
    {
        REQUIRE(GetMessageClass<MessageProtocol::Serial>::value);
        REQUIRE(std::is_same_v<GetMessageClass<MessageProtocol::Serial>::Type, MessageSerial>);
    }

    SECTION("JSON")
    {
        REQUIRE(GetMessageClass<MessageProtocol::JSON>::value);
        REQUIRE(std::is_same_v<GetMessageClass<MessageProtocol::JSON>::Type, MessageJSON>);
    }
}
