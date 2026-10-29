#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <set>
#include <map>
#include <vector>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <cctype>
#include <algorithm>

namespace StrategyRecovery {
using nlohmann::json;
struct Policy {
    unsigned autonomousAttempts=3, guidedAttempts=8, totalAttempts=24, questions=3;
};
inline bool foreground(const std::string& operation) {
    return operation=="application.focus" || operation=="application.launch" || operation.starts_with("keyboard.") || operation.starts_with("mouse.");
}
inline std::string inputMode(const std::string& operation) {
    return operation.starts_with("keyboard.") || operation.starts_with("mouse.")?"GLOBAL_INPUT":foreground(operation)?"FOREGROUND_REQUIRED":"BACKGROUND_SAFE";
}
struct StepContext {
    json attempts=json::array(),hints=json::array();
    std::set<std::string> tried;
    std::map<std::string,size_t> readStates;
    unsigned sinceGuidance=0,questionCount=0,rejectedPlans=0;
    bool active=false,guided=false,foregroundAllowed=false,foregroundSuggested=false;
    std::string blocker="missing_evidence",goalClass="workflow_recovery";
    json episode;
    bool canTry(const Policy& policy) const {
        return attempts.size()<policy.totalAttempts && sinceGuidance<(guided?policy.guidedAttempts:policy.autonomousAttempts);
    }
    json budget(const Policy& policy) const {
        const auto limit=guided?policy.guidedAttempts:policy.autonomousAttempts;
        return {{"attempts_remaining",sinceGuidance>=limit?0:limit-sinceGuidance},
            {"total_remaining",attempts.size()>=policy.totalAttempts?0:policy.totalAttempts-attempts.size()},
            {"questions_remaining",questionCount>=policy.questions?0:policy.questions-questionCount}};
    }
    json context(const Policy& policy) const {
        return {{"active",active},{"blocker",blocker},{"attempts",attempts},{"user_hints",hints},
            {"budget",budget(policy)},{"input_mode",foregroundSuggested?"FOREGROUND_REQUIRED":"BACKGROUND_SAFE"}};
    }
};
inline bool identifier(const std::string& value) {
    if(value.empty() || value.size()>64)return false;
    for(unsigned char c:value)if(!std::isalnum(c) && c!='_' && c!='.' && c!='-')return false;return true;
}
// Durable projection deliberately excludes raw answers, paths, queries, error
// prose, goal text, content, DOMs, screenshots and hint values. In-memory hints
// remain available to this workflow, never read back from this log.
inline json privateGuidance(const json& guidance) {
    json out={{"decision",guidance.value("decision",std::string{})}};
    if(guidance.contains("hint") && guidance["hint"].is_object())out["hint_type"]=guidance["hint"].value("type",std::string{});
    if(guidance.value("decision",std::string{})=="ADD_LOCATION_HINT")out["hint_type"]="location";
    if(guidance.contains("required_primitives"))out["required_primitives"]=guidance["required_primitives"];
    return out;
}
inline json privateAttempts(const json& attempts) {
    json out=json::array();
    for(const auto& attempt:attempts)out.push_back({{"strategy",attempt.value("strategy",std::string{})},
        {"outcome",attempt.value("outcome",std::string{})},{"mode",attempt.value("mode",std::string("BACKGROUND_SAFE"))}});
    return out;
}
inline bool append(const std::filesystem::path& path,const json& record) {
    // One local bounded JSONL file and one rotated predecessor; no telemetry.
    constexpr uintmax_t maxBytes=1024*1024;const auto line=record.dump();if(line.size()>16384)return false;
    std::error_code error;std::filesystem::create_directories(path.parent_path(),error);if(error)return false;
    const bool exists=std::filesystem::exists(path,error);if(error)return false;
    const auto bytes=exists?std::filesystem::file_size(path,error):0;if(error)return false;
    if(bytes+line.size()+1>maxBytes) {
        auto previous=path;previous+=".1";
        std::filesystem::remove(previous,error);error.clear();std::filesystem::rename(path,previous,error);if(error)return false;
    }
    std::ofstream stream(path,std::ios::app|std::ios::binary);stream<<line<<'\n';return stream.good();
}
}
