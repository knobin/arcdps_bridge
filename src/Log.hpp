//
//  src/Log.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_LOG_HPP
#define BRIDGE_LOG_HPP

//
// No log    -> level 0
// Error     -> level 1
// Warning   -> level 2
// Info      -> level 3
// Debug     -> level 4
// Msg Debug -> level 5 (Prints messages and info between client and server.)
//

#define BRIDGE_LOG_LEVEL_ERROR 1
#define BRIDGE_LOG_LEVEL_WARNING 2
#define BRIDGE_LOG_LEVEL_INFO 3
#define BRIDGE_LOG_LEVEL_DEBUG 4
#define BRIDGE_LOG_LEVEL_MSG_DEBUG 5

// Standard log output (should be defined in CMakeLists.txt).
#if !defined(BRIDGE_LOG_LEVEL) 
    #ifdef BRIDGE_BUILD_DEBUG
        #define BRIDGE_LOG_LEVEL BRIDGE_LOG_LEVEL_DEBUG
    #else
        #define BRIDGE_LOG_LEVEL BRIDGE_LOG_LEVEL_INFO
    #endif
#endif

#if BRIDGE_LOG_LEVEL > 0 // 1 and above.

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
        static void destroy();

        static std::shared_ptr<spdlog::logger>& getLogger();

    private:
        static std::shared_ptr<spdlog::logger> s_logger;
    };

    #define BRIDGE_LOG_INIT(...) Logger::init(__VA_ARGS__)
    #define BRIDGE_LOG_DESTROY(...) Logger::destroy()
#else
    #define BRIDGE_LOG_INIT(...)
    #define BRIDGE_LOG_DESTROY(...)
    
#endif

#if BRIDGE_LOG_LEVEL > 0 // 1 and above.
    #define BRIDGE_ERROR(...) Logger::getLogger()->error(__VA_ARGS__)
#else
    #define BRIDGE_ERROR(...)
#endif

#if BRIDGE_LOG_LEVEL > 1 // 2 and above.
    #define BRIDGE_WARN(...) Logger::getLogger()->warn(__VA_ARGS__)
#else
    #define BRIDGE_WARN(...)
#endif

#if BRIDGE_LOG_LEVEL > 2 // 3 and above.
    #define BRIDGE_INFO(...) Logger::getLogger()->info(__VA_ARGS__)
#else
    #define BRIDGE_INFO(...)
#endif

#if BRIDGE_LOG_LEVEL > 3 // 4 and above.
    #define BRIDGE_DEBUG(...) Logger::getLogger()->debug(__VA_ARGS__)
#else
    #define BRIDGE_DEBUG(...)
#endif

#if BRIDGE_LOG_LEVEL > 4 // 5 and above.
    #define BRIDGE_MSG_DEBUG(...) Logger::getLogger()->debug(__VA_ARGS__)
#else
    #define BRIDGE_MSG_DEBUG(...)
#endif

#endif // BRIDGE_LOG_HPP