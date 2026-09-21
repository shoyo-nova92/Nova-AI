#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

using namespace std;

Logger::Logger(const filesystem::path& logFile)
{
    logFileStream.open(logFile);
}

bool Logger::isReady() const
{
    return logFileStream.is_open();
}

void Logger::log(
    const string& level,
    const string& event,
    const string& message,
    const string& status
)
{
    auto now = chrono::system_clock::now();

    time_t currentTime =
        chrono::system_clock::to_time_t(now);

    tm localTime{};

#ifdef _WIN32
    localtime_s(&localTime, &currentTime);
#else
    localtime_r(&currentTime, &localTime);
#endif

    ostringstream timestamp;

    timestamp << put_time(
        &localTime,
        "%Y-%m-%d %H:%M:%S"
    );

    string logEntry =
        "[" + timestamp.str() + "] "
        "[" + level + "] "
        "[" + event + "] "
        "[status=" + status + "] "
        + message;

    cout << logEntry << endl;

    logFileStream << logEntry << endl;
    logFileStream.flush();
}