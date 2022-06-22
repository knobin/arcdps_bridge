//
//  PipeThread.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "PipeThread.hpp"
#include "PipeHandler.hpp"
#include "Log.hpp"

// C++ Headers
#include <cstdint>
#include <sstream>

PipeThread::PipeThread(void* handle, TrackedEvents* te, const ApplicationData& appdata)
    : m_handle{handle}, m_te{te}, m_appData{appdata}
{
    
}

PipeThread::~PipeThread()
{
    BRIDGE_INFO("~PipeThread, running: ", m_run);
}

void PipeThread::start()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_thread = std::thread([handler = this](){
        BRIDGE_INFO("Started PipeThread");
        handler->m_run = true;

        BRIDGE_INFO("Client connected, sending bridge information...");
        std::string msg = BridgeInfoToJSON(handler->m_appData.Info);
        handler->m_status = Status::Sending;
        SendStatus sendStatus = WriteToPipe(handler->m_handle, msg);
        if (!sendStatus.success)
        {
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            BRIDGE_INFO("Failed to send bridge information.");
            return;
        }

        BRIDGE_INFO("Waiting for client to subscribe...");
        handler->m_status = Status::Reading;
        ReadStatus readStatus = ReadFromPipe(handler->m_handle);
        if (!readStatus.success)
        {
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            BRIDGE_INFO("Failed to read bridge information.");
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
                BRIDGE_INFO("substr: \"", readStatus.data.substr(firstDigit, lastDigit - firstDigit), "\"");
                std::istringstream iss{readStatus.data.substr(firstDigit, lastDigit - firstDigit)};
                int i = 0;
                iss >> i;
                filter = static_cast<MessageTypeU>(i);
                BRIDGE_INFO("Recieved filter \"", static_cast<int>(filter), "\" from client.", i);
            }
        }

        if (filter == 0)
        {
            BRIDGE_INFO("Filter is 0, Closing PipeThread.");
            CloseHandle(handler->m_handle);
            handler->m_handle = nullptr;
            return;
        }

        MessageTypeU combatValue = static_cast<MessageTypeU>(MessageType::Combat);
        if ((filter & combatValue) == combatValue)
        {
            handler->m_eventTrack.combat = true;
            handler->m_te->startTracking(MessageType::Combat);
        }
        MessageTypeU extraValue = static_cast<MessageTypeU>(MessageType::Extra);
        if ((filter & extraValue) == extraValue)
        {
            handler->m_eventTrack.extra = true;
            handler->m_te->startTracking(MessageType::Extra);
        }
        MessageTypeU squadValue = static_cast<MessageTypeU>(MessageType::Squad);
        if ((filter & squadValue) == squadValue)
        {
            handler->m_eventTrack.squad = true;
            handler->m_te->startTracking(MessageType::Squad);
        }

        BRIDGE_INFO("Client has subscribed to \"Combat\": ", handler->m_eventTrack.combat);
        BRIDGE_INFO("Client has subscribed to \"Extra\": ", handler->m_eventTrack.extra);
        BRIDGE_INFO("Client has subscribed to \"Squad\": ", handler->m_eventTrack.squad);

        if (handler->m_eventTrack.squad)
        {
            handler->m_status = Status::Sending;
            BRIDGE_INFO("Sending Squad information to client...");
            std::ostringstream ss{};
            ss << "{\"type\":\"Squad\",\"squad\":{";
            ss << "\"trigger\":\"status\",";
            ss << "\"self\":\"" << handler->m_appData.Self << "\",";
            ss << "\"members\":" << handler->m_appData.Squad.toJSON();
            ss << "}}";
            SendStatus sendStatus = WriteToPipe(handler->m_handle, ss.str());
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

                    BRIDGE_INFO("Checking pipe status...");
                    DWORD availBytes{};
                    if (!PeekNamedPipe(handler->m_handle, 0, 0, 0, &availBytes, 0))
                    {
                        DWORD err = GetLastError();
                        if (err == ERROR_BROKEN_PIPE || err == ERROR_NO_DATA)
                        {
                            BRIDGE_INFO("Client unexpectedly disconnected!");
                            broken = true;
                            break;
                        }
                    }
                    BRIDGE_INFO("Client is connected.");
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
                    BRIDGE_INFO("Empty message found");
                    continue;
                }
            }

            // Send retrieved message.
            BRIDGE_INFO("Sending message...");
            handler->m_status = Status::Sending;
            sendStatus = WriteToPipe(handler->m_handle, msg);

            if (!sendStatus.success)
            {
                if (sendStatus.error == ERROR_BROKEN_PIPE || sendStatus.error == ERROR_NO_DATA)
                {
                    BRIDGE_INFO("Client unexpectedly disconnected!");
                    break;
                }
            }

            BRIDGE_MSG_INFO("Data sent to client!");
        }

        BRIDGE_INFO("PipeThread is closing!");
        handler->m_status = Status::NONE;
        CloseHandle(handler->m_handle);
        BRIDGE_INFO("Ended PipeThread!");
        handler->m_run = false;
        
        // Untrack events.
        if (handler->m_eventTrack.combat)
            handler->m_te->untrack(MessageType::Combat);
        if (handler->m_eventTrack.extra)
            handler->m_te->untrack(MessageType::Extra);
        if (handler->m_eventTrack.squad)
            handler->m_te->untrack(MessageType::Squad);
    }); 
}

void PipeThread::stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_INFO("Starting to close PipeThread...");

    if (m_run)
    {
        // Add empty message in case of blocked waiting.
        if (m_status == Status::WaitingForMessage)
        {
            BRIDGE_INFO("PipeThread is waiting for message, attempting to send empty message...");
            std::unique_lock<std::mutex> lock(m_msgCont.mutex);
            m_msgCont.queue.emplace("");
            m_msgCont.cv.notify_one();
        }
    }

    BRIDGE_INFO("Waiting for PipeThread to join...");
    m_thread.join();
    BRIDGE_INFO("PipeThread joined.");
    BRIDGE_INFO("PipeThread Closed!");

    m_run = false;
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
    case MessageType::Extra:
        send = m_eventTrack.extra;
        break;
    case MessageType::Squad:
        send = m_eventTrack.squad;
        break;
    default:
        break;
    }

    if (send)
    {
        std::unique_lock<std::mutex> lock(m_msgCont.mutex);
        m_msgCont.queue.push(msg);
        m_msgCont.cv.notify_one();
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
        status.success = ReadFile(handle, buffer, BUFSIZE*sizeof(TCHAR), &status.numBytesRead, NULL);
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
