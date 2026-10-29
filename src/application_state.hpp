#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
#include <string>
#include <vector>

// Stable backend facts only: diagnostic prose and observation tokens are not state.
namespace ApplicationState {
inline std::string fingerprint(const nlohmann::json& facts) {
    using nlohmann::json;
    json state=json::object();
    for(const auto key:{"target_requested","target_observed","target_observation_supported","target_proof_level"})
        if(facts.contains(key))state[key]=facts[key];
    for(const auto collection:{"processes","windows"}) {
        std::vector<std::string> rows;
        if(facts.contains(collection) && facts[collection].is_array())for(const auto& row:facts[collection]) {
            json stable=json::object();
            for(const auto key:{"process_id","process_name","process_created_100ns","process_image_path","window_handle","window_title","uia_name","uia_help_text","document_controls","window_visible","target_path","target_observed","target_observation_supported","target_proof_level"})
                if(row.contains(key))stable[key]=row[key];
            // Focus changes are irrelevant to requested document identity.
            if(facts.value("target_requested",std::string{}).empty() && row.contains("foreground"))stable["foreground"]=row["foreground"];
            rows.push_back(stable.dump());
        }
        std::sort(rows.begin(),rows.end());state[collection]=rows;
    }
    return state.dump();
}
inline bool targetUnavailable(const nlohmann::json& facts) {
    return facts.contains("target_observation_supported") && facts["target_observation_supported"]==false &&
        !facts.value("target_requested",std::string{}).empty() &&
        facts.contains("windows") && facts["windows"].is_array() && !facts["windows"].empty() &&
        !facts.value("target_observed",false);
}
}
