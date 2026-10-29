#include "runtime_supervisor.hpp"
#include "branch3_runtime.hpp"
#include <cctype>
#include <iostream>
#include <string>
SupervisorAction RuntimeSupervisor::control(char key) {
    switch (std::toupper(static_cast<unsigned char>(key))) {
        case 'Q': running_ = false; runtime_.abandonSession(); return SupervisorAction::QUIT;
        case 'V': runtime_.abandonSession(); return SupervisorAction::NEW_REQUEST;
        case 'R': return runtime_.controls().retry ? SupervisorAction::RETRY : SupervisorAction::INVALID;
        case 'A': return runtime_.controls().answer ? SupervisorAction::ANSWER : SupervisorAction::INVALID;
        case 'C': return runtime_.controls().reobserve ? SupervisorAction::CONTINUE : SupervisorAction::INVALID;
        default: return SupervisorAction::INVALID;
    }
}
void RuntimeSupervisor::handleOutcome(const TaskResult& result, std::ostream& output) const {
    output << "\n[NOVA RESULT]\nStatus: " << outcomeName(result.outcome) << "\n";
    output << "Cause: " << causeName(result.cause) << "\n";
    if (result.outcome == RuntimeOutcome::COMPLETED) output << "Task completed.\n";
    else if (result.outcome == RuntimeOutcome::NEEDS_USER) output << "I need your input before I can continue.\n";
    else if(result.cause==RuntimeCause::INVALID_PLAN && !runtime_.controls().guidance && !runtime_.controls().answer)output << "Task could not be prepared as a valid executable workflow.\n";
    else output << "Task paused.\n";
    if (!result.verifiedProgress.empty()) {
        output << "Verified progress:\n";
        for (const auto& item : result.verifiedProgress) output << "- " << item << "\n";
    }
    if (!result.lastExecutionIssue.empty()) output << "Last execution issue: " << result.lastExecutionIssue << "\n";
    if (!result.recovery.empty()) output << "Recovery: " << result.recovery << "\n";
    if (!result.blockedStage.empty()) output << "Blocked at: " << result.blockedStage << "\n";
    output << "Reason: " << result.message << "\n";
    if (!result.answer.empty()) output << "\n[NOVA]\n" << result.answer << "\n";
    const auto available=runtime_.controls();
    if(result.outcome!=RuntimeOutcome::COMPLETED) {
        if(available.retry)output << "[R] Retry dependency  ";
        if(available.answer)if(const auto pending=runtime_.pendingUserDecision()) {
            output << "Question: " << pending->questionId << "\n" << pending->intent << "\n";
            for(size_t i=0;i<pending->candidates.size();++i) {
                const auto& row=pending->candidates[i];
                output << "[" << i+1 << "] Use " << row.value("name",std::string{}) << " — " << row.value("path",std::string{}) << " (score=" << row.value("match_score",0.0) << ")\n";
            }
            if(!pending->candidates.empty())output << "[O] Show matches  [S] Search again  ";
            output << (pending->reason=="STRATEGY_GUIDANCE"?"[A] Provide a clue or alternative method  ":"[A] Answer  ");
        }
        if(available.guidance)output << "[A] Provide a clue or alternative method  ";
        if(available.reobserve)output << "[C] Re-observe and continue  ";
    }
    output << "[V] New request  [Q] Quit\n";
}
SupervisorAction RuntimeSupervisor::readControl(std::istream& input, std::ostream& output, const TaskResult& result) {
    handleOutcome(result, output);
    std::string line;
    while (std::getline(input, line)) {
        if(const auto pending=runtime_.pendingUserDecision()) {
            if(line=="A" || line=="a") {output << "Your answer: ";if(!std::getline(input,line))return control('Q');}
            else if(line=="V" || line=="v" || line=="Q" || line=="q")return control(line[0]);
            else if((line=="R" || line=="r") && (result.outcome==RuntimeOutcome::EXTERNAL_UNAVAILABLE || result.outcome==RuntimeOutcome::RETRYABLE))return control('R');
            else if(line=="C" || line=="c" || line=="R" || line=="r") {output << "Answer the active question, or choose V or Q.\n";continue;}
            questionId_=pending->questionId;answer_=line;return SupervisorAction::ANSWER;
        }
        if(result.outcome!=RuntimeOutcome::COMPLETED && runtime_.controls().guidance && (line=="A" || line=="a" || line.size()>1)) {
            const bool promptAnswer=line=="A" || line=="a";
            const auto revised=runtime_.requestGuidance();
            if(const auto pending=runtime_.pendingUserDecision()) {
                if(promptAnswer){output << pending->intent << "\nYour guidance: ";if(!std::getline(input,line))return control('Q');}
                questionId_=pending->questionId;answer_=line;return SupervisorAction::ANSWER;
            }
            output << revised.message << "\n";continue;
        }
        if (line.size() != 1) { output << "Choose one menu key.\n"; continue; }
        if ((line[0]=='r' || line[0]=='R') && result.outcome!=RuntimeOutcome::EXTERNAL_UNAVAILABLE && result.outcome!=RuntimeOutcome::RETRYABLE) {
            output << "Retry is for an unavailable dependency. Choose one of the displayed actions.\n"; continue;
        }
        if (result.outcome == RuntimeOutcome::COMPLETED && (line[0] == 'r' || line[0] == 'R' || line[0] == 'c' || line[0] == 'C')) {
            output << "Task is complete. Choose V or Q.\n"; continue;
        }
        const auto action = control(line[0]);
        if (action != SupervisorAction::INVALID) return action;
        output << "Choose an available menu key.\n";
    }
    return control('Q');
}
