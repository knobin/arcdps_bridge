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

class MessageTracking : public PipeThread::EventTracking
{
public:
    // Using protocols.
    void incProtocol(MessageProtocol protocol);
    void decProtocol(MessageProtocol protocol);
    [[nodiscard]] bool usingProtocol(MessageProtocol protocol) const;

private:
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

    void sendBridgeInfo(const std::shared_ptr<Message>& msg, uint64_t validator);
    void sendMessage(const std::shared_ptr<Message>& msg);

    template <typename... Args>
    void sendMessages(Args&&... args)
    {
        std::unique_lock<std::mutex> lock(m_mutex);

        if (m_running)
        {
            (forwardMessageToThreads(args), ...);
        }
    }

    [[nodiscard]] bool isTrackingType(MessageType type) const;
    [[nodiscard]] bool isUsingProtocol(MessageProtocol protocol) const;
    [[nodiscard]] std::underlying_type_t<MessageProtocol> usingProtocols() const;

private:
    PipeThread* dispatchPipeThread(void* handle, std::size_t id);
    void cleanup();

    void forwardMessageToThreads(const std::shared_ptr<Message>& msg)
    {
        if (msg == nullptr)
            return;

        if (!msg->valid())
            return;

        for (std::unique_ptr<PipeThread>& pt : m_threads)
            if (pt->started() && pt->protocol() == msg->protocol())
                pt->sendMessage(msg);
    }

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

std::shared_ptr<Message> ConnectionStatusMessage(uint64_t id, const nlohmann::json& info, bool success,
                                                 const std::string& error = "");

#endif // BRIDGE_PIPEHANDLER_HPP