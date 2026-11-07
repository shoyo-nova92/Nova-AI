#pragma once

#include "input_event.hpp"

class KeyboardInput
{
public:
    InputEvent waitForInput();
};