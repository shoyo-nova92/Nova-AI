#include "action_primitive_resolver.hpp"
#include "llm_client.hpp"
#include "primitive_registry.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <sstream>

using namespace std;
using json = nlohmann::json;

namespace
{
    string toLower(string text)
    {
        transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(
                    tolower(character)
                );
            }
        );

        return text;
    }

    bool contains(
        const string& text,
        const string& value
    )
    {
        return text.find(value) != string::npos;
    }

    PrimitiveOperation makePrimitive(
        PrimitiveOperationType type,
        const string& name
    )
    {
        PrimitiveOperation operation;

        operation.type = type;
        operation.name = name;
        operation.parameters =
        PrimitiveParameters{};
        operation.resolved = true;

        return operation;
    }

    string buildPrimitiveList()
    {
        const vector<string> primitives =
            PrimitiveRegistry::getSupportedNames();

        ostringstream output;

        for (const auto& primitive : primitives)
        {
            output << "- "
                   << primitive
                   << "\n";
        }

        return output.str();
    }

    bool addValidatedPrimitive(
        MultiStepAction& action,
        const string& primitiveName,
        string& errorMessage
    )
    {
        PrimitiveOperationType type;

        if (!PrimitiveRegistry::resolveType(
                primitiveName,
                type))
        {
            errorMessage =
                "LLM proposed unsupported primitive: " +
                primitiveName;

            return false;
        }

        action.operations.push_back(
            makePrimitive(
                type,
                primitiveName
            )
        );

        return true;
    }
}

ActionPrimitiveResolutionResult
ActionPrimitiveResolver::resolve(
    const MultiStepAction& action
)
{
    ActionPrimitiveResolutionResult result;

    result.status =
        ActionPrimitiveResolutionStatus::FAILED;

    result.action = action;
    result.message = "";
    result.clarificationQuestion = "";

    // --------------------------------------------------
    // INPUT VALIDATION
    // --------------------------------------------------

    if (action.id.empty())
    {
        result.message =
            "Cannot resolve action without an ID.";

        return result;
    }

    if (action.description.empty())
    {
        result.message =
            "Cannot resolve action without a description.";

        return result;
    }

    const string text =
        toLower(action.description);

    MultiStepAction resolvedAction =
        action;

    resolvedAction.operations.clear();
    // --------------------------------------------------
    // DETERMINISTIC: FILESYSTEM CREATE
    // --------------------------------------------------

    if (
        contains(text, "create")
        ||
        contains(text, "add")
    )
    {
        size_t createPosition =
            text.find("create");

        if (createPosition == string::npos)
        {
            createPosition =
                text.find("add");
        }

        size_t directoryPosition =
            text.find(
                " directory",
                createPosition
            );

        size_t folderPosition =
            text.find(
                " folder",
                createPosition
            );

        size_t filePosition =
            text.find(
                " file",
                createPosition
            );

        size_t javaPosition =
            text.find(
                ".java",
                createPosition
            );

        size_t cppPosition =
            text.find(
                ".cpp",
                createPosition
            );

        size_t pythonPosition =
            text.find(
                ".py",
                createPosition
            );

        size_t txtPosition =
            text.find(
                ".txt",
                createPosition
            );

        // --------------------------------------------------
        // Determine which object appears first after "create"
        // --------------------------------------------------

        size_t directoryObjectPosition =
            string::npos;

        if (directoryPosition != string::npos)
        {
            directoryObjectPosition =
                directoryPosition;
        }

        if (
            folderPosition != string::npos
            &&
            (
                directoryObjectPosition ==
                string::npos
                ||
                folderPosition <
                directoryObjectPosition
            )
        )
        {
            directoryObjectPosition =
                folderPosition;
        }

        size_t fileObjectPosition =
            string::npos;

        if (filePosition != string::npos)
        {
            fileObjectPosition =
                filePosition;
        }

        const size_t extensionPositions[] =
        {
            javaPosition,
            cppPosition,
            pythonPosition,
            txtPosition
        };

        for (size_t position : extensionPositions)
        {
            if (
                position != string::npos
                &&
                (
                    fileObjectPosition ==
                    string::npos
                    ||
                    position <
                    fileObjectPosition
                )
            )
            {
                fileObjectPosition =
                    position;
            }
        }

        // --------------------------------------------------
        // CREATE DIRECTORY / FOLDER
        // --------------------------------------------------

        if (
            directoryObjectPosition !=
            string::npos
            &&
            (
                fileObjectPosition ==
                string::npos
                ||
                directoryObjectPosition <
                fileObjectPosition
            )
        )
        {
            resolvedAction.operations.push_back(
                makePrimitive(
                    PrimitiveOperationType::FILESYSTEM_CREATE,
                    "filesystem.create"
                )
            );

            resolvedAction.operation =
                "filesystem.create";

            resolvedAction.capability =
                "filesystem";

            resolvedAction.resolved =
                true;

            result.status =
                ActionPrimitiveResolutionStatus::RESOLVED;

            result.action =
                resolvedAction;

            result.message =
                "Resolved deterministically to filesystem.create.";

            return result;
        }

        // --------------------------------------------------
        // CREATE FILE
        // --------------------------------------------------

        if (
            fileObjectPosition !=
            string::npos
        )
        {
            resolvedAction.operations.push_back(
                makePrimitive(
                    PrimitiveOperationType::FILESYSTEM_CREATE_FILE,
                    "filesystem.create_file"
                )
            );

            resolvedAction.operation =
                "filesystem.create_file";

            resolvedAction.capability =
                "filesystem";

            resolvedAction.resolved =
                true;

            result.status =
                ActionPrimitiveResolutionStatus::RESOLVED;

            result.action =
                resolvedAction;

            result.message =
                "Resolved deterministically to filesystem.create_file.";

            return result;
        }
    }

    
    // --------------------------------------------------
    // DETERMINISTIC: APPLICATION LAUNCH
    // --------------------------------------------------

    if (
        contains(text, "open")
        ||
        contains(text, "launch")
        ||
        contains(text, "start")
    )
    {
        resolvedAction.operations.push_back(
            makePrimitive(
                PrimitiveOperationType::APPLICATION_LAUNCH,
                "application.launch"
            )
        );

        resolvedAction.operation =
            "application.launch";

        resolvedAction.capability =
            "application_control";

        resolvedAction.resolved = true;

        result.status =
            ActionPrimitiveResolutionStatus::RESOLVED;

        result.action =
            resolvedAction;

        result.message =
            "Resolved deterministically to application.launch.";

        return result;
    }

    // --------------------------------------------------
    // DETERMINISTIC: TEXT WRITE
    // --------------------------------------------------

    if (
        contains(text, "write")
        ||
        contains(text, "type")
        ||
        contains(text, "enter")
    )
    {
        resolvedAction.operations.push_back(
            makePrimitive(
                PrimitiveOperationType::TEXT_WRITE,
                "text.write"
            )
        );

        resolvedAction.operation =
            "text.write";

        resolvedAction.capability =
            "text_input";

        resolvedAction.resolved = true;

        result.status =
            ActionPrimitiveResolutionStatus::RESOLVED;

        result.action =
            resolvedAction;

        result.message =
            "Resolved deterministically to text.write.";

        return result;
    }

    // --------------------------------------------------
    // LLM FALLBACK
    // --------------------------------------------------

    const string systemPrompt =
        R"(
You are Nova's Action / Primitive Resolver.

Your job is to convert ONE logical action step into
one or more primitive operations.

You are NOT allowed to execute anything.

You are NOT allowed to invent primitives.

You may ONLY use primitives from the supplied registry.

CRITICAL RULE:

If the logical step is incomplete, ambiguous, or does not
contain enough information to determine the required primitive
operations without inventing missing facts, DO NOT guess.

Instead return:

{
  "status": "clarification_required",
  "question": "A concise question that Nova should ask the user."
}

For a sufficiently clear step, return:

{
  "status": "resolved",
  "operations": [
    {
      "name": "primitive.name"
    }
  ]
}

Rules:

- Use the smallest meaningful primitive sequence.
- Do not add redundant operations.
- Do not repeat the same primitive unless the logical step
  genuinely requires multiple uses.
- Preserve the meaning of the logical step.
- Do not resolve concrete filesystem paths.
- Do not invent application names.
- Do not provide shell commands.
- Do not provide explanations outside JSON.
- Every operation name MUST come from the supplied primitive registry.
- If information is missing, ask for clarification instead of guessing.
)";

    const string userPrompt =
        "Logical step:\n\n" +
        action.description +
        "\n\n" +
        "Available primitives:\n" +
        buildPrimitiveList();

    LLMClient client;

    LLMResponse llmResponse =
        client.generate(
            systemPrompt,
            userPrompt
        );

    if (!llmResponse.success)
    {
        result.message =
            "Primitive resolution LLM request failed: " +
            llmResponse.message;

        return result;
    }

    if (llmResponse.content.empty())
    {
        result.message =
            "Primitive resolution LLM returned empty content.";

        return result;
    }

    // --------------------------------------------------
    // PARSE LLM RESPONSE
    // --------------------------------------------------

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
                "LLM primitive response does not contain a valid status.";

            return result;
        }

        const string status =
            parsed["status"].get<string>();

        // --------------------------------------------------
        // CLARIFICATION REQUIRED
        // --------------------------------------------------

        if (status == "clarification_required")
        {
            if (
                !parsed.contains("question")
                ||
                !parsed["question"].is_string()
            )
            {
                result.message =
                    "LLM requested clarification without providing a question.";

                return result;
            }

            result.status =
                ActionPrimitiveResolutionStatus::CLARIFICATION_REQUIRED;

            result.clarificationQuestion =
                parsed["question"].get<string>();

            result.message =
                "Action requires user clarification.";

            return result;
        }

        // --------------------------------------------------
        // RESOLVED
        // --------------------------------------------------

        if (status != "resolved")
        {
            result.message =
                "LLM returned an unsupported resolution status: " +
                status;

            return result;
        }

        if (
            !parsed.contains("operations")
            ||
            !parsed["operations"].is_array()
        )
        {
            result.message =
                "Resolved LLM response does not contain a valid operations array.";

            return result;
        }

        if (parsed["operations"].empty())
        {
            result.message =
                "Resolved LLM response contains no operations.";

            return result;
        }

        for (
            const auto& operationJson :
            parsed["operations"]
        )
        {
            if (
                !operationJson.contains("name")
                ||
                !operationJson["name"].is_string()
            )
            {
                result.message =
                    "LLM primitive response contains an operation without a valid name.";

                result.action.operations.clear();

                return result;
            }

            const string primitiveName =
                operationJson["name"].get<string>();

            string validationError;

            if (!addValidatedPrimitive(
                    resolvedAction,
                    primitiveName,
                    validationError))
            {
                result.message =
                    validationError;

                result.action.operations.clear();

                return result;
            }
        }

        resolvedAction.resolved = true;

        result.status =
            ActionPrimitiveResolutionStatus::RESOLVED;

        result.action =
            resolvedAction;

        result.message =
            "Resolved through focused LLM primitive composition.";

        return result;
    }
    catch (const json::exception& error)
    {
        result.message =
            string(
                "Invalid JSON from primitive resolver LLM: "
            ) +
            error.what();

        result.action.operations.clear();

        return result;
    }
}