//
//  Log.hpp
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

    private:
        void writeToFile(const std::string& str) const;

    private:
        std::string m_filepath{};
        static Logger s_instance;
    };

    template<typename... Args>
    inline void Logger::info(Args&&... args) const
    {
        std::time_t timer{std::time(0)};
        std::tm bt{};
        localtime_s(&bt, &timer);
        char time_buf[80];
        std::string time_str{time_buf, std::strftime(time_buf, sizeof(time_buf), "%F %T", &bt)};

        std::ostringstream oss{};
        oss << "[" << time_str << "] ";
        (oss << ... << args);
        writeToFile(oss.str());
    }

    #define BRIDGE_LOG_INIT(...) Logger::init(__VA_ARGS__)
    #define BRIDGE_INFO(...) Logger::instance().info(__VA_ARGS__)
    
    #ifdef BRIDGE_MSG_LOG
        #define BRIDGE_MSG_INFO(...) Logger::instance().info(__VA_ARGS__)
    #else
        #define BRIDGE_MSG_INFO(...)
    #endif

#else
    #define BRIDGE_LOG_INIT(...)
    #define BRIDGE_INFO(...)
    #define BRIDGE_MSG_INFO(...)
#endif

#endif // BRIDGE_LOG_HPP