//
//  src/PipeThread.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PIPETHREAD_HPP
#define BRIDGE_PIPETHREAD_HPP

// Local Headers
#include "ApplicationData.hpp"
#include "Message.hpp"
#include "SquadModifyHandler.hpp"

// C++ Headers
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Windows Headers
#include <windows.h>

class MessageTracking;

class PipeThread
{
public:
    enum class Status : uint8_t
    {
        NONE = 0,                 // Not created.
        WaitingForConnection = 4, // Pipe is created and waiting for connection.
        Reading = 8,              // Pipe is reading / waiting for a message from the client.
        WaitingForMessage = 16,   // Pipe connection exists and is waiting for a message to be queued.
        Sending = 32              // Pipe connection exists and is sending.
    };

    struct MessageContainer
    {
        std::mutex mutex{};
        std::condition_variable cv{};
        std::queue<Message> queue{};
    };

    struct EventTracking
    {
        bool combat{false};
        bool extras{false};
        bool squad{false};
    };

public:
    PipeThread() = delete;
    PipeThread(std::size_t id, void* handle, MessageTracking* mt, const ApplicationData& appdata,
               const SquadModifyHandler* squadModifyHandler);
    ~PipeThread();

    void start();
    void stop();

    [[nodiscard]] bool started() const { return m_threadStarted; }
    [[nodiscard]] bool running() const { return m_running; }

    [[nodiscard]] std::size_t id() const { return m_id; }

    void sendBridgeInfo(const Message& msg, uint64_t validator);
    void sendMessage(const Message& msg);

    [[nodiscard]] EventTracking eventTracking() const { return m_eventTrack; }

private:
    MessageContainer m_msgCont{};
    const ApplicationData& m_appData;
    std::thread m_thread{};
    std::mutex m_mutex{};
    const SquadModifyHandler* m_squadModifyHandler;
    EventTracking m_eventTrack{};
    void* m_handle{nullptr};
    MessageTracking* m_mt{nullptr};
    Status m_status{Status::NONE};
    std::atomic<bool> m_run{false};
    std::atomic<bool> m_running{false};
    std::size_t m_id;
    uint64_t m_bridgeValidator{0}; // 0 = not sent to client yet.
    bool m_threadStarted{false};
};

// Read Helper.
struct ReadStatus
{
    DWORD numBytesRead{0};
    DWORD error{0};
    BOOL success{false};
    std::string data{};
};
ReadStatus ReadFromPipe(HANDLE handle);

// Write Helper.
struct SendStatus
{
    DWORD numBytesWritten{0};
    DWORD error{0};
    BOOL success{false};
};
SendStatus WriteToPipe(HANDLE handle, const std::string& msg);
SendStatus WriteToPipe(HANDLE handle, const uint8_t* data, std::size_t count);

#endif // BRIDGE_PIPETHREAD_HPP