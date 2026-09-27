#pragma once

#include "action.hpp"
#include "capability.hpp"
#include "primitive_operation.hpp"

class CapabilityResolver
{
public:

    // Branch 2
    Capability resolve(
        const Action& action
    );

    // Branch 3
    Capability resolve(
        const PrimitiveOperation& operation
    );
};