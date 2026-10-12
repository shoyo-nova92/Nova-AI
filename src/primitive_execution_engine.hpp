#pragma once

#include "primitive_operation.hpp"
#include "execution_result.hpp"

class PrimitiveExecutionEngine
{
public:
    ExecutionResult execute(
        const PrimitiveOperation& operation
    );
};