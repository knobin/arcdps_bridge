//
//  src/PipeThread.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#define NOMINMAX

// Local Headers
#include "PipeThread.hpp"
#include "Log.hpp"
#include "PipeHandler.hpp"

// nlohmann_json Headers
#include <nlohmann/json.hpp>

// C++ Headers
#include <cstdint>
#include <limits>
#include <sstream>

static Message StatusMessage(uint64_t id, bool success, const std::string& error = "")
{
    const uint64_t timestamp{GetMillisecondsSinceEpoch()};

    nlohmann::json j{{"success", success}};
    if (!success)
        j["error"] = error;

    return InfoMessage<MessageType::Status>(id, timestamp, j);
}

static Message ClosingMessage(uint64_t id)
{
    return InfoMessage<MessageType::Closing>(id, GetMillisecondsSinceEpoch());
}

static Message SquadStatusMessage(uint64_t id, const std::string& self, const PlayerContainer& squad,
                                  MessageProtocol protocol)
{
    const uint64_t timestamp{GetMillisecondsSinceEpoch()};

    SerialData serial{};
    nlohmann::json json{};

    if (protocol == MessageProtocol::Serial)
    {
        serial = squad.toSerial(self.size() + 1); // + 1 for null terminator.
        serial_w_string(&serial.ptr[Message::DataOffset()], self.c_str(), self.size());
    }
    else if (protocol == MessageProtocol::JSON)
    {
        json = squad.toJSON();
        json["self"] = self;
    }

    return SquadMessage<MessageType::SquadStatus>(id, timestamp, serial, json);
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

#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_MSG_DEBUG
static std::string SerialDataToStr(const SerialData& data)
{
    std::ostringstream oss{};
    for (std::size_t i{0}; i < data.count; ++i)
    {
        uint8_t mask = 128;
        int j = 0;
        while (j < 8)
        {
            if (data.ptr[i] & mask)
                oss << "1";
            else
                oss << "0";
            mask = mask >> 1;
            ++j;
        }
    }
    return oss.str();
}

static void PrintMsgDebug(const Message& msg, MessageProtocol protocol, std::size_t threadID)
{
    if (protocol == MessageProtocol::Serial && msg.hasSerial())
    {
        BRIDGE_MSG_DEBUG("[ptid {}] Sending \"{}\" to client.", threadID, SerialDataToStr(msg.toSerial()));
    }
    else if (protocol == MessageProtocol::JSON && msg.hasJSON())
    {
        BRIDGE_MSG_DEBUG("[ptid {}] Sending \"{}\" to client.", threadID, msg.toJSON());
    }
}

    #define BRIDGE_PRINT_MSG(...) PrintMsgDebug(__VA_ARGS__)
#else
    #define BRIDGE_PRINT_MSG(...)
#endif

static SendStatus SendToClient(void* handle, const Message& msg, MessageProtocol protocol)
{
    SendStatus send{};

    if (protocol == MessageProtocol::Serial)
    {
        SerialData data{msg.toSerial()};
        send = WriteToPipe(handle, data.ptr.get(), data.count);
    }
    else if (protocol == MessageProtocol::JSON)
    {
        send = WriteToPipe(handle, msg.toJSON());
    }

    return send;
}

PipeThread::PipeThread(std::size_t id, void* handle, MessageTracking* mt, const ApplicationData& appdata,
                       const SquadModifyHandler* squadModifyHandler)
    : m_handle{handle},
      m_mt{mt},
      m_appData{appdata},
      m_id{id},
      m_squadModifyHandler{squadModifyHandler}
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

    m_thread = std::thread([handler = this]() {
        if (!handler->m_run)
        {
            BRIDGE_ERROR("[ptid {}] Could not start PipeThread, m_run = {}", handler->m_id, handler->m_run);
            return;
        }

#if BRIDGE_LOG_LEVEL > BRIDGE_LOG_LEVEL_0
        const std::size_t threadID = handler->m_id;
#endif
        void* handle = handler->m_handle;
        handler->m_running = true;
        BRIDGE_INFO("[ptid {}] Started PipeThread.", threadID);

        BRIDGE_DEBUG("[ptid {}] Client connected, sending bridge information...", threadID);
        Message msg{};
        {
            std::unique_lock<std::mutex> infoLock(handler->m_appData.Info.mutex);
            msg = InfoMessage<MessageType::BridgeInfo>(handler->m_appData.requestID(), GetMillisecondsSinceEpoch(),
                                                       handler->m_appData.Info);
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

        // Check if data is valid json.
        if (!nlohmann::json::accept(readStatus.data))
        {
            const Message statusMsg{StatusMessage(false, "invalid JSON")};
            WriteToPipe(handle, statusMsg.toJSON());
            BRIDGE_ERROR("[ptid {}] Recieved invalid JSON, Ending PipeThread.", threadID);
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            return;
        }

        nlohmann::json j = nlohmann::json::parse(readStatus.data);

        // Subscribe.
        using MessageCategoryU = std::underlying_type_t<MessageCategory>;
        MessageCategoryU filter = 0;

        if (j.contains("subscribe") && j["subscribe"].is_number())
        {
            int32_t sub_value = j["subscribe"].get<int32_t>();

            constexpr auto ui8min = static_cast<int32_t>(std::numeric_limits<MessageCategoryU>::min());
            constexpr auto ui8max = static_cast<int32_t>(std::numeric_limits<MessageCategoryU>::max());

            if (sub_value >= ui8min && sub_value <= ui8max)
                filter = static_cast<MessageCategoryU>(sub_value);

            BRIDGE_DEBUG("[ptid {}] Recieved filter \"{}\" from client.", threadID, static_cast<int>(filter));
        }

        auto combatValue = static_cast<MessageCategoryU>(MessageCategory::Combat);
        if ((filter & combatValue) == combatValue)
        {
            handler->m_eventTrack.combat = true;
            handler->m_mt->trackCategory(MessageCategory::Combat);
        }
        auto extrasValue = static_cast<MessageCategoryU>(MessageCategory::Extras);
        if ((filter & extrasValue) == extrasValue)
        {
            handler->m_eventTrack.extras = true;
            handler->m_mt->trackCategory(MessageCategory::Extras);
        }
        auto squadValue = static_cast<MessageCategoryU>(MessageCategory::Squad);
        if ((filter & squadValue) == squadValue)
        {
            handler->m_eventTrack.squad = true;
            handler->m_mt->trackCategory(MessageCategory::Squad);
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

        if (j.contains("protocol") && j["protocol"].is_string())
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
            const Message statusMsg{StatusMessage(handler->m_appData.requestID(), true)};
            SendStatus send = WriteToPipe(handle, statusMsg.toJSON());
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid {}] Error sending data with err: {}!", threadID, send.error);
            }
        }

        if (handler->m_eventTrack.squad)
        {
            handler->m_status = Status::Sending;

            handler->m_squadModifyHandler->work(
                [id = handler->m_appData.requestID(), &msg, &appData = handler->m_appData, protocol, &msgCont = handler->m_msgCont]() {
                msg = SquadStatusMessage(id, appData.SelfAccountName, appData.Squad, protocol);

                // Clear queue if any messages.
                std::unique_lock<std::mutex> msgLock(msgCont.mutex);
                if (!msgCont.queue.empty())
                    std::queue<Message>().swap(msgCont.queue);
            });

            SendStatus send{SendToClient(handle, msg, protocol)};
            BRIDGE_PRINT_MSG(msg, protocol, threadID);
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
                    if (handler->m_msgCont.cv.wait_for(lock, std::chrono::milliseconds(msTimeout)) ==
                        std::cv_status::timeout)
                    {
                        BRIDGE_DEBUG("[ptid {}] Checking pipe status...", threadID);
                        DWORD availBytes{};
                        if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &availBytes, nullptr))
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
            handler->m_status = Status::Sending;
            sendStatus = SendToClient(handle, msg, protocol);
            BRIDGE_PRINT_MSG(msg, protocol, threadID);

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

            const Message closingMsg{ClosingMessage(handler->m_appData.requestID())};
            SendToClient(handle, closingMsg, protocol);
            BRIDGE_PRINT_MSG(closingMsg, protocol, threadID);
            // Ignore sending error here.
        }

        // Untrack protocol.
        handler->m_mt->unuseProtocol(protocol);

        // Untrack events.
        if (handler->m_eventTrack.combat)
            handler->m_mt->untrackCategory(MessageCategory::Combat);
        if (handler->m_eventTrack.extras)
            handler->m_mt->untrackCategory(MessageCategory::Extras);
        if (handler->m_eventTrack.squad)
            handler->m_mt->untrackCategory(MessageCategory::Squad);

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

    switch (msg.category())
    {
        case MessageCategory::Combat:
            send = m_eventTrack.combat;
            break;
        case MessageCategory::Extras:
            send = m_eventTrack.extras;
            break;
        case MessageCategory::Squad:
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
    return WriteToPipe(handle, reinterpret_cast<const uint8_t*>(msg.c_str()), msg.size());
}

SendStatus WriteToPipe(HANDLE handle, const uint8_t* data, std::size_t count)
{
    const DWORD length{static_cast<DWORD>(count)};
    SendStatus status{};
    status.success = WriteFile(handle, data, length, &status.numBytesWritten, nullptr);

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
        status.success = ReadFile(handle, buffer, BUFSIZE * sizeof(TCHAR), &status.numBytesRead, nullptr);
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
