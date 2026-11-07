#pragma once
#include <string>
#include <vector>
class StateManager {
public:
    static constexpr std::size_t MAX_HISTORY_ENTRIES = 10;
    static constexpr std::size_t MAX_HISTORY_ENTRY_BYTES = 768;
    explicit StateManager(std::string goal);
    void setObservation(std::string observation);
    void addHistory(std::string entry);
    std::string render() const;
    bool hasEvidence(const std::string& evidence) const;
    void addEvidence(std::string evidence);
    unsigned iterations() const { return iterations_; }
    void nextIteration() { ++iterations_; }
    bool repeatedFailure(const std::string& operation) const;
    void recordFailure(const std::string& operation);
    void recordSuccess(const std::string& operation);
private:
    std::string goal_;
    std::string observation_;
    std::vector<std::string> history_;
    std::vector<std::string> failures_;
    std::vector<std::string> evidence_;
    unsigned iterations_ = 0;
};
