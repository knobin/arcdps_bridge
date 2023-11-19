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

    std::string str{j.dump()};
    auto buffer = MessageBuffer::Create(str.size());
    std::memcpy(buffer.ptr.get() + MessageHeaderByteCount(), reinterpret_cast<const uint8_t*>(str.data()), str.size());

    return InfoMessage<MessageType::Status>(id, timestamp, buffer);
}

static Message ClosingMessage(uint64_t id)
{
    return InfoMessage<MessageType::Closing>(id, GetMillisecondsSinceEpoch(), MessageBuffer{});
}

static Message SquadStatusMessage(uint64_t id, const std::string& self, const Squad::PlayerContainer& squad)
{
    const uint64_t timestamp{GetMillisecondsSinceEpoch()};
    auto buffer = squad.CreateMessageBuffer(self);
    return SquadMessage<MessageType::SquadStatus>(id, timestamp, buffer);
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

static SendStatus SendToClient(void* handle, const Message& msg)
{
    return WriteToPipe(handle, msg);
}

PipeThread::PipeThread(std::size_t id, void* handle, const ApplicationData& appdata, EventTracking *eventTracking,
                       const SquadModifyHandler* squadModifyHandler)
    : m_handle{handle},
      m_appData{appdata},
      m_handlerEventTracking{eventTracking},
      m_id{id},
      m_squadModifyHandler{squadModifyHandler}
{
    BRIDGE_DEBUG("Created PipeThread [ptid {}]", m_id);
}

PipeThread::~PipeThread()
{
#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_DEBUG
    for (auto i{1}; i < MessageTypeCount; ++i)
    {
        const auto type = static_cast<MessageType>(i);
        BRIDGE_DEBUG("~PipeThread tracking \"{}\": {}.", MessageTypeStrings[i - 1], isTrackingType(type));
    }
    BRIDGE_DEBUG("~PipeThread [ptid {}], running: {}", m_id, m_running);
#endif
}

std::underlying_type_t<MessageType> MsgTypeStringToType(std::string_view str)
{
    for (auto i{0}; i < MessageTypeStrings.size(); ++i)
    {
        if (MessageTypeStrings[i] == str)
            return static_cast<std::underlying_type_t<MessageType>>(i + 1);
    }

    return 0;
}

void PipeThread::start(uint64_t bridgeValidator)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Don't start the thread again if already started, start() needs to be follow by stop().
    if (m_threadStarted)
        return;

    // Enable thread function to run.
    m_threadStarted = true;
    m_run = true;
    m_bridgeValidator = bridgeValidator;

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
            Message statusMsg{StatusMessage(handler->m_appData.requestID(), false, "Invalid JSON")};
            WriteToPipe(handle, statusMsg);
            BRIDGE_ERROR("[ptid {}] Recieved invalid JSON, Ending PipeThread.", threadID);
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            return;
        }

        nlohmann::json j = nlohmann::json::parse(readStatus.data);

        // Subscribe, array of strings.
        std::size_t subCount{0};
        if (j.contains("subscribe") && j["subscribe"].is_array())
        {
            std::vector<std::string> types = j["subscribe"];

            for (const auto& str : types)
            {
                const auto msgTypeValue = MsgTypeStringToType(str);
                if (msgTypeValue > 0)
                {
                    handler->incType(static_cast<MessageType>(msgTypeValue));
                    BRIDGE_DEBUG("[ptid {}] Client subscribed to \"{}\".", threadID, str);
                    ++subCount;
                }
                else
                {
                    const std::string err = "Invalid Message Type \"" + str + "\".";
                    const auto statusMsg{StatusMessage(handler->m_appData.requestID(), false, err)};
                    WriteToPipe(handle, statusMsg);
                    BRIDGE_ERROR("[ptid {}] No such Message Type \"{}\", Ending PipeThread.", threadID, str);
                    CloseHandle(handle);
                    handler->m_handle = nullptr;
                    handler->m_running = false;
                    return;
                }
            }
        }

        // Subscription error (if no types are subscribed to).
        if (subCount == 0)
        {
            const auto statusMsg{StatusMessage(handler->m_appData.requestID(), false, "No types are subscribed to")};
            WriteToPipe(handle, statusMsg);
            BRIDGE_ERROR("[ptid {}] No types are subscribed to, Ending PipeThread.", threadID);
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            return;
        }

        // Success!
        {
            const Message statusMsg{StatusMessage(handler->m_appData.requestID(), true)};
            SendStatus send = WriteToPipe(handle, statusMsg);
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid {}] Error sending data with err: {}!", threadID, send.error);
            }
        }

        BRIDGE_INFO("[ptid {}] Client is now connected and can recieve events.", threadID);

        Message msg{};

        if (handler->isTrackingType(MessageType::SquadStatus))
        {
            handler->m_status = Status::Sending;

            handler->m_squadModifyHandler->work(
                [id = handler->m_appData.requestID(), &msg, &appData = handler->m_appData,
                 &msgCont = handler->m_msgCont]() {
                msg = SquadStatusMessage(id, appData.SelfAccountName, appData.Squad);

                // Clear queue if any messages.
                std::unique_lock<std::mutex> msgLock(msgCont.mutex);
                if (!msgCont.queue.empty())
                    std::queue<Message>().swap(msgCont.queue);
            });

            SendStatus send{SendToClient(handle, msg)};
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
                if (!msg.valid())
                {
                    BRIDGE_WARN("[ptid {}] Empty message found", threadID);
                    continue;
                }
            }

            // Send retrieved message.
            handler->m_status = Status::Sending;
            SendStatus sendStatus = SendToClient(handle, msg);
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

            Message closingMsg{ClosingMessage(handler->m_appData.requestID())};
            SendToClient(handle, closingMsg);
            BRIDGE_PRINT_MSG(closingMsg, protocol, threadID);
            // Ignore sending error here.
        }

        // Untrack events.
        handler->resetTypeTracking();

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
                m_msgCont.queue.emplace();
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

    if (isTrackingType(msg.type()))
    {
        std::unique_lock<std::mutex> msgLock(m_msgCont.mutex);
        if (m_msgCont.queue.size() < m_appData.Config.msgQueueSize)
        {
            m_msgCont.queue.push(msg);
            m_msgCont.cv.notify_one();
        }
    }
}

void PipeThread::incType(MessageType type)
{
    if (!isTrackingType(type))
    {
        m_eventTracking.incType(type);
        if (m_handlerEventTracking)
            m_handlerEventTracking->incType(type);
    }
}

void PipeThread::decType(MessageType type)
{
    if (isTrackingType(type))
    {
        m_eventTracking.decType(type);
        if (m_handlerEventTracking)
            m_handlerEventTracking->decType(type);
    }
}

void PipeThread::trackType(MessageType type)
{
    incType(type);
}

void PipeThread::resetTypeTracking()
{
    for (std::ptrdiff_t i{1}; i < MessageTypeCount; ++i)
    {
        decType(static_cast<MessageType>(i));
    }
}

bool PipeThread::isTrackingType(MessageType type) const
{
    return m_eventTracking.isTrackingType(type);
}

SendStatus WriteToPipe(HANDLE handle, const Message* msg)
{
    return WriteToPipe(handle, msg->data(), msg->count());
}

SendStatus WriteToPipe(HANDLE handle, const Message& msg)
{
    return WriteToPipe(handle, msg.data(), msg.count());
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

constexpr std::size_t BUFFER_SIZE{512};

ReadStatus ReadFromPipe(HANDLE handle)
{
    ReadStatus status{};
    TCHAR buffer[BUFFER_SIZE]{};

    do
    {
        status.success = ReadFile(handle, buffer, BUFFER_SIZE * sizeof(TCHAR), &status.numBytesRead, nullptr);
        status.error = GetLastError();
        if (!status.success && status.error != ERROR_MORE_DATA)
            break;

        if (status.numBytesRead < BUFFER_SIZE)
            buffer[status.numBytesRead] = '\0';

        status.data += std::string{buffer};
    } while (!status.success);

    if (!status.success)
        status.error = GetLastError();

    return status;
}

void EventTracking::incType(MessageType type)
{
    using MessageTypeU = std::underlying_type_t<MessageType>;
    const auto index = static_cast<MessageTypeU>(type);
    ++m_types[static_cast<std::ptrdiff_t>(index)];
}

void EventTracking::decType(MessageType type)
{
    if (isTrackingType(type))
    {
        using MessageTypeU = std::underlying_type_t<MessageType>;
        const auto index = static_cast<MessageTypeU>(type);
        --m_types[static_cast<std::ptrdiff_t>(index)];
    }
}

bool EventTracking::isTrackingType(MessageType type) const
{
    using MessageTypeU = std::underlying_type_t<MessageType>;
    const auto index = static_cast<MessageTypeU>(type);
    return m_types[static_cast<std::ptrdiff_t>(index)];
}