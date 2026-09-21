#pragma once

#include <string>

using namespace std;

enum class InputSource
{
    SPEECH,
    KEYBOARD
};

struct InputEvent
{
    InputSource source;
    string rawText;
};