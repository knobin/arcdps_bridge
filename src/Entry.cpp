//
//  src/Entry.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#define NOMINMAX

// Local Headers
#include "ApplicationData.hpp"
#include "SquadModifyHandler.hpp"
#include "Log.hpp"
#include "PipeHandler.hpp"

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

// C++ Headers
#include <cstddef>
#include <limits>
#include <memory>

// Windows Headers
#include <windows.h>

static ApplicationData AppData{};
static std::unique_ptr<PipeHandler> Server{nullptr}; // {std::string{AppData.PipeName}, AppData};
static std::unique_ptr<SquadModifyHandler> SquadHandler{nullptr};

static std::string GetDllPath(HMODULE hModule)
{
    char path[MAX_PATH];
    if (GetModuleFileName(hModule, path, sizeof(path)) == 0)
        return "";
    std::string spath = std::string{path};
    std::size_t lastBackslash = spath.find_last_of('\\');
    if (lastBackslash != std::string::npos)
        spath = spath.substr(0, lastBackslash + 1);
    return spath;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            Server = std::make_unique<PipeHandler>(std::string{AppData.PipeName}, AppData);
            SquadHandler = std::make_unique<SquadModifyHandler>(AppData.Squad);

            std::string dllPath = GetDllPath(hModule);
            BRIDGE_LOG_INIT(dllPath + std::string{AppData.LogFile});
            BRIDGE_INFO("Starting Bridge service [{}] [{}].", AppData.Info.version, BRIDGE_BUILD_STR);
            if (dllPath.empty())
            {
                BRIDGE_ERROR("GetModuleFileName failed with error \"{}\"", GetLastError());
            }
            BRIDGE_INFO("DLL path = \"{}\"", dllPath);

            std::string configFile = dllPath + std::string{AppData.ConfigFile};
            AppData.Config = InitConfigs(configFile);

            AppData.CharacterTypeCache.reserve(50);
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            SquadHandler.reset();
            BRIDGE_INFO("Ended Bridge service.");
            BRIDGE_LOG_DESTROY();
            break;
        }
    }
    return TRUE;
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game)
 */
static uintptr_t mod_wnd(HWND, UINT uMsg, WPARAM, LPARAM)
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
                          const PlayerInfo& player, std::size_t validator)
{
    if (Server->trackingEvent(MessageType::Squad))
    {
        std::ostringstream ss{};
        ss << "{\"type\":\"squad\",\"squad\":{"
           << "\"trigger\":\"" << trigger << "\","
           << "\"" << trigger << "\":{"
           << "\"source\":\"" << sType << "\","
           << "\"validator\":" << validator << ","
           << "\"member\":" << player.toJSON() << "}}}";
        Server->sendMessage(ss.str(), MessageType::Squad);
    }
}

static void UpdateCombatPlayerSuccess(const PlayerInfoEntry& entry)
{
    SendPlayerMsg("update", "combat", entry.player, entry.validator);
}

static void UpdateCombatAddPlayerInfo(const PlayerInfoEntry& existing, ag* src, ag* dst)
{
    std::string charName{src->name};
    auto update = [charName, prof = dst->prof, elite = dst->elite](PlayerInfo& player)
    {
        player.characterName = charName;
        player.profession = prof;
        player.elite = elite;
        player.inInstance = true;

    };
    SquadHandler->updatePlayer(existing, update, UpdateCombatPlayerSuccess);
}

static void UpdateCombatCharInfo(const std::string& name, CharacterType ct)
{
    auto existing = AppData.Squad.find_if([&name](const auto& p){ return p.second.player.characterName == name; });

    if (existing)
    {
        auto update = [ct](PlayerInfo& player)
        {
            player.profession = ct.profession;
            player.elite = ct.elite;
        };
        SquadHandler->updatePlayer(*existing, update, UpdateCombatPlayerSuccess);
    }
}

static void RemoveFromSquad(const std::string& accountName, const std::string& sType)
{
    if (auto entry = AppData.Squad.find(accountName))
    {
        if (entry->player.self)
        {
            BRIDGE_DEBUG("Removing self \"{}\", saving character info for \"{}\".", accountName, entry->player.characterName);
            AppData.Self = entry->player;
            
        }
        
        // Will send an event to clients.
        auto success = [sType](PlayerInfoEntry& entry)
        {
            SendPlayerMsg("remove", sType, entry.player, entry.validator);
        };
        SquadHandler->removePlayer(accountName, success);
        
        // Clear squad after event is sent.
        if (entry->player.self)
            SquadHandler->clear();
    }
}

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2.
 * return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not
 * all statechanges present, see evtc statechange enum */
static uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    // Add character name, profession, and elite to PlayerInfo.
    if (!ev && !src->elite)
    {
        if (src->prof)
        {
            // Added.
            BRIDGE_DEBUG("Added, checking dst->name \"{}\"", src->name);
            std::string accountName{dst->name};

            if (auto exists = AppData.Squad.find(accountName))
            {
                // Update existing listing.
                UpdateCombatAddPlayerInfo(*exists, src, dst);
            }
            else
            {
                // Player not already in squad.
                // This event happened before the extras event, add the player.

                PlayerInfo player{};
                player.accountName = accountName;
                player.characterName = std::string{src->name};
                player.profession = dst->prof;
                player.elite = dst->elite;
                player.inInstance = true;
                player.subgroup = static_cast<uint8_t>(dst->team);
                player.self = dst->self;

                // If there is no knowledge of self account name, set it here.
                if (dst->self && AppData.Self.accountName.empty())
                {
                    AppData.Self = player;
                    BRIDGE_INFO("Self account name (Combat): \"{}\"", AppData.Self.accountName);
                }

                auto success = [](const PlayerInfoEntry& entry)
                { 
                    SendPlayerMsg("add", "combat", entry.player, entry.validator); 
                };
                auto failedAdd = [accountName, src, dst]()
                {
                    // Entry got added just in the right time for add() to fail.
                    if (auto added = AppData.Squad.find(accountName))
                        UpdateCombatAddPlayerInfo(*added, src, dst);
                };
                SquadHandler->addPlayer(player, success, failedAdd);
            }

            CharacterType ct{};
            ct.profession = dst->prof;
            ct.elite = dst->elite;
            BRIDGE_DEBUG("Added, CharCache, checking src->name \"{}\"", src->name);
            AppData.CharacterTypeCache.insert_or_assign(std::string{src->name}, ct);
#ifdef BRIDGE_BUILD_DEBUG
            if (AppData.CharacterTypeCache.size() > 50)
            {
                BRIDGE_WARN("CharCache size > 50 !");
            }
#endif
        }
        else
        {
            // Removed.
            BRIDGE_DEBUG("Removed, checking dst->name \"{}\"", src->name);
            std::string accountName{dst->name};

            if (auto exists = AppData.Squad.find(accountName))
            {
                if (exists->player.role != static_cast<uint8_t>(UserRole::None))
                {
                    // Player was added by ArcDPS Unofficial Extras.
                    // Update information.

                    auto update = [](PlayerInfo& player)
                    { 
                        player.inInstance = false;
                    };
                    SquadHandler->updatePlayer(*exists, update, UpdateCombatPlayerSuccess);
                }
                else
                {
                    // Player was added by ArcDPS combat api.
                    // Remove player.

                    RemoveFromSquad(accountName, "combat");
                }
                
            }

            BRIDGE_DEBUG("Removed, CharCache, checking src->name \"{}\"", src->name);
            AppData.CharacterTypeCache.erase(std::string{src->name});
        }
    }
    else if (src->name)
    {
        // BRIDGE_DEBUG("CharCheck, checking src->name, val = {}", src->name);
        auto it = AppData.CharacterTypeCache.find(std::string{src->name});
        if (it != AppData.CharacterTypeCache.end())
        {
            constexpr auto uint32_max = std::numeric_limits<uint32_t>::max();
            if ((it->second.profession != src->prof || it->second.elite != src->elite) && (src->elite != uint32_max))
            {
                BRIDGE_DEBUG("CharChache: old = [{}, {}], new = [{}, {}].", it->second.profession, it->second.elite, src->prof, src->elite);
                it->second.profession = src->prof;
                it->second.elite = src->elite;
                UpdateCombatCharInfo(src->name, it->second);
            }
        }
    }

    if (!Server->trackingEvent(MessageType::Combat))
        return 0;

    // Combat event.
    std::ostringstream ss{};
    ss << "{\"type\":\"combat\",\"combat\":{";

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

    Server->sendMessage(ss.str(), MessageType::Combat);

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

    // Start the PipeHandler server.
    Server->start();

    return &arc_exports;
}

/* release mod -- return ignored */
static uintptr_t mod_release()
{
    BRIDGE_INFO("Releasing ArcDPS Bridge");
    AppData.Info.arcLoaded = false;

    // Stop and release the PipeHandler server.
    // Will also close all active connections to clients.
    Server->stop();
    Server.reset();

    return 0;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client load */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversionstr, void*, void*, HMODULE, void*, void*, UINT)
{
    AppData.Info.arcvers = std::string{arcversionstr};
    BRIDGE_INFO("ArcDPS version: \"{}\"", AppData.Info.arcvers);
    return mod_init;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client exit */
extern "C" __declspec(dllexport) void* get_release_addr()
{
    AppData.Info.arcvers = "";
    return mod_release;
}

static std::string ExtrasDataToJSON(const UserInfo* user)
{
    std::ostringstream ss{};
    ss << "{\"accountName\":\"" << std::string{user->AccountName} << "\","
       << "\"role\":" << static_cast<int>(static_cast<uint8_t>(user->Role)) << ","
       << "\"subgroup\":" << static_cast<int>(user->Subgroup + 1) << ","
       << "\"joinTime\":" << user->JoinTime << ","
       << "\"readyStatus\":" << ((user->ReadyStatus) ? "true" : "false") << "}";
    return ss.str();
}

static void UpdateExtrasPlayerInfo(const PlayerInfoEntry& existing, const UserInfo& user)
{
    auto update = [&user](PlayerInfo& player)
    {
        player.role = static_cast<uint8_t>(user.Role);
        player.subgroup = user.Subgroup + 1; // Starts at 0.
        if (player.joinTime != 0 && user.JoinTime != 0)
            player.joinTime = user.JoinTime;
    };
    auto success = [](const PlayerInfoEntry& entry)
    { 
        SendPlayerMsg("update", "extras", entry.player, entry.validator); 
    };
    SquadHandler->updatePlayer(existing, update, success);
}

// Callback for arcDPS unofficial extras API.
void squad_update_callback(const UserInfo* updatedUsers, uint64_t updatedUsersCount)
{
    for (uint64_t i{0}; i < updatedUsersCount; ++i)
    {
        const UserInfo* uinfo{&updatedUsers[i]};
        if (uinfo)
        {
            std::string accountName{uinfo->AccountName};

            if (uinfo->Role == UserRole::None)
            {
                // Remove.
                RemoveFromSquad(accountName, "extras");
            }
            else
            {
                // ArcDPS might have added the player already.
                auto exists = AppData.Squad.find(accountName);
                if (!exists)
                {
                    // Add.

                    PlayerInfo player{};
                    
                    if (AppData.Self.accountName == accountName)
                    {
                        BRIDGE_INFO("Adding self, using character name: \"{}\".", AppData.Self.characterName);
                        player = AppData.Self;
                        player.inInstance = true;
                        player.self = true;
                        auto it = AppData.CharacterTypeCache.find(player.characterName);
                        if (it != AppData.CharacterTypeCache.end())
                        {
                            player.profession = it->second.profession;
                            player.elite = it->second.elite;
                        }
                    }
                    else
                        player.accountName = accountName;
                    
                    player.role = static_cast<uint8_t>(uinfo->Role);
                    player.subgroup = uinfo->Subgroup + 1; // Starts at 0.
                    player.joinTime = uinfo->JoinTime;

                    auto success = [](const PlayerInfoEntry& entry)
                    { 
                        SendPlayerMsg("add", "extras", entry.player, entry.validator); 
                    };
                    auto failedAdd = [accountName, uinfo]()
                    {
                        // Entry got added just in the right time for add() to fail.
                        if (auto added = AppData.Squad.find(accountName))
                            UpdateExtrasPlayerInfo(*added, *uinfo);
                    };
                    SquadHandler->addPlayer(player, success, failedAdd);
                }
                else
                {
                    // Update.
                    UpdateExtrasPlayerInfo(*exists, *uinfo);
                }
            }

            if (Server->trackingEvent(MessageType::Extras))
            {
                const std::string data{ExtrasDataToJSON(uinfo)};
                std::ostringstream ss{};
                ss << "{\"type\":\"extras\","
                   << "\"extras\":" << data << "}";
                Server->sendMessage(ss.str(), MessageType::Extras);
            }
        }
    }
}

// Exported init function for arcDPS unofficial extras API.
extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* pExtrasInfo,
                                                                               void* pSubscriberInfo)
{
    AppData.Info.extrasFound = true;
    AppData.Info.extrasVersion = std::string{pExtrasInfo->StringVersion};
    BRIDGE_INFO("Unofficial Extras version: \"{}\"", AppData.Info.extrasVersion);

    if (!AppData.Config.enabled || !AppData.Config.extras)
    {
        BRIDGE_INFO("Unofficial Extras is disabled.");
        return;
    }

    if (pExtrasInfo->ApiVersion != 2)
    {
        BRIDGE_ERROR("Extras api version error, expected 2 and got \"{}\"", pExtrasInfo->ApiVersion);
        return;
    }

    if (pExtrasInfo->MaxInfoVersion == 1)
    {
        BRIDGE_INFO("Unofficial Extras is enabled.");
        AppData.Info.extrasLoaded = true;

        ExtrasSubscriberInfoV1 extrasInfo{};
        extrasInfo.InfoVersion = 1;
        extrasInfo.SubscriberName = "Unofficial Bridge";
        extrasInfo.SquadUpdateCallback = squad_update_callback;
        *static_cast<ExtrasSubscriberInfoV1*>(pSubscriberInfo) = extrasInfo;

        // PlayerCollection.self.accountName = pExtrasInfo->SelfAccountName;
        AppData.Self.accountName = pExtrasInfo->SelfAccountName;
        AppData.Self.self = true;
        BRIDGE_INFO("Self account name (Extras): \"{}\"", AppData.Self.accountName);
        return;
    }

    BRIDGE_ERROR("Extra max info version \"{}\" is not supported.", pExtrasInfo->MaxInfoVersion);
}
