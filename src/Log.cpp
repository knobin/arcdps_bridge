//
//  src/Log.cpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

// Local Headers
#include "Log.hpp"

#if !defined(BRIDGE_LOG_FILESIZE)
    #define BRIDGE_LOG_FILESIZE (10 * 1024 * 1024)
#endif

std::shared_ptr<spdlog::logger> Logger::s_logger;

void Logger::init(const std::string& filepath)
{
    spdlog::set_pattern("%^[%Y-%m-%d %T.%e] [t %t] [%n] [%l] %v%$");
    s_logger = spdlog::rotating_logger_mt("bridge", filepath, BRIDGE_LOG_FILESIZE, 3);

#if BRIDGE_LOG_LEVEL >= BRIDGE_LOG_LEVEL_DEBUG || defined(BRIDGE_BUILD_DEBUG) // 4 and above or debug build.
    s_logger->set_level(spdlog::level::debug);
    s_logger->flush_on(spdlog::level::info);
    s_logger->info("Logger started with level: {} ({}) and flush on: {} ({}).", spdlog::level::debug, "debug",
                   spdlog::level::info, "info");
#else
    s_logger->set_level(spdlog::level::info);
    s_logger->flush_on(spdlog::level::err);
    s_logger->info("Logger started with level: {} ({}) and flush on: {} ({}).", spdlog::level::info, "info",
                   spdlog::level::err, "err");
#endif
}

void Logger::destroy()
{
    s_logger->info("Logger ended.");
    s_logger->flush();
    s_logger.reset();
}

std::shared_ptr<spdlog::logger>& Logger::getLogger()
{
    return s_logger;
}
