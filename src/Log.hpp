//
//  src/Log.hpp
//  ArcDPS Bridge
//
//  Created by Robin Gustafsson on 2022-06-21.
//

#ifndef BRIDGE_LOG_HPP
#define BRIDGE_LOG_HPP

#ifdef BRIDGE_DEBUG

    // C++ Headers
    #include <mutex>
    #include <fstream>
    #include <string>
    #include <ostream>

    class Logger
    {
    public:
        static void init(const std::string& filepath);
        static Logger& instance();

        template<typename... Args>
        void info(Args&&... args) const;

        template<typename... Args>
        void warn(Args&&... args) const;

        template<typename... Args>
        void error(Args&&... args) const;

    private:
        void writeToFile(const std::string& str) const;

        std::string timestamp() const;

        template<typename... Args>
        void log(const std::string& type, Args&&... args) const;
    private:
        std::string m_filepath{};
        mutable std::size_t m_writeCount{0};
        static Logger s_instance;
    };

    template<typename... Args>
    inline void Logger::log(const std::string& type, Args&&... args) const
    {
        std::ostringstream oss{};
        oss << "[" << timestamp() << "] [" << type << "] ";
        (oss << ... << args);
        writeToFile(oss.str());
    }

    template<typename... Args>
    inline void Logger::info(Args&&... args) const
    {
        log("info", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Logger::warn(Args&&... args) const
    {
        log("warn", std::forward<Args>(args)...);
    }

    template<typename... Args>
    inline void Logger::error(Args&&... args) const
    {
        log("error", std::forward<Args>(args)...);
    }

    #define BRIDGE_LOG_INIT(...) Logger::init(__VA_ARGS__)
    #define BRIDGE_INFO(...) Logger::instance().info(__VA_ARGS__)
    #define BRIDGE_WARN(...) Logger::instance().warn(__VA_ARGS__)
    #define BRIDGE_ERROR(...) Logger::instance().error(__VA_ARGS__)
    
    #ifdef BRIDGE_MSG_LOG
        #define BRIDGE_MSG_INFO(...) Logger::instance().info(__VA_ARGS__)
    #else
        #define BRIDGE_MSG_INFO(...)
    #endif

#else
    #define BRIDGE_LOG_INIT(...)
    #define BRIDGE_INFO(...)
    #define BRIDGE_WARN(...)
    #define BRIDGE_ERROR(...)
    #define BRIDGE_MSG_INFO(...)
#endif

#endif // BRIDGE_LOG_HPP