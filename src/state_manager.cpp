#include "state_manager.hpp"
#include <sstream>
#include <utility>
StateManager::StateManager(std::string goal) : goal_(std::move(goal)) {}
void StateManager::setObservation(std::string observation) { observation_ = std::move(observation); }
void StateManager::addHistory(std::string entry) { history_.push_back(std::move(entry)); if (history_.size() > 20) history_.erase(history_.begin()); }
bool StateManager::repeatedFailure(const std::string& operation) const { unsigned count=0; for (auto it=failures_.rbegin();it!=failures_.rend() && *it==operation;++it) ++count; return count>=3; }
void StateManager::recordFailure(const std::string& operation) { failures_.push_back(operation); if (failures_.size()>10) failures_.erase(failures_.begin()); }
void StateManager::recordSuccess() { failures_.clear(); }
std::string StateManager::render() const {
    std::ostringstream out; out << "[STATE]\nGoal: " << goal_ << "\nIteration: " << iterations_ << "\nObservation: " << (observation_.empty()?"not yet available":observation_) << "\nRecent history:\n";
    if (history_.empty()) out << "- none\n"; else for (const auto& item:history_) out << "- " << item << "\n";
    return out.str();
}
