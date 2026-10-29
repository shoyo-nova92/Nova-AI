#pragma once
#include "runtime_outcome.hpp"
#include <iosfwd>
class Branch3Runtime;
enum class SupervisorAction { NEW_REQUEST, RETRY, CONTINUE, ANSWER, QUIT, INVALID };
class RuntimeSupervisor {
public:
    explicit RuntimeSupervisor(Branch3Runtime& runtime) : runtime_(runtime) {}
    const std::string& answer() const {return answer_;}
    const std::string& questionId() const {return questionId_;}
    bool running() const { return running_; }
    SupervisorAction control(char key);
    SupervisorAction readControl(std::istream& input, std::ostream& output, const TaskResult& result);
    void handleOutcome(const TaskResult& result, std::ostream& output) const;
private:
    Branch3Runtime& runtime_;
    bool running_ = true;
    std::string answer_,questionId_;
};
