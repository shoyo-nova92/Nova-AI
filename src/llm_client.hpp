#pragma once

#include <string>

using namespace std;

struct LLMResponse
{
    bool success;
    string content;
    string message;
};

class LLMClient
{
public:

    LLMResponse generate(
        const string& prompt
    );

    LLMResponse generate(
        const string& systemPrompt,
        const string& userPrompt
    );
};