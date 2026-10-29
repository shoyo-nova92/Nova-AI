#ifdef _WIN32
#include <windows.h>
#endif
#include "input_normalizer.hpp"
#include "logger.hpp"
#include "input_coordinator.hpp"
#include "request_classifier.hpp"
#include "conversation_handler.hpp"
#include "branch3_runtime.hpp"
#include "runtime_supervisor.hpp"
#include "utf8_text.hpp"
#include <filesystem>
#include <iostream>
#include <string>
namespace {
TaskResult processOneRequest(const InputEvent& input, Logger& logger, Branch3Runtime& runtime) {
    TaskResult result;
    try {
        result.blockedStage="Input normalization";
        const auto normalized=InputNormalizer{}.normalize(input.rawText);
        if (!normalized.success) { result.message=normalized.message; result.outcome=RuntimeOutcome::NEEDS_USER; return result; }
        logger.log("INFO","INPUT_READY",input.rawText,"SUCCESS");
        std::cout << "\n[INPUT PRESERVED]\n" << normalized.normalizedText << '\n';
        result.blockedStage="Root routing";
        const auto classified=RequestClassifier{}.classify(normalized.normalizedText);
        if (!classified.success) {
            result.outcome=RuntimeOutcome::EXTERNAL_UNAVAILABLE; result.cause=dependencyCause(classified.failureCategory);
            result.message=classified.message; return result;
        }
        switch (classified.type) {
            case RequestType::SINGLE_ACTION:
            case RequestType::MULTI_STEP_GOAL: {
                result.blockedStage="Goal execution";
                // Literal user input is authoritative; never route through legacy phrase parsers.
                const auto branch=runtime.start(input.rawText);

                logger.log("INFO","GOAL_RUNTIME",branch.message,outcomeName(branch.outcome));
                return branch;
            }
            case RequestType::CONVERSATION: {
                result.blockedStage="Conversation";
                const auto response=ConversationHandler{}.handle(input.rawText);
                if (response.success) {
                    std::cout << "\n[NOVA] " << response.content << '\n';
                    result.outcome=RuntimeOutcome::COMPLETED; result.cause=RuntimeCause::NONE; result.blockedStage.clear();
                } else { result.outcome=RuntimeOutcome::EXTERNAL_UNAVAILABLE; result.cause=dependencyCause(response.failureCategory); }
                result.message=response.message; return result;
            }
            case RequestType::CLARIFICATION_REQUIRED:
                result.outcome=RuntimeOutcome::NEEDS_USER;
                result.message="Please specify the target, location and intended result so I can act without guessing."; return result;
            case RequestType::EXISTING_WORKFLOW:
                result.outcome=RuntimeOutcome::NEEDS_USER;
                result.message="No saved workflow definition is available. Describe its steps or provide its definition."; return result;
            case RequestType::UNSUPPORTED:
                result.outcome=RuntimeOutcome::NEEDS_USER;
                result.message="This request needs a capability or connection unavailable in the current registry. Please provide the missing integration or an alternative task."; return result;
        }
    } catch (const Utf8::Error& error) { result.cause=RuntimeCause::INVALID_UTF8; result.message=error.what(); }
    catch (const std::exception& error) { result.cause=RuntimeCause::VALIDATION_ERROR; result.message=error.what(); }
    return result;
}
}
int main(int argc, char** argv) {
#ifdef _WIN32
    SetConsoleCP(CP_UTF8); SetConsoleOutputCP(CP_UTF8);
#endif
    std::cout << std::unitbuf;
    std::filesystem::create_directories("logs");
    Logger logger("logs/runtime.log");
    if (!logger.isReady()) { std::cerr << "Logger initialization failed.\n"; return 1; }
    logger.log("INFO","RUNTIME_START","Nova runtime started.","SUCCESS");
    bool keyboard=argc>1 && std::string(argv[1])=="--text";
    InputCoordinator input;
    Branch3Runtime runtime;
    runtime.setEventSink([](const std::string& event){std::cout << event;});
    RuntimeSupervisor supervisor(runtime);
    while (supervisor.running()) {
        const auto event=input.waitForInput(keyboard);
        if (event.rawText=="q" || event.rawText=="Q" || event.rawText=="quit") break;
        auto result=processOneRequest(event,logger,runtime);
        while (supervisor.running()) {
            const auto control=supervisor.readControl(std::cin,std::cout,result);
            if (control==SupervisorAction::QUIT) break;
            if (control==SupervisorAction::NEW_REQUEST) { keyboard=true; break; }
            try {
                const auto resumed=control==SupervisorAction::ANSWER ? runtime.answerUser(supervisor.questionId(),supervisor.answer()) : control==SupervisorAction::RETRY ? runtime.retryBlockedDependency() : runtime.continueSession();

                result=resumed;
                logger.log("INFO","RUNTIME_RESUME",resumed.message,outcomeName(resumed.outcome));
            } catch (const Utf8::Error& error) { result.outcome=RuntimeOutcome::FAILED; result.cause=RuntimeCause::INVALID_UTF8; result.message=error.what(); }
            catch (const std::exception& error) { result.outcome=RuntimeOutcome::FAILED; result.cause=RuntimeCause::VALIDATION_ERROR; result.message=error.what(); }
        }
    }
    logger.log("INFO","RUNTIME_STOP","Nova shut down.","SUCCESS");
    return 0;
}
