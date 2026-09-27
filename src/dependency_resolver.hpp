#pragma once

#include "multistep_plan.hpp"

#include <string>
#include <vector>

using namespace std;

struct DependencyResolutionResult
{
    bool valid;

    vector<string> executionOrder;

    string message;
};

class DependencyResolver
{
public:
    DependencyResolutionResult resolve(
        const MultiStepPlan& plan
    );
};