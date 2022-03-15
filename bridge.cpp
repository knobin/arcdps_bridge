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

// Windows Headers
#include <windows.h>

#ifdef DEBUG

template<typename... Args>
static void Print(Args&&... args)
{
    static std::mutex LogMutex;

    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y-%m-%d %X", &tstruct);

    std::unique_lock<std::mutex> lock(LogMutex);
    std::ofstream
        outfile; // Opens file for every call to print. Bad. but fine for debugging purposes.
    outfile.open("arcdps_bridge.log", std::ios_base::app);
    outfile << "[" << buf << "] ";
    (outfile << ... << args) << '\n';
    outfile.close();
}

#define DEBUG_LOG(...) Print(__VA_ARGS__)

#else

#define DEBUG_LOG(...)

#endif

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
    NONE = 0,
    WaitingForConnection = 4, // Pipe is created and waiting for connection.
    WaitinForMessage = 8,     // Pipe connection exists.
    Sending = 16              // Pipe connection exists.
};

// Variables for thread that is sending data.
static HANDLE PipeHandle;
static constexpr std::string_view PipeName{"\\\\.\\pipe\\arcdps-bridge"};
static ThreadStatus PipeThreadStatus{ThreadStatus::NONE};
static std::atomic<bool> Run{true};
static std::thread PipeThread{};
static std::mutex MsgMutex{};
static std::condition_variable MsgCV{};
static std::queue<std::string> MsgQueue{};

// Information about the bridge.
static bool ExtraEnabled{false};
static std::string ExtraVersion{""};
static std::string Version{"1.0"};
static bool ArcLoaded{false};
static std::string arcvers{""};

enum class MessageType : uint8_t
{
    NONE = 0,
    Info = 1,
    Combat = 2,
    Extra = 4
};

static void SendToClient(const std::string& msg, MessageType type)
{
    // Send if pipe has been created and a connection exists.
    bool send{(PipeThreadStatus != ThreadStatus::NONE &&
               PipeThreadStatus != ThreadStatus::WaitingForConnection)};

    // If either not created or no connection exists.
    // Only put "Extra" messages on the queue.
    if (!send && (type == MessageType::Extra))
        send = true;

    if (send)
    {
        std::unique_lock<std::mutex> lock(MsgMutex);
        if (MsgQueue.size() < 200)
        {
            DEBUG_LOG("Message of type \"", static_cast<int>(type), "\" queued for sending.");
            MsgQueue.push(msg);
            MsgCV.notify_one();
        }
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
            break;
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

/* combat callback -- may be called asynchronously, use id param to keep track of order, first event id will be 2. return ignored */
/* at least one participant will be party/squad or minion of, or a buff applied by squad in the case of buff remove. not all statechanges present, see evtc statechange enum */
static uintptr_t mod_combat(cbtevent* ev, ag* src, ag* dst, char* skillname, uint64_t id,
                            uint64_t revision)
{
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

/*
static std::string ConnectInfo() {
  std::ostringstream ss{};
  ss << "{\"type\":\"Info\"" <<
  ",\"Version\":\"" << Version << "\"" <<
  ",\"ArcLoaded\":\"" << ((ArcLoaded) ? "true" : "false") <<
  ((ArcLoaded) ? ",\"ArcVersion\":\"" + arcvers + "\"" : "") <<
  ",\"ExtraEnabled\":" << ((ExtraEnabled) ? "true" : "false") <<
  ((ExtraEnabled) ? ",\"ExtraVersion\":\"" + ExtraVersion + "\"" : "") << "}";
}
*/

static void PipeThreadFunc()
{
    DEBUG_LOG("Started Pipe server!");

    bool ShouldClose{false};
    while (!ShouldClose)
    {
        PipeThreadStatus = ThreadStatus::NONE;
        CloseHandle(PipeHandle);

        DEBUG_LOG("Creating Named pipe \"", std::string{PipeName}, "\"");
        PipeHandle = CreateNamedPipe(PipeName.data(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE, 1, 0,
                                     0, 0, NULL);

        if (PipeHandle == NULL || PipeHandle == INVALID_HANDLE_VALUE)
        {
            DEBUG_LOG("Error creating pipe with err: ", GetLastError(), "!");
            continue;
        }

        DEBUG_LOG("Created Named pipe \"", std::string{PipeName}, "\"");

        DEBUG_LOG("Waiting for client!");
        PipeThreadStatus = ThreadStatus::WaitingForConnection;
        BOOL result = ConnectNamedPipe(PipeHandle, NULL); // Blocking

        if (!result)
        {
            DEBUG_LOG("Error connecting pipe with err:", GetLastError(), "!");
            continue;
        }

        DEBUG_LOG("Client connected!");

        while (Run)
        {
            DEBUG_LOG("Retrieving message to send.");
            std::string msg{""};

            {
                std::unique_lock<std::mutex> lock(MsgMutex);
                PipeThreadStatus = ThreadStatus::WaitinForMessage;

                // Block thread until message is added to queue.
                while (MsgQueue.empty())
                    MsgCV.wait(lock);

                msg = MsgQueue.front();
                MsgQueue.pop();

                // Do not send empty message.
                if (msg.empty())
                {
                    DEBUG_LOG("Empty message found");
                    continue;
                }
            }

            // Send retrieved message.
            PipeThreadStatus = ThreadStatus::Sending;
            DEBUG_LOG("Sending \"", msg, "\" to client!");
            const DWORD length{static_cast<DWORD>(msg.size())};
            DWORD numBytesWritten = 0;
            result = WriteFile(PipeHandle, msg.c_str(), length, &numBytesWritten, NULL);

            if (!result)
            {
                DWORD err = GetLastError();
                DEBUG_LOG("Error sending data with err: ", err, "!");
                if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                {
                    DEBUG_LOG("Client unexpectedly disconnected!");
                    break;
                }
            }

            DEBUG_LOG("Data sent to client!");
        }

        if (!Run)
        {
            DEBUG_LOG("Pipe server is closing!");
            ShouldClose = true;
        }
    }

    PipeThreadStatus = ThreadStatus::NONE;
    CloseHandle(PipeHandle);
    DEBUG_LOG("Ended pipe server!");
}

static arcdps_exports arc_exports;
/* initialize mod -- return table that arcdps will use for callbacks */
static arcdps_exports* mod_init()
{
    DEBUG_LOG("Initializing ArcDPS Bridge");
    ArcLoaded = true;

    /* for arcdps */
    memset(&arc_exports, 0, sizeof(arcdps_exports));
    arc_exports.sig = 0x1EB0697;
    arc_exports.imguivers = 18000; // IMGUI_VERSION_NUM;
    arc_exports.size = sizeof(arcdps_exports);
    arc_exports.out_name = "ArcDPS Bridge";
    arc_exports.out_build = "1.0";
    arc_exports.wnd_nofilter = mod_wnd;
    arc_exports.combat = mod_combat;
    // arc_exports.size = (uintptr_t)"error message if you decide to not load, sig must be 0";

    DEBUG_LOG("Creating PipeThread.");
    Run = true;
    PipeThread = std::thread(PipeThreadFunc);

    return &arc_exports;
}

/* release mod -- return ignored */
static uintptr_t mod_release()
{
    DEBUG_LOG("Releasing ArcDPS Bridge");
    DEBUG_LOG("Starting to close PipeThread...");
    ArcLoaded = false;

    // Begin to close PipeThread.
    Run = false;

    // Add empty message in case of blocked waiting.
    if (PipeThreadStatus == ThreadStatus::WaitinForMessage)
    {
        DEBUG_LOG("PipeThread is waiting for message, attempting to send empty message...");
        std::unique_lock<std::mutex> lock(MsgMutex);
        MsgQueue.emplace("");
        MsgCV.notify_one();
    }

    // No connection exists, create a new connection to exit blocking.
    if (PipeThreadStatus == ThreadStatus::WaitingForConnection)
    {
        // CancelSynchronousIo(PipeHandle);
        DEBUG_LOG("PipeThread is waiting for a connection, attempting to connect...");
        HANDLE pipe = CreateFile(PipeName.data(), GENERIC_READ | GENERIC_WRITE, 0, NULL,
                                 OPEN_EXISTING, 0, NULL);
        CloseHandle(pipe);
    }

    DEBUG_LOG("Waiting for PipeThread to join...");
    PipeThread.join();
    DEBUG_LOG("PipeThread Closed!");
    return 0;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client load */
extern "C" __declspec(dllexport) void* get_init_addr(char* arcversionstr, void* imguicontext,
                                                     void* dxptr, HMODULE new_arcdll,
                                                     void* mallocfn, void* freefn, UINT dxver)
{
    arcvers = std::string{arcversionstr};
    return mod_init;
}

/* export -- arcdps looks for this exported function and calls the address it returns on client exit */
extern "C" __declspec(dllexport) void* get_release_addr()
{
    arcvers = "";
    return mod_release;
}

static std::string ExtraDataToJSON(const UserInfo* user)
{
    std::ostringstream ss{};
    ss << "{\"type\":\"Extra\","
       << "\"extra\":{\"AccountName\":\"" << std::string{user->AccountName} << "\","
       << "\"Role\":" << static_cast<uint64_t>(static_cast<uint8_t>(user->Role)) << ","
       << "\"Subgroup\":" << static_cast<uint64_t>(user->Subgroup) << "}}\0";
    return ss.str();
}

// Callback for arcDPS unofficial extras API.
void squad_update_callback(const UserInfo* updatedUsers, uint64_t updatedUsersCount)
{
    for (uint64_t i{0}; i < updatedUsersCount; ++i)
    {
        const UserInfo* info{&updatedUsers[i]};
        if (info)
        {
            SendToClient(ExtraDataToJSON(info), MessageType::Extra);
        }
    }
}

// Exported init function for arcDPS unofficial extras API.
extern "C" __declspec(dllexport) void arcdps_unofficial_extras_subscriber_init(
    const ExtrasAddonInfo* pExtrasInfo, ExtrasSubscriberInfo* pSubscriberInfo)
{
    if (pExtrasInfo->ApiVersion != 1)
    {
        DEBUG_LOG("Extra api version error, expected 1 and got ", pExtrasInfo->ApiVersion);
        return;
    }

    DEBUG_LOG("Enabled arcdps extra hook");
    ExtraEnabled = true;
    ExtraVersion = std::string{pExtrasInfo->StringVersion};

    pSubscriberInfo->SubscriberName = "ArcDPS Bridge";
    pSubscriberInfo->SquadUpdateCallback = squad_update_callback;
}