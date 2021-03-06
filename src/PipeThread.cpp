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
    BRIDGE_DEBUG("[ptid = {}] Created PipeThread", m_id);
}

PipeThread::~PipeThread()
{
    BRIDGE_DEBUG("[ptid = {}] ~PipeThread, running: {}", m_id, m_run);
}

void PipeThread::start()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_thread = std::thread([handler = this](){
        BRIDGE_INFO("[ptid = {}] Started PipeThread", handler->m_id);
        handler->m_run = true;

        BRIDGE_INFO("[ptid = {}] Client connected, sending bridge information...", handler->m_id);
        std::string msg = BridgeInfoToJSON(handler->m_appData.Info);
        handler->m_status = Status::Sending;
        SendStatus sendStatus = WriteToPipe(handler->m_handle, msg);
        if (!sendStatus.success)
        {
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            handler->m_run = false;
            BRIDGE_ERROR("[ptid = {}] Failed to send bridge information.", handler->m_id);
            return;
        }

        BRIDGE_INFO("[ptid = {}] Waiting for client to subscribe...", handler->m_id);
        handler->m_status = Status::Reading;
        ReadStatus readStatus = ReadFromPipe(handler->m_handle);
        if (!readStatus.success)
        {
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            handler->m_run = false;
            BRIDGE_ERROR("[ptid = {}] Failed to read bridge information.", handler->m_id);
            return;
        }
        BRIDGE_MSG_INFO("[ptid = {}] Retrieved \"{}\" from client!", readStatus.data, m_id);

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
                BRIDGE_DEBUG("[ptid = {}] substr: \"{}\"", handler->m_id, readStatus.data.substr(firstDigit, lastDigit - firstDigit));
                std::istringstream iss{readStatus.data.substr(firstDigit, lastDigit - firstDigit)};
                int i = 0;
                iss >> i;
                filter = static_cast<MessageTypeU>(i);
                BRIDGE_INFO("[ptid = {}] Recieved filter \"{}\" from client.", handler->m_id, static_cast<int>(filter));
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

        BRIDGE_INFO("[ptid = {}] Client has subscribed to \"Combat\": ", handler->m_id, handler->m_eventTrack.combat);
        BRIDGE_INFO("[ptid = {}] Client has subscribed to \"Extras\": ", handler->m_id, handler->m_eventTrack.extras);
        BRIDGE_INFO("[ptid = {}] Client has subscribed to \"Squad\": ", handler->m_id, handler->m_eventTrack.squad);

        if (!(handler->m_eventTrack.combat || handler->m_eventTrack.extras || handler->m_eventTrack.squad))
        {
            const auto statusObj = "{\"type\":\"status\",\"status\":{\"success\":false,\"error\":\"no subscription\"}}";
            WriteToPipe(handler->m_handle, statusObj);
            BRIDGE_ERROR("[ptid = {}] No subscription, Closing PipeThread.", handler->m_id);
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            handler->m_run = false;
            return;
        }

        {
            const auto statusObj = "{\"type\":\"status\",\"status\":{\"success\":true}}";
            SendStatus send = WriteToPipe(handler->m_handle, statusObj);
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid = {}] Error sending data with err: {}!", handler->m_id, send.error);   
            }
        }

        if (handler->m_eventTrack.squad)
        {
            handler->m_status = Status::Sending;
            BRIDGE_INFO("[ptid = {}] Sending Squad information to client...", handler->m_id);
            std::ostringstream ss{};
            ss << "{\"type\":\"squad\",\"squad\":{"
               << "\"trigger\":\"status\","
               << "\"status\":{"
               << "\"self\":\"" << handler->m_appData.Self.accountName << "\","
               << "\"members\":" << handler->m_appData.Squad.toJSON()
               << "}}}";
            SendStatus send =WriteToPipe(handler->m_handle, ss.str());
            if (!send.success)
            {
                BRIDGE_ERROR("[ptid = {}] Error sending data with err: {}!", handler->m_id, send.error);   
            }
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

                    BRIDGE_DEBUG("[ptid = {}] Checking pipe status...", handler->m_id);
                    DWORD availBytes{};
                    if (!PeekNamedPipe(handler->m_handle, 0, 0, 0, &availBytes, 0))
                    {
                        DWORD err = GetLastError();
                        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                        {
                            BRIDGE_ERROR("[ptid = {}] Client unexpectedly disconnected!", handler->m_id);
                            broken = true;
                            break;
                        }
                    }
                    BRIDGE_DEBUG("[ptid = {}] Client is connected.", handler->m_id);
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
                    BRIDGE_WARN("[ptid = {}] Empty message found", handler->m_id);
                    continue;
                }
            }

            // Send retrieved message.
            BRIDGE_MSG_INFO("[ptid = {}] Sending message...", handler->m_id);
            handler->m_status = Status::Sending;
            sendStatus = WriteToPipe(handler->m_handle, msg);

            if (!sendStatus.success)
            {
                if (sendStatus.error == ERROR_BROKEN_PIPE || sendStatus.error == ERROR_NO_DATA)
                {
                    BRIDGE_ERROR("[ptid = {}]  Client unexpectedly disconnected!", handler->m_id);
                    break;
                }
            }

            BRIDGE_MSG_INFO("[ptid = {}] Data sent to client!", handler->m_id);
        }

        BRIDGE_INFO("[ptid = {}] PipeThread is closing!", handler->m_id);
        handler->m_status = Status::NONE;
        CloseHandle(handler->m_handle);
        BRIDGE_INFO("[ptid = {}] Ended PipeThread!", handler->m_id);
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

    BRIDGE_INFO("[ptid = {}] Starting to close PipeThread...", m_id);

    if (m_run)
    {
        m_run = false;

        // Add empty message in case of blocked waiting.
        if (m_status == Status::WaitingForMessage)
        {
            BRIDGE_INFO("[ptid = {}] PipeThread is waiting for message, attempting to send empty message...", m_id);
            std::unique_lock<std::mutex> msgLock(m_msgCont.mutex);
            m_msgCont.queue.emplace("");
            m_msgCont.cv.notify_one();
        }
    }

    BRIDGE_INFO("[ptid = {}] Waiting for PipeThread to join...", m_id);
    m_thread.join();
    BRIDGE_INFO("[ptid = {}] PipeThread joined.", m_id);
    BRIDGE_INFO("[ptid = {}] PipeThread Closed!", m_id);
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
    BRIDGE_MSG_INFO("[ptid = {}] Sending \"", msg, "\" to client!", m_id);
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
