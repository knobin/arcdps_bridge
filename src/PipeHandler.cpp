//
//  src/PipeHandler.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "PipeHandler.hpp"
#include "Log.hpp"

// Windows Headers
#include <windows.h>

PipeHandler::PipeHandler(const std::string pipeName, const ApplicationData& appdata)
    : m_pipeName{pipeName}, m_appData{appdata}
{
}

PipeHandler::~PipeHandler()
{
    BRIDGE_DEBUG("~PipeHandler tracking combat events: {}.", m_trackedEvents.isTracking(MessageType::Combat));
    BRIDGE_DEBUG("~PipeHandler tracking extras events: {}.", m_trackedEvents.isTracking(MessageType::Extras));
    BRIDGE_DEBUG("~PipeHandler tracking squad events: {}.", m_trackedEvents.isTracking(MessageType::Squad));
    BRIDGE_DEBUG("~PipeHandler, running: {} threads: {}", m_run, m_threads.size());
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

    m_pipeMain = std::thread([handler = this](){
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
            if (auto t = handler->dispatchPipeThread(handle, threadID))
                t->start();
            else
            {
                // Unused threadID here.
                BRIDGE_WARN("Unused threadID: {}, resetting threadCounter from {} to {}.", threadID, threadCounter, threadID);
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
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_threads.size() < m_appData.Config.maxClients)
    {
        // Maybe add some error handling here in case if vector throws.
        return m_threads.emplace_back(std::make_unique<PipeThread>(id, handle, &m_trackedEvents, m_appData)).get();
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
        if (!(*it)->started())
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

void PipeHandler::sendMessage(const std::string& msg, MessageType type)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_running)
    {
        for (std::unique_ptr<PipeThread>& pt : m_threads)
            if (pt->started())
                pt->sendMessage(msg, type);
    }
}

bool PipeHandler::trackingEvent(MessageType mt) const
{
    return m_trackedEvents.isTracking(mt);
}

void TrackedEvents::startTracking(MessageType mt)
{
    switch (mt)
    {
        case MessageType::NONE:
            break;
        case MessageType::Combat:
            ++m_combat;
            break;
        case MessageType::Extras:
            ++m_extras;
            break;
        case MessageType::Squad:
            ++m_squad;
            break;
        default:
            break;
    }
}

void TrackedEvents::untrack(MessageType mt)
{
    switch (mt)
    {
        case MessageType::NONE:
            break;
        case MessageType::Combat:
            --m_combat;
            break;
        case MessageType::Extras:
            --m_extras;
            break;
        case MessageType::Squad:
            --m_squad;
            break;
        default:
            break;
    }
}

bool TrackedEvents::isTracking(MessageType mt) const
{
    bool ret = false;

    switch (mt)
    {
        case MessageType::NONE:
            break;
        case MessageType::Combat:
            ret = static_cast<bool>(m_combat);
            break;
        case MessageType::Extras:
            ret = static_cast<bool>(m_extras);
            break;
        case MessageType::Squad:
            ret = static_cast<bool>(m_squad);
            break;
        default:
            break;
    }

    return ret;
}