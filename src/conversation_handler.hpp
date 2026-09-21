#pragma once

#include "llm_client.hpp"

#include <string>

using namespace std;

class ConversationHandler
{
public:
    LLMResponse handle(
        const string& input
    );
};