#include "request_classifier.hpp"
#include "llm_client.hpp"

#include <nlohmann/json.hpp>

#include <string>

using namespace std;
using json = nlohmann::json;

namespace
{
    string requestTypeToString(RequestType type)
    {
        switch (type)
        {
            case RequestType::CONVERSATION:
                return "conversation";

            case RequestType::SINGLE_ACTION:
                return "single_action";

            case RequestType::MULTI_STEP_GOAL:
                return "multi_step_goal";

            case RequestType::EXISTING_WORKFLOW:
                return "existing_workflow";

            case RequestType::CLARIFICATION_REQUIRED:
                return "clarification_required";

            case RequestType::UNSUPPORTED:
                return "unsupported";
        }

        return "";
    }

    bool stringToRequestType(
        const string& value,
        RequestType& type
    )
    {
        if (value == "conversation")
        {
            type = RequestType::CONVERSATION;
            return true;
        }

        if (value == "single_action")
        {
            type = RequestType::SINGLE_ACTION;
            return true;
        }

        if (value == "multi_step_goal")
        {
            type = RequestType::MULTI_STEP_GOAL;
            return true;
        }

        if (value == "existing_workflow")
        {
            type = RequestType::EXISTING_WORKFLOW;
            return true;
        }

        if (value == "clarification_required")
        {
            type = RequestType::CLARIFICATION_REQUIRED;
            return true;
        }

        if (value == "unsupported")
        {
            type = RequestType::UNSUPPORTED;
            return true;
        }

        return false;
    }
}

ClassificationResult RequestClassifier::classify(
    const string& input
)
{
    ClassificationResult result;

    result.type = RequestType::CLARIFICATION_REQUIRED;
    result.success = false;
    result.message = "";

    if (input.empty())
    {
        result.message =
            "Cannot classify empty input.";

        return result;
    }

    const string systemPrompt = R"(
You are Nova's Root Router.

Your ONLY responsibility is to determine which processing branch should handle the user's request.

You MUST choose exactly ONE of these classifications:

1. conversation
   General questions, explanations, knowledge requests, opinions,
   casual conversation, or requests that do not require Nova to perform
   an external computer action.

2. single_action
   One clearly identifiable action that Nova can perform as one operation.

3. multi_step_goal
   A goal requiring multiple dependent actions or multiple operations.

4. existing_workflow
   A request to start, run, or invoke an already-defined Nova workflow.

5. clarification_required
   The user's request is ambiguous, incomplete, or missing information
   required to determine the correct processing branch.

6. unsupported
   The user explicitly requests functionality that Nova does not support
   or cannot handle through its available capabilities.

Examples:

"What is inheritance in Java?"
-> conversation

"Create a folder called Nova on my desktop."
-> single_action

"Open Chrome and search for Python tutorials."
-> multi_step_goal

"Start my morning routine."
-> existing_workflow

"Open that file."
-> clarification_required

"Control an application Nova has no integration for."
-> unsupported

IMPORTANT:

You are ONLY selecting the root branch.

Do NOT:
- parse the requested action
- identify capabilities
- extract parameters
- create an execution plan
- decide how the request should be executed
- perform the request
- answer the user
- explain your classification

Do not invent missing information.

Return ONLY valid JSON.

The JSON format MUST be exactly:

{
  "classification": "..."
}

The classification value MUST be exactly one of:

"conversation"
"single_action"
"multi_step_goal"
"existing_workflow"
"clarification_required"
"unsupported"

Do not return markdown.
Do not return code fences.
Do not return explanations.
Do not return any text outside the JSON object.
)";

    const string userPrompt =
        "Classify this request:\n" + input;

    LLMClient llmClient;

    LLMResponse response =
        llmClient.generate(
            systemPrompt,
            userPrompt
        );

    if (!response.success)
    {
        result.message =
            "Root Router LLM failed: "
            + response.message;

        return result;
    }

    try
    {
        json parsed =
            json::parse(response.content);

        if (!parsed.contains("classification"))
        {
            result.message =
                "LLM response missing classification.";

            return result;
        }

        if (!parsed["classification"].is_string())
        {
            result.message =
                "LLM classification is not a string.";

            return result;
        }

        string classification =
            parsed["classification"].get<string>();

        RequestType requestType;

        if (!stringToRequestType(
                classification,
                requestType
            ))
        {
            result.message =
                "LLM returned unknown classification: "
                + classification;

            return result;
        }

        result.type = requestType;
        result.success = true;
        result.message =
            "Request classified successfully.";

        return result;
    }
    catch (const exception& exception)
    {
        result.message =
            "Failed to parse Root Router response: "
            + string(exception.what());

        return result;
    }
}