#pragma once

#include <string>
#include <functional>

using namespace std;

struct PartAResult
{
    bool success;
    bool keyboardRequest;
    string rawText;
    string message;
};

class PartABridge
{
public:
    using WakeGreeting=std::function<void(const std::function<bool()>& cancelled)>;
    PartAResult waitForText(WakeGreeting greeting={});
};
