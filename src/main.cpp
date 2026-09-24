#include "input_normalizer.hpp"
#include "logger.hpp"
#include "input_coordinator.hpp"
#include "request_classifier.hpp"
#include "conversation_handler.hpp"
#include "single_action_pipeline.hpp"

#include <filesystem>
#include <iostream>
#include <string>

using namespace std;

namespace
{
    string requestTypeToString(RequestType type)
    {
        switch (type)
        {
            case RequestType::CONVERSATION:
                return "CONVERSATION";

            case RequestType::SINGLE_ACTION:
                return "SINGLE_ACTION";

            case RequestType::MULTI_STEP_GOAL:
                return "MULTI_STEP_GOAL";

            case RequestType::EXISTING_WORKFLOW:
                return "EXISTING_WORKFLOW";

            case RequestType::CLARIFICATION_REQUIRED:
                return "CLARIFICATION_REQUIRED";

            case RequestType::UNSUPPORTED:
                return "UNSUPPORTED";
        }

        return "UNKNOWN";
    }
}

int main()
{
    namespace fs = filesystem;

    fs::create_directories("logs");

    Logger logger(
        "logs/runtime.log"
    );

    if (!logger.isReady())
    {
        cerr
            << "[ERROR] Logger initialization failed."
            << endl;

        return 1;
    }

    logger.log(
        "INFO",
        "RUNTIME_START",
        "Nova runtime started.",
        "SUCCESS"
    );

    // --------------------------------------------------------
    // UNIFIED INPUT
    // --------------------------------------------------------

    logger.log(
        "INFO",
        "INPUT_COORDINATOR_START",
        "Starting unified input coordinator.",
        "STARTED"
    );

    InputCoordinator inputCoordinator;

    InputEvent inputEvent =
        inputCoordinator.waitForInput();

    if (inputEvent.rawText.empty())
    {
        logger.log(
            "ERROR",
            "INPUT_FAILURE",
            "Input coordinator returned empty input.",
            "FAILURE"
        );

        cerr
            << "[INPUT FAILURE] "
            << "No input received."
            << endl;

        return 1;
    }

    // --------------------------------------------------------
    // INPUT SOURCE
    // --------------------------------------------------------

    string sourceName;

    if (
        inputEvent.source ==
        InputSource::SPEECH
    )
    {
        sourceName = "SPEECH";
    }
    else
    {
        sourceName = "KEYBOARD";
    }

    logger.log(
        "INFO",
        "INPUT_SOURCE",
        "Input source: " + sourceName,
        "SUCCESS"
    );

    // --------------------------------------------------------
    // UNIFIED RAW INPUT
    // --------------------------------------------------------

    cout << endl;
    cout << "[RAW TEXT]" << endl;
    cout << inputEvent.rawText << endl;

    logger.log(
        "INFO",
        "INPUT_READY",
        "Unified input received: "
            + inputEvent.rawText,
        "SUCCESS"
    );

    // --------------------------------------------------------
    // PART B — LLM INPUT NORMALIZATION
    // --------------------------------------------------------

    logger.log(
        "INFO",
        "INPUT_NORMALIZATION",
        "Starting LLM input normalization.",
        "STARTED"
    );

    InputNormalizer normalizer;

    NormalizedInput normalized =
        normalizer.normalize(
            inputEvent.rawText
        );

    if (!normalized.success)
    {
        logger.log(
            "ERROR",
            "INPUT_NORMALIZATION",
            normalized.message,
            "FAILURE"
        );

        cerr
            << "[NORMALIZATION FAILURE] "
            << normalized.message
            << endl;

        return 1;
    }

    cout << endl;
    cout << "[NORMALIZED TEXT]" << endl;
    cout << normalized.normalizedText << endl;

    logger.log(
        "INFO",
        "INPUT_NORMALIZED",
        "LLM normalized input: "
            + normalized.normalizedText,
        "SUCCESS"
    );

    // --------------------------------------------------------
    // PART B — LLM ROOT ROUTER
    // --------------------------------------------------------

    logger.log(
        "INFO",
        "ROOT_ROUTER",
        "Starting LLM root routing.",
        "STARTED"
    );

    RequestClassifier classifier;

    ClassificationResult classification =
        classifier.classify(
            normalized.normalizedText
        );

    if (!classification.success)
    {
        logger.log(
            "ERROR",
            "ROOT_ROUTER",
            classification.message,
            "FAILURE"
        );

        cerr
            << "[ROOT ROUTER FAILURE] "
            << classification.message
            << endl;

        return 1;
    }

    string requestType =
        requestTypeToString(
            classification.type
        );

    cout << endl;
    cout << "[REQUEST TYPE]" << endl;
    cout << requestType << endl;

    logger.log(
        "INFO",
        "ROOT_ROUTER",
        "Request classified as: "
            + requestType,
        "SUCCESS"
    );

    // --------------------------------------------------------
    // BRANCH 1 — CONVERSATION
    // --------------------------------------------------------

    if (
        classification.type ==
        RequestType::CONVERSATION
    )
    {
        logger.log(
            "INFO",
            "CONVERSATION_HANDLER",
            "Routing request to Conversation Handler.",
            "STARTED"
        );

        ConversationHandler conversationHandler;

        LLMResponse llmResponse =
            conversationHandler.handle(
                normalized.normalizedText
            );

        if (!llmResponse.success)
        {
            logger.log(
                "ERROR",
                "CONVERSATION_HANDLER",
                llmResponse.message,
                "FAILURE"
            );

            cerr
                << "[CONVERSATION FAILURE] "
                << llmResponse.message
                << endl;

            return 1;
        }

        cout << endl;
        cout << "[LLM RESPONSE]" << endl;
        cout << llmResponse.content << endl;

        logger.log(
            "INFO",
            "CONVERSATION_COMPLETE",
            "Conversation response received from LLM.",
            "SUCCESS"
        );
    }

    // --------------------------------------------------------
    // BRANCH 2 — SINGLE ACTION
    // --------------------------------------------------------

    if (
        classification.type ==
        RequestType::SINGLE_ACTION
    )
    {
        logger.log(
            "INFO",
            "SINGLE_ACTION_PIPELINE",
            "Routing request to Single Action Pipeline.",
            "STARTED"
        );

        SingleActionPipeline singleActionPipeline;

        SingleActionResult actionResult =
            singleActionPipeline.process(
                normalized.normalizedText
            );

        // ----------------------------------------------------
        // PIPELINE FAILURE
        // ----------------------------------------------------

        if (!actionResult.success)
        {
            logger.log(
                "ERROR",
                "SINGLE_ACTION_PIPELINE",
                actionResult.message,
                "FAILURE"
            );

            cerr
                << "[SINGLE ACTION FAILURE] "
                << actionResult.message
                << endl;

            return 1;
        }

        // ----------------------------------------------------
        // ACTION PARSER
        // ----------------------------------------------------

        cout << endl;
        cout << "[ACTION PARSER]" << endl;
        cout << "Action: "
            << actionResult.action.name
            << endl;

        logger.log(
            "INFO",
            "ACTION_PARSER",
            "Action identified: "
                + actionResult.action.name,
            "SUCCESS"
        );

        // ----------------------------------------------------
        // CAPABILITY RESOLVER
        // ----------------------------------------------------

        cout << endl;
        cout << "[CAPABILITY RESOLVER]" << endl;
        cout << "Capability: "
            << actionResult.capability.name
            << endl;

        logger.log(
            "INFO",
            "CAPABILITY_RESOLVER",
            "Capability resolved: "
                + actionResult.capability.name,
            "SUCCESS"
        );

        // ----------------------------------------------------
        // PARAMETER RESOLVER
        // ----------------------------------------------------

        cout << endl;
        cout << "[PARAMETER RESOLVER]" << endl;

        cout << "Name: "
            << (
                    actionResult.parameters.name.empty()
                    ? "[none]"
                    : actionResult.parameters.name
                )
            << endl;

        cout << "Location: "
            << (
                    actionResult.parameters.location.empty()
                    ? "[none]"
                    : actionResult.parameters.location
                )
            << endl;

        cout << "Application: "
            << (
                    actionResult.parameters.application.empty()
                    ? "[none]"
                    : actionResult.parameters.application
                )
            << endl;

        logger.log(
            "INFO",
            "PARAMETER_RESOLVER",
            "Action parameters resolved.",
            "SUCCESS"
        );

        // ----------------------------------------------------
        // ACTION VALIDATOR
        // ----------------------------------------------------

        cout << endl;
        cout << "[ACTION VALIDATOR]" << endl;

        cout << "Valid: "
            << (
                    actionResult.validation.valid
                    ? "true"
                    : "false"
                )
            << endl;

        logger.log(
            "INFO",
            "ACTION_VALIDATOR",
            actionResult.validation.message,
            actionResult.validation.valid
                ? "SUCCESS"
                : "FAILURE"
        );

        // ----------------------------------------------------
        // EXECUTION PLAN
        // ----------------------------------------------------

        cout << endl;
        cout << "[EXECUTION PLAN]" << endl;

        cout << "Valid: "
            << (
                    actionResult.executionPlan.valid
                    ? "true"
                    : "false"
                )
            << endl;

        cout << "Type: "
            << actionResult.executionPlan.type
            << endl;

        cout << "Action: "
            << actionResult.executionPlan.action.name
            << endl;

        cout << "Capability: "
            << actionResult.executionPlan.capability.name
            << endl;

        logger.log(
            "INFO",
            "EXECUTION_PLAN",
            "Single action execution plan created.",
            actionResult.executionPlan.valid
                ? "SUCCESS"
                : "FAILURE"
        );
        // ----------------------------------------------------
        // EXECUTION RESULT
        // ----------------------------------------------------

        cout << endl;
        cout << "[EXECUTION RESULT]" << endl;

        cout << "Status: "
            << static_cast<int>(
                actionResult.executionResult.status
            )
            << endl;

        cout << "Reason: "
            << static_cast<int>(
                actionResult.executionResult.reason
            )
            << endl;

        cout << "Action: "
            << actionResult.executionResult.action
            << endl;

        cout << "Confidence: "
            << actionResult.executionResult.confidence
            << endl;

        cout << "Target Resolved: "
            << (
                actionResult.executionResult.targetResolved
                ? "true"
                : "false"
            )
            << endl;

        cout << "Execution Mechanism Resolved: "
            << (
                actionResult.executionResult.executionMechanismResolved
                ? "true"
                : "false"
            )
            << endl;

        cout << "Message: "
            << actionResult.executionResult.message
            << endl;

        logger.log(
            "INFO",
            "EXECUTION_RESULT",
            actionResult.executionResult.message,
            actionResult.executionResult.status
                == ExecutionStatus::SUCCESS
                ? "SUCCESS"
                : "FAILURE"
        );

        cout << endl;
        cout << "[SINGLE ACTION PIPELINE COMPLETE]"
            << endl;

        logger.log(
            "INFO",
            "SINGLE_ACTION_PIPELINE",
            "Single action execution completed successfully.",
            "SUCCESS"
        );
    }

    // --------------------------------------------------------
    // BRANCH 3 — MULTI-STEP GOAL
    // --------------------------------------------------------

    if (
        classification.type ==
        RequestType::MULTI_STEP_GOAL
    )
    {
        cout << endl;
        cout << "[BRANCH 3]" << endl;
        cout << "MULTI_STEP_GOAL" << endl;

        logger.log(
            "INFO",
            "MULTI_STEP_GOAL",
            "Multi-step goal branch selected.",
            "SUCCESS"
        );
    }

    // --------------------------------------------------------
    // BRANCH 4 — EXISTING WORKFLOW
    // --------------------------------------------------------

    if (
        classification.type ==
        RequestType::EXISTING_WORKFLOW
    )
    {
        cout << endl;
        cout << "[BRANCH 4]" << endl;
        cout << "EXISTING_WORKFLOW" << endl;

        logger.log(
            "INFO",
            "EXISTING_WORKFLOW",
            "Existing workflow branch selected.",
            "SUCCESS"
        );
    }

    // --------------------------------------------------------
    // BRANCH 5 — CLARIFICATION REQUIRED
    // --------------------------------------------------------

    if (
        classification.type ==
        RequestType::CLARIFICATION_REQUIRED
    )
    {
        cout << endl;
        cout << "[BRANCH 5]" << endl;
        cout << "CLARIFICATION_REQUIRED" << endl;

        logger.log(
            "INFO",
            "CLARIFICATION_REQUIRED",
            "Clarification branch selected.",
            "SUCCESS"
        );
    }

    // --------------------------------------------------------
    // BRANCH 6 — UNSUPPORTED
    // --------------------------------------------------------

    if (
        classification.type ==
        RequestType::UNSUPPORTED
    )
    {
        cout << endl;
        cout << "[BRANCH 6]" << endl;
        cout << "UNSUPPORTED" << endl;

        logger.log(
            "INFO",
            "UNSUPPORTED",
            "Unsupported request branch selected.",
            "SUCCESS"
        );
    }

    return 0;
}