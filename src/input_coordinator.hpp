#pragma once

#include "input_event.hpp"

class InputCoordinator
{
public:
    InputEvent waitForInput();
};