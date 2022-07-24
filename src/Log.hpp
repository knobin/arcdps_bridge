//
//  src/Log.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_LOG_HPP
#define BRIDGE_LOG_HPP

//
// No log  -> level 0
// Info    -> level 1
// Warning -> level 2
// Error   -> level 3
// Debug   -> level 4
//

// Standard log output.
#if !defined(BRIDGE_DEBUG_LEVEL) // Should be defined in CMakeLists.txt.
    #define BRIDGE_DEBUG_LEVEL 3 // Includes: info, warn and error.
#endif

#if BRIDGE_DEBUG_LEVEL > 0 // 1 and above.

    // spdlog Headers
    #if defined(_MSC_VER)
        __pragma(warning(push, 0))
    #endif
        #include "spdlog/spdlog.h"
        #include "spdlog/sinks/rotating_file_sink.h"
    #if defined(_MSC_VER)
        __pragma(warning(pop))
    #endif

    // C++ Headers
    #include <memory>

    class Logger
    {
    public:
        static void init(const std::string& filepath);

        static std::shared_ptr<spdlog::logger>& getLogger();

    private:
        static std::shared_ptr<spdlog::logger> s_logger;
    };

    #define BRIDGE_LOG_INIT(...) Logger::init(__VA_ARGS__)
    #define BRIDGE_INFO(...) Logger::getLogger()->info(__VA_ARGS__)
    #ifdef BRIDGE_MSG_LOG
        #define BRIDGE_MSG_INFO(...) Logger::getLogger()->info(__VA_ARGS__)
    #else
        #define BRIDGE_MSG_INFO(...)
    #endif
#else
    #define BRIDGE_LOG_INIT(...)
    #define BRIDGE_INFO(...)
    #define BRIDGE_MSG_INFO(...)
#endif

#if BRIDGE_DEBUG_LEVEL > 1 // 2 and above.
    #define BRIDGE_WARN(...) Logger::getLogger()->warn(__VA_ARGS__)
#else
    #define BRIDGE_WARN(...)
#endif

#if BRIDGE_DEBUG_LEVEL > 2 // 3 and above.
    #define BRIDGE_ERROR(...) Logger::getLogger()->error(__VA_ARGS__)
#else
    #define BRIDGE_ERROR(...)
#endif

#if BRIDGE_DEBUG_LEVEL > 3 // 4 and above.
    #define BRIDGE_DEBUG(...) Logger::getLogger()->debug(__VA_ARGS__)
#else
    #define BRIDGE_DEBUG(...)
#endif

#endif // BRIDGE_LOG_HPP