#pragma once

#include <string>

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
    PartAResult waitForText();
};