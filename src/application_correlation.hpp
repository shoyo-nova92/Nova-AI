#pragma once
#include "workflow_kernel.hpp"
#include "utf8_text.hpp"
#include <filesystem>
#include <cstdint>

// Derived observations are built from host facts before insertion into FactStore.
// No planner claims, foreground state, or dispatch arguments alone are proof.
namespace ApplicationCorrelation {
using nlohmann::json;
inline uint64_t ticks(const json& facts,const char* key) {
    if(!facts.contains(key) || !facts[key].is_number_unsigned() && !facts[key].is_number_integer())return 0;
    if(facts[key].is_number_integer() && facts[key].get<int64_t>()<0)return 0;
    return facts[key].get<uint64_t>();
}
inline bool sameInstance(const json& a,const json& b) {
    return ticks(a,"process_id") && ticks(a,"process_id")==ticks(b,"process_id") &&
        ticks(a,"process_created_100ns") && ticks(a,"process_created_100ns")==ticks(b,"process_created_100ns") &&
        !a.value("process_image_path",std::string{}).empty() &&
        workflow::sameIdentity(a.value("process_image_path",std::string{}),b.value("process_image_path",std::string{}));
}
inline bool labelMatches(const std::string& label,const std::string& name) {
    if(name.empty())return false;
    if(workflow::sameIdentity(label,name))return true;
    for(const auto separator:{" - "," — "," – "}) {
        const auto end=label.find(separator);
        if(end!=std::string::npos && workflow::sameIdentity(label.substr(0,end),name))return true;
    }
    return false;
}
inline bool documentNameMatches(const json& row,const std::string& target) {
    const auto name=Utf8::path(Utf8::toPath(target).filename());
    // A directly exposed contradictory path defeats a basename correlation.
    for(const auto key:{"window_title","uia_name","uia_help_text"}) {
        const auto label=row.value(key,std::string{});
        if(Utf8::toPath(label).is_absolute() && !workflow::sameIdentity(label,target))return false;
    }
    unsigned controls=0;
    if(row.contains("document_controls") && row["document_controls"].is_array())for(const auto& control:row["document_controls"])
        if(labelMatches(control.value("name",std::string{}),name) || labelMatches(control.value("help_text",std::string{}),name))++controls;
    // Several same-labelled documents cannot be bound to one dispatch.
    if(controls>1)return false;
    return controls==1 || labelMatches(row.value("window_title",std::string{}),name);
}
inline bool exact(const json& row,const std::string& target) {
    const auto association=row.value("target_evidence",json::object());
    if(!association.is_object())return false;
    const auto source=association.value("source",std::string{});
    return row.value("target_observed",false) && association.value("association",std::string{})=="exact_document_path" &&
        (source=="window_title" || source=="document_or_tab_name" || source=="document_or_tab_help_text") &&
        workflow::sameIdentity(association.value("observed_path",std::string{}),target);
}
inline bool qualifies(const json& dispatch,const json& observation,const json& row) {
    const auto requested=dispatch.value("canonical_requested_target",std::string{});
    const auto resolved=dispatch.value("target_path",std::string{});
    const auto session=dispatch.value("runtime_session",std::string{});
    const auto start=ticks(dispatch,"launch_started_100ns"),end=ticks(dispatch,"launch_completed_100ns");
    const auto created=ticks(dispatch,"process_created_100ns");
    return dispatch.value("operation_succeeded",false) && dispatch.value("dispatch_accepted",false) &&
        dispatch.value("target_resolved",false) && dispatch.value("new_process_instance",false) &&
        !requested.empty() && Utf8::toPath(requested).is_absolute() && Utf8::toPath(resolved).is_absolute() &&
        workflow::sameIdentity(requested,resolved) && workflow::sameIdentity(resolved,observation.value("target_requested",std::string{})) &&
        !session.empty() && session==observation.value("runtime_session",std::string{}) &&
        !dispatch.value("launch_id",std::string{}).empty() && start && end>=start && created>=start && created<=end &&
        ticks(observation,"observation_started_100ns")>=end &&
        sameInstance(dispatch,row) && row.value("window_visible",false) &&
        !row.value("window_handle",std::string{}).empty() && documentNameMatches(row,resolved);
}
inline bool correlated(const json& observation,const json& row,const std::string& target) {
    const auto proof=row.value("target_evidence",json::object());
    if(!proof.is_object() || proof.value("association",std::string{})!="correlated_document_target" ||
        proof.value("dispatch_evidence_id",std::string{}).empty() || !proof.contains("dispatch_context"))return false;
    return qualifies(proof["dispatch_context"],observation,row) &&
        workflow::sameIdentity(proof["dispatch_context"].value("target_path",std::string{}),target);
}
inline json enrich(json facts,const std::vector<const workflow::Evidence*>& candidates,
                   const std::vector<const workflow::Evidence*>& allDispatches) {
    if(!facts.contains("windows") || !facts["windows"].is_array())return facts;
    const auto target=facts.value("target_requested",std::string{});
    bool observed=false;std::string level="unavailable";
    for(auto& row:facts["windows"]) {
        if(exact(row,target)) {row["target_proof_level"]="exact";observed=true;level="exact";continue;}
        row["target_proof_level"]="unavailable";
        // A duplicate document label in this instance is ambiguous even after launch.
        unsigned matching=0;
        for(const auto& other:facts["windows"])
            if(sameInstance(row,other) && other.value("window_visible",false) && documentNameMatches(other,target))++matching;
        if(matching!=1)continue;
        for(const auto* evidence:candidates) {
            if(evidence->category!=workflow::EvidenceCategory::APPLICATION_DISPATCH || evidence->operation!="application.launch" ||
                !qualifies(evidence->facts,facts,row))continue;
            bool conflict=false;
            for(const auto* other:allDispatches) {
                const auto& dispatch=other->facts;
                if(other->operation=="application.launch" && dispatch.value("operation_succeeded",false) &&
                    sameInstance(dispatch,row) && ticks(dispatch,"launch_completed_100ns")<=ticks(facts,"observation_started_100ns") &&
                    !workflow::sameIdentity(dispatch.value("target_path",std::string{}),target))conflict=true;
            }
            if(conflict)continue;
            row["target_observed"]=true;row["target_path"]=target;row["target_observation_supported"]=true;
            row["target_proof_level"]="correlated";row.erase("target_observation_reason");
            row["target_evidence"]={{"association","correlated_document_target"},{"source","launch_process_window_correlation"},
                {"dispatch_evidence_id",evidence->id},{"dispatch_context",evidence->facts}};
            observed=true;if(level!="exact")level="correlated";break;
        }
    }
    facts["target_proof_level"]=level;
    if(observed) {facts["target_observed"]=true;facts["target_observation_supported"]=true;facts.erase("target_observation_reason");}
    return facts;
}
// Bounded diagnostic projection: no accessibility tree or unbounded launch history.
inline json diagnostic(const json& facts,const std::vector<const workflow::Evidence*>& dispatches) {
    const auto target=facts.value("target_requested",std::string{});
    json out={{"requested_target",Utf8::truncate(target,1024,"correlation.target")},{"canonical_target",Utf8::truncate(target,1024,"correlation.canonical")},{"window_exists",facts.value("window_exists",false)},
        {"proof",facts.value("target_proof_level",std::string("unavailable"))},{"failure_reason",facts.value("target_observation_reason",std::string{})}};
    const json empty=json::object();const json* row=&empty;
    if(facts.contains("windows") && !facts["windows"].empty()) {
        row=&facts["windows"][0];for(const auto& window:facts["windows"])if(window.value("target_observed",false)){row=&window;break;}
    }
    const json* dispatch=&empty;
    for(const auto* item:dispatches)if(item->operation=="application.launch" && workflow::sameIdentity(item->facts.value("target_path",std::string{}),target))dispatch=&item->facts;
    out["dispatch_pid"]=ticks(*dispatch,"process_id");out["observed_pid"]=ticks(*row,"process_id");
    out["process_instance_match"]=sameInstance(*dispatch,*row);
    out["executable_match"]=!dispatch->value("process_image_path",std::string{}).empty() && workflow::sameIdentity(dispatch->value("process_image_path",std::string{}),row->value("process_image_path",std::string{}));
    out["observation_after_dispatch"]=ticks(*dispatch,"launch_completed_100ns") && ticks(facts,"observation_started_100ns")>=ticks(*dispatch,"launch_completed_100ns");
    out["window_visible"]=row->value("window_visible",false);out["window_title"]=Utf8::truncate(row->value("window_title",std::string{}),256,"correlation.title");
    out["uia_name"]=row->value("uia_name",std::string{});out["uia_help_text"]=row->value("uia_help_text",std::string{});
    out["document_label"]="";
    if(row->contains("document_controls") && (*row)["document_controls"].is_array())for(const auto& control:(*row)["document_controls"])
        if(labelMatches(control.value("name",std::string{}),Utf8::path(Utf8::toPath(target).filename()))) {out["document_label"]=control["name"];break;}
    out["basename_match"]=documentNameMatches(*row,target);out["direct_full_path_match"]=exact(*row,target);
    out["correlated_identity_match"]=correlated(facts,*row,target);
    if(out["proof"]=="unavailable") {
        if(dispatch->empty())out["failure_reason"]="No scoped canonical launch dispatch";
        else if(!dispatch->value("new_process_instance",false))out["failure_reason"]="Process reused, PID unavailable, or delegated launch";
        else if(!sameInstance(*dispatch,*row))out["failure_reason"]="No visible window bound to the dispatched process instance";
        else if(!documentNameMatches(*row,target))out["failure_reason"]="Document label missing or ambiguous";
        else out["failure_reason"]="Insufficient or conflicting launch/process/window context";
    }
    return out;
}
}
