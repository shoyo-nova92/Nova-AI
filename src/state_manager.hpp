#pragma once
#include <string>
#include <vector>
class StateManager {
public:
    explicit StateManager(std::string goal);
    void setObservation(std::string observation);
    void addHistory(std::string entry);
    std::string render() const;
    unsigned iterations() const { return iterations_; }
    void nextIteration() { ++iterations_; }
    bool repeatedFailure(const std::string& operation) const;
    void recordFailure(const std::string& operation);
    void recordSuccess();
private:
    std::string goal_;
    std::string observation_;
    std::vector<std::string> history_;
    std::vector<std::string> failures_;
    unsigned iterations_ = 0;
};
