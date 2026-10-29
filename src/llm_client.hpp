#pragma once

#include <string>

using namespace std;

enum class LLMFailureCategory { NONE, TIMEOUT, CONNECTION_ERROR, AUTH_ERROR, RATE_LIMIT,
    PROVIDER_ERROR, INVALID_RESPONSE, CONFIGURATION_ERROR, INVALID_UTF8 };
inline const char* llmFailureName(LLMFailureCategory category) {
    switch(category) {
#define LLM_CATEGORY(name) case LLMFailureCategory::name: return #name;
        LLM_CATEGORY(NONE) LLM_CATEGORY(TIMEOUT) LLM_CATEGORY(CONNECTION_ERROR)
        LLM_CATEGORY(AUTH_ERROR) LLM_CATEGORY(RATE_LIMIT) LLM_CATEGORY(PROVIDER_ERROR)
        LLM_CATEGORY(INVALID_RESPONSE) LLM_CATEGORY(CONFIGURATION_ERROR) LLM_CATEGORY(INVALID_UTF8)
#undef LLM_CATEGORY
    }
    return "UNKNOWN";
}
// Transport classification is independent of human-readable diagnostics.
LLMFailureCategory classifyLLMFailure(int curlCode, long httpStatus);
struct LLMResponse
{
    bool success = false;
    string content;
    string message;
    LLMFailureCategory failureCategory = LLMFailureCategory::NONE;
    long httpStatus = 0;
};

class LLMClient
{
public:

    LLMResponse generate(
        const string& prompt
    );

    LLMResponse generate(
        const string& systemPrompt,
        const string& userPrompt,
        const string& role = "general"
    );
};
