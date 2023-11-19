//
//  src/PipeHandler.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "PipeHandler.hpp"

#include <utility>
#include "Log.hpp"

PipeHandler::PipeHandler(std::string pipeName, const ApplicationData& appdata,
                         const SquadModifyHandler* squadModifyHandler)
    : m_pipeName{std::move(pipeName)},
      m_appData{appdata},
      m_squadModifyHandler{squadModifyHandler}
{

#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_DEBUG
    for (auto i{1}; i < MessageTypeCount; ++i)
    {
        const auto type = static_cast<MessageType>(i);
        BRIDGE_DEBUG("PipeHandler tracking \"{}\": {}.", MessageTypeStrings[i - 1], m_eventTracking.isTrackingType(type));
    }
#endif
}

PipeHandler::~PipeHandler()
{
#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_DEBUG
    for (auto i{1}; i < MessageTypeCount; ++i)
    {
        const auto type = static_cast<MessageType>(i);
        BRIDGE_DEBUG("~PipeHandler tracking \"{}\": {}.", MessageTypeStrings[i - 1],
                     m_eventTracking.isTrackingType(type));
    }
    BRIDGE_DEBUG("~PipeHandler, running: {} threads: {}", m_run, m_threads.size());
#endif
}

void PipeHandler::start()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Don't start the thread again if already started, start() needs to be follow by stop().
    if (m_threadStarted)
        return;

    // Enable thread function to run.
    m_threadStarted = true;
    m_run = true;

    m_pipeMain = std::thread([handler = this]() {
        if (!handler->m_run)
        {
            BRIDGE_ERROR("Could not start PipeHandler thread, m_run = {}", handler->m_run);
            return;
        }

        std::size_t threadCounter = 1;
        handler->m_running = true;
        BRIDGE_DEBUG("Started PipeHandler thread");

        while (handler->m_run)
        {
            BRIDGE_DEBUG("Creating Named Pipe \"{}\"", handler->m_pipeName);

            HANDLE handle = CreateNamedPipe(handler->m_pipeName.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE,
                                            PIPE_UNLIMITED_INSTANCES, 0, 0, 0, NULL);

            if (handle == NULL || handle == INVALID_HANDLE_VALUE)
            {
                BRIDGE_ERROR("Error creating pipe with err: {}!", GetLastError());
                continue;
            }

            BRIDGE_INFO("Created Named pipe \"{}\"", handler->m_pipeName);

            BRIDGE_INFO("Waiting for client!");
            handler->m_waitingForConnection = true;
            BOOL result = ConnectNamedPipe(handle, NULL); // Blocking
            handler->m_waitingForConnection = false;

            if (!result)
            {
                BRIDGE_ERROR("Error connecting pipe with err: {}!", GetLastError());
                CloseHandle(handle);
                continue;
            }

            if (!handler->m_run)
            {
                BRIDGE_WARN("Client connected when PipeHandler thread is closing.");
                CloseHandle(handle);
                continue;
            }

            BRIDGE_INFO("Client connected, starting a PipeThread instance...");

            handler->cleanup();

            std::size_t threadID = threadCounter++; // ex. threadID = 1, threadCounter = 2.
            bool success{false};

            {
                std::unique_lock<std::mutex> lock(handler->m_mutex);

                uint64_t bridgeValidator{0};
                nlohmann::json info{};

                {
                    std::unique_lock<std::mutex> infoLock(handler->m_appData.Info.mutex);
                    bridgeValidator = handler->m_appData.Info.validator;
                    info = ToJSON(handler->m_appData.Info);
                }

                if (auto t = handler->dispatchPipeThread(handle, threadID))
                {
                    BRIDGE_DEBUG("Sending ConnectionStatus message to client [{}].", threadID);

                    Message msg{ConnectionStatusMessage(handler->m_appData.requestID(), info, true)};
                    SendStatus sendStatus = WriteToPipe(handle, msg);
                    if (sendStatus.success)
                    {
                        BRIDGE_DEBUG("Successfully started client with id = {}.", threadID);
                        success = true;
                        t->start(bridgeValidator);
                    }
                }
                else
                {
                    constexpr std::string_view err{"Could not create PipeThread due to max amount of clients are connected."};
                    BRIDGE_DEBUG("Sending error \"{}\" to client [{}].", err, threadID);
                    Message msg{ConnectionStatusMessage(handler->m_appData.requestID(), info, false, std::string{err})};
                    WriteToPipe(handle, msg);
                }
            }

            if (!success)
            {
                // Unused threadID here.
                BRIDGE_WARN("Unused threadID: {}, resetting threadCounter from {} to {}.", threadID, threadCounter,
                            threadID);
                threadCounter = threadID; // Reset counter (post increment on threadCounter above).
                CloseHandle(handle);
            }
        }

        handler->m_running = false;
        BRIDGE_DEBUG("Ended PipeHandler thread.");
    });
}

PipeThread* PipeHandler::dispatchPipeThread(void* handle, std::size_t id)
{
    // std::unique_lock<std::mutex> lock(m_mutex);
    // Will be locked by the caller.

    if (m_threads.size() < m_appData.Config.maxClients)
    {
        // Maybe add some error handling here in case if vector throws.
        return m_threads
            .emplace_back(std::make_unique<PipeThread>(id, handle, m_appData, &m_eventTracking, m_squadModifyHandler))
            .get();
    }

    BRIDGE_ERROR("Could not create PipeThread due to max amount of clients are connected.");
    return nullptr;
}

void PipeHandler::cleanup()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_DEBUG("PipeHandler cleanup started.");

    // Remove threads that are not running.
    for (auto it = m_threads.begin(); it != m_threads.end();)
    {
        if (!(*it)->running())
        {
            BRIDGE_DEBUG("Removing closed PipeThread [ptid {}].", (*it)->id());
            (*it)->stop();
            it = m_threads.erase(it);
        }
        else
            ++it;
    }

    BRIDGE_DEBUG("PipeHandler cleanup finished.");
}

void PipeHandler::stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_DEBUG("Closing PipeHandler");

    if (m_running)
    {
        m_run = false;

        if (m_waitingForConnection)
        {
            // CancelSynchronousIo(PipeThread.handle);
            BRIDGE_DEBUG("PipeHandler thread is waiting for a connection, attempting to connect...");
            HANDLE pipe = CreateFile(m_pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            CloseHandle(pipe);
        }
    }

    BRIDGE_DEBUG("Waiting for PipeHandler thread to join...");
    m_pipeMain.join();
    BRIDGE_DEBUG("PipeHandler thread joined!");

    if (!m_threads.empty())
    {
        // Remove all PipeThreads.
        BRIDGE_DEBUG("Removing all PipeThreads.");
        for (auto it = m_threads.begin(); it != m_threads.end();)
        {
            (*it)->stop();
            BRIDGE_DEBUG("Removing PipeThread [ptid {}].", (*it)->id());
            it = m_threads.erase(it);
        }
    }

    // Allow thread to be started again.
    m_threadStarted = false;

    BRIDGE_DEBUG("PipeHandler stopped.");
}

void PipeHandler::sendBridgeInfo(const Message& msg, uint64_t validator)
{
    if (!msg.valid())
        return;

    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_running)
    {
        for (std::unique_ptr<PipeThread>& pt : m_threads)
            if (pt->started())
                pt->sendBridgeInfo(msg, validator);
    }
}

void PipeHandler::sendMessage(const Message& msg)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_running)
        forwardMessageToThreads(msg);
}

bool PipeHandler::isTrackingType(MessageType type) const
{
    return m_eventTracking.isTrackingType(type);
}

Message ConnectionStatusMessage(uint64_t id, const nlohmann::json& info, bool success,
                                                 const std::string& error)
{
    const uint64_t timestamp{GetMillisecondsSinceEpoch()};

    nlohmann::json j{{"version", BridgeVersion.version},
                     {"majorApiVersion", BridgeVersion.majorApiVersion},
                     {"minorApiVersion", BridgeVersion.minorApiVersion},
                     {"info", info},
                     {"success", success},
                     {"types", MessageTypeStrings}};
    if (!success)
        j["error"] = error;

    std::string str{j.dump()};
    auto buffer = MessageBuffer::Create(str.size());
    std::memcpy(buffer.ptr.get() + MessageHeaderByteCount(), reinterpret_cast<const uint8_t*>(str.data()), str.size());

    return InfoMessage<MessageType::ConnectionStatus>(id, timestamp, buffer);
}
