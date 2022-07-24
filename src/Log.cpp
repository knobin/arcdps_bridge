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

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init(const std::string& filepath)
{
    spdlog::set_pattern("%^[%Y-%m-%d %T] [%n] [%l] %v%$");
    s_logger = spdlog::rotating_logger_mt("bridge", filepath, BRIDGE_LOG_FILESIZE, 3);
    s_logger->set_level(spdlog::level::debug);
    s_logger->flush_on(spdlog::level::info);
}
    
std::shared_ptr<spdlog::logger>& Logger::getLogger()
{
    return s_logger;
}
