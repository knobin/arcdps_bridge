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

// C++ Headers
#include <atomic>
#include <mutex>
#include <queue>
#include <string>
#include <thread>

// Windows Headers
#include <windows.h>

class TrackedEvents;

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
        std::queue<std::string> queue{};
    };

    struct EventTracking
    {
        bool combat{false};
        bool extra{false};
        bool squad{false};
    };

public:
    PipeThread() = delete;
    PipeThread(void* handle, TrackedEvents* te, const ApplicationData& appdata);
    ~PipeThread();

    void start();
    void stop();
    bool running() const
    {
        return m_run;
    }

    void sendMessage(const std::string& msg, MessageType type);
    EventTracking eventTracking() const
    {
        return m_eventTrack;
    }

private:
    MessageContainer m_msgCont{};
    const ApplicationData& m_appData;
    std::thread m_thread{};
    std::mutex m_mutex{};
    EventTracking m_eventTrack{};
    void* m_handle{nullptr};
    TrackedEvents* m_te{nullptr};
    Status m_status{Status::NONE};
    std::atomic<bool> m_run{true};
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

#endif // BRIDGE_PIPETHREAD_HPP