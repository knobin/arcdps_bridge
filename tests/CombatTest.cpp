//
//  tests/CombatTest.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-10.
//

// Catch2 Headers
#include "catch2/catch_test_macros.hpp"

// Local Headers
#include "FuzzHelper.hpp"

// Bridge Headers
#include "Combat.hpp"

// C++ Headers
#include <sstream>
#include <utility>

///////////////////////////////////////////////////////////////////////////////
//                                 cbtevent                                  //
///////////////////////////////////////////////////////////////////////////////

//
// serial (cbtevent).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("serial_size(const cbtevent& ev)")
{
    // Size for all kinds of cbtevent is the same.

    constexpr std::size_t uint64_size = sizeof(uint64_t);
    constexpr std::size_t int32_size = sizeof(int32_t);
    constexpr std::size_t uint32_size = sizeof(uint32_t);
    constexpr std::size_t uint16_size = sizeof(uint16_t);
    constexpr std::size_t uint8_size = sizeof(uint8_t);

    constexpr std::size_t cbtevent_size = (3 * uint64_size) + (2 * int32_size) + (2 * uint32_size) + (4 * uint16_size) +
                                          (12 * uint8_size);

    constexpr cbtevent ev{};
    REQUIRE(serial_size(ev) == cbtevent_size);
}

static uint8_t* RequireCombatEvent(const cbtevent& ev, uint8_t* storage)
{
    uint8_t* location = storage;

    location = RequireAtLocation(location, ev.time);
    location = RequireAtLocation(location, ev.src_agent);
    location = RequireAtLocation(location, ev.dst_agent);
    location = RequireAtLocation(location, ev.value);
    location = RequireAtLocation(location, ev.buff_dmg);
    location = RequireAtLocation(location, ev.overstack_value);
    location = RequireAtLocation(location, ev.skillid);
    location = RequireAtLocation(location, ev.src_instid);
    location = RequireAtLocation(location, ev.dst_instid);
    location = RequireAtLocation(location, ev.src_master_instid);
    location = RequireAtLocation(location, ev.dst_master_instid);
    location = RequireAtLocation(location, ev.iff);
    location = RequireAtLocation(location, ev.buff);
    location = RequireAtLocation(location, ev.result);
    location = RequireAtLocation(location, ev.is_activation);
    location = RequireAtLocation(location, ev.is_buffremove);
    location = RequireAtLocation(location, ev.is_ninety);
    location = RequireAtLocation(location, ev.is_fifty);
    location = RequireAtLocation(location, ev.is_moving);
    location = RequireAtLocation(location, ev.is_statechange);
    location = RequireAtLocation(location, ev.is_flanking);
    location = RequireAtLocation(location, ev.is_shields);
    location = RequireAtLocation(location, ev.is_offcycle);

    return location;
}

static constexpr cbtevent SimpleCombatEvent()
{
    cbtevent ev{};

    ev.time = 1;
    ev.src_agent = 2;
    ev.dst_agent = 3;
    ev.value = 4;
    ev.buff_dmg = 5;
    ev.overstack_value = 6;
    ev.skillid = 7;
    ev.src_instid = 8;
    ev.dst_instid = 9;
    ev.src_master_instid = 10;
    ev.dst_master_instid = 11;
    ev.iff = 12;
    ev.buff = 13;
    ev.result = 14;
    ev.is_activation = 15;
    ev.is_buffremove = 16;
    ev.is_ninety = 17;
    ev.is_fifty = 18;
    ev.is_moving = 19;
    ev.is_statechange = 20;
    ev.is_flanking = 21;
    ev.is_shields = 22;
    ev.is_offcycle = 23;
    ev.pad61 = 24;
    ev.pad62 = 25;
    ev.pad63 = 26;
    ev.pad64 = 27;

    return ev;
}

TEST_CASE("to_serial(const cbtevent& ev, uint8_t* storage, std::size_t)")
{
    // Size for all kinds of cbtevent is the same.

    cbtevent ev = SimpleCombatEvent();
    constexpr std::size_t cbtevent_size = serial_size(cbtevent{});

    uint8_t storage[cbtevent_size] = {};
    to_serial(ev, storage, serial_size(ev));

    uint8_t* location = RequireCombatEvent(ev, storage);
    REQUIRE(storage + cbtevent_size == location);
}

//
// json (cbtevent).
//

static std::string CombatEventStrJSON(const cbtevent& ev)
{
    std::ostringstream oss{};
    oss << "{"
        << "\"buff\":" << static_cast<uint32_t>(ev.buff) << ","
        << "\"buff_dmg\":" << ev.buff_dmg << ","
        << "\"dst_agent\":" << ev.dst_agent << ","
        << "\"dst_instid\":" << ev.dst_instid << ","
        << "\"dst_master_instid\":" << ev.dst_master_instid << ","
        << "\"iff\":" << static_cast<uint32_t>(ev.iff) << ","
        << "\"is_activation\":" << static_cast<uint32_t>(ev.is_activation) << ","
        << "\"is_buffremove\":" << static_cast<uint32_t>(ev.is_buffremove) << ","
        << "\"is_fifty\":" << static_cast<uint32_t>(ev.is_fifty) << ","
        << "\"is_flanking\":" << static_cast<uint32_t>(ev.is_flanking) << ","
        << "\"is_moving\":" << static_cast<uint32_t>(ev.is_moving) << ","
        << "\"is_ninety\":" << static_cast<uint32_t>(ev.is_ninety) << ","
        << "\"is_offcycle\":" << static_cast<uint32_t>(ev.is_offcycle) << ","
        << "\"is_shields\":" << static_cast<uint32_t>(ev.is_shields) << ","
        << "\"is_statechange\":" << static_cast<uint32_t>(ev.is_statechange) << ","
        << "\"overstack_value\":" << static_cast<uint32_t>(ev.overstack_value) << ","
        << "\"result\":" << static_cast<uint32_t>(ev.result) << ","
        << "\"skillid\":" << ev.skillid << ","
        << "\"src_agent\":" << ev.src_agent << ","
        << "\"src_instid\":" << ev.src_instid << ","
        << "\"src_master_instid\":" << ev.src_master_instid << ","
        << "\"time\":" << ev.time << ","
        << "\"value\":" << ev.value << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const cbtevent& evt)")
{
    cbtevent ev = SimpleCombatEvent();

    nlohmann::json j = ev;
    REQUIRE(j.dump() == CombatEventStrJSON(ev));
}

//
// Budget fuzzing (cbtevent).
//

struct CombatEventNode : Node
{
    explicit CombatEventNode(const cbtevent& ev)
        : value{ev}
    {}

    cbtevent value{};

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override
    {
        return RequireCombatEvent(value, storage);
    }
    [[nodiscard]] std::size_t count() const override
    {
        return serial_size(value);
    }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == CombatEventStrJSON(value));
    }
};

static cbtevent RandomCombatEvent()
{
    cbtevent ev{};

    ev.time = RandomIntegral<decltype(ev.time)>();
    ev.src_agent = RandomIntegral<decltype(ev.src_agent)>();
    ev.dst_agent = RandomIntegral<decltype(ev.dst_agent)>();
    ev.value = RandomIntegral<decltype(ev.value)>();
    ev.buff_dmg = RandomIntegral<decltype(ev.buff_dmg)>();
    ev.overstack_value = RandomIntegral<decltype(ev.overstack_value)>();
    ev.skillid = RandomIntegral<decltype(ev.skillid)>();
    ev.src_instid = RandomIntegral<decltype(ev.src_instid)>();
    ev.dst_instid = RandomIntegral<decltype(ev.dst_instid)>();
    ev.src_master_instid = RandomIntegral<decltype(ev.src_master_instid)>();
    ev.dst_master_instid = RandomIntegral<decltype(ev.dst_master_instid)>();
    ev.iff = RandomIntegral<decltype(ev.iff)>();
    ev.buff = RandomIntegral<decltype(ev.buff)>();
    ev.result = RandomIntegral<decltype(ev.result)>();
    ev.is_activation = RandomIntegral<decltype(ev.is_activation)>();
    ev.is_buffremove = RandomIntegral<decltype(ev.is_buffremove)>();
    ev.is_ninety = RandomIntegral<decltype(ev.is_ninety)>();
    ev.is_fifty = RandomIntegral<decltype(ev.is_fifty)>();
    ev.is_moving = RandomIntegral<decltype(ev.is_moving)>();
    ev.is_statechange = RandomIntegral<decltype(ev.is_statechange)>();
    ev.is_flanking = RandomIntegral<decltype(ev.is_flanking)>();
    ev.is_shields = RandomIntegral<decltype(ev.is_shields)>();
    ev.is_offcycle = RandomIntegral<decltype(ev.is_offcycle)>();

    ev.pad61 = RandomIntegral<decltype(ev.pad61)>();
    ev.pad62 = RandomIntegral<decltype(ev.pad62)>();
    ev.pad63 = RandomIntegral<decltype(ev.pad63)>();
    ev.pad64 = RandomIntegral<decltype(ev.pad64)>();

    return ev;
}

static std::unique_ptr<CombatEventNode> CombatEventNodeCreator()
{
    return std::make_unique<CombatEventNode>(RandomCombatEvent());
}

TEST_CASE("Budget fuzzing (only cbtevent)")
{
    BudgetFuzzer<32, 1024, 2>([]() { 
        return CombatEventNodeCreator();
    });
}

///////////////////////////////////////////////////////////////////////////////
//                                    ag                                     //
///////////////////////////////////////////////////////////////////////////////

//
// serial (ag).
//

// It's important this value does not change (breaks version compatibility).
TEST_CASE("ag_partial_size")
{
    constexpr std::size_t uintptr_size = sizeof(uintptr_t);
    constexpr std::size_t uint32_size = sizeof(uint32_t);
    constexpr std::size_t uint16_size = sizeof(uint16_t);

    constexpr std::size_t ag_partial_expected = uintptr_size + (3 * uint32_size) + uint16_size;

    REQUIRE(ag_partial_size == ag_partial_expected);
}

TEST_CASE("serial_size(const ag& agent)")
{
    char n[10] = "Test Name";

    ag agent{n, 1, 2, 3, 4, 5};
    constexpr std::size_t expected_size = ag_partial_size + 10;
    REQUIRE(serial_size(agent) == expected_size);
}

static uint8_t* RequireAgent(const ag& agent, uint8_t* storage, std::size_t count)
{
    uint8_t* location = storage;
    const std::size_t str_count = count - ag_partial_size - 1;

    location = RequireStringAtLocation(location, agent.name, str_count);
    location = RequireAtLocation(location, agent.id);
    location = RequireAtLocation(location, agent.prof);
    location = RequireAtLocation(location, agent.elite);
    location = RequireAtLocation(location, agent.self);
    location = RequireAtLocation(location, agent.team);

    return location;
}

TEST_CASE("to_serial(const ag& agent, uint8_t* storage, std::size_t)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name";

        ag agent{n, 1, 2, 3, 4, 5};
        constexpr std::size_t ag_size = ag_partial_size + 10;

        uint8_t storage[ag_size] = {};
        to_serial(agent, storage, ag_size);

        uint8_t* location = RequireAgent(agent, storage, ag_size);
        REQUIRE(storage + ag_size == location);
    }

    SECTION("Empty name")
    {
        ag agent{nullptr, 1, 2, 3, 4, 5};
        constexpr std::size_t ag_size = ag_partial_size + 1; // + 1 for null terminator.

        uint8_t storage[ag_size] = {};
        to_serial(agent, storage, ag_size);

        uint8_t* location = RequireAgent(agent, storage, ag_size);
        REQUIRE(storage + ag_size == location);
    }
}

//
// json (ag).
//

static std::string AgentStrJSON(const ag& agent)
{
    std::ostringstream oss{};

    const std::string name = (agent.name) ? "\"" + std::string{agent.name} + "\"" : "null";

    oss << "{"
        << "\"elite\":" << agent.elite << ","
        << "\"id\":" << agent.id << ","
        << "\"name\":" << name << ","
        << "\"prof\":" << agent.prof << ","
        << "\"self\":" << agent.self << ","
        << "\"team\":" << agent.team << "}";

    return oss.str();
}

TEST_CASE("to_json(nlohmann::json& j, const ag& agent)")
{
    SECTION("Valid name")
    {
        char n[10] = "Test Name";
        ag agent{n, 1, 2, 3, 4, 5};

        nlohmann::json j = agent;
        REQUIRE(j.dump() == AgentStrJSON(agent));
    }

    SECTION("Empty name")
    {
        ag agent{nullptr, 1, 2, 3, 4, 5};

        nlohmann::json j = agent;
        REQUIRE(j.dump() == AgentStrJSON(agent));
    }
}

//
// Budget fuzzing (ag).
//

struct AgentNode : Node
{
    AgentNode(std::optional<std::string> ag_name, const ag& agent)
        : name{std::move(ag_name)}, value{agent}
    {
        if (name)
            value.name = &(*name)[0];
    }

    std::optional<std::string> name{};
    ag value{};

    uint8_t* write(uint8_t* storage) override
    {
        const std::size_t count = serial_size(value);
        to_serial(value, storage, count);
        return storage + count;
    }
    uint8_t* require(uint8_t* storage) override
    {
        return RequireAgent(value, storage, count());
    }
    [[nodiscard]] std::size_t count() const override
    {
        return serial_size(value);
    }
    void json_require() override
    {
        nlohmann::json j = value;
        REQUIRE(j.dump() == AgentStrJSON(value));
    }
};

static ag RandomAgent()
{
    ag agent{};

    agent.name = nullptr;
    agent.id = RandomIntegral<decltype(agent.id)>();
    agent.prof = RandomIntegral<decltype(agent.prof)>();
    agent.elite = RandomIntegral<decltype(agent.elite)>();
    agent.self = RandomIntegral<decltype(agent.self)>();
    agent.team = RandomIntegral<decltype(agent.team)>();

    return agent;
}

static std::unique_ptr<AgentNode> AgentNodeCreator()
{
    std::optional<std::string> ag_name = OptionalRandomString();
    return std::make_unique<AgentNode>(ag_name, RandomAgent());
}


TEST_CASE("Budget fuzzing (only ag)")
{
    BudgetFuzzer<32, 1024, 2>([]() { 
        return AgentNodeCreator(); 
    });
}

///////////////////////////////////////////////////////////////////////////////
//                      Budget fuzzing (ag & cbtevent)                       //
///////////////////////////////////////////////////////////////////////////////

static std::unique_ptr<Node> NodeCreator()
{
    std::array<std::function<std::unique_ptr<Node>()>, 2> creators{
        []() { return AgentNodeCreator(); },
        []() { return CombatEventNodeCreator(); }
    };

    return creators[RandomIntegral<std::size_t>() % creators.size()]();
}

TEST_CASE("Budget fuzzing (ag and cbtevent)")
{
    BudgetFuzzer<32, 1024, 2>([]() { 
        return NodeCreator(); 
    });
}

///////////////////////////////////////////////////////////////////////////////
//          Combat Message generators (from mod_combat callback)             //
///////////////////////////////////////////////////////////////////////////////

//
// serial (combat args).
//

static uint8_t* RequireCombatToSerial(uint8_t* storage, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    uint8_t bits = 0;
    bits |= ((ev) ? 1 : 0);
    bits |= ((src) ? 2 : 0);
    bits |= ((dst) ? 4 : 0);

    uint8_t* location = storage;
    location = RequireAtLocation(location, bits);

    if (ev)
        location = RequireCombatEvent(*ev, location);

    if (src)
        location = RequireAgent(*src, location, serial_size(*src));

    if (dst)
        location = RequireAgent(*dst, location, serial_size(*dst));
    
    if (skillname)
        location = RequireStringAtLocation(location, skillname, std::strlen(skillname));
    else
        location = RequireAtLocation(location, static_cast<uint8_t>('\0'));

    location = RequireAtLocation(location, id);
    location = RequireAtLocation(location, revision);

    return location;
}

static void FieldTesterSerial(std::size_t count, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    SerialData serial = combat_to_serial(ev, src, dst, skillname, id, revision);
    uint8_t* storage = &serial.ptr[SerialStartPadding];
    auto location = RequireCombatToSerial(storage, ev, src, dst, skillname, id, revision);
    REQUIRE(storage + count == location);
}

template<typename Tester>
void RequireCombatFields(Tester func, cbtevent& ev, ag& src, ag& dst, std::optional<std::string> skillname, uint64_t id, uint64_t revision)
{
    constexpr std::size_t ev_size = serial_size(cbtevent{});
    const std::size_t src_ag_size = serial_size(src);
    const std::size_t dst_ag_size = serial_size(dst);

    std::size_t skillname_size = 0;
    char* skillname_ptr = nullptr;

    if (skillname)
    {
        skillname_size = skillname->size(); // Does not include null terminator.
        skillname_ptr = &(*skillname)[0];
    }

    // + 1 for non null bits, + 1 for null terminator in skillname.
    const std::size_t total_size = 1 + ev_size + src_ag_size + dst_ag_size + skillname_size + 1 + (2 * sizeof(uint64_t));

    func(total_size, &ev, &src, &dst, skillname_ptr, id, revision);
    func(total_size - skillname_size, &ev, &src, &dst, nullptr, id, revision);
    func(total_size - dst_ag_size, &ev, &src, nullptr, skillname_ptr, id, revision);
    func(total_size - skillname_size - dst_ag_size, &ev, &src, nullptr, nullptr, id, revision);

    func(total_size - src_ag_size, &ev, nullptr, &dst, skillname_ptr, id, revision);
    func(total_size - skillname_size - src_ag_size, &ev, nullptr, &dst, nullptr, id, revision);
    func(total_size - skillname_size - dst_ag_size, &ev, &src, nullptr, nullptr, id, revision);
    func(total_size - skillname_size - dst_ag_size - src_ag_size, &ev, nullptr, nullptr, nullptr, id, revision);

    func(total_size - ev_size, nullptr, &src, &dst, skillname_ptr, id, revision);
    func(total_size - ev_size - skillname_size, nullptr, &src, &dst, nullptr, id, revision);
    func(total_size - ev_size - dst_ag_size, nullptr, &src, nullptr, skillname_ptr, id, revision);
    func(total_size - ev_size - dst_ag_size - skillname_size, nullptr, &src, nullptr, nullptr, id, revision);

    func(total_size - ev_size - src_ag_size, nullptr, nullptr, &dst, skillname_ptr, id, revision);
    func(total_size - ev_size - src_ag_size - skillname_size, nullptr, nullptr, &dst, nullptr, id, revision);
    func(total_size - ev_size - src_ag_size - dst_ag_size, nullptr, nullptr, nullptr, skillname_ptr, id, revision);
    func(total_size - ev_size - src_ag_size - dst_ag_size - skillname_size, nullptr, nullptr, nullptr, nullptr, id, revision);
}

TEST_CASE("combat_to_serial()")
{
    cbtevent ev = SimpleCombatEvent();

    char src_name[12] = "Source Name";
    ag src{src_name, 1, 2, 3, 4, 5};

    char dst_name[17] = "Destination Name";
    ag dst{dst_name, 1, 2, 3, 4, 5};

    std::optional<std::string> skillname = "Skillname";

    uint64_t id = 1;
    uint64_t revision = 2;

    RequireCombatFields(FieldTesterSerial, ev, src, dst, skillname, id, revision);
}

//
// json (combat args).
//

static std::string CombatArgsStrJSON(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    std::ostringstream oss{};

    const std::string ev_str = (ev) ? CombatEventStrJSON(*ev) : "null";
    const std::string src_str = (src) ? AgentStrJSON(*src) : "null";
    const std::string dst_str = (dst) ? AgentStrJSON(*dst) : "null";
    const std::string skillname_str = (skillname) ? "\"" + std::string{skillname} + "\"" : "null";

    oss << "{"
        << "\"dst\":" << dst_str << ","
        << "\"ev\":" << ev_str << ","
        << "\"id\":" << id << ","
        << "\"revision\":" << revision << ","
        << "\"skillname\":" << skillname_str << ","
        << "\"src\":" << src_str 
        << "}";

    return oss.str();
}

static void FieldTesterJSON(std::size_t, cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    auto json = combat_to_json(ev, src, dst, skillname, id, revision);
    REQUIRE(json.dump() == CombatArgsStrJSON(ev, src, dst, skillname, id, revision));
}

TEST_CASE("combat_to_json()")
{
    cbtevent ev = SimpleCombatEvent();

    char src_name[12] = "Source Name";
    ag src{src_name, 1, 2, 3, 4, 5};

    char dst_name[17] = "Destination Name";
    ag dst{dst_name, 1, 2, 3, 4, 5};

    std::string skillname = "Skillname";

    uint64_t id = 1;
    uint64_t revision = 2;

    RequireCombatFields(FieldTesterJSON, ev, src, dst, skillname, id, revision);
}

//
// Budget fuzzing (combat args).
//

TEST_CASE("Budget fuzzing (combat args)")
{
    const std::size_t tests = RandomIntegral<std::size_t, 0, 128>();
    for (std::size_t i{0}; i < tests; ++i)
    {
        // Random argument data.
        cbtevent ev = RandomCombatEvent();
        ag src = RandomAgent();
        ag dst = RandomAgent();

        std::optional<std::string> skillname = OptionalRandomString();

        uint64_t id = RandomIntegral<uint64_t>();
        uint64_t revision = RandomIntegral<uint64_t>();

        RequireCombatFields(FieldTesterSerial, ev, src, dst, skillname, id, revision);
        RequireCombatFields(FieldTesterJSON, ev, src, dst, skillname, id, revision);
    }
}