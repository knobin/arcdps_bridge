//
//  src/PipeHandler.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PIPEHANDLER_HPP
#define BRIDGE_PIPEHANDLER_HPP

// Local Headers
#include "PipeThread.hpp"

// C++ Headers
#include <vector>
#include <memory>

class TrackedEvents
{
public:
    void startTracking(MessageType mt);
    void untrack(MessageType mt);
    bool isTracking(MessageType mt) const;
private:
    std::atomic<uint32_t> m_combat{false};
    std::atomic<uint32_t> m_extra{false};
    std::atomic<uint32_t> m_squad{false};
};

class PipeHandler
{
public:
    PipeHandler() = delete;
    PipeHandler(const std::string pipeName, const ApplicationData& appdata);
    ~PipeHandler();
public:

    void start();
    void stop();
    bool running() const { return m_run; }
    bool waitingForConnection() const { return m_waitingForConnection; }

    void sendMessage(const std::string& msg, MessageType type);

    bool trackingEvent(MessageType mt) const;

private:
    PipeThread* dispatchPipeThread(void* handle);
    void cleanup();
    
private:
    std::vector<std::unique_ptr<PipeThread>> m_threads{};
    const ApplicationData& m_appData;
    std::string m_pipeName{};
    std::mutex m_mutex{};
    std::thread m_pipeMain{};
    TrackedEvents m_trackedEvents{};
    std::atomic<bool> m_run{false};
    std::atomic<bool> m_waitingForConnection{false};
};

#endif // BRIDGE_PIPEHANDLER_HPP