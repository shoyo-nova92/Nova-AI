#pragma once

#include "multistep_plan.hpp"
#include <string>

using namespace std;

class GoalDecomposer
{
public:
    MultiStepPlan decompose(const string& goal);
};