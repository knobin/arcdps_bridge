//
//  src/Combat.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-18.
//

// Local Headers
#include "Combat.hpp"

//
// Combat event (cbtevent). 
//

void to_serial(const cbtevent& ev, uint8_t* storage, std::size_t)
{
    uint8_t* location = storage;

    location = serial_w_integral(location, ev.time);
    location = serial_w_integral(location, ev.src_agent);
    location = serial_w_integral(location, ev.dst_agent);
    location = serial_w_integral(location, ev.value);
    location = serial_w_integral(location, ev.buff_dmg);
    location = serial_w_integral(location, ev.overstack_value);
    location = serial_w_integral(location, ev.skillid);
    location = serial_w_integral(location, ev.src_instid);
    location = serial_w_integral(location, ev.dst_instid);
    location = serial_w_integral(location, ev.src_master_instid);
    location = serial_w_integral(location, ev.dst_master_instid);
    location = serial_w_integral(location, ev.iff);
    location = serial_w_integral(location, ev.buff);
    location = serial_w_integral(location, ev.result);
    location = serial_w_integral(location, ev.is_activation);
    location = serial_w_integral(location, ev.is_buffremove);
    location = serial_w_integral(location, ev.is_ninety);
    location = serial_w_integral(location, ev.is_fifty);
    location = serial_w_integral(location, ev.is_moving);
    location = serial_w_integral(location, ev.is_statechange);
    location = serial_w_integral(location, ev.is_flanking);
    location = serial_w_integral(location, ev.is_shields);
    location = serial_w_integral(location, ev.is_offcycle);
}

inline void to_json(nlohmann::json& j, const cbtevent& evt)
{
    j = nlohmann::json{{"time", evt.time},
                       {"src_agent", evt.src_agent},
                       {"dst_agent", evt.dst_agent},
                       {"value", evt.value},
                       {"buff_dmg", evt.buff_dmg},
                       {"overstack_value", evt.overstack_value},
                       {"skillid", evt.skillid},
                       {"src_instid", evt.src_instid},
                       {"dst_instid", evt.dst_instid},
                       {"src_master_instid", evt.src_master_instid},
                       {"dst_master_instid", evt.dst_master_instid},
                       {"iff", evt.iff},
                       {"buff", evt.buff},
                       {"result", evt.result},
                       {"is_activation", evt.is_activation},
                       {"is_buffremove", evt.is_buffremove},
                       {"is_ninety", evt.is_ninety},
                       {"is_fifty", evt.is_fifty},
                       {"is_moving", evt.is_moving},
                       {"is_statechange", evt.is_statechange},
                       {"is_flanking", evt.is_flanking},
                       {"is_shields", evt.is_shields},
                       {"is_offcycle", evt.is_offcycle}};
}

//
// Agent (ag).
//

std::size_t serial_size(const ag& agent)
{
    std::size_t str_count{1};
    if (agent.name != nullptr)
        str_count += std::strlen(agent.name);

    return str_count + ag_partial_size;
}

void to_serial(const ag& agent, uint8_t* storage, std::size_t count)
{
    const std::size_t str_count = count - ag_partial_size - 1;

    uint8_t* location = storage;
    location = serial_w_string(location, agent.name, str_count);
    location = serial_w_integral(location, static_cast<uint64_t>(agent.id));
    location = serial_w_integral(location, agent.prof);
    location = serial_w_integral(location, agent.elite);
    location = serial_w_integral(location, agent.self);
    location = serial_w_integral(location, agent.team);
}

void to_json(nlohmann::json& j, const ag& agent)
{
    j = nlohmann::json{{"name", nullptr},      {"id", agent.id},     {"prof", agent.prof},
                       {"elite", agent.elite}, {"self", agent.self}, {"team", agent.team}};

    if (agent.name)
        j["name"] = agent.name;
}

//
// Combat Message generators (from mod_combat callback).
//

SerialData combat_to_serial(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    constexpr std::size_t id_count = sizeof(id);
    constexpr std::size_t revision_count = sizeof(revision);
    const std::size_t ev_count = ((ev) ? serial_size(cbtevent{}) : 0);
    const std::size_t src_count = ((src) ? serial_size(*src) : 0);
    const std::size_t dst_count = ((dst) ? serial_size(*dst) : 0);
    std::size_t skillname_count = 1;
    if (skillname)
        skillname_count += std::strlen(skillname);

    const std::size_t total_count = 1 + id_count + revision_count + ev_count + src_count + dst_count + skillname_count;
    SerialData serial = CreateSerialData(total_count);

    uint8_t* location = &serial.ptr[SerialStartPadding];

    // Non nullptr bits.
    uint8_t bits = 0;
    bits |= ((ev) ? 1 : 0);
    bits |= ((src) ? 2 : 0);
    bits |= ((dst) ? 4 : 0);
    location = serial_w_integral(location, bits);

    std::size_t index = SerialStartPadding + 1;

    if (ev)
        to_serial(*ev, &serial.ptr[index], ev_count);

    if (src)
        to_serial(*src, &serial.ptr[index + ev_count], src_count);

    if (dst)
        to_serial(*dst, &serial.ptr[index + ev_count + src_count], dst_count);

    location = &serial.ptr[index + ev_count + src_count + dst_count];
    location = serial_w_string(location, skillname, skillname_count - 1);
    location = serial_w_integral(location, id);
    location = serial_w_integral(location, revision);

    return serial;
}

nlohmann::json combat_to_json(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    nlohmann::json json = {
        {"id", id}, 
        {"revision", revision}, 
        {"ev", nullptr}, 
        {"src", nullptr}, 
        {"dst", nullptr}, 
        {"skillname", nullptr},
    };

    if (ev)
        json["ev"] = *ev;
    if (src)
        json["src"] = *src;
    if (dst)
        json["dst"] = *dst;

    std::string sn{};
    if (skillname)
    {
        sn = std::string{skillname};
        std::size_t pos = 0;
        while ((pos = sn.find("\"", pos)) != std::string::npos)
        {
            sn.replace(pos, 1, "\\\"");
            pos += 2;
        }
    }

    if (!sn.empty())
        json["skillname"] = sn;

    return json;
}