//
//  bridge.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-03-15.
//

// ArcDPS Unofficial Extras Header
#include "Definitions.h"

// C++ Headers
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <filesystem>
#include <array>
#include <type_traits>

// Windows Headers
#include <windows.h>

// Path to the DLL.
static std::string DllPath{""};
static constexpr std::string_view ConfigFile{"arcdps_bridge.ini"};

static struct Config
{
    bool enabled{true}; // Should the extension be enabled.
    bool arcDPS{true}; // Should ArcDPS be used.
    bool extras{true}; // Should the Unofficial Extras be used.
    bool logging{true}; // Should debug logging be printed.
    bool msgLog{false}; // Should messages sent be logged.
    const std::string_view logFile{"arcdps_bridge.log"};
} Configs;

#ifdef DEBUG

template<typename... Args>
static void Print(Args&&... args)
{
    if (!Configs.logging)
        return;

    static std::mutex LogMutex;

    std::time_t timer{std::time(0)};
    std::tm bt{};
    localtime_s(&bt, &timer);
    char time_buf[80];
    std::string time_str{time_buf, std::strftime(time_buf, sizeof(time_buf), "%F %T", &bt)};

    std::unique_lock<std::mutex> lock(LogMutex);
    std::ofstream outfile; // Opens file for every call to print. Bad. but fine for debugging purposes.
    outfile.open(DllPath + std::string{Configs.logFile}, std::ios_base::app);
    outfile << "[" << time_str << "] ";
    (outfile << ... << args) << '\n';
    outfile.close();
}

#define DEBUG_LOG(...) Print(__VA_ARGS__)
#define DEBUG_MSG_LOG(...) if (Configs.msgLog) Print(__VA_ARGS__)

#else

#define DEBUG_LOG(...)
#define DEBUG_MSG_LOG(...)

#endif

static void CreateConfigFile()
{
    const std::string path{DllPath + std::string{ConfigFile}};
    DEBUG_LOG("Creating Config File \"", path, "\"");

    std::ofstream configFile;
    configFile.open(path);

    configFile << "[general]\n";
    configFile << "enabled = " << ((Configs.enabled) ? "true" : "false") << "\n";
    configFile << "extras = " << ((Configs.extras) ? "true" : "false") << "\n";
    configFile << "arcDPS = " << ((Configs.extras) ? "true" : "false") << "\n";

    configFile << "[debug]\n";
    configFile << "logging = " << ((Configs.logging) ? "true" : "false") << "\n";
    configFile << "msgLog = " << ((Configs.logging) ? "true" : "false") << "\n";

    configFile.close();
}

static void LoadConfigFile()
{
    const std::string path{DllPath + std::string{ConfigFile}};
    DEBUG_LOG("Loading Config File \"", path, "\"");

    std::ifstream configFile;
    configFile.open(path, std::ifstream::in);

    std::string line{};
    unsigned int lineNumber{0};
    std::string header{};
    while (std::getline(configFile, line))
    {
        ++lineNumber;
        auto it = std::remove_if(line.begin(), line.end(), ::isspace);
        line.erase(it, line.end());

        if (!line.empty())
        {
            if (line.front() == '[' && line.back() == ']')
            {
                header = line.substr(1, line.size() - 2);
                DEBUG_LOG("Found Config Header \"", header, "\"");
            }
            else if (!header.empty())
            {
                std::size_t equalPos = line.find('=');
                if (equalPos != std::string::npos
                    && equalPos > 0 && equalPos < line.size() - 1)
                {
                    std::string name = line.substr(0, equalPos);
                    std::string value = line.substr(equalPos + 1);
                    DEBUG_LOG("Found Config Entry \"", name, "\" = ", value);

                    if (header == "general")
                    {
                        if (name == "enabled")
                            Configs.enabled = ((value == "true") ? true : false);
                        else if (name == "extras")
                            Configs.extras = ((value == "true") ? true : false);
                        else if (name == "arcDPS")
                            Configs.arcDPS = ((value == "true") ? true : false);
                    }
                    else if (header == "debug")
                    {
                        if (name == "logging")
                            Configs.logging = ((value == "true") ? true : false);
                        else if (name == "msgLog")
                            Configs.msgLog = ((value == "true") ? true : false);
                    }
                }
            }
        }

    }
}

/* arcdps export table */
struct arcdps_exports
{
    uintptr_t size; /* size of exports table */
    uint32_t sig;   /* pick a number between 0 and uint32_t max that isn't used by other modules */
    uint32_t imguivers;    /* set this to IMGUI_VERSION_NUM. if you don't use imgui, 18000 (as of 2021-02-02) */
    const char* out_name;  /* name string */
    const char* out_build; /* build string */
    void* wnd_nofilter; /* wndproc callback, fn(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam), return assigned to umsg */
    void* combat; /* combat event callback, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) */
    void* imgui; /* ::present callback, before imgui::render, fn(uint32_t not_charsel_or_loading) */
    void* options_end; /* ::present callback, appending to the end of options window in arcdps, fn() */
    void* combat_local; /* combat event callback like area but from chat log, fn(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id, uint64_t revision) */
    void* wnd_filter; /* wndproc callback like wnd_nofilter above, input filered using modifiers */
    void* options_windows; /* called once per 'window' option checkbox, with null at the end, non-zero return disables arcdps drawing that checkbox, fn(char* windowname) */
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

enum class ThreadStatus : uint8_t
{
    NONE = 0,                 // Not created.
    WaitingForConnection = 4, // Pipe is created and waiting for connection.
    Reading = 8,              // Pipe is reading / waiting for a message from the client.
    WaitingForMessage = 16,   // Pipe connection exists and is waiting for a message to be queued.
    Sending = 32              // Pipe connection exists and is sending.
};

// Variables for thread that is sending data.
static struct ThreadData
{
    HANDLE handle;
    const std::string_view pipeName{"\\\\.\\pipe\\arcdps-bridge"};
    ThreadStatus status{ThreadStatus::NONE};
    std::atomic<bool> run{true};
    std::thread thread{};
} PipeThread;

static struct MessageContainer
{
    std::mutex mutex{};
    std::condition_variable cv{};
    std::queue<std::string> queue{};
} MsgCont;

// Information about the bridge.
static struct Info
{
    const std::string_view version{"1.0"}; // Bridge version.
    std::string extraVersion{""}; // Unofficial Extras version.
    std::string arcvers{""}; // ArcDPS version.
    bool arcLoaded{false}; // Is ArcDPS loaded.
    bool extraLoaded{false}; // Is Unofficial Extras loaded.
} BridgeInfo;

static std::string BridgeInfoToJSON()
{
    std::ostringstream ss{};
    ss << "{\"type\":\"Info\",";
    ss << "\"Version\":\"" << std::string{BridgeInfo.version} << "\","
       << "\"ExtraVersion\":\"" << BridgeInfo.extraVersion << "\","
       << "\"ArcVersion\":\"" << BridgeInfo.arcvers << "\","
       << "\"ArcLoaded\":" << ((BridgeInfo.arcLoaded) ? "true" : "false") << ","
       << "\"ExtraLoaded\":" << ((BridgeInfo.extraLoaded) ? "true" : "false") << "}";
    return ss.str();
}

static struct EventTracking
{
    bool combat{false};
    bool extra{false};
} UseEvents;

static struct PlayerContainer
{
    struct PlayerInfo
    {
        std::string accountName{};
        UserRole role{};
        uint8_t subgroup{};
        bool valid{false};
    };
    std::string self{};
    std::array<PlayerInfo, 50> players{};
    mutable std::mutex mutex{};

    PlayerInfo* add(const std::string& accountName)
    {
        std::unique_lock<std::mutex> lock(mutex);

        auto player = std::find_if(players.begin(), players.end(), [&accountName](const PlayerInfo& p) {
            return (accountName == p.accountName);
        });
        bool updated = player != players.end();
        if (player == players.end())
        {
            player = std::find_if(players.begin(), players.end(), [](const PlayerInfo& p) {
                return !p.valid;
            });
        }

        if (player != players.end())
        {
            DEBUG_LOG(((updated) ? "Updated" : "Added"), " \"", accountName, "\" ", ((updated) ? "in" : "to"), " squad.");
            player->accountName = accountName;
            return &(*player);
        }

        DEBUG_LOG("Exceeding squad limit of 50 players trying to add \"", accountName, "\".");
        return nullptr;
    }

    void remove(const std::string& accountName)
    {
        std::unique_lock<std::mutex> lock(mutex);

        if (accountName == self)
        {
            DEBUG_LOG("Self \"", accountName, "\" left squad, removing all players.");
            for (std::size_t i{0}; i < players.size(); ++i)
                players[i].valid = false;
        }
        else
        {
            DEBUG_LOG("Removing \"", accountName, "\" from squad.");
            auto player = std::find_if(players.begin(), players.end(), [&accountName](const PlayerInfo& p) {
                return (accountName == p.accountName);
            });
            if (player != players.end())
                player->valid = false;
        }
    }

    std::string toJSON() const
    {
        std::unique_lock<std::mutex> lock(mutex);

        std::ostringstream ss{};
        ss << "{\"type\":\"Squad\",\"squad\":{";
        ss << "\"self\":\"" << self << "\",";
        ss << "\"members\":[";

        uint8_t added{0};
        for (std::size_t i{0}; i < players.size(); ++i)
            if (players[i].valid)
            {
                const PlayerContainer::PlayerInfo& player{players[i]};
                ss << ((added++ > 0) ? "," : "") << "{"
                   << "\"AccountName\":\"" << player.accountName << "\","
                   << "\"Role\":" << static_cast<int>(player.role) << ","
                   << "\"Subgroup\":" << static_cast<int>(player.subgroup)
                   << "}";
            }


        ss << "]}}";
        return ss.str();
    }

} PlayerCollection;

enum class MessageType : uint8_t
{
    NONE = 0,
    Info = 1,
    Combat = 2,
    Extra = 4,
    Squad = 8
};

static void SendToClient(const std::string& msg, MessageType type)
{
    // Send if pipe has been created and a connection exists.
    if (PipeThread.status != ThreadStatus::NONE
        && PipeThread.status != ThreadStatus::WaitingForConnection)
    {
        std::unique_lock<std::mutex> lock(MsgCont.mutex);
        if (MsgCont.queue.size() < 200)
        {
            DEBUG_MSG_LOG("Message of type \"", static_cast<int>(type), "\" queued for sending.");
            MsgCont.queue.push(msg);
            MsgCont.cv.notify_one();
        }
    }
}

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
static uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id,
                            uint64_t revision)
{
    if (!UseEvents.combat)
        return 0;

    // Combat event.
    std::ostringstream ss{};
    ss << "{\"type\":\"Combat\",";

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
    ss << "}\0";

    SendToClient(ss.str(), MessageType::Combat);

    return 0;
}

struct SendStatus
{
    DWORD numBytesWritten{0};
    DWORD error{0};
    BOOL success{false};
};

static SendStatus WriteToPipe(HANDLE handle, const std::string& msg)
{
    DEBUG_MSG_LOG("Sending \"", msg, "\" to client!");
    const DWORD length{static_cast<DWORD>(msg.size())};
    SendStatus status{};
    status.success = WriteFile(handle, msg.c_str(), length, &status.numBytesWritten, NULL);
    if (!status.success)
    {
        status.error = GetLastError();
        DEBUG_LOG("Error sending data with err: ", status.error, "!");
    }
    return status;
}

struct ReadStatus
{
    DWORD numBytesRead{0};
    DWORD error{0};
    BOOL success{false};
    std::string data{};
};

#define BUFSIZE 512

static ReadStatus ReadFromPipe(HANDLE handle)
{
    DEBUG_LOG("Reading data from client!");
    ReadStatus status{};
    TCHAR buffer[BUFSIZE];

    do
    {
        status.success = ReadFile(handle, buffer, BUFSIZE*sizeof(TCHAR), &status.numBytesRead, NULL);
        status.error = GetLastError();
        if (!status.success && status.error != ERROR_MORE_DATA)
            break;

        if (status.numBytesRead < BUFSIZE)
            buffer[status.numBytesRead] = '\0';

        status.data += std::string{buffer};
    } while (!status.success);

    if (!status.success)
    {
        status.error = GetLastError();
        DEBUG_LOG("Error reading data with err: ", status.error, "!");
        return status;
    }
    DEBUG_MSG_LOG("Retrieved \"", status.data, "\" from client!");
    return status;
}

static void PipeThreadFunc()
{
    DEBUG_LOG("Started Pipe server!");

    bool ShouldClose{false};
    while (!ShouldClose)
    {
        PipeThread.status = ThreadStatus::NONE;
        CloseHandle(PipeThread.handle);

        DEBUG_LOG("Creating Named pipe \"", std::string{PipeThread.pipeName}, "\"");
        PipeThread.handle = CreateNamedPipe(PipeThread.pipeName.data(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE,
                                            1, 0, 0, 0, NULL);

        if (PipeThread.handle == NULL || PipeThread.handle == INVALID_HANDLE_VALUE)
        {
            DEBUG_LOG("Error creating pipe with err: ", GetLastError(), "!");
            continue;
        }

        DEBUG_LOG("Created Named pipe \"", std::string{PipeThread.pipeName}, "\"");

        DEBUG_LOG("Waiting for client!");
        PipeThread.status = ThreadStatus::WaitingForConnection;
        BOOL result = ConnectNamedPipe(PipeThread.handle, NULL); // Blocking

        if (!result)
        {
            DEBUG_LOG("Error connecting pipe with err:", GetLastError(), "!");
            continue;
        }

        DEBUG_LOG("Client connected, sending bridge information...");
        std::string msg = BridgeInfoToJSON();
        PipeThread.status = ThreadStatus::Sending;
        SendStatus sendStatus = WriteToPipe(PipeThread.handle, msg);
        if (!result)
            continue;

        DEBUG_LOG("Waiting for client to subscribe...");
        PipeThread.status = ThreadStatus::Reading;
        ReadStatus readStatus = ReadFromPipe(PipeThread.handle);
        if (!result)
            continue;

        // Expecting: {"subscribe":15}
        // The number is the MessageType's you want to recieved. 15 in this case is all of them.
        // This was to extract the number should probably change in the future.
        std::size_t firstDigit = readStatus.data.find_first_of("0123456789");
        using MessageTypeU = std::underlying_type_t<MessageType>;
        MessageTypeU filter = 0;
        if (firstDigit != std::string::npos)
        {
            std::size_t lastDigit = readStatus.data.find_first_not_of("0123456789", firstDigit);
            if (lastDigit != std::string::npos && lastDigit > firstDigit)
            {
                DEBUG_LOG("substr: \"", readStatus.data.substr(firstDigit, lastDigit - firstDigit), "\"");
                std::istringstream iss{readStatus.data.substr(firstDigit, lastDigit - firstDigit)};
                int i = 0;
                iss >> i;
                filter = static_cast<MessageTypeU>(i);
                DEBUG_LOG("Recieved filter \"", static_cast<int>(filter), "\" from client.", i);
            }
        }

        if (filter == 0)
        {
            DEBUG_LOG("Filter is 0!");
            continue;
        }
        else if ((filter & 8) == 8)
        {
            PipeThread.status = ThreadStatus::Sending;
            DEBUG_LOG("Sending Squad information to client...");
            SendStatus sendStatus = WriteToPipe(PipeThread.handle, PlayerCollection.toJSON());
        }

        MessageTypeU combatValue = static_cast<MessageTypeU>(MessageType::Combat);
        UseEvents.combat = ((filter & combatValue) == combatValue);
        DEBUG_LOG("Client has subscribed to \"Combat\": ", UseEvents.combat);

        MessageTypeU extraValue = static_cast<MessageTypeU>(MessageType::Extra);
        UseEvents.extra = ((filter & extraValue) == extraValue);
        DEBUG_LOG("Client has subscribed to \"Extra\": ", UseEvents.extra);

        while (PipeThread.run)
        {
            DEBUG_MSG_LOG("Retrieving message to send.");
            msg = "";

            {
                std::unique_lock<std::mutex> lock(MsgCont.mutex);
                PipeThread.status = ThreadStatus::WaitingForMessage;

                // Block thread until message is added to queue.
                while (MsgCont.queue.empty())
                    MsgCont.cv.wait(lock);

                msg = MsgCont.queue.front();
                MsgCont.queue.pop();

                // Do not send empty message.
                if (msg.empty())
                {
                    DEBUG_LOG("Empty message found");
                    continue;
                }
            }

            // Send retrieved message.
            PipeThread.status = ThreadStatus::Sending;
            sendStatus = WriteToPipe(PipeThread.handle, msg);

            if (!sendStatus.success)
            {
                if (sendStatus.error == ERROR_BROKEN_PIPE || sendStatus.error == ERROR_NO_DATA)
                {
                    DEBUG_LOG("Client unexpectedly disconnected!");
                    break;
                }
            }

            DEBUG_MSG_LOG("Data sent to client!");
        }

        if (!PipeThread.run)
        {
            DEBUG_LOG("Pipe server is closing!");
            ShouldClose = true;
        }
    }

    PipeThread.status = ThreadStatus::NONE;
    CloseHandle(PipeThread.handle);
    DEBUG_LOG("Ended pipe server!");
}


static void GetDllPath(HMODULE hModule)
{
    char path[MAX_PATH];
    if (GetModuleFileName(hModule, path, sizeof(path)) == 0)
    {
        DEBUG_LOG("GetModuleFileName failed with error \"", GetLastError(), "\"");
        return;
    }
    std::string spath = std::string{path};
    std::size_t lastBackslash = spath.find_last_of('\\');
    if (lastBackslash != std::string::npos)
        DllPath = spath.substr(0, lastBackslash + 1);
    DEBUG_LOG("DLL path = \"", DllPath, "\"");
}

static void StartPipeThread()
{
    if (Configs.enabled)
    {
        DEBUG_LOG("Creating PipeThread.");
        PipeThread.run = true;
        PipeThread.thread = std::thread(PipeThreadFunc);
    }
}

static void StopPipeThread()
{
    if (Configs.enabled)
    {
        DEBUG_LOG("Starting to close PipeThread...");

        // Begin to close PipeThread.
        PipeThread.run = false;

        // No connection exists, create a new connection to exit blocking.
        if (PipeThread.status == ThreadStatus::WaitingForConnection)
        {
            // CancelSynchronousIo(PipeThread.handle);
            DEBUG_LOG("PipeThread is waiting for a connection, attempting to connect...");
            HANDLE pipe = CreateFile(PipeThread.pipeName.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                     OPEN_EXISTING, 0, NULL);
            CloseHandle(pipe);
        }

        // Add empty message in case of blocked waiting.
        if (PipeThread.status == ThreadStatus::WaitingForMessage)
        {
            DEBUG_LOG("PipeThread is waiting for message, attempting to send empty message...");
            std::unique_lock<std::mutex> lock(MsgCont.mutex);
            MsgCont.queue.emplace("");
            MsgCont.cv.notify_one();
        }

        DEBUG_LOG("Waiting for PipeThread to join...");
        PipeThread.thread.join();
        DEBUG_LOG("PipeThread Closed!");
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        {
            GetDllPath(hModule);

            if (std::filesystem::exists(DllPath + std::string{ConfigFile}))
                LoadConfigFile();
            else
                CreateConfigFile();

            StartPipeThread();
        }
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
        case DLL_PROCESS_DETACH:
        {
            StopPipeThread();
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

static arcdps_exports arc_exports;
/* initialize mod -- return table that arcdps will use for callbacks */
static arcdps_exports* mod_init()
{
    /* for arcdps */
    memset(&arc_exports, 0, sizeof(arcdps_exports));
    arc_exports.sig = 0x1EB0697;
    arc_exports.imguivers = 18000; // IMGUI_VERSION_NUM;
    arc_exports.size = sizeof(arcdps_exports);
    arc_exports.out_name = "ArcDPS Bridge";
    arc_exports.out_build = BridgeInfo.version.data();
    arc_exports.wnd_nofilter = mod_wnd;
    arc_exports.combat = mod_combat;

    if (Configs.enabled && Configs.arcDPS)
    {
        BridgeInfo.arcLoaded = true;
        DEBUG_LOG("ArcDPS is enabled.");
    }
    else
    {
        // This will create a warning in the arcdps log.
        // Will maybe change this later, due to having a silent warning instead.
        // Since this is not an error, only a way to turn of the extension and
        // also have it loaded at the same time.
        arc_exports.sig = 0;
        arc_exports.size = (uintptr_t)"ArcDPS is disabled by configs!";
        DEBUG_LOG("ArcDPS is disabled by configs!");
    }

    return &arc_exports;
}

/* release mod -- return ignored */
static uintptr_t mod_release()
{
    DEBUG_LOG("Releasing ArcDPS Bridge");
    BridgeInfo.arcLoaded = false;

    return 0;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client load */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversionstr, void* imguicontext,
                                                     void* dxptr, HMODULE new_arcdll,
                                                     void* mallocfn, void* freefn, UINT dxver)
{
    BridgeInfo.arcvers = std::string{arcversionstr};
    return mod_init;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client exit */
extern "C" __declspec(dllexport) void* get_release_addr()
{
    BridgeInfo.arcvers = "";
    return mod_release;
}

static std::string ExtraDataToJSON(const UserInfo* user)
{
    std::ostringstream ss{};
    ss << "{\"AccountName\":\"" << std::string{user->AccountName} << "\","
       << "\"Role\":" << static_cast<uint64_t>(static_cast<uint8_t>(user->Role)) << ","
       << "\"Subgroup\":" << static_cast<uint64_t>(user->Subgroup) << "}";
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
            std::string data{ExtraDataToJSON(uinfo)};

            if (uinfo->Role == UserRole::None)
                PlayerCollection.remove(std::string{uinfo->AccountName});
            else
            {
                PlayerContainer::PlayerInfo *player{PlayerCollection.add(std::string{uinfo->AccountName})};
                if (player)
                {
                    player->accountName = uinfo->AccountName;
                    player->role = uinfo->Role;
                    player->subgroup = uinfo->Subgroup;
                    player->valid = true;
                }
            }

            if (UseEvents.extra)
            {
                std::ostringstream ss{};
                ss << "{\"type\":\"Extra\","
                   << "\"extra\":" << data << "}";
                SendToClient(ss.str(), MessageType::Extra);
            }

        }
    }
}

// Exported init function for arcDPS unofficial extras API.
extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(
    const ExtrasAddonInfo* pExtrasInfo, void* pSubscriberInfo)
{
    if (!Configs.enabled && !Configs.extras)
    {
        DEBUG_LOG("Unofficial Extras is disabled.");
        return;
    }

    if (pExtrasInfo->ApiVersion != 2)
    {
        DEBUG_LOG("Extra api version error, expected 2 and got ", pExtrasInfo->ApiVersion);
        return;
    }

    if (pExtrasInfo->MaxInfoVersion == 1)
    {
        DEBUG_LOG("Enabled arcdps extra hook");
        BridgeInfo.extraLoaded = true;
        BridgeInfo.extraVersion = std::string{pExtrasInfo->StringVersion};

        ExtrasSubscriberInfoV1 extrasInfo{};
        extrasInfo.InfoVersion = 1;
        extrasInfo.SubscriberName = "ArcDPS Bridge";
        extrasInfo.SquadUpdateCallback = squad_update_callback;
        *static_cast<ExtrasSubscriberInfoV1*>(pSubscriberInfo) = extrasInfo;

        PlayerCollection.self = pExtrasInfo->SelfAccountName;
        DEBUG_LOG("Self account name: \"", PlayerCollection.self, "\"");
        return;
    }

    DEBUG_LOG("Extra max info version \"", pExtrasInfo->MaxInfoVersion, "\" is not supported.");
}
