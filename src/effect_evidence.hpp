#pragma once
#include "effect_assurance.hpp"
#include "predicate_registry.hpp"
#include "application_correlation.hpp"
#include "semantic_evidence.hpp"

namespace EffectEvidence {
using nlohmann::json;
inline bool validatedDispatch(const workflow::Evidence& dispatch) {
    const auto& f=dispatch.facts;
    return dispatch.operation=="application.launch" && dispatch.category==workflow::EvidenceCategory::APPLICATION_DISPATCH &&
        f.value("operation_succeeded",false) && f.value("dispatch_accepted",false) && f.value("target_resolved",false) &&
        !f.value("launch_id",std::string{}).empty() && !f.value("runtime_session",std::string{}).empty() && ApplicationCorrelation::ticks(f,"launch_started_100ns") && ApplicationCorrelation::ticks(f,"launch_completed_100ns")>=ApplicationCorrelation::ticks(f,"launch_started_100ns") &&
        SemanticEvidence::samePath(f.value("canonical_requested_target",std::string{}),f.value("target_path",std::string{}));
}
inline bool eligible(const workflow::Evidence& dispatch) {
    const auto kind=dispatch.facts.value("effect_kind",std::string{});
    return validatedDispatch(dispatch) && dispatch.facts.value("effect_risk",std::string{})=="LOW" && (kind=="OPEN_FOLDER" || kind=="OPEN_DOCUMENT" || kind=="OPEN_APPLICATION");
}
inline workflow::Evidence assess(const workflow::Evidence& dispatch,const std::vector<workflow::Evidence>& evidence) {
    workflow::Evidence result;result.stepId=dispatch.stepId;result.operation="effect.assess";result.category=workflow::EvidenceCategory::EFFECT_ASSURANCE;
    const auto& f=dispatch.facts;const auto target=f.value("target_path",std::string{});
    const auto end=ApplicationCorrelation::ticks(f,"launch_completed_100ns");
    result.facts={{"effect_id",f.value("runtime_session",std::string{})+":"+dispatch.id},{"dispatch_evidence_id",dispatch.id},
        {"target_path",target},{"effect_kind",f.value("effect_kind",std::string{})},{"effect_risk",f.value("effect_risk",std::string{})},
        {"effect_assurance","UNAVAILABLE"},{"evidence_source","unavailable"},{"contradiction",false}};
    if(!validatedDispatch(dispatch))return result;
    EffectAssurance level=eligible(dispatch)?EffectAssurance::DISPATCH_CONFIRMED:EffectAssurance::UNAVAILABLE;
    std::string source=eligible(dispatch)?"backend_dispatch":"unavailable";
    for(const auto& item:evidence) {
        if(!item.current || item.stepId!=dispatch.stepId || item.facts.value("runtime_session",std::string{})!=f.value("runtime_session",std::string{}) ||
            ApplicationCorrelation::ticks(item.facts,"observation_started_100ns")<end)continue;
        if(item.category==workflow::EvidenceCategory::SHELL_OBSERVATION && f.value("effect_kind",std::string{})=="OPEN_FOLDER") {
            for(const auto& row:item.facts.value("windows",json::array()))if(row.value("source",std::string{})=="native_shell_folder" && row.value("visible",false) &&
                !row.value("window_handle",std::string{}).empty() && SemanticEvidence::samePath(row.value("canonical_location",std::string{}),target)) {
                level=EffectAssurance::EXACT;source="native_shell";result.facts["observation_evidence_id"]=item.id;result.facts["observed_after_dispatch"]=true;
            }
        }
        if(item.category==workflow::EvidenceCategory::APPLICATION_OBSERVATION && SemanticEvidence::samePath(item.facts.value("target_requested",std::string{}),target)) {
            for(const auto& row:item.facts.value("windows",json::array())) {
                const auto association=row.value("target_evidence",json::object());
                const auto path=association.value("observed_path",std::string{});
                if(ApplicationCorrelation::sameInstance(f,row) && !path.empty() && !SemanticEvidence::samePath(path,target))result.facts["contradiction"]=true;
                if(ApplicationCorrelation::sameInstance(f,row)) {
                    auto contradicts=[&](const std::string& label) {return Utf8::toPath(label).is_absolute() && !SemanticEvidence::samePath(label,target);};
                    for(const auto key:{"window_title","uia_name","uia_help_text"})if(contradicts(row.value(key,std::string{})))result.facts["contradiction"]=true;
                    if(row.contains("document_controls") && row["document_controls"].is_array() && row["document_controls"].size()==1)
                        for(const auto key:{"name","help_text"})if(contradicts(row["document_controls"][0].value(key,std::string{})))result.facts["contradiction"]=true;
                }
                if(!row.value("window_visible",false))continue;
                if(ApplicationCorrelation::exact(row,target)) {level=EffectAssurance::EXACT;source="native_document";result.facts["observation_evidence_id"]=item.id;}
                else if(level!=EffectAssurance::EXACT && ApplicationCorrelation::correlated(item.facts,row,target)) {level=EffectAssurance::CORRELATED;source="trusted_process_correlation";result.facts["observation_evidence_id"]=item.id;}
            }
        }
    }
    if(result.facts["contradiction"]==true) {level=EffectAssurance::UNAVAILABLE;source="contradictory_observation";}
    result.facts["effect_assurance"]=assuranceName(level);result.facts["evidence_source"]=source;return result;
}
inline bool matches(const workflow::Evidence& evidence,const json& proof) {
    const auto& f=evidence.facts;
    if(!evidence.current || (evidence.category!=workflow::EvidenceCategory::EFFECT_ASSURANCE && evidence.category!=workflow::EvidenceCategory::HUMAN_ATTESTATION) ||
        f.value("contradiction",false) || f.value("effect_id",std::string{}).empty() || f.value("dispatch_evidence_id",std::string{}).empty())return false;
    const auto expected=PredicateRegistry::constraints(proof);
    if(expected.size()!=(proof.value("source",std::string{})=="effect"?2:1) || !expected.contains("target_path") || !SemanticEvidence::samePath(expected["target_path"].get<std::string>(),f.value("target_path",std::string{})))return false;
    if(expected.contains("effect_type") && expected["effect_type"]!=f.value("effect_kind",std::string{}))return false;
    const auto assurance=assuranceFrom(f.value("effect_assurance",std::string{}));
    if(evidence.category==workflow::EvidenceCategory::HUMAN_ATTESTATION && assurance!=EffectAssurance::USER_CONFIRMED)return false;
    if(assurance==EffectAssurance::USER_CONFIRMED && (evidence.category!=workflow::EvidenceCategory::HUMAN_ATTESTATION || f.value("evidence_source",std::string{})!="human_attestation" || !f.value("confirmed",false)))return false;
    if(PredicateRegistry::kindOf(proof)=="target_observed")return assurance==EffectAssurance::EXACT || assurance==EffectAssurance::CORRELATED;
    return (PredicateRegistry::kindOf(proof)=="opened" || proof.value("source",std::string{})=="effect" && PredicateRegistry::kindOf(proof)=="effect_achieved") && f.value("effect_risk",std::string{})=="LOW" && acceptsAssurance(assurance,proof);
}
}
