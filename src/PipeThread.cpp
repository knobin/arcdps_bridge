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

// C++ Headers
#include <cstdint>
#include <sstream>

#ifdef BRIDGE_DEBUG

#include <vector>
#include <utility>
#include <mutex>
#include <algorithm>

static std::vector<std::pair<const PipeThread*, std::size_t>> s_threadIDs{};
static std::mutex s_mutex{};

static std::size_t GetUniqueThreadID(const PipeThread* pThread)
{
    std::unique_lock<std::mutex> lock(s_mutex);

    auto it = std::find_if(s_threadIDs.cbegin(), s_threadIDs.cend(), [pThread](const auto& p){
        return p.first == pThread;
    });

    if (it != s_threadIDs.cend())
        return it->second;

    std::sort(s_threadIDs.begin(), s_threadIDs.end(), [](auto &left, auto &right) {
        return left.second < right.second;
    });

    std::size_t id{1};
    for (const auto& p : s_threadIDs) 
        if (p.second == id)
            ++id;
        else
            break;

    s_threadIDs.push_back({pThread, id});
    BRIDGE_INFO("Created PipeThread id = ", id);
    return id;
}

static void RemoveThreadID(const PipeThread* pThread)
{
    std::unique_lock<std::mutex> lock(s_mutex);

    for (auto it = s_threadIDs.begin(); it != s_threadIDs.end();)
    {
        if (it->first == pThread)
        {
            BRIDGE_INFO("Removed PipeThread id = ", it->second);
            it = s_threadIDs.erase(it);
        }
        else
            ++it;
    }
}

static std::string GetPTIDStr(const PipeThread* pThread) 
{
    std::ostringstream ss{};
    ss << "[ptid = " << GetUniqueThreadID(pThread) << "] ";
    return ss.str();
}

#define BRIDGE_CREATE_PTID(...) GetUniqueThreadID(__VA_ARGS__)
#define BRIDGE_REMOVE_PTID(...) RemoveThreadID(__VA_ARGS__)
#define BRIDGE_PTID_STR(...) GetPTIDStr(__VA_ARGS__)

#else

#define BRIDGE_CREATE_PTID(...)
#define BRIDGE_REMOVE_PTID(...)
#define BRIDGE_PTID_STR(...) ""

#endif

PipeThread::PipeThread(void* handle, TrackedEvents* te, const ApplicationData& appdata)
    : m_handle{handle}, m_te{te}, m_appData{appdata}
{
    BRIDGE_CREATE_PTID(this);
}

PipeThread::~PipeThread()
{
    BRIDGE_INFO(BRIDGE_PTID_STR(this), "~PipeThread, running: ", m_run);
    BRIDGE_REMOVE_PTID(this);
}

void PipeThread::start()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_thread = std::thread([handler = this](){
        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Started PipeThread");
        handler->m_run = true;

        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client connected, sending bridge information...");
        std::string msg = BridgeInfoToJSON(handler->m_appData.Info);
        handler->m_status = Status::Sending;
        SendStatus sendStatus = WriteToPipe(handler->m_handle, msg);
        if (!sendStatus.success)
        {
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            handler->m_run = false;
            BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Failed to send bridge information.");
            return;
        }

        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Waiting for client to subscribe...");
        handler->m_status = Status::Reading;
        ReadStatus readStatus = ReadFromPipe(handler->m_handle);
        if (!readStatus.success)
        {
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            handler->m_run = false;
            BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Failed to read bridge information.");
            return;
        }

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
                BRIDGE_INFO(BRIDGE_PTID_STR(handler), "substr: \"", readStatus.data.substr(firstDigit, lastDigit - firstDigit), "\"");
                std::istringstream iss{readStatus.data.substr(firstDigit, lastDigit - firstDigit)};
                int i = 0;
                iss >> i;
                filter = static_cast<MessageTypeU>(i);
                BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Recieved filter \"", static_cast<int>(filter), "\" from client.");
            }
        }

        MessageTypeU combatValue = static_cast<MessageTypeU>(MessageType::Combat);
        if ((filter & combatValue) == combatValue)
        {
            handler->m_eventTrack.combat = true;
            handler->m_te->startTracking(MessageType::Combat);
        }
        MessageTypeU extrasValue = static_cast<MessageTypeU>(MessageType::Extras);
        if ((filter & extrasValue) == extrasValue)
        {
            handler->m_eventTrack.extras = true;
            handler->m_te->startTracking(MessageType::Extras);
        }
        MessageTypeU squadValue = static_cast<MessageTypeU>(MessageType::Squad);
        if ((filter & squadValue) == squadValue)
        {
            handler->m_eventTrack.squad = true;
            handler->m_te->startTracking(MessageType::Squad);
        }

        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client has subscribed to \"Combat\": ", handler->m_eventTrack.combat);
        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client has subscribed to \"Extras\": ", handler->m_eventTrack.extras);
        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client has subscribed to \"Squad\": ", handler->m_eventTrack.squad);

        if (!(handler->m_eventTrack.combat || handler->m_eventTrack.extras || handler->m_eventTrack.squad))
        {
            const auto statusObj = "{\"type\":\"status\",\"status\":{\"success\":false,\"error\":\"no subscription\"}}";
            WriteToPipe(handler->m_handle, statusObj);
            BRIDGE_INFO(BRIDGE_PTID_STR(handler), "No subscription, Closing PipeThread.");
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            handler->m_run = false;
            return;
        }

        {
            const auto statusObj = "{\"type\":\"status\",\"status\":{\"success\":true}}";
            WriteToPipe(handler->m_handle, statusObj);
        }

        if (handler->m_eventTrack.squad)
        {
            handler->m_status = Status::Sending;
            BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Sending Squad information to client...");
            std::ostringstream ss{};
            ss << "{\"type\":\"squad\",\"squad\":{"
               << "\"trigger\":\"status\","
               << "\"status\":{"
               << "\"self\":\"" << handler->m_appData.Self.accountName << "\","
               << "\"members\":" << handler->m_appData.Squad.toJSON()
               << "}}}";
            WriteToPipe(handler->m_handle, ss.str());
        }

        while (handler->m_run)
        {
            BRIDGE_MSG_INFO("Retrieving message to send.");
            msg = "";

            {
                std::unique_lock<std::mutex> lock(handler->m_msgCont.mutex);
                handler->m_status = Status::WaitingForMessage;

                // Block thread until message is added to queue (1s max).
                bool broken = false;
                while (handler->m_msgCont.queue.empty())
                {
                    handler->m_msgCont.cv.wait_for(lock, std::chrono::seconds(120));

                    BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Checking pipe status...");
                    DWORD availBytes{};
                    if (!PeekNamedPipe(handler->m_handle, 0, 0, 0, &availBytes, 0))
                    {
                        DWORD err = GetLastError();
                        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                        {
                            BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client unexpectedly disconnected!");
                            broken = true;
                            break;
                        }
                    }
                    BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client is connected.");
                }

                // Client has disconnected.
                if (broken)
                    break;

                // Retrieve message.
                msg = handler->m_msgCont.queue.front();
                handler->m_msgCont.queue.pop();

                // Do not send empty message.
                if (msg.empty())
                {
                    BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Empty message found");
                    continue;
                }
            }

            // Send retrieved message.
            BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Sending message...");
            handler->m_status = Status::Sending;
            sendStatus = WriteToPipe(handler->m_handle, msg);

            if (!sendStatus.success)
            {
                if (sendStatus.error == ERROR_BROKEN_PIPE || sendStatus.error == ERROR_NO_DATA)
                {
                    BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Client unexpectedly disconnected!");
                    break;
                }
            }

            BRIDGE_MSG_INFO("Data sent to client!");
        }

        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "PipeThread is closing!");
        handler->m_status = Status::NONE;
        CloseHandle(handler->m_handle);
        BRIDGE_INFO(BRIDGE_PTID_STR(handler), "Ended PipeThread!");
        handler->m_run = false;

        // Untrack events.
        if (handler->m_eventTrack.combat)
            handler->m_te->untrack(MessageType::Combat);
        if (handler->m_eventTrack.extras)
            handler->m_te->untrack(MessageType::Extras);
        if (handler->m_eventTrack.squad)
            handler->m_te->untrack(MessageType::Squad);
    });
}

void PipeThread::stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_INFO(BRIDGE_PTID_STR(this), "Starting to close PipeThread...");

    if (m_run)
    {
        m_run = false;

        // Add empty message in case of blocked waiting.
        if (m_status == Status::WaitingForMessage)
        {
            BRIDGE_INFO(BRIDGE_PTID_STR(this), "PipeThread is waiting for message, attempting to send empty message...");
            std::unique_lock<std::mutex> msgLock(m_msgCont.mutex);
            m_msgCont.queue.emplace("");
            m_msgCont.cv.notify_one();
        }
    }

    BRIDGE_INFO(BRIDGE_PTID_STR(this), "Waiting for PipeThread to join...");
    m_thread.join();
    BRIDGE_INFO(BRIDGE_PTID_STR(this), "PipeThread joined.");
    BRIDGE_INFO(BRIDGE_PTID_STR(this), "PipeThread Closed!");
}

void PipeThread::sendMessage(const std::string& msg, MessageType type)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    bool send = false;

    switch (type)
    {
        case MessageType::NONE:
            break;
        case MessageType::Combat:
            send = m_eventTrack.combat;
            break;
        case MessageType::Extras:
            send = m_eventTrack.extras;
            break;
        case MessageType::Squad:
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
    BRIDGE_MSG_INFO("Sending \"", msg, "\" to client!");
    const DWORD length{static_cast<DWORD>(msg.size())};
    SendStatus status{};
    status.success = WriteFile(handle, msg.c_str(), length, &status.numBytesWritten, NULL);
    if (!status.success)
    {
        status.error = GetLastError();
        BRIDGE_INFO("Error sending data with err: ", status.error, "!");
    }
    return status;
}

#define BUFSIZE 512

ReadStatus ReadFromPipe(HANDLE handle)
{
    BRIDGE_MSG_INFO("Reading data from client!");
    ReadStatus status{};
    TCHAR buffer[BUFSIZE];

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
    {
        status.error = GetLastError();
        BRIDGE_INFO("Error reading data with err: ", status.error, "!");
        return status;
    }
    BRIDGE_MSG_INFO("Retrieved \"", status.data, "\" from client!");
    return status;
}
