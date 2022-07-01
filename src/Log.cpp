//
//  src/Log.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "Log.hpp"

#if !defined(BRIDGE_LOG_FILESIZE)
    #define BRIDGE_LOG_FILESIZE 10*1024*1024
#endif

Logger Logger::s_instance;
static bool valid{false};

void Logger::init(const std::string& filepath)
{
    s_instance = Logger{};
    s_instance.m_filepath = filepath;
    valid = true;
}

Logger& Logger::instance()
{
    return s_instance;
}

void Logger::writeToFile(const std::string& str) const
{
    if (!valid)
        return;

    static std::mutex WriteMutex;
    std::unique_lock<std::mutex> lock(WriteMutex);

    std::ofstream outfile; // Opens file for every call to print. Bad. but fine for debugging purposes.    
    if (m_writeCount > BRIDGE_LOG_FILESIZE)
    {
        outfile.open(m_filepath);
        m_writeCount = 0;
    }
    else
    {
        outfile.open(m_filepath, std::ios_base::app);
    }
        
    m_writeCount += (str.size() * sizeof(std::string::value_type));
    outfile << str << std::endl;
    outfile.close();
}

std::string Logger::timestamp() const
{
    std::time_t timer{std::time(0)};
    std::tm bt{};
    localtime_s(&bt, &timer);
    char time_buf[80];
    return {time_buf, std::strftime(time_buf, sizeof(time_buf), "%F %T", &bt)};
}