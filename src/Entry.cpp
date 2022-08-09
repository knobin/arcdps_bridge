//
//  src/Entry.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#define NOMINMAX

// Local Headers
#include "ApplicationData.hpp"
#include "Combat.hpp"
#include "Extras.hpp"
#include "Log.hpp"
#include "SquadModifyHandler.hpp"
#include "PipeHandler.hpp"

// C++ Headers
#include <cstddef>
#include <limits>

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

            BRIDGE_INFO("Bridge service is enabled by configs: {}.", AppData.Config.enabled);

            if (AppData.Config.enabled)
            {
                Server = std::make_unique<PipeHandler>(std::string{AppData.PipeName}, AppData);
                SquadHandler = std::make_unique<SquadModifyHandler>(AppData.Squad);

                AppData.CharacterTypeCache.reserve(50);
            }
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

///////////////////////////////////////////////////////////////////////////////////////////////////

template<MessageType Type>
static void SendPlayerMsg(const PlayerInfoEntry& entry)
{
    static_assert(Type == MessageType::SquadAdd || Type == MessageType::SquadRemove ||
                    Type == MessageType::SquadUpdate,
                  "Type is not a Squad message");

    if (Server->trackingEvent(MessageSource::Squad))
    {
        SerialData serial{};
        nlohmann::json json{};

        if (Server->usingProtocol(MessageProtocol::Serial))
        {
            ; // TODO.
        }

        if (Server->usingProtocol(MessageProtocol::JSON))
        {
            json = entry;
        }

        const Message squadMsg{SquadMessage<Type>(serial, json)};
        Server->sendMessage(squadMsg);
    }
}

static void SquadModifySender(SquadAction action, const PlayerInfoEntry& entry)
{
    switch (action)
    {
        case SquadAction::Add:
            SendPlayerMsg<MessageType::SquadAdd>(entry);
            break;
        case SquadAction::Update:
            SendPlayerMsg<MessageType::SquadUpdate>(entry);
            break;
        case SquadAction::Remove:
            SendPlayerMsg<MessageType::SquadRemove>(entry);
            break;
    }
}

static void UpdateCombatPlayerInfo(PlayerInfo& player, ag* src, ag* dst)
{
    player.characterName = std::string{src->name};
    player.profession = dst->prof;
    player.elite = dst->elite;
    player.inInstance = true;
}

static void UpdateCombatCharInfo(const std::string& name, CharacterType ct)
{
    auto p = [&name](const PlayerInfo& player) { return player.characterName == name; };
    auto updater = [ct](PlayerInfo& player)
    {
        player.profession = ct.profession;
        player.elite = ct.elite;
    };
    SquadHandler->updatePlayer(p, SquadModifySender, updater);
}

static void RemoveFromSquad(const std::string& accountName, const std::string& sType)
{
    auto success = [accountName , sType](SquadAction, PlayerInfoEntry& entry)
    {
        SendPlayerMsg<MessageType::SquadRemove>(entry);

        if (entry.player.self)
        {
            BRIDGE_DEBUG("Removing self \"{}\", saving character info for \"{}\".", accountName,
                            entry.player.characterName);
            AppData.Self = entry.player;
        }
    };
    SquadHandler->removePlayer(accountName, success);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static Message GenerateCombatMessage(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision)
{
    const bool useSerial{Server->usingProtocol(MessageProtocol::Serial)};
    const bool useJSON{Server->usingProtocol(MessageProtocol::JSON)};

    if (!useSerial && !useJSON)
        return Message{};

    SerialData serial{};
    nlohmann::json json{};

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

    if (useSerial)
    {
        ; // TODO.
    }

    if (useJSON)
    {
        json = {
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

        if (!sn.empty())
            json["skillname"] = sn;
    }
   
    return CombatMessage<MessageType::CombatEvent>(serial, json);
}

/* window callback -- return is assigned to umsg (return zero to not be processed by arcdps or game)
 */
static uintptr_t mod_wnd(HWND, UINT uMsg, WPARAM, LPARAM)
{
    return uMsg;
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
                BRIDGE_DEBUG("Self account name (Combat): \"{}\"", AppData.Self.accountName);
            }

            auto updater = [accountName, src, dst](PlayerInfo& player)
            {
                // Entry got added just in the right time for add to fail.
                UpdateCombatPlayerInfo(player, src, dst);
            };
            SquadHandler->addPlayer(player, SquadModifySender, updater);

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

            if (AppData.Info.extrasLoaded)
            {
                // Unofficial Extras is present and used in the bridge.
                // Therefore only Unofficial Extras can remove players.
                // Update information.

                auto updater = [](PlayerInfo& player)
                { 
                    player.inInstance = false;
                };
                SquadHandler->updatePlayer(accountName, SquadModifySender, updater);
            }
            else
            {
                // Unofficial Extras is not used (callback not set or extension not present at all).
                // Then only combat events can add/remove players.

                // If extras is loaded it will remove the player. This will ensure that players 
                // added by combat api will not be removed when not in the same instance.
                // Which enables the ability to "recover" over time from a crash if the game 
                // crashed when in a squad.

                BRIDGE_DEBUG("ArcDPS combat event removed \"{}\", since extras is not used.", accountName);
                RemoveFromSquad(accountName, "combat");
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

    if (!Server->trackingEvent(MessageSource::Combat))
        return 0;

    Message combatMsg{GenerateCombatMessage(ev, src, dst, skillname, id, revision)};
    
    if (!combatMsg.empty())
        Server->sendMessage(combatMsg);

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

    if (AppData.Config.enabled)
    {
        if (AppData.Config.arcDPS)
        {
            arc_exports.combat = mod_combat;
            AppData.Info.arcLoaded = true;
            BRIDGE_INFO("ArcDPS is enabled.");
        }
#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_INFO
        else
        {
            BRIDGE_INFO("ArcDPS is disabled by configs!");
        }
#endif   
    }
    else
    {
        // This will create a warning in the arcdps log.
        // Will maybe change this later, due to having a silent warning instead.
        // Since this is not an error, only a way to turn of the extension.
        // This will cause the extension to exit after this though.
        arc_exports.sig = 0;
        arc_exports.size = (uintptr_t) "Unofficial bridge is disabled by configs!";
        BRIDGE_INFO("Bridge service is disabled by configs, exiting...");
    }

    // Start the PipeHandler server (if brige is enabled).
    if (AppData.Config.enabled)
        Server->start();

    return &arc_exports;
}

/* release mod -- return ignored */
static uintptr_t mod_release()
{
    BRIDGE_INFO("Releasing ArcDPS Bridge");
    AppData.Info.arcLoaded = false;

    // Stop and release the PipeHandler server (if brige is enabled).
    // Will also close all active connections to clients.
    if (AppData.Config.enabled)
    {
        Server->stop();
        Server.reset();
    }

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

///////////////////////////////////////////////////////////////////////////////////////////////////

static void ExtrasPlayerInfoUpdater(PlayerInfo& player, const UserInfo& user)
{
    player.role = static_cast<uint8_t>(user.Role);
    player.subgroup = user.Subgroup + 1; // Starts at 0.
    player.readyStatus = user.ReadyStatus;
    if (player.joinTime != 0 && user.JoinTime != 0)
        player.joinTime = user.JoinTime;
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
            UserRole role{uinfo->Role};

            if (role == UserRole::None)
            {
                // Remove.
                RemoveFromSquad(accountName, "extras");
            }
            else if (role == UserRole::SquadLeader || role == UserRole::Lieutenant || role == UserRole::Member)
            {
                // Add.
                
                PlayerInfo player{};
                        
                if (AppData.Self.accountName == accountName)
                {
                    BRIDGE_DEBUG("Adding self, using character name: \"{}\".", AppData.Self.characterName);
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
                {
                    player.accountName = accountName;
                }

                ExtrasPlayerInfoUpdater(player, *uinfo);
                
                auto updater = [uinfo](PlayerInfo& player) { ExtrasPlayerInfoUpdater(player, *uinfo); };
                SquadHandler->addPlayer(player, SquadModifySender, updater);
            }

            if (Server->trackingEvent(MessageSource::Extras))
            {
                SerialData serial{};
                nlohmann::json json{};

                if (Server->usingProtocol(MessageProtocol::Serial))
                {
                    ; // TODO.
                }

                if (Server->usingProtocol(MessageProtocol::JSON))
                    json = *uinfo;

                const Message extrasMsg{ExtrasMessage<MessageType::ExtrasSquadUpdate>(serial, json)};
                Server->sendMessage(extrasMsg);
            }
        }
    }
}

// Exported init function for arcDPS unofficial extras API.
extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(const ExtrasAddonInfo* pExtrasInfo,
                                                                               void* pSubscriberInfo)
{
    bool loaded{false};
    std::string version{pExtrasInfo->StringVersion};

    BRIDGE_INFO("Unofficial Extras version: \"{}\"", version);

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

    if (pExtrasInfo->MaxInfoVersion >= 1)
    {
        BRIDGE_INFO("Unofficial Extras is enabled.");
        loaded = true;

        ExtrasSubscriberInfoV1 extrasInfo{};
        extrasInfo.InfoVersion = 1;
        extrasInfo.SubscriberName = "Unofficial Bridge";
        extrasInfo.SquadUpdateCallback = squad_update_callback;
        *static_cast<ExtrasSubscriberInfoV1*>(pSubscriberInfo) = extrasInfo;

        // PlayerCollection.self.accountName = pExtrasInfo->SelfAccountName;
        AppData.Self.accountName = pExtrasInfo->SelfAccountName;
        AppData.Self.self = true;
        BRIDGE_DEBUG("Self account name (Extras): \"{}\"", AppData.Self.accountName);
    }
    else
    {
        BRIDGE_ERROR("Extras MaxInfoVersion: \"{}\" is not supported.", pExtrasInfo->MaxInfoVersion);
    }

    // Send updated bridge information.
    {
        std::unique_lock<std::mutex> lock(AppData.Info.mutex);

        AppData.Info.extrasFound = true;
        AppData.Info.extrasLoaded = loaded;
        AppData.Info.extrasVersion = version;
        ++AppData.Info.validator;
        BRIDGE_DEBUG("Updated BridgeInfo");

        SerialData serial{};
        nlohmann::json json{};

        if (Server->usingProtocol(MessageProtocol::Serial))
        {
            ; // TODO.
        }

        if (Server->usingProtocol(MessageProtocol::JSON))
            json = AppData.Info;

        // Send the new info to connected clients that have already received the old information.
        const Message bridgeMsg{InfoMessage<MessageType::BridgeInfo>(serial, json)};
        Server->sendBridgeInfo(bridgeMsg, AppData.Info.validator);
    }
}
