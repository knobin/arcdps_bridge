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

PipeThread::PipeThread(std::size_t id, void* handle, TrackedEvents* te, const ApplicationData& appdata)
    : m_handle{handle}, m_te{te}, m_appData{appdata}, m_id{id}
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
            BRIDGE_INFO("[ptid {}] Could not start PipeThread, m_run = {}", handler->m_id, handler->m_run);
            return;
        }

        std::size_t threadID = handler->m_id;
        void* handle = handler->m_handle;
        handler->m_running = true;
        BRIDGE_INFO("[ptid {}] Started PipeThread", threadID);

        BRIDGE_INFO("[ptid {}] Client connected, sending bridge information...", threadID);
        std::string msg = BridgeInfoToJSON(handler->m_appData.Info);
        handler->m_status = Status::Sending;
        BRIDGE_MSG_DEBUG("[ptid {}] Sending \"{}\" to client.", threadID, msg);
        SendStatus sendStatus = WriteToPipe(handle, msg);
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
        std::size_t firstDigit = readStatus.data.find_first_of("0123456789");
        using MessageTypeU = std::underlying_type_t<MessageType>;
        MessageTypeU filter = 0;
        if (firstDigit != std::string::npos)
        {
            std::size_t lastDigit = readStatus.data.find_first_not_of("0123456789", firstDigit);
            if (lastDigit != std::string::npos && lastDigit > firstDigit)
            {
                BRIDGE_DEBUG("[ptid {}] substr: \"{}\"", threadID, readStatus.data.substr(firstDigit, lastDigit - firstDigit));
                std::istringstream iss{readStatus.data.substr(firstDigit, lastDigit - firstDigit)};
                int i = 0;
                iss >> i;
                filter = static_cast<MessageTypeU>(i);
                BRIDGE_INFO("[ptid {}] Recieved filter \"{}\" from client.", threadID, static_cast<int>(filter));
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

        BRIDGE_INFO("[ptid {}] Client has subscribed to \"Combat\": {}", threadID, handler->m_eventTrack.combat);
        BRIDGE_INFO("[ptid {}] Client has subscribed to \"Extras\": {}", threadID, handler->m_eventTrack.extras);
        BRIDGE_INFO("[ptid {}] Client has subscribed to \"Squad\": {}", threadID, handler->m_eventTrack.squad);

        if (!(handler->m_eventTrack.combat || handler->m_eventTrack.extras || handler->m_eventTrack.squad))
        {
            const auto statusObj = "{\"type\":\"status\",\"status\":{\"success\":false,\"error\":\"no subscription\"}}";
            WriteToPipe(handle, statusObj);
            BRIDGE_ERROR("[ptid {}] No subscription, Ending PipeThread.", threadID);
            CloseHandle(handle);
            handler->m_handle = nullptr;
            handler->m_running = false;
            return;
        }

        {
            const auto statusObj = "{\"type\":\"status\",\"status\":{\"success\":true}}";
            SendStatus send = WriteToPipe(handle, statusObj);
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid {}] Error sending data with err: {}!", threadID, send.error);   
            }
        }

        if (handler->m_eventTrack.squad)
        {
            handler->m_status = Status::Sending;
            BRIDGE_DEBUG("[ptid {}] Sending Squad information to client...", threadID);
            std::ostringstream ss{};
            ss << "{\"type\":\"squad\",\"squad\":{"
               << "\"trigger\":\"status\","
               << "\"status\":{"
               << "\"self\":\"" << handler->m_appData.Self.accountName << "\","
               << "\"members\":" << handler->m_appData.Squad.toJSON()
               << "}}}";
            SendStatus send = WriteToPipe(handle, ss.str());
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
            msg = "";

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
            BRIDGE_MSG_DEBUG("[ptid {}] Sending \"{}\" to client.", threadID, msg);
            handler->m_status = Status::Sending;
            sendStatus = WriteToPipe(handle, msg);

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

        // Untrack events.
        if (handler->m_eventTrack.combat)
            handler->m_te->untrack(MessageType::Combat);
        if (handler->m_eventTrack.extras)
            handler->m_te->untrack(MessageType::Extras);
        if (handler->m_eventTrack.squad)
            handler->m_te->untrack(MessageType::Squad);

        handler->m_status = Status::NONE;
        CloseHandle(handle);
        handler->m_handle = nullptr;
        handler->m_running = false;

        BRIDGE_INFO("[ptid {}] Ended PipeThread.", threadID);
    });
}

void PipeThread::stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_INFO("Closing PipeThread [ptid {}]...", m_id);

    if (m_running)
    {
        m_run = false;

        {
            std::lock_guard<std::mutex> msgLock(m_msgCont.mutex);

            // Add empty message in case of blocked waiting.
            if (m_status == Status::WaitingForMessage)
            {
                BRIDGE_DEBUG("PipeThread [ptid {}] is waiting for message, attempting to send empty message...", m_id);    
                m_msgCont.queue.push("");
                m_msgCont.cv.notify_one();
            }
        }
    }

    BRIDGE_DEBUG("Waiting for PipeThread [ptid {}] to join...", m_id);
    m_thread.join();
    BRIDGE_DEBUG("PipeThread [ptid {}] joined.", m_id);

    // Allow thread to be started again.
    m_threadStarted = false;

    BRIDGE_INFO("PipeThread [ptid {}] Closed!", m_id);
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
    const DWORD length{static_cast<DWORD>(msg.size())};
    SendStatus status{};
    status.success = WriteFile(handle, msg.c_str(), length, &status.numBytesWritten, NULL);

    if (!status.success)
        status.error = GetLastError();

    return status;
}

#define BUFSIZE 512

ReadStatus ReadFromPipe(HANDLE handle)
{
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
        status.error = GetLastError();
    
    return status;
}
