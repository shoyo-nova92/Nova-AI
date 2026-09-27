#pragma once

#include <string>

using namespace std;

enum class ContentResolutionStatus
{
    RESOLVED,
    CLARIFICATION_REQUIRED,
    FAILED
};

struct ContentResolutionResult
{
    ContentResolutionStatus status;

    string content;

    string message;

    string clarificationQuestion;
};

class ContentResolver
{
public:

    ContentResolutionResult resolve(
        const string& description,
        const string& target
    );
};