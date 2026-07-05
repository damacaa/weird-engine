#pragma once

#include <string>
#include <vector>
#include <mutex>

namespace WeirdEngine
{
    enum class LogLevel
    {
        Info,
        Warning,
        Error
    };

    struct LogMessage
    {
        LogLevel level;
        std::string message;
    };

    class Logger
    {
    public:
        static void log(const std::string& message);
        static void warning(const std::string& message);
        static void error(const std::string& message);

        static bool s_enableConsoleOutput;
        static void drawImGuiConsole();

    private:
        static std::vector<LogMessage> s_messages;
        static std::mutex s_mutex;
    };
}
