//
//  src/Entry.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "ApplicationData.hpp"
#include "Log.hpp"
#include "PipeHandler.hpp"

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

// C++ Headers
#include <cstddef>
#include <string>

// Windows Headers
#include <windows.h>

static ApplicationData AppData{};
static PipeHandler Server{std::string{AppData.PipeName}, AppData};

static std::string GetDllPath(HMODULE hModule)
{
    char path[MAX_PATH];
    if (GetModuleFileName(hModule, path, sizeof(path)) == 0)
    {
        BRIDGE_INFO("GetModuleFileName failed with error \"", GetLastError(), "\"");
        return "";
    }
    std::string spath = std::string{path};
    std::size_t lastBackslash = spath.find_last_of('\\');
    if (lastBackslash != std::string::npos)
        spath = spath.substr(0, lastBackslash + 1);
    BRIDGE_INFO("DLL path = \"", spath, "\"");
    return spath;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            BRIDGE_INFO("Starting Bridge service.");
            std::string dllPath = GetDllPath(hModule);
            BRIDGE_LOG_INIT(dllPath + std::string{AppData.LogFile});
            std::string configFile = dllPath + std::string{AppData.ConfigFile};
            AppData.Config = InitConfigs(configFile);

            Server.start();
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            Server.stop();
            BRIDGE_INFO("Ended Bridge service.");
            break;
        }
    }
    return TRUE;
}

/* dll attach -- from winapi */
static void dll_init(HANDLE hModule)
{
    return;
}

/* dll detach -- from winapi */
static void dll_exit()
{
    return;
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game)
 */
static uintptr_t mod_wnd(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    return uMsg;
}

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

static std::string cbteventToJSON(cbtevent* evt)
{
    std::ostringstream ss{};
    ss << "{"
       << "\"time\":" << evt->time << ","
       << "\"src_agent\":" << evt->src_agent << ","
       << "\"dst_agent\":" << evt->dst_agent << ","
       << "\"value\":" << evt->value << ","
       << "\"buff_dmg\":" << evt->buff_dmg << ","
       << "\"overstack_value\":" << evt->overstack_value << ","
       << "\"skillid\":" << evt->skillid << ","
       << "\"src_instid\":" << evt->src_instid << ","
       << "\"dst_instid\":" << evt->dst_instid << ","
       << "\"src_master_instid\":" << evt->src_master_instid << ","
       << "\"dst_master_instid\":" << evt->dst_master_instid << ","
       << "\"iff\":" << static_cast<uint32_t>(evt->iff) << ","
       << "\"buff\":" << static_cast<uint32_t>(evt->buff) << ","
       << "\"result\":" << static_cast<uint32_t>(evt->result) << ","
       << "\"is_activation\":" << static_cast<uint32_t>(evt->is_activation) << ","
       << "\"is_buffremove\":" << static_cast<uint32_t>(evt->is_buffremove) << ","
       << "\"is_ninety\":" << static_cast<uint32_t>(evt->is_ninety) << ","
       << "\"is_fifty\":" << static_cast<uint32_t>(evt->is_fifty) << ","
       << "\"is_moving\":" << static_cast<uint32_t>(evt->is_moving) << ","
       << "\"is_statechange\":" << static_cast<uint32_t>(evt->is_statechange) << ","
       << "\"is_flanking\":" << static_cast<uint32_t>(evt->is_flanking) << ","
       << "\"is_shields\":" << static_cast<uint32_t>(evt->is_shields) << ","
       << "\"is_offcycle\":" << static_cast<uint32_t>(evt->is_offcycle) << "}";
    return ss.str();
}

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

static std::string agToJSON(ag* agent)
{
    std::ostringstream ss{};
    ss << "{"
       << "\"name\":" << ((agent->name) ? "\"" + std::string{agent->name} + "\"" : "null") << ","
       << "\"id\":" << agent->id << ","
       << "\"prof\":" << agent->prof << ","
       << "\"elite\":" << agent->elite << ","
       << "\"self\":" << agent->self << ","
       << "\"team\":" << agent->team << "}";
    return ss.str();
}

static void SendPlayerMsg(const std::string& trigger, const std::string& sType,
                          const PlayerContainer::PlayerInfo& player)
{
    if (Server.trackingEvent(MessageType::Squad))
    {
        std::ostringstream ss{};
        ss << "{\"type\":\"Squad\",\"squad\":{";
        ss << "\"trigger\":\"" << trigger << "\",";
        ss << "\"source\":\"" << sType << "\",";
        ss << "\"member\":" << player.toJSON() << "}}";
        Server.sendMessage(ss.str(), MessageType::Squad);
    }
}

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2.
 * return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not
 * all statechanges present, see evtc statechange enum */
static uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    // Add character name, profession, and elite to PlayerInfo.
    if (!ev && !src->elite && src->prof)
    {
        std::string accountName{dst->name};
        auto exists = AppData.Squad.find(accountName);

        if (exists)
        {
            exists->characterName = std::string{src->name};
            exists->profession = dst->prof;
            exists->elite = dst->elite;
            if (auto pi = AppData.Squad.update(*exists))
                SendPlayerMsg("update", "combat", *pi);
        }
        else
        {
            // Player not already in squad.
            // This event happened before the extra event, add the player.

            PlayerContainer::PlayerInfo player{};
            player.accountName = accountName;
            player.characterName = std::string{src->name};
            player.profession = dst->prof;
            player.elite = dst->elite;

            if (auto pi = AppData.Squad.add(player))
                SendPlayerMsg("add", "combat", *pi);
        }
    }

    if (!Server.trackingEvent(MessageType::Combat))
        return 0;

    // Combat event.
    std::ostringstream ss{};
    ss << "{\"type\":\"Combat\",\"combat\":{";

    ss << "\"id\":" << id << ",\"revision\":" << revision;

    ss << ",\"ev\":" << ((ev) ? cbteventToJSON(ev) : "null");
    ss << ",\"src\":" << ((src) ? agToJSON(src) : "null");
    ss << ",\"dst\":" << ((dst) ? agToJSON(dst) : "null");

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

    ss << ",\"skillname\":" << ((!sn.empty()) ? "\"" + sn + "\"" : "null");
    ss << "}}\0";

    Server.sendMessage(ss.str(), MessageType::Combat);

    return 0;
}

static arcdps_exports arc_exports;
/* initialize mod -- return table that arcdps will use for callbacks */
static arcdps_exports* mod_init()
{
    /* for arcdps */
    memset(&arc_exports, 0, sizeof(arcdps_exports));
    arc_exports.sig = 0x1EB0697;
    arc_exports.imguivers = 18000; // IMGUI_VERSION_NUM;
    arc_exports.size = sizeof(arcdps_exports);
    arc_exports.out_name = "Unofficial Bridge";
    arc_exports.out_build = AppData.Info.version.data();
    arc_exports.wnd_nofilter = mod_wnd;
    arc_exports.combat = mod_combat;

    if (AppData.Config.enabled && AppData.Config.arcDPS)
    {
        AppData.Info.arcLoaded = true;
        BRIDGE_INFO("ArcDPS is enabled.");
    }
    else
    {
        // This will create a warning in the arcdps log.
        // Will maybe change this later, due to having a silent warning instead.
        // Since this is not an error, only a way to turn of the extension and
        // also have it loaded at the same time.
        arc_exports.sig = 0;
        arc_exports.size = (uintptr_t) "ArcDPS is disabled by configs!";
        BRIDGE_INFO("ArcDPS is disabled by configs!");
    }

    return &arc_exports;
}

/* release mod -- return ignored */
static uintptr_t mod_release()
{
    BRIDGE_INFO("Releasing ArcDPS Bridge");
    AppData.Info.arcLoaded = false;

    return 0;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client load */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversionstr, void* imguicontext, void* dxptr,
                                                     HMODULE new_arcdll, void* mallocfn, void* freefn, UINT dxver)
{
    AppData.Info.arcvers = std::string{arcversionstr};
    return mod_init;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client exit */
extern "C" __declspec(dllexport) void* get_release_addr()
{
    AppData.Info.arcvers = "";
    return mod_release;
}

static std::string ExtraDataToJSON(const UserInfo* user)
{
    std::ostringstream ss{};
    ss << "{\"AccountName\":\"" << std::string{user->AccountName} << "\","
       << "\"Role\":" << static_cast<int>(static_cast<uint8_t>(user->Role)) << ","
       << "\"Subgroup\":" << static_cast<int>(user->Subgroup) << ","
       << "\"JoinTime\":" << user->JoinTime << ","
       << "\"ReadyStatus\":" << ((user->ReadyStatus) ? "true" : "false") << "}";
    return ss.str();
}

// Callback for arcDPS unofficial extras API.
void squad_update_callback(const UserInfo* updatedUsers, uint64_t updatedUsersCount)
{
    for (uint64_t i{0}; i < updatedUsersCount; ++i)
    {
        const UserInfo* uinfo{&updatedUsers[i]};
        if (uinfo)
        {
            if (uinfo->Role == UserRole::None)
            {
                if (auto pi = AppData.Squad.remove(std::string{uinfo->AccountName}))
                    SendPlayerMsg("remove", "extra", *pi);

                if (AppData.Self == uinfo->AccountName)
                    AppData.Squad.clear();
            }
            else
            {
                // ArcDPS might have added the player already.
                // Don't add it again since this has less info than the ArcDPS event.
                auto exists = AppData.Squad.find(uinfo->AccountName);
                if (!exists)
                {
                    PlayerContainer::PlayerInfo player{};
                    player.accountName = uinfo->AccountName;
                    player.role = static_cast<uint8_t>(uinfo->Role);
                    player.subgroup = uinfo->Subgroup;
                    player.joinTime = uinfo->JoinTime;

                    if (auto pi = AppData.Squad.add(player))
                        SendPlayerMsg("add", "extra", *pi);
                }
                else
                {
                    // If already added, update role and subgroup.
                    exists->role = static_cast<uint8_t>(uinfo->Role);
                    exists->subgroup = uinfo->Subgroup;
                    if (exists->joinTime != 0 && uinfo->JoinTime != 0)
                        exists->joinTime = uinfo->JoinTime;

                    if (auto pi = AppData.Squad.add(*exists))
                        SendPlayerMsg("update", "extra", *pi);
                }
            }

            if (Server.trackingEvent(MessageType::Extra))
            {
                const std::string data{ExtraDataToJSON(uinfo)};
                std::ostringstream ss{};
                ss << "{\"type\":\"Extra\","
                   << "\"extra\":" << data << "}";
                Server.sendMessage(ss.str(), MessageType::Extra);
            }
        }
    }
}

// Exported init function for arcDPS unofficial extras API.
extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* pExtrasInfo,
                                                                               void* pSubscriberInfo)
{
    if (!AppData.Config.enabled && !AppData.Config.extras)
    {
        BRIDGE_INFO("Unofficial Extras is disabled.");
        return;
    }

    if (pExtrasInfo->ApiVersion != 2)
    {
        BRIDGE_INFO("Extra api version error, expected 2 and got ", pExtrasInfo->ApiVersion);
        return;
    }

    if (pExtrasInfo->MaxInfoVersion == 1)
    {
        BRIDGE_INFO("Enabled arcdps extra hook");
        AppData.Info.extraLoaded = true;
        AppData.Info.extraVersion = std::string{pExtrasInfo->StringVersion};

        ExtrasSubscriberInfoV1 extrasInfo{};
        extrasInfo.InfoVersion = 1;
        extrasInfo.SubscriberName = "ArcDPS Bridge";
        extrasInfo.SquadUpdateCallback = squad_update_callback;
        *static_cast<ExtrasSubscriberInfoV1*>(pSubscriberInfo) = extrasInfo;

        // PlayerCollection.self.accountName = pExtrasInfo->SelfAccountName;
        AppData.Self = pExtrasInfo->SelfAccountName;
        BRIDGE_INFO("Self account name: \"", AppData.Self, "\"");
        return;
    }

    BRIDGE_INFO("Extra max info version \"", pExtrasInfo->MaxInfoVersion, "\" is not supported.");
}