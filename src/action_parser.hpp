#pragma once

#include "action.hpp"

#include <string>

using namespace std;

struct ActionParserResult
{
    bool success;
    Action action;
    string message;
};

class ActionParser
{
public:
    ActionParserResult parse(
        const string& input
    );
};