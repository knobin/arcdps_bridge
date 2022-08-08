//
//  src/PipeThread.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "PipeThread.hpp"
#include "Log.hpp"
#include "PipeHandler.hpp"

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// C++ Headers
#include <cstdint>
#include <sstream>

static Message StatusMessage(bool success, std::string error = "")
{
    nlohmann::json j{{"success", success}};
    if (!success)
        j["error"] = error;

    return InfoMessage<MessageType::Status>({}, j.dump());
}

static std::underlying_type_t<MessageProtocol> IsProtocolStr(const std::string& str)
{
    using MPU = std::underlying_type_t<MessageProtocol>;

    if (MessageProtocolToStr(MessageProtocol::Serial) == str)
        return static_cast<MPU>(MessageProtocol::Serial);

    if (MessageProtocolToStr(MessageProtocol::JSON) == str)
        return static_cast<MPU>(MessageProtocol::JSON);

    return 0;
}

PipeThread::PipeThread(std::size_t id, void* handle, MessageTracking* mt, const ApplicationData& appdata)
    : m_handle{handle}, m_mt{mt}, m_appData{appdata}, m_id{id}
{
    BRIDGE_DEBUG("Created PipeThread [ptid {}]", m_id);
}

PipeThread::~PipeThread()
{
    BRIDGE_DEBUG("~PipeThread [ptid {}], running: {}", m_id, m_running);
}

void PipeThread::start()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Don't start the thread again if already started, start() needs to be follow by stop().
    if (m_threadStarted)
        return;

    // Enable thread function to run.
    m_threadStarted = true;
    m_run = true;

    m_thread = std::thread([handler = this](){
        if (!handler->m_run)
        {
            BRIDGE_ERROR("[ptid {}] Could not start PipeThread, m_run = {}", handler->m_id, handler->m_run);
            return;
        }

        std::size_t threadID = handler->m_id;
        void* handle = handler->m_handle;
        handler->m_running = true;
        BRIDGE_INFO("[ptid {}] Started PipeThread.", threadID);

        BRIDGE_DEBUG("[ptid {}] Client connected, sending bridge information...", threadID);
        Message msg{};
        {
            std::unique_lock<std::mutex> infoLock(handler->m_appData.Info.mutex);
            msg = InfoMessage<MessageType::BridgeInfo>({}, handler->m_appData.Info);
            {
                std::unique_lock<std::mutex> handlerLock(handler->m_mutex);
                handler->m_bridgeValidator = handler->m_appData.Info.validator;
            }
        }
        handler->m_status = Status::Sending;
        BRIDGE_MSG_DEBUG("[ptid {}] Sending \"{}\" to client.", threadID, msg.toJSON());
        SendStatus sendStatus = WriteToPipe(handle, msg.toJSON());
        if (!sendStatus.success)
        {
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            BRIDGE_ERROR("[ptid {}] Failed to send bridge information, Ending PipeThread.", threadID);
            return;
        }

        BRIDGE_DEBUG("[ptid {}] Waiting for client to subscribe...", threadID);
        handler->m_status = Status::Reading;
        ReadStatus readStatus = ReadFromPipe(handle);
        if (!readStatus.success)
        {
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            BRIDGE_ERROR("[ptid {}] Failed to read bridge information, Ending PipeThread.", threadID);
            return;
        }
        BRIDGE_MSG_DEBUG("[ptid {}] Retrieved \"{}\" from client!", threadID, readStatus.data);

        // Expecting: {"subscribe":15}
        // The number is the MessageType's you want to recieved. 15 in this case is all of them.
        // This was to extract the number should probably change in the future.
        nlohmann::json j = nlohmann::json::parse(readStatus.data);

        // Subscribe.
        using MessageSourceU = std::underlying_type_t<MessageSource>;
        MessageSourceU filter = 0;

        if (j.contains("subscribe"))
        {
            filter = j["subscribe"].get<MessageSourceU>();
            BRIDGE_DEBUG("[ptid {}] Recieved filter \"{}\" from client.", threadID, static_cast<int>(filter));    
        }

        MessageSourceU combatValue = static_cast<MessageSourceU>(MessageSource::Combat);
        if ((filter & combatValue) == combatValue)
        {
            handler->m_eventTrack.combat = true;
            handler->m_mt->trackEvent(MessageSource::Combat);
        }
        MessageSourceU extrasValue = static_cast<MessageSourceU>(MessageSource::Extras);
        if ((filter & extrasValue) == extrasValue)
        {
            handler->m_eventTrack.extras = true;
            handler->m_mt->trackEvent(MessageSource::Extras);
        }
        MessageSourceU squadValue = static_cast<MessageSourceU>(MessageSource::Squad);
        if ((filter & squadValue) == squadValue)
        {
            handler->m_eventTrack.squad = true;
            handler->m_mt->trackEvent(MessageSource::Squad);
        }

        BRIDGE_INFO("[ptid {}] Client has subscribed to \"Combat\": {}", threadID, handler->m_eventTrack.combat);
        BRIDGE_INFO("[ptid {}] Client has subscribed to \"Extras\": {}", threadID, handler->m_eventTrack.extras);
        BRIDGE_INFO("[ptid {}] Client has subscribed to \"Squad\": {}", threadID, handler->m_eventTrack.squad);

        // Subscription error (if any).
        if (!(handler->m_eventTrack.combat || handler->m_eventTrack.extras || handler->m_eventTrack.squad))
        {
            const Message statusMsg{StatusMessage(false, "no subscription")};
            WriteToPipe(handle, statusMsg.toJSON());
            BRIDGE_ERROR("[ptid {}] No subscription, Ending PipeThread.", threadID);
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            return;
        }

        // Protocol.
        using MessageProtocolU = std::underlying_type_t<MessageProtocol>;
        MessageProtocolU protocolNum = 0;
        
        if (j.contains("protocol"))
        {
            std::string protocol = j["protocol"].get<std::string>();
            BRIDGE_DEBUG("[ptid {}] Recieved protocol \"{}\" from client.", threadID, protocol);
            protocolNum = IsProtocolStr(protocol);
        }

        // Protocol error (if any).
        if (protocolNum == 0)
        {
            const Message statusMsg{StatusMessage(false, "no such protocol")};
            WriteToPipe(handle, statusMsg.toJSON());
            BRIDGE_ERROR("[ptid {}] No such protocol, Ending PipeThread.", threadID);
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            return;
        }

        const MessageProtocol protocol{static_cast<MessageProtocol>(protocolNum)};
        handler->m_mt->useProtocol(protocol);
        BRIDGE_INFO("[ptid {}] Client is using protocol \"{}\"", threadID, MessageProtocolToStr(protocol));

        // Success!
        {
            const Message statusMsg{StatusMessage(true)};
            SendStatus send = WriteToPipe(handle, statusMsg.toJSON());
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid {}] Error sending data with err: {}!", threadID, send.error);   
            }
        }

        if (handler->m_eventTrack.squad)
        {
            handler->m_status = Status::Sending;

            SerialData serial{};
            nlohmann::json json{};
            
            if (protocol == MessageProtocol::Serial)
            {
                ; // TODO.
            }

            if (protocol == MessageProtocol::JSON)
            {
                json = {{"self", handler->m_appData.Self.accountName},
                         handler->m_appData.Squad.toJSON()};
            }

            msg = SquadMessage<MessageType::SquadStatus>(serial, json);
            BRIDGE_MSG_DEBUG("[ptid {}] Sending Squad information to client: {}", threadID, ss.str());
            SendStatus send = WriteToPipe(handle, msg.toJSON());
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid {}] Error sending data with err: {}!", threadID, send.error);   
            }
        }

        std::size_t msTimeout = handler->m_appData.Config.clientTimeoutTimer;
        BRIDGE_DEBUG("[ptid {}] Using client timeout time of {}ms", threadID, msTimeout);

        bool disconnected = false;
        while (handler->m_run)
        {
            BRIDGE_MSG_DEBUG("Retrieving message to send.");
            msg = Message{};

            {
                std::unique_lock<std::mutex> lock(handler->m_msgCont.mutex);
                handler->m_status = Status::WaitingForMessage;

                // Block thread until message is added to queue (1s max).
                while (handler->m_msgCont.queue.empty())
                {
                    if (handler->m_msgCont.cv.wait_for(lock, std::chrono::milliseconds(msTimeout)) == std::cv_status::timeout)
                    {
                        BRIDGE_DEBUG("[ptid {}] Checking pipe status...", threadID);
                        DWORD availBytes{};
                        if (!PeekNamedPipe(handle, 0, 0, 0, &availBytes, 0))
                        {
                            DWORD err = GetLastError();
                            if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                            {
                                BRIDGE_ERROR("[ptid {}] Client unexpectedly disconnected!", threadID);
                                disconnected = true;
                                break;
                            }
                        }
                        BRIDGE_DEBUG("[ptid {}] Client is connected.", threadID);
                    }
                }

                // Client has disconnected.
                if (disconnected)
                    break;

                // Retrieve message.
                msg = handler->m_msgCont.queue.front();
                handler->m_msgCont.queue.pop();

                // Do not send empty message.
                if (msg.empty())
                {
                    BRIDGE_WARN("[ptid {}] Empty message found", threadID);
                    continue;
                }
            }

            // Send retrieved message.
            BRIDGE_MSG_DEBUG("[ptid {}] Sending \"{}\" to client.", threadID, msg.toJSON());
            handler->m_status = Status::Sending;
            sendStatus = WriteToPipe(handle, msg.toJSON());

            if (!sendStatus.success)
            {
                if (sendStatus.error == ERROR_BROKEN_PIPE || sendStatus.error == ERROR_NO_DATA)
                {
                    BRIDGE_ERROR("[ptid {}]  Client unexpectedly disconnected!", threadID);
                    disconnected = true;
                    break;
                }
            }

            BRIDGE_MSG_DEBUG("[ptid {}] Data sent to client!", threadID);
        }

        BRIDGE_DEBUG("[ptid {}] PipeThread is closing!", threadID);

        if (!disconnected && handle)
        {
            // If client is still connected and the thread is closing, send closing event.
            BRIDGE_DEBUG("[ptid {}] Sending closing event to client.", threadID);
            const auto disconnectedObj = "{\"type\":\"closing\"}";
            WriteToPipe(handle, disconnectedObj);
        
        }

        // Untrack protocol.
        handler->m_mt->unuseProtocol(protocol);

        // Untrack events.
        if (handler->m_eventTrack.combat)
            handler->m_mt->untrackEvent(MessageSource::Combat);
        if (handler->m_eventTrack.extras)
            handler->m_mt->untrackEvent(MessageSource::Extras);
        if (handler->m_eventTrack.squad)
            handler->m_mt->untrackEvent(MessageSource::Squad);

        handler->m_status = Status::NONE;
        CloseHandle(handle);
        handler->m_handle = nullptr;
        handler->m_running = false;

        BRIDGE_INFO("[ptid {}] Closed client connection.", threadID);
        BRIDGE_INFO("[ptid {}] Ended PipeThread.", threadID);
    });
}

void PipeThread::stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_DEBUG("Closing PipeThread [ptid {}]...", m_id);

    if (m_running)
    {
        m_run = false;

        {
            std::lock_guard<std::mutex> msgLock(m_msgCont.mutex);

            // Add empty message in case of blocked waiting.
            if (m_status == Status::WaitingForMessage)
            {
                BRIDGE_DEBUG("PipeThread [ptid {}] is waiting for message, attempting to send empty message...", m_id);    
                m_msgCont.queue.push(Message{});
                m_msgCont.cv.notify_one();
            }
        }
    }

    BRIDGE_DEBUG("Waiting for PipeThread [ptid {}] to join...", m_id);
    m_thread.join();
    BRIDGE_DEBUG("PipeThread [ptid {}] joined.", m_id);

    // Allow thread to be started again.
    m_threadStarted = false;

    BRIDGE_DEBUG("PipeThread [ptid {}] Closed!", m_id);
}


void PipeThread::sendBridgeInfo(const Message& msg, uint64_t validator)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (validator > m_bridgeValidator)
    {
        m_bridgeValidator = validator;

        std::unique_lock<std::mutex> msgLock(m_msgCont.mutex);
        if (m_msgCont.queue.size() < m_appData.Config.msgQueueSize)
        {
            BRIDGE_DEBUG("Sending updated BridgeInfo to client [ptid {}].", m_id);
            m_msgCont.queue.push(msg);
            m_msgCont.cv.notify_one();
        }
    }
}

void PipeThread::sendMessage(const Message& msg)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    bool send = false;

    switch (msg.source())
    {
        case MessageSource::Combat:
            send = m_eventTrack.combat;
            break;
        case MessageSource::Extras:
            send = m_eventTrack.extras;
            break;
        case MessageSource::Squad:
            send = m_eventTrack.squad;
            break;
        default:
            break;
    }

    if (send)
    {
        std::unique_lock<std::mutex> msgLock(m_msgCont.mutex);
        if (m_msgCont.queue.size() < m_appData.Config.msgQueueSize)
        {
            m_msgCont.queue.push(msg);
            m_msgCont.cv.notify_one();
        }
    }
}


SendStatus WriteToPipe(HANDLE handle, const std::string& msg)
{
    const DWORD length{static_cast<DWORD>(msg.size())};
    SendStatus status{};
    status.success = WriteFile(handle, msg.c_str(), length, &status.numBytesWritten, NULL);

    if (!status.success)
        status.error = GetLastError();

    return status;
}

constexpr std::size_t BUFSIZE{512};

ReadStatus ReadFromPipe(HANDLE handle)
{
    ReadStatus status{};
    TCHAR buffer[BUFSIZE]{};

    do
    {
        status.success = ReadFile(handle, buffer, BUFSIZE * sizeof(TCHAR), &status.numBytesRead, NULL);
        status.error = GetLastError();
        if (!status.success && status.error != ERROR_MORE_DATA)
            break;

        if (status.numBytesRead < BUFSIZE)
            buffer[status.numBytesRead] = '\0';

        status.data += std::string{buffer};
    } while (!status.success);

    if (!status.success)
        status.error = GetLastError();
    
    return status;
}
