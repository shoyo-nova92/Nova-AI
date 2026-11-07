#pragma once

#include <filesystem>
#include <fstream>
#include <string>

using namespace std;

class Logger
{
public:
    Logger(const filesystem::path& logFile);

    bool isReady() const;

    void log(
        const string& level,
        const string& event,
        const string& message,
        const string& status
    );

private:
    ofstream logFileStream;
};