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
    BRIDGE_DEBUG("~PipeHandler, running: {} threads: {}", m_run, m_threads.size());
}

void PipeHandler::start()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    m_pipeMain = std::thread([handler = this](){
        BRIDGE_INFO("Started PipeHandler thread");

        bool ShouldClose{false};
        std::size_t threadCounter = 0;
        handler->m_run = true;
        while (!ShouldClose)
        {
            BRIDGE_INFO("Creating NamedPipe \"{}\"", handler->m_pipeName);

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
                CloseHandle(handle);
                BRIDGE_ERROR("Error connecting pipe with err: {}!", GetLastError());
                continue;
            }

            if (!handler->m_run)
            {
                ShouldClose = true;
                continue;
            }

            BRIDGE_INFO("Client connected, starting a PipeThread instance...");

            handler->cleanup();

            if (auto t = handler->dispatchPipeThread(handle, ++threadCounter))
                t->start();
            else
                CloseHandle(handle);
        }
        handler->m_run = false;
    });
}

PipeThread* PipeHandler::dispatchPipeThread(void* handle, std::size_t id)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // Maybe add some error handling here in case if vector throws.
    return m_threads.emplace_back(std::make_unique<PipeThread>(id, handle, &m_trackedEvents, m_appData)).get();
}

void PipeHandler::cleanup()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_INFO("PipeHandler cleanup started.");

    // Remove threads that are not running.
    for (auto it = m_threads.begin(); it != m_threads.end();)
    {
        if (!(*it)->running())
        {
            BRIDGE_INFO("Removing closed PipeThread [ptid = {}].", (*it)->id());
            (*it)->stop();
            it = m_threads.erase(it);
        }
        else
            ++it;
    }

    BRIDGE_INFO("PipeHandler cleanup finished.");
}

void PipeHandler::stop()
{
    std::unique_lock<std::mutex> lock(m_mutex);

    BRIDGE_INFO("Starting to stop PipeHandler");

    if (m_run)
    {
        m_run = false;

        if (m_waitingForConnection)
        {
            // CancelSynchronousIo(PipeThread.handle);
            BRIDGE_INFO("PipeHandler thread is waiting for a connection, attempting to connect...");
            HANDLE pipe = CreateFile(m_pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
            CloseHandle(pipe);
        }
    }

    BRIDGE_INFO("Waiting for PipeHandler thread to join...");
    m_pipeMain.join();
    BRIDGE_INFO("PipeHandler thread joined!");

    if (!m_threads.empty())
    {
        // Remove all PipeThreads.
        BRIDGE_INFO("Removing all PipeThreads.");
        for (auto it = m_threads.begin(); it != m_threads.end();)
        {
            (*it)->stop();
            BRIDGE_INFO("Removing PipeThread [ptid = {}].", (*it)->id());
            it = m_threads.erase(it);
        }
    }

    BRIDGE_INFO("PipeHandler stopped.");
}

void PipeHandler::sendMessage(const std::string& msg, MessageType type)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_run)
    {
        for (std::unique_ptr<PipeThread>& pt : m_threads)
            if (pt->running())
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