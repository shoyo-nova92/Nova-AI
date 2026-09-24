#include "input_normalizer.hpp"
#include "llm_client.hpp"

#include <nlohmann/json.hpp>

#include <string>

using namespace std;
using json = nlohmann::json;

NormalizedInput InputNormalizer::normalize(
    const string& input
)
{
    NormalizedInput result;

    result.originalText = input;
    result.normalizedText = "";
    result.success = false;
    result.message = "";

    if (input.empty())
    {
        result.message = "Cannot normalize empty input.";
        return result;
    }

    const string systemPrompt = R"(
You are Nova's Input Normalizer.

Your ONLY responsibility is to clean and normalize the user's input.

Do NOT:
- classify the request
- determine the user's intent
- select an action
- determine capabilities
- resolve parameters
- create an execution plan
- execute anything
- add information that was not present
- answer the user

You MUST:
- remove filler words and speech disfluencies
- remove unnecessary politeness and conversational padding
- remove "Nova" when it is only being used to address the assistant
- preserve the user's actual meaning
- preserve names
- preserve application names
- preserve file paths
- preserve numbers
- preserve quoted strings
- preserve requested operations
- preserve meaningful capitalization
- preserve all information required by later processing

Example:

Input:
"uhh Nova, can you like create a folder called Nova on my desktop"

Normalized:
"create a folder called Nova on my desktop"

If the input is already clean, return it without unnecessary changes.

Return ONLY valid JSON.

The JSON format MUST be exactly:

{
  "normalized_text": "..."
}

Do not return markdown.
Do not return code fences.
Do not return explanations.
Do not return any text outside the JSON object.
)";

    const string userPrompt =
        "Normalize this input:\n" + input;

    LLMClient llmClient;

    LLMResponse response =
        llmClient.generate(
            systemPrompt,
            userPrompt
        );

    if (!response.success)
    {
        result.message =
            "Input normalization LLM failed: "
            + response.message;

        return result;
    }

    try
    {
        json parsed =
            json::parse(response.content);

        if (!parsed.contains("normalized_text"))
        {
            result.message =
                "LLM response missing normalized_text.";

            return result;
        }

        if (!parsed["normalized_text"].is_string())
        {
            result.message =
                "LLM normalized_text is not a string.";

            return result;
        }

        result.normalizedText =
            parsed["normalized_text"].get<string>();

        if (result.normalizedText.empty())
        {
            result.message =
                "LLM returned empty normalized text.";

            return result;
        }

        result.success = true;
        result.message =
            "Input normalized successfully.";

        return result;
    }
    catch (const exception& exception)
    {
        result.message =
            "Failed to parse LLM normalization response: "
            + string(exception.what());

        return result;
    }
}