#include "input_normalizer.hpp"
#include "logger.hpp"
#include "input_coordinator.hpp"
#include "request_classifier.hpp"
#include "conversation_handler.hpp"
#include "single_action_pipeline.hpp"

#include <filesystem>
#include <iostream>

using namespace std;

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
    // PART B — INPUT NORMALIZATION
    // --------------------------------------------------------

    logger.log(
        "INFO",
        "INPUT_NORMALIZATION",
        "Starting input normalization.",
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
        "Normalized text: "
            + normalized.normalizedText,
        "SUCCESS"
    );

    // --------------------------------------------------------
    // PART B — REQUEST CLASSIFICATION
    // --------------------------------------------------------

    logger.log(
        "INFO",
        "REQUEST_CLASSIFICATION",
        "Starting request classification.",
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
            "REQUEST_CLASSIFICATION",
            classification.message,
            "FAILURE"
        );

        cerr
            << "[CLASSIFICATION FAILURE] "
            << classification.message
            << endl;

        return 1;
    }

    if (
        classification.type ==
        RequestType::CONVERSATION
    )
    {
        cout << endl;
        cout << "[REQUEST TYPE]" << endl;
        cout << "CONVERSATION" << endl;
    }

    logger.log(
        "INFO",
        "REQUEST_CLASSIFIED",
        classification.message,
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
        cout << endl;
        cout << "[REQUEST TYPE]" << endl;
        cout << "SINGLE_ACTION" << endl;

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

        cout << endl;
        cout << "[SINGLE ACTION PIPELINE COMPLETE]"
            << endl;

        logger.log(
            "INFO",
            "SINGLE_ACTION_PIPELINE",
            "Single action planning completed successfully.",
            "SUCCESS"
        );
    }

    return 0;
}