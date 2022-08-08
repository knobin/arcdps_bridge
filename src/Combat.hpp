//
//  src/Combat.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-08-08.
//

#ifndef BRIDGE_COMBAT_HPP
#define BRIDGE_COMBAT_HPP

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// C++ Headers
#include <cstdint>

/* arcdps export table */
struct arcdps_exports
{
    uintptr_t size;        /* size of exports table */
    uint32_t sig;          /* pick a number between 0 and uint32_t max that isn't used by other modules */
    uint32_t imguivers;    /* set this to IMGUI_VERSION_NUM. if you don't use imgui, 18000 (as of 2021-02-02) */
    const char* out_name;  /* name string */
    const char* out_build; /* build string */
    void* wnd_nofilter; /* wndproc callback, fn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam), return assigned to
                           umsg */
    void* combat; /* combat event callback, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t
                     revision) */
    void* imgui;  /* ::present callback, before imgui::render, fn(uint32_t not_charsel_or_loading) */
    void* options_end;  /* ::present callback, appending to the end of options window in arcdps, fn() */
    void* combat_local; /* combat event callback like area but from chat log, fn(cbtevent* ev, ag* src, ag* dst, char*
                           skillname, uint64_t id, uint64_t revision) */
    void* wnd_filter;   /* wndproc callback like wnd_nofilter above, input filered using modifiers */
    void* options_windows; /* called once per 'window' option checkbox, with null at the end, non-zero return disables
                              arcdps drawing that checkbox, fn(char* windowname) */
};

/* combat event - see evtc docs for details, revision param in combat cb is equivalent of revision byte header */
struct cbtevent
{
    uint64_t time;
    uint64_t src_agent;
    uint64_t dst_agent;
    int32_t value;
    int32_t buff_dmg;
    uint32_t overstack_value;
    uint32_t skillid;
    uint16_t src_instid;
    uint16_t dst_instid;
    uint16_t src_master_instid;
    uint16_t dst_master_instid;
    uint8_t iff;
    uint8_t buff;
    uint8_t result;
    uint8_t is_activation;
    uint8_t is_buffremove;
    uint8_t is_ninety;
    uint8_t is_fifty;
    uint8_t is_moving;
    uint8_t is_statechange;
    uint8_t is_flanking;
    uint8_t is_shields;
    uint8_t is_offcycle;
    uint8_t pad61;
    uint8_t pad62;
    uint8_t pad63;
    uint8_t pad64;
};

/* agent short */
struct ag
{
    char* name;     /* agent name. may be null. valid only at time of event. utf8 */
    uintptr_t id;   /* agent unique identifier */
    uint32_t prof;  /* profession at time of event. refer to evtc notes for identification */
    uint32_t elite; /* elite spec at time of event. refer to evtc notes for identification */
    uint32_t self;  /* 1 if self, 0 if not */
    uint16_t team;  /* sep21+ */
};

inline void to_json(nlohmann::json& j, const cbtevent& evt)
{
    j = nlohmann::json{
        {"time", evt.time},
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
        {"iff", static_cast<uint32_t>(evt.iff)},
        {"buff", static_cast<uint32_t>(evt.buff)},
        {"result", static_cast<uint32_t>(evt.result)},
        {"is_activation", static_cast<uint32_t>(evt.is_activation)},
        {"is_buffremove", static_cast<uint32_t>(evt.is_buffremove)},
        {"is_ninety", static_cast<uint32_t>(evt.is_ninety)},
        {"is_fifty", static_cast<uint32_t>(evt.is_fifty)},
        {"is_moving", static_cast<uint32_t>(evt.is_moving)},
        {"is_statechange", static_cast<uint32_t>(evt.is_statechange)},
        {"is_flanking", static_cast<uint32_t>(evt.is_flanking)},
        {"is_shields", static_cast<uint32_t>(evt.is_shields)},
        {"is_offcycle", static_cast<uint32_t>(evt.is_offcycle)}
    };
}

inline void to_json(nlohmann::json& j, const ag& agent)
{
    j = nlohmann::json{
        {"name", nullptr},
        {"id", agent.id}, 
        {"prof", agent.prof},
        {"elite", agent.elite}, 
        {"self", agent.self}, 
        {"team", agent.team}
    };

    if (agent.name)
        j["name"] = std::string{agent.name};
}

#endif // BRIDGE_COMBAT_HPP
