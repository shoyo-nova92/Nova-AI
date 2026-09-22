#pragma once

#include "action.hpp"
#include "capability.hpp"

class CapabilityResolver
{
public:
    Capability resolve(
        const Action& action
    );
};