//
//  src/PipeHandler.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_PIPEHANDLER_HPP
#define BRIDGE_PIPEHANDLER_HPP

// Local Headers
#include "ApplicationData.hpp"
#include "Message.hpp"
#include "PipeThread.hpp"
#include "SquadModifyHandler.hpp"

// C++ Headers
#include <memory>
#include <vector>

class MessageTracking
{
public:
    // Tracked event categories.
    void trackCategory(MessageCategory category);
    void untrackCategory(MessageCategory category);
    [[nodiscard]] bool isTrackingCategory(MessageCategory category) const;

    // Using protocols.
    void useProtocol(MessageProtocol protocol);
    void unuseProtocol(MessageProtocol protocol);
    [[nodiscard]] bool usingProtocol(MessageProtocol protocol) const;

private:
    // Categories.
    std::atomic<std::size_t> m_combat{0};
    std::atomic<std::size_t> m_extras{0};
    std::atomic<std::size_t> m_squad{0};

    // Protocols.
    std::atomic<std::size_t> m_serial{0};
    std::atomic<std::size_t> m_json{0};
};

class PipeHandler
{
public:
    PipeHandler() = delete;
    PipeHandler(const std::string& pipeName, const ApplicationData& appdata,
                const SquadModifyHandler* squadModifyHandler);
    ~PipeHandler();

public:
    void start();
    void stop();

    [[nodiscard]] bool started() const { return m_threadStarted; }
    [[nodiscard]] bool waitingForConnection() const { return m_waitingForConnection; }

    void sendBridgeInfo(const Message& msg, uint64_t validator);
    void sendMessage(const Message& msg);

    [[nodiscard]] bool trackingCategory(MessageCategory category) const;
    [[nodiscard]] bool usingProtocol(MessageProtocol protocol) const;

private:
    PipeThread* dispatchPipeThread(void* handle, std::size_t id);
    void cleanup();

private:
    std::vector<std::unique_ptr<PipeThread>> m_threads{};
    const ApplicationData& m_appData;
    std::string m_pipeName{};
    std::mutex m_mutex{};
    std::thread m_pipeMain;
    const SquadModifyHandler* m_squadModifyHandler;
    MessageTracking m_msgTracking{};
    std::atomic<bool> m_run{false};
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_waitingForConnection{false};
    bool m_threadStarted{false};
};

#endif // BRIDGE_PIPEHANDLER_HPP