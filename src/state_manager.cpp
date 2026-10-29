#include "state_manager.hpp"
#include "browser_state.hpp"
#include <sstream>
#include <utility>
#include <algorithm>
StateManager::StateManager(std::string goal) : goal_(std::move(goal)) {}
void StateManager::setObservation(std::string observation) { observation_ = BrowserState::compact(observation); }
void StateManager::addHistory(std::string entry) { history_.push_back(Utf8::truncate(BrowserState::summary(entry), MAX_HISTORY_ENTRY_BYTES, "history.entry")); if (history_.size() > MAX_HISTORY_ENTRIES) history_.erase(history_.begin()); }
bool StateManager::repeatedFailure(const std::string& signature) const {
    const std::string prefix = signature + "|reason=";
    unsigned count = 0;
    for (const auto& failure:failures_) if (failure.starts_with(prefix)) ++count;
    return count >= 3;
}
void StateManager::recordFailure(const std::string& signature) { failures_.push_back(signature); if (failures_.size()>10) failures_.erase(failures_.begin()); }
void StateManager::recordSuccess(const std::string& operation) {
    const auto prefix=operation+"|reason=";
    std::erase_if(failures_,[&](const std::string& item){return item.starts_with(prefix);});
}
std::string StateManager::render() const {
    std::ostringstream out; out << "[STATE]\nGoal: " << Utf8::truncate(goal_, 16000, "goal") << "\nIteration: " << iterations_ << "\nObservation: " << (observation_.empty()?"not yet available":observation_) << "\nRecent history:\n";
    if (history_.empty()) out << "- none\n"; else for (const auto& item:history_) out << "- " << item << "\n";
    return out.str();
}
bool StateManager::hasEvidence(const std::string& evidence) const {
    if (evidence.empty()) return false;
    if (observation_.find(evidence)!=std::string::npos) return true;
    for (const auto& item:evidence_) if (item.find(evidence)!=std::string::npos) return true;
    return false;
}
void StateManager::addEvidence(std::string evidence) {
    evidence_.push_back(Utf8::truncate(evidence,4096,"execution.evidence"));
    if (evidence_.size()>32) evidence_.erase(evidence_.begin());
}
