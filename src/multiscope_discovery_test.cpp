#include "branch3_runtime.hpp"
#include "discovery_scope.hpp"
#include "primitive_execution_engine.hpp"
#include <chrono>
#include <fstream>
#include <iostream>

using nlohmann::json;
namespace fs=std::filesystem;
LLMResponse reply(json value){return {true,value.dump(),""};}
LLMResponse operation(const std::string& id,const std::string& name,json params){return reply({{"status","operation"},{"step_id",id},{"operation",{{"name",name},{"parameters",params}}}});}
ExecutionResult success(const PrimitiveOperation& op){ExecutionResult out{};out.status=ExecutionStatus::SUCCESS;out.action=op.name;return out;}
json contract(json roots,const std::string& strength="PREFERRED") {
    return {{"source","filesystem"},{"kind","search_result_matches"},{"match",{{"query","resume"},{"entry_type","file"},{"extension",".pdf"},{"search_roots",roots},{"root_semantics","ANY"},{"scope_strength",strength},{"scope_provenance",strength=="REQUIRED"?"USER_CONSTRAINT":"USER_HINT"}}}};
}
json plan(const json& proof,bool open=false) {
    json steps=json::array({{{"id","step_1"},{"description","Find the resume PDF in the discovery scopes"},{"depends_on",json::array()},{"postcondition",proof}}});
    if(open)steps.push_back({{"id","step_2"},{"description","Open the discovered PDF"},{"depends_on",json::array({"step_1"})}});
    return {{"steps",steps}};
}
int main() {
    int failures=0;auto check=[&](const std::string& name,bool okay){std::cout<<name<<": "<<(okay?"PASS":"FAIL")<<'\n';failures+=!okay;};
    const auto fixture=fs::temp_directory_path()/("NovaMultiScopeOffline_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(fixture/"Documents");fs::create_directories(fixture/"Downloads");fs::create_directories(fixture/"Other");
    std::ofstream(fixture/"Documents"/"unrelated.pdf")<<"fixture";
    std::ofstream(fixture/"Downloads"/"Shourya_Resume_2026.pdf")<<"fixture";
    const auto documents=Utf8::path(fixture/"Documents"),downloads=Utf8::path(fixture/"Downloads"),other=Utf8::path(fixture/"Other");
    const auto path=Utf8::path(fixture/"Downloads"/"Shourya_Resume_2026.pdf");
    const auto proof=contract(json::array({documents,downloads}));
    PrimitiveExecutionEngine engine;
    // Only filesystem.search reaches the real backend, confined to our fixture.
    // Every launch/observation is mocked; no provider or desktop interaction.
    for(const auto strength:{"PREFERRED","REQUIRED"}) {
        int decompositions=0,searchPlans=0,launches=0;std::vector<std::string> searched;std::string dispatched;bool canonicalScope=false;
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply(plan(contract(json::array({documents,downloads}),strength),true));},{},[&](const auto&,const std::string& prompt){
            if(prompt.find("CURRENT_READY_STEP:\nstep_1")!=std::string::npos){++searchPlans;canonicalScope=prompt.find("DISCOVERY_SCOPE:")!=std::string::npos;return operation("step_1","filesystem.search",{{"location",documents},{"query","resume"},{"entry_type","file"},{"extension",".pdf"}});}
            if(!launches)return operation("step_2","application.launch",{{"target","$resolved_target"}});
            return reply({{"status","satisfied"},{"step_id","step_2"},{"verification",{{"source","execution"},{"operation","application.launch"}}}});
        },[&](const PrimitiveOperation& op){if(op.name=="filesystem.search"){searched.push_back(op.parameters.location);return engine.execute(op);}if(op.name=="application.launch"){++launches;dispatched=op.parameters.target;}return success(op);});
        runtime.setRecoverySink([](const auto&){});
        // Small normal autonomous budget still permits every stated cheap root.
        StrategyRecovery::Policy policy;policy.autonomousAttempts=2;runtime.setRecoveryPolicy(policy);
        auto result=runtime.start("Find my resume PDF, probably in Documents or Downloads; open it.");
        check(std::string(strength)+"_multiple_roots_admitted_without_redecomposition",decompositions==1 && searched.size()==2 && searchPlans==1 && canonicalScope);
        check(std::string(strength)+"_empty_first_root_then_second_root_same_step",searched.size()==2 && SemanticEvidence::samePath(searched[0],documents) && SemanticEvidence::samePath(searched[1],downloads));
        bool verified=false;for(const auto& progress:result.verifiedProgress)verified |= progress.find("step_1: VERIFIED")!=std::string::npos;
        check(std::string(strength)+"_search_verified_before_confirmation",verified && result.cause==RuntimeCause::TARGET_CONFIRMATION && launches==0);
        if(auto pending=runtime.pendingUserDecision())result=runtime.answerUser(pending->questionId,"1");
        check(std::string(strength)+"_canonical_selected_resource_flows_to_open",result.success && launches==1 && SemanticEvidence::samePath(dispatched,path) && decompositions==1 && searched.size()==2);
    }
    {
        workflow::Evidence e;e.operation="filesystem.search";e.facts={{"operation_succeeded",true},{"category","FILESYSTEM_SEARCH_RESULT"},{"query","resume"},{"search_root",downloads},{"results",json::array({{{"path",path},{"entry_type","file"},{"extension",".pdf"},{"match_score",0.94}}})}};
        std::string error;
        check("required_any_accepts_second_root",SemanticEvidence::matches(e,contract(json::array({documents,downloads}),"REQUIRED"),error));
        auto single=contract(json::array({documents}),"REQUIRED");single["match"].erase("search_roots");single["match"].erase("root_semantics");single["match"]["search_root"]=documents;
        check("required_single_rejects_other_root",!SemanticEvidence::matches(e,single,error));
        check("preferred_roots_permit_compatible_broadening",SemanticEvidence::matches(e,contract(json::array({documents})),error));
        auto outside=e;outside.facts["results"][0]["path"]=downloads+"-sibling/resume.pdf";
        check("preferred_still_requires_candidate_beneath_actual_root",!SemanticEvidence::matches(outside,proof,error));
        outside.facts["search_root"]=downloads+"-sibling";
        check("required_any_preserves_component_boundary",!SemanticEvidence::matches(outside,contract(json::array({documents,downloads}),"REQUIRED"),error));
        const auto before=e.facts.dump();SemanticEvidence::matches(e,proof,error);check("verification_does_not_mutate_evidence",before==e.facts.dump());
    }
    {
        auto duplicate=contract(json::array({documents,documents+"/./"}));
        const auto bound=DiscoveryScope::bind(duplicate);check("equivalent_roots_deduplicate",bound["match"]["search_roots"].size()==1);
        auto deferred=DiscoveryScope::bind(contract(json::array({"documents","downloads"})),[](const std::string&){return std::string{};});
        check("unresolved_preferred_known_folder_alias_is_retained",deferred["match"]["search_roots"]==json::array({"documents","downloads"}));
        bool rejected=false;try{DiscoveryScope::bind(contract(json::array({"unknown_required_resource"}),"REQUIRED"));}catch(const std::invalid_argument&){rejected=true;}
        check("unrepresentable_required_resource_rejected",rejected);
        rejected=false;try{DiscoveryScope::bind(contract(json::array({"C:\\bad|path"})));}catch(const std::invalid_argument&){rejected=true;}
        check("malformed_actual_path_rejected_even_if_preferred",rejected);
        std::vector<std::string> resolved;
        const auto known=DiscoveryScope::bind(contract(json::array({"documents","downloads"})),[&](const std::string& clue){resolved.push_back(clue);return clue=="documents"?documents:downloads;});
        check("logical_roots_resolved_individually",resolved==std::vector<std::string>({"documents","downloads"}) && known["match"]["search_roots"]==json::array({documents,downloads}));
        for(const auto field:{"search_roots","root_semantics","scope_strength","scope_provenance"}) {
            auto invalid=proof;invalid["match"][field]=false;std::string error;check(std::string("schema_rejects_wrong_type_")+field,!PredicateRegistry::validateSemantic(invalid,error));
        }
    }
    {
        fs::create_directories(fixture/"empty1");fs::create_directories(fixture/"empty2");
        int decompositions=0,plans=0,searches=0;
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply(plan(contract(json::array({documents,Utf8::path(fixture/"empty1"),Utf8::path(fixture/"empty2"),downloads}))));},{},[&](const auto&,const auto&){++plans;return reply({{"status","ask_user"},{"step_id","step_1"},{"message","Which of the roots should I search?"}});},[&](const PrimitiveOperation& op){++searches;return engine.execute(op);});
        runtime.setRecoverySink([](const auto&){});StrategyRecovery::Policy policy;policy.autonomousAttempts=2;runtime.setRecoveryPolicy(policy);
        check("cheap_stated_roots_precede_question_and_extend_soft_budget",runtime.start("Find resume in the preferred scopes").success && searches==4 && plans==1 && decompositions==1);
    }
    {
        int executed=0;
        Branch3Runtime runtime([&](const auto&,const auto&){return reply(plan(contract(json::array({documents}),"REQUIRED")));},{},[&](const auto&,const auto&){return operation("step_1","filesystem.search",{{"location",downloads},{"query","resume"},{"entry_type","file"},{"extension",".pdf"}});},[&](const PrimitiveOperation& op){++executed;return engine.execute(op);});
        runtime.setRecoverySink([](const auto&){});
        const auto result=runtime.start("Find resume only in the required scope");
        check("required_scope_cannot_silently_broaden_before_execution",executed==0 && result.outcome==RuntimeOutcome::NEEDS_USER && result.cause==RuntimeCause::STRATEGY_GUIDANCE);
    }
    {
        int executed=0;
        Branch3Runtime runtime([&](const auto&,const auto&){return reply(plan(contract(json::array({"C:\\bad|path"}))));},{},[&](const auto&,const auto&){return operation("step_1","filesystem.search",{{"location",downloads},{"query","resume"}});},[&](const PrimitiveOperation& op){++executed;return engine.execute(op);});
        const auto result=runtime.start("Find resume in this invalid physical path");
        check("malformed_path_rejected_during_graph_admission",executed==0 && result.cause==RuntimeCause::INVALID_PLAN);
    }
    {
        int decompositions=0,executed=0;
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply(plan(contract(json::array({"unresolved folder clue",downloads}))));},{},[&](const auto&,const auto&){return operation("step_1","filesystem.search",{{"location",downloads},{"query","resume"},{"entry_type","file"},{"extension",".pdf"}});},[&](const PrimitiveOperation& op){++executed;return engine.execute(op);});
        runtime.setRecoverySink([](const auto&){});
        check("unresolved_preference_does_not_prevent_graph_admission",runtime.start("Find resume in preferred locations").success && decompositions==1 && executed==1);
    }
    {
        // Both initial roots fail; guidance adds a third without routing or
        // decomposition, then deterministic verification records recovery.
        std::ofstream(fixture/"Other"/"Resume.pdf")<<"fixture";
        int decompositions=0,searchPlans=0;std::vector<std::string> searched;json episodes=json::array();
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply(plan(contract(json::array({documents,other+"/empty"}))));},{},[&](const auto&,const auto&){++searchPlans;return operation("step_1","filesystem.search",{{"location",documents},{"query","resume"},{"entry_type","file"},{"extension",".pdf"}});},[&](const PrimitiveOperation& op){searched.push_back(op.parameters.location);return engine.execute(op);},[&](const auto&,const auto&){return reply({{"decision","ADD_LOCATION_HINT"},{"value",downloads}});});
        fs::create_directories(fixture/"Other"/"empty");StrategyRecovery::Policy policy;policy.autonomousAttempts=2;runtime.setRecoveryPolicy(policy);runtime.setRecoverySink([&](const auto& episode){episodes.push_back(episode);});
        auto result=runtime.start("Find the resume PDF in the preferred scopes");
        check("exhausted_scopes_reach_guidance_not_invalid_plan",result.outcome==RuntimeOutcome::NEEDS_USER && result.cause==RuntimeCause::STRATEGY_GUIDANCE && searched.size()==2 && decompositions==1);
        const auto pending=runtime.pendingUserDecision();
        check("question_derived_from_both_scope_attempts",pending && pending->intent.find("Documents")!=std::string::npos && pending->context["attempts"].size()==2);
        if(pending)result=runtime.answerUser(pending->questionId,"check Downloads too");
        check("new_location_guidance_resumes_same_workflow",result.success && decompositions==1 && searchPlans==1 && searched.size()==3 && SemanticEvidence::samePath(searched.back(),downloads));
        bool verified=false;for(const auto& episode:episodes)verified |= episode.value("status",std::string{})=="VERIFIED_SUCCESS";
        check("successful_guidance_emits_verified_recovery_episode",verified);
    }
    // The fixture is uniquely owned by this process under the temporary folder.
    if(fixture.parent_path()==fs::temp_directory_path() && fixture.filename().string().starts_with("NovaMultiScopeOffline_"))fs::remove_all(fixture);
    return failures?1:0;
}
