//
//  src/Log.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "Log.hpp"

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
    outfile.open(m_filepath, std::ios_base::app);
    outfile << str << std::endl;
    outfile.close();
} 