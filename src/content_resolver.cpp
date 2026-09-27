#include "content_resolver.hpp"

#include "llm_client.hpp"

#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

ContentResolutionResult ContentResolver::resolve(
    const string& description,
    const string& target
)
{
    ContentResolutionResult result;

    result.status =
        ContentResolutionStatus::FAILED;

    result.content = "";
    result.message = "";
    result.clarificationQuestion = "";

    if (description.empty())
    {
        result.message =
            "Cannot resolve content from empty description.";

        return result;
    }

    if (target.empty())
    {
        result.message =
            "Cannot resolve content without a target.";

        return result;
    }

    const string systemPrompt =
        R"(
You are Nova's Content Resolver.

Your job is to convert a logical content instruction into
the actual content that should be written to the specified file.

You are NOT allowed to execute anything.

You are NOT allowed to modify files.

You must return ONLY valid JSON.

For a sufficiently clear instruction, return:

{
  "status": "resolved",
  "content": "the exact content to write"
}

If the instruction is incomplete or ambiguous and you cannot
determine the intended content without inventing user intent,
return:

{
  "status": "clarification_required",
  "question": "A concise question Nova should ask the user."
}

Rules:

- Preserve the requested intent.
- Generate the actual file content, not a description of it.
- Do not include Markdown fences.
- Do not include explanations outside JSON.
- Do not provide shell commands.
- Do not execute anything.
- Do not invent unrelated functionality.
- Keep generated content minimal and appropriate to the request.
)";

    const string userPrompt =
        "Target file:\n" +
        target +
        "\n\n" +
        "Content instruction:\n" +
        description;

    LLMClient client;

    LLMResponse llmResponse =
        client.generate(
            systemPrompt,
            userPrompt
        );

    if (!llmResponse.success)
    {
        result.message =
            "Content resolution LLM request failed: " +
            llmResponse.message;

        return result;
    }

    if (llmResponse.content.empty())
    {
        result.message =
            "Content resolution LLM returned empty content.";

        return result;
    }

    try
    {
        json parsed =
            json::parse(
                llmResponse.content
            );

        if (
            !parsed.contains("status")
            ||
            !parsed["status"].is_string()
        )
        {
            result.message =
                "Content resolver response does not contain a valid status.";

            return result;
        }

        const string status =
            parsed["status"].get<string>();

        if (status == "clarification_required")
        {
            if (
                !parsed.contains("question")
                ||
                !parsed["question"].is_string()
            )
            {
                result.message =
                    "Content resolver requested clarification without providing a question.";

                return result;
            }

            result.status =
                ContentResolutionStatus::CLARIFICATION_REQUIRED;

            result.clarificationQuestion =
                parsed["question"].get<string>();

            result.message =
                "Content resolution requires clarification.";

            return result;
        }

        if (status != "resolved")
        {
            result.message =
                "Content resolver returned an unknown status.";

            return result;
        }

        if (
            !parsed.contains("content")
            ||
            !parsed["content"].is_string()
        )
        {
            result.message =
                "Content resolver returned resolved status without valid content.";

            return result;
        }

        result.content =
            parsed["content"].get<string>();

        if (result.content.empty())
        {
            result.message =
                "Content resolver returned empty content.";

            return result;
        }

        result.status =
            ContentResolutionStatus::RESOLVED;

        result.message =
            "Content resolved successfully.";

        return result;
    }
    catch (const exception& exception)
    {
        result.message =
            string("Failed to parse content resolver response: ") +
            exception.what();

        return result;
    }
}