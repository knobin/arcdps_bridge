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
#include <iterator>
#include <unordered_map>

// Windows Headers
#include <windows.h>

// Hash function for unordered_multimap.
struct djb2_hash
{
    std::size_t operator()(const std::string& str) const
    {
        std::size_t hash = 5381;

        for (const char c : str)
            hash = ((hash << 5) + hash) + static_cast<int>(c); /* hash * 33 + c */

        return hash;
    }
};

static ApplicationData AppData{};
static std::unique_ptr<PipeHandler> Server{nullptr}; // {std::string{AppData.PipeName}, AppData};
static std::unique_ptr<SquadModifyHandler> SquadHandler{nullptr};
static std::unique_ptr<std::unordered_multimap<std::string, CharacterType, djb2_hash>> CharacterTypeCache{nullptr};

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
                SquadHandler = std::make_unique<SquadModifyHandler>(AppData.Squad);
                Server = std::make_unique<PipeHandler>(std::string{AppData.PipeName}, AppData, SquadHandler.get());

                CharacterTypeCache = std::make_unique<std::unordered_multimap<std::string, CharacterType, djb2_hash>>();
                CharacterTypeCache->reserve(50);
            }
            break;
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            SquadHandler.reset();
            CharacterTypeCache.reset();
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

    if (Server->trackingCategory(MessageCategory::Squad))
    {
        SerialData serial{};
        nlohmann::json json{};

        if (Server->usingProtocol(MessageProtocol::Serial))
        {
            const std::size_t playerentry_size = serial_size(entry);
            serial = CreateSerialData(playerentry_size);
            to_serial(entry, &serial.ptr[SerialStartPadding], playerentry_size);
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

static void RemoveFromSquad(const std::string& accountName, uint8_t bits)
{
    SquadHandler->removePlayer(accountName, SquadModifySender, bits);
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

    if (useSerial)
        serial = combat_to_serial(ev, src, dst, skillname, id, revision);

    if (useJSON)
        json = combat_to_json(ev, src, dst, skillname, id, revision);

    return CombatMessage<MessageType::CombatEvent>(serial, json);
}

template <typename Iter>
static void UpdateCharCacheIter(Iter& it, uint32_t profession, uint32_t elite)
{
    constexpr auto uint32_max = std::numeric_limits<uint32_t>::max();
    if ((it->second.profession != profession || it->second.elite != elite) && (elite != uint32_max))
    {
        BRIDGE_DEBUG("CharChache: old = [{}, {}], new = [{}, {}].", it->second.profession, it->second.elite, profession,
                     elite);
        it->second.profession = profession;
        it->second.elite = elite;
        UpdateCombatCharInfo(it->first, it->second);
    }
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
            if (dst->self && AppData.SelfAccountName.empty())
            {
                AppData.SelfAccountName = accountName;
                BRIDGE_DEBUG("Self account name (Combat): \"{}\"", AppData.SelfAccountName);
            }

            auto updater = [accountName, src, dst](PlayerInfo& player)
            {
                // Entry got added just in the right time for add to fail.
                UpdateCombatPlayerInfo(player, src, dst);
            };
            SquadHandler->addPlayer(player, SquadModifySender, updater, SquadModifyHandler::CombatBit | SquadModifyHandler::ExtrasBit);

            CharacterType ct{};
            ct.profession = dst->prof;
            ct.elite = dst->elite;
            BRIDGE_DEBUG("Added, CharCache, checking src->name \"{}\"", src->name);
            CharacterTypeCache->emplace(std::string{src->name}, ct);
#ifdef BRIDGE_BUILD_DEBUG
            if (CharacterTypeCache->size() > 50)
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
            uint8_t bits = SquadModifyHandler::CombatBit;
            
            if (AppData.Info.extrasLoaded)
            {
                auto updater = [](PlayerInfo& player) { player.inInstance = false; };
                SquadHandler->updatePlayer(accountName, SquadModifySender, updater);
            }
            else
            {
                bits |= SquadModifyHandler::ExtrasBit;
            }

            // Remove (or decrement counter).
            RemoveFromSquad(accountName, bits);

            BRIDGE_DEBUG("Removed, CharCache, checking src->name \"{}\"", src->name);
            CharacterTypeCache->erase(std::string{src->name});
        }
    }
    else if (src->name)
    {
        // BRIDGE_DEBUG("CharCheck, checking src->name, val = {}", src->name);
        
        const std::string charName{src->name};
        auto range = CharacterTypeCache->equal_range(charName);
        const auto count = std::distance(range.first, range.second);

        if (count == 1)
            UpdateCharCacheIter(range.first, src->prof, src->elite);
        else
            for (auto it = range.first; it != range.second; ++it)
                if (it->first == charName)
                    UpdateCharCacheIter(range.first, src->prof, src->elite);
    }

    if (!Server->trackingCategory(MessageCategory::Combat))
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
                uint8_t bits = SquadModifyHandler::ExtrasBit;
                if (!AppData.Info.arcLoaded || (AppData.SelfAccountName == accountName))
                    bits |= SquadModifyHandler::CombatBit;

                RemoveFromSquad(accountName, bits);
            }
            else if (role == UserRole::SquadLeader || role == UserRole::Lieutenant || role == UserRole::Member)
            {
                // Add.
                
                PlayerInfo player{};
                player.accountName = accountName;
                        
                if (AppData.SelfAccountName == accountName)
                {
                    player.inInstance = true;
                    player.self = true;
                }

                ExtrasPlayerInfoUpdater(player, *uinfo);
                
                auto updater = [uinfo](PlayerInfo& player) { ExtrasPlayerInfoUpdater(player, *uinfo); };
                SquadHandler->addPlayer(player, SquadModifySender, updater, SquadModifyHandler::ExtrasBit);
            }

            if (Server->trackingCategory(MessageCategory::Extras))
            {
                SerialData serial{};
                nlohmann::json json{};

                if (Server->usingProtocol(MessageProtocol::Serial))
                {
                    const std::size_t uinfo_count = serial_size(*uinfo);
                    serial = CreateSerialData(uinfo_count);
                    to_serial(*uinfo, &serial.ptr[SerialStartPadding], uinfo_count);
                }

                if (Server->usingProtocol(MessageProtocol::JSON))
                    json = *uinfo;

                const Message extrasMsg{ExtrasMessage<MessageType::ExtrasSquadUpdate>(serial, json)};
                Server->sendMessage(extrasMsg);
            }
        }
    }
}

static void language_changed_callback(Language pNewLanguage)
{
    if (Server->trackingCategory(MessageCategory::Extras))
    {
        SerialData serial{};
        nlohmann::json json{};

        if (Server->usingProtocol(MessageProtocol::Serial))
        {
            constexpr std::size_t lang_count = serial_size(Language{});
            serial = CreateSerialData(lang_count);
            to_serial(pNewLanguage, &serial.ptr[SerialStartPadding], lang_count);
        }

        if (Server->usingProtocol(MessageProtocol::JSON))
            json = pNewLanguage;

        const Message extrasMsg{ExtrasMessage<MessageType::ExtrasLanguageChanged>(serial, json)};
        Server->sendMessage(extrasMsg);
    }
}

static void keybind_changed_callback(KeyBinds::KeyBindChanged pChangedKeyBind)
{
    if (Server->trackingCategory(MessageCategory::Extras))
    {
        SerialData serial{};
        nlohmann::json json{};

        if (Server->usingProtocol(MessageProtocol::Serial))
        {
            constexpr std::size_t keybind_count = serial_size(KeyBinds::KeyBindChanged{});
            serial = CreateSerialData(keybind_count);
            to_serial(pChangedKeyBind, &serial.ptr[SerialStartPadding], keybind_count);
        }

        if (Server->usingProtocol(MessageProtocol::JSON))
            to_json(json, pChangedKeyBind);

        const Message extrasMsg{ExtrasMessage<MessageType::ExtrasLanguageChanged>(serial, json)};
        Server->sendMessage(extrasMsg);
    }
}

static void InitExtrasV1(ExtrasSubscriberInfoV1& info)
{
    info.InfoVersion = 1;
    info.SubscriberName = "Unofficial Bridge";
    info.SquadUpdateCallback = squad_update_callback;
    info.LanguageChangedCallback = language_changed_callback;
    info.KeyBindChangedCallback = keybind_changed_callback;
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
        InitExtrasV1(extrasInfo);
        *static_cast<ExtrasSubscriberInfoV1*>(pSubscriberInfo) = extrasInfo;

        // PlayerCollection.self.accountName = pExtrasInfo->SelfAccountName;
        AppData.SelfAccountName = pExtrasInfo->SelfAccountName;
        BRIDGE_DEBUG("Self account name (Extras): \"{}\"", AppData.SelfAccountName);
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
            const std::size_t appdata_size = serial_size(AppData.Info);
            serial = CreateSerialData(appdata_size);
            to_serial(AppData.Info, &serial.ptr[SerialStartPadding], appdata_size);
        }

        if (Server->usingProtocol(MessageProtocol::JSON))
            json = AppData.Info;

        // Send the new info to connected clients that have already received the old information.
        const Message bridgeMsg{InfoMessage<MessageType::BridgeInfo>(serial, json)};
        Server->sendBridgeInfo(bridgeMsg, AppData.Info.validator);
    }
}
