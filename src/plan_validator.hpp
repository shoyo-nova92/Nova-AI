#pragma once

#include "multistep_plan.hpp"

#include <string>

using namespace std;

enum class PlanValidationStatus
{
    VALID,
    INVALID
};

struct PlanValidationResult
{
    PlanValidationStatus status;
    string message;
};

class PlanValidator
{
public:
    PlanValidationResult validate(
        const MultiStepPlan& plan
    );
};