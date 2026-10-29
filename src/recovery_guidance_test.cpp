#include "branch3_runtime.hpp"
#include "runtime_supervisor.hpp"
#include <iostream>
#include <sstream>
#include <chrono>
using nlohmann::json;
LLMResponse reply(json value){return {true,value.dump(),""};}
LLMResponse op(const std::string& step,const std::string& name,json parameters){return reply({{"status","operation"},{"step_id",step},{"operation",{{"name",name},{"parameters",parameters}}}});}
ExecutionResult okay(const PrimitiveOperation& operation,json facts=json::object()){ExecutionResult out{};out.action=operation.name;out.status=ExecutionStatus::SUCCESS;out.factsJson=facts.dump();return out;}
json step(std::string id,std::string intent,std::vector<std::string> deps={}){return {{"id",id},{"description",intent},{"depends_on",deps}};}
struct Fixture {
    int failInterpretations=0,decompositions=0,plans=0,prelude=0,searches=0,lists=0,focus=0,keys=0,launches=0,interpretations=0,guidedPlans=0,routerCalls=0;
    bool succeed=true,invalidProof=false,global=false,contextSeen=false,hintSeen=false,noSuccessBeforeProof=true;
    json records=json::array();std::string dispatched;
    const std::string root="C:/fixture",target="C:/fixture/Orion Tracker Setup.exe";
    Branch3Runtime runtime;
    Fixture():runtime([this](const auto&,const auto&){
        ++decompositions;
        auto first=step("step_1","Verify the prerequisite");first["postcondition"]={{"source","filesystem"},{"kind","path_exists"},{"path",root+"/known.txt"}};
        auto find=step("step_2","Find the requested installer",{"step_1"});find["postcondition"]={{"source","filesystem"},{"kind","search_result_matches"},{"match",{{"query","orion tracker"},{"extension","exe"},{"search_root",root}}}};
        return reply({{"steps",json::array({first,find,step("step_3","Run the selected target",{"step_2"})})}});
    },{},[this](const auto&,const std::string& prompt){
        ++plans;
        if(prompt.find("CURRENT_READY_STEP:\nstep_1")!=std::string::npos)return op("step_1","filesystem.inspect",{{"target",root+"/known.txt"}});
        if(prompt.find("CURRENT_READY_STEP:\nstep_3")!=std::string::npos) {
            if(!launches)return op("step_3","application.launch",{{"target","$resolved_target"}});
            return reply({{"status","satisfied"},{"step_id","step_3"},{"verification",{{"source","execution"},{"operation","application.launch"}}}});
        }
        hintSeen |= prompt.find("name_contains")!=std::string::npos && prompt.find("setup")!=std::string::npos;
        if(interpretations>0 && prompt.find("\"user_hints\":[{")!=std::string::npos) {
            ++guidedPlans;
            if(global && !focus)return op("step_2","application.focus",{{"application","Fixture App"}});
            if(global && !keys)return op("step_2","keyboard.type",{{"content","tracker"}});
            if(!global && guidedPlans==1)return op("step_2","filesystem.list_directory",{{"location",root},{"max_results",12}});
            return op("step_2","filesystem.search",{{"location",root},{"query",succeed?"orion tracker":"no match "+std::to_string(guidedPlans)}});
        }
        if(!searches)return op("step_2","filesystem.search",{{"location",root},{"query","orion tracker"}});
        return op("step_2","filesystem.list_directory",{{"location",root},{"max_results",10}});
    },[this](const PrimitiveOperation& operation){
        if(operation.name=="filesystem.inspect"){++prelude;return okay(operation,{{"path",root+"/known.txt"},{"exists",true}});}
        if(operation.name=="filesystem.list_directory"){++lists;return okay(operation,{{"entry_count",0}});}
        if(operation.name=="application.focus"){++focus;return okay(operation);}
        if(operation.name=="keyboard.type"){++keys;return okay(operation);}
        if(operation.name=="application.launch"){++launches;dispatched=operation.parameters.target;return okay(operation);}
        ++searches;json results=json::array();
        if(interpretations>0 && guidedPlans>0 && succeed)results.push_back({{"candidate_id","candidate_1"},{"path",target},{"name","Orion Tracker Setup.exe"},{"parent_path",root},{"entry_type","file"},{"extension",".exe"},{"match_score",0.94}});
        for(const auto& record:records)if(record["status"]=="VERIFIED_SUCCESS")noSuccessBeforeProof=false;
        return okay(operation,{{"category","FILESYSTEM_SEARCH_RESULT"},{"query",invalidProof && interpretations>0?"unrelated":operation.parameters.query},{"search_root",root},{"clear_winner",true},{"results",results}});
    },[this](const auto&,const std::string& input){
        ++interpretations;if(failInterpretations>0){--failInterpretations;return LLMResponse{false,"","fixture timeout",LLMFailureCategory::TIMEOUT};}const auto request=json::parse(input);
        contextSeen=request.contains("context") && request["context"].contains("original_goal") && request["context"].contains("current_step") && request["context"].contains("available_primitives") && request["context"].contains("current_evidence") && request["context"]["strategy_context"].contains("budget");
        const auto answer=request.value("user_answer",std::string{});
        if(answer=="skip permission" || answer=="authorize without yes")return reply({{"decision","GRANT_FOREGROUND"}});
        if(answer=="smuggle an operation")return reply({{"decision","SUGGEST_STRATEGY"},{"strategy","search"},{"required_primitives",json::array({"filesystem.search"})},{"operation",{{"name","filesystem.delete"}}}});
        if(answer=="use an imaginary search")return reply({{"decision","SUGGEST_STRATEGY"},{"strategy","windows_search"},{"required_primitives",json::array({"windows.search"})}});
        if(answer=="filename had setup")return reply({{"decision","ADD_SEARCH_HINT"},{"hint",{{"type","name_contains"},{"value","setup"}}}});
        if(answer=="maybe another folder")return reply({{"decision","ADD_LOCATION_HINT"},{"value","C:/fixture/old"}});
        global=answer=="use the app and keyboard";
        return reply({{"decision","SUGGEST_STRATEGY"},{"strategy",global?"foreground_search":"enumerate_then_search"},{"required_primitives",global?json::array({"application.focus","keyboard.type","filesystem.search"}):json::array({"filesystem.list_directory","filesystem.search"})}});
    }) {
        StrategyRecovery::Policy policy;policy.autonomousAttempts=2;policy.guidedAttempts=4;policy.totalAttempts=12;runtime.setRecoveryPolicy(policy);
        runtime.setRecoverySink([this](const json& record){records.push_back(record);});
    }
    ~Fixture(){runtime.abandonSession();} // Destroy the runtime while captured test records still exist.
    Branch3RuntimeResult start(){return runtime.start("Find the orion tracker installer and run it after confirming the inferred file");}
    size_t count(const std::string& status)const{size_t n=0;for(const auto& record:records)if(record["status"]==status)++n;return n;}
};
int main(){
    int failed=0;auto check=[&](const std::string& name,bool good){std::cout<<name<<": "<<(good?"PASS":"FAIL")<<'\n';if(!good)++failed;};
    {
        Fixture f;const auto paused=f.start();const auto pending=f.runtime.pendingUserDecision();
        check("autonomous_alternatives_precede_guidance",paused.cause==RuntimeCause::STRATEGY_GUIDANCE && paused.outcome==RuntimeOutcome::NEEDS_USER && f.searches==1 && f.lists==1 && f.plans==3);
        check("question_uses_structured_attempts_and_blocker",pending && pending->context["attempts"].size()==2 && pending->intent.find("filesystem.search")!=std::string::npos && pending->intent.find("filesystem.list_directory")!=std::string::npos && pending->context.contains("available_primitives"));
        RuntimeSupervisor supervisor(f.runtime);std::istringstream in("try scanning entries\n");std::ostringstream out;
        auto action=supervisor.readControl(in,out,paused);check("free_form_terminal_answer_is_not_a_new_request",action==SupervisorAction::ANSWER && supervisor.questionId()==pending->questionId);
        const auto confirmed=f.runtime.answerUser(supervisor.questionId(),supervisor.answer());
        check("guidance_interpreter_receives_full_active_context",f.contextSeen);
        check("same_workflow_preserves_verified_prerequisite",f.decompositions==1 && f.routerCalls==0 && f.prelude==1 && confirmed.verifiedProgress.size()==2);
        check("registered_user_strategy_is_applied",f.guidedPlans==2 && f.lists==2 && f.searches==2);
        check("candidate_confirmation_remains_separate",confirmed.cause==RuntimeCause::TARGET_CONFIRMATION && f.runtime.pendingUserDecision()->reason=="TARGET_CONFIRMATION" && f.launches==0);
        check("recovery_is_recorded_only_after_verification",f.count("USER_SUGGESTED")==1 && f.count("ATTEMPTED")==1 && f.count("VERIFIED_SUCCESS")==1 && f.noSuccessBeforeProof);
        const auto complete=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"1");
        check("candidate_choice_resumes_canonical_target_without_rediscovery",complete.success && f.launches==1 && f.dispatched==f.target && f.searches==2 && f.decompositions==1);
        check("episode_tracks_goal_completion",f.records.back()["status"]=="VERIFIED_SUCCESS" && f.records.back()["goal_result"]=="COMPLETED");
    }
    {
        Fixture f;f.start();auto id=f.runtime.pendingUserDecision()->questionId;const auto plans=f.plans;
        auto result=f.runtime.answerUser(id,"use an imaginary search");
        check("unregistered_user_capability_is_rejected_without_execution",result.outcome==RuntimeOutcome::NEEDS_USER && f.plans==plans && f.runtime.pendingUserDecision()->questionId==id && f.focus==0 && f.keys==0 && f.launches==0);
        check("unsupported_suggestion_is_failed_not_learned_success",f.count("FAILED")==1 && f.count("VERIFIED_SUCCESS")==0 && f.records.back()["outcome"]=="unsupported_capability");
        f.runtime.answerUser(id,"I don't know");check("cannot_clarify_retains_question_without_new_planning",f.plans==plans && f.runtime.pendingUserDecision()->questionId==id);
    }
    {
        Fixture f;f.start();f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"filename had setup");
        check("search_hint_enriches_strategy_context",f.hintSeen && f.count("VERIFIED_SUCCESS")==1);
        const auto log=f.records.dump();check("durable_records_exclude_raw_guidance_paths_and_hint_values",log.find("filename had setup")==std::string::npos && log.find("C:/fixture")==std::string::npos && log.find("orion tracker")==std::string::npos && log.find("\"value\"")==std::string::npos);
    }
    {
        Fixture f;f.start();f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"maybe another folder");check("location_hint_is_accepted_as_guidance",f.contextSeen && f.guidedPlans==2 && f.decompositions==1);
    }
    {
        Fixture f;f.succeed=false;f.start();const auto first=f.runtime.pendingUserDecision()->questionId;
        const auto failure=f.runtime.answerUser(first,"try scanning entries");
        check("failed_guidance_has_no_verified_success_record",failure.cause==RuntimeCause::STRATEGY_GUIDANCE && f.count("FAILED")==1 && f.count("VERIFIED_SUCCESS")==0 && f.runtime.pendingUserDecision()->questionId!=first);
        check("strategy_budget_persists_across_answers",f.runtime.pendingUserDecision()->context["attempts"].size()==6 && f.runtime.pendingUserDecision()->context["budget"]["total_remaining"]==6);
    }
    {
        Fixture f;f.start();const auto permission=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"use the app and keyboard");
        check("foreground_recovery_requires_explicit_permission",permission.cause==RuntimeCause::USER_ACTION_REQUIRED && f.runtime.pendingUserDecision()->reason=="USER_ACTION_REQUIRED" && f.focus==0 && f.keys==0);
        check("permission_question_is_not_recovery_success",f.count("USER_SUGGESTED")==1 && f.count("ATTEMPTED")==0 && f.count("VERIFIED_SUCCESS")==0);
        const auto id=f.runtime.pendingUserDecision()->questionId;f.runtime.answerUser("stale", "yes");check("stale_answer_cannot_grant_input_control",f.focus==0 && f.keys==0);
        f.runtime.answerUser(id,"authorize without yes");check("interpreter_cannot_invent_explicit_foreground_consent",f.focus==0 && f.keys==0 && f.runtime.pendingUserDecision()->questionId==id);
        const auto resumed=f.runtime.answerUser(id,"yes");check("permission_resumes_held_strategy_in_same_workflow",f.focus==1 && f.keys==1 && f.decompositions==1 && resumed.cause==RuntimeCause::TARGET_CONFIRMATION && f.count("VERIFIED_SUCCESS")==1);
    }
    {
        Fixture f;f.start();f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"use the app and keyboard");
        const auto denied=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"no");check("declined_foreground_control_runs_no_global_input",f.focus==0 && f.keys==0 && denied.cause==RuntimeCause::STRATEGY_GUIDANCE && f.count("VERIFIED_SUCCESS")==0);
    }
    {
        int plans=0,decompositions=0;json records=json::array();bool context=false;
        auto inspect=step("step_1","Inspect the requested exact resource");inspect["postcondition"]={{"source","filesystem"},{"kind","path_exists"},{"path","C:/fixture/exact.txt"}};
        Branch3Runtime r([&](const auto&,const auto&){++decompositions;return reply({{"steps",json::array({inspect})}});},{},[&](const auto&,const std::string& prompt){if(++plans==1)return reply({{"status","ask_user"},{"message","What exact resource should I inspect?"}});context=prompt.find("sensitive-guidance-sentinel")!=std::string::npos;return op("step_1","filesystem.inspect",{{"target","C:/fixture/exact.txt"}});},[](const auto& operation){return okay(operation,{{"path","C:/fixture/exact.txt"},{"exists",true}});},[](const auto&,const auto&){return reply({{"decision","PROVIDE_INFORMATION"},{"information","sensitive-guidance-sentinel"}});});
        r.setRecoverySink([&](const auto& record){records.push_back(record);});r.start("Inspect the exact resource");const auto question=r.pendingUserDecision();check("missing_information_is_first_class",question && question->reason=="MISSING_INFORMATION");
        const auto done=r.answerUser(question->questionId,"sensitive-guidance-sentinel");check("information_answer_enriches_same_step_without_redecomposition",done.success && decompositions==1 && plans==2 && context);
        check("raw_missing_information_is_not_durable_learning_data",records.dump().find("sensitive-guidance-sentinel")==std::string::npos);
    }
    {
        Fixture f;f.start();const auto id=f.runtime.pendingUserDecision()->questionId;const auto plans=f.plans;
        f.runtime.answerUser(id,"skip permission");f.runtime.answerUser(id,"smuggle an operation");
        check("guidance_cannot_authorize_input_or_smuggle_an_operation",f.plans==plans && f.focus==0 && f.keys==0 && f.launches==0 && f.records.empty() && f.runtime.pendingUserDecision()->questionId==id);
    }
    {
        Fixture f;StrategyRecovery::Policy policy;policy.autonomousAttempts=2;policy.guidedAttempts=4;policy.totalAttempts=4;policy.questions=1;f.runtime.setRecoveryPolicy(policy);f.succeed=false;f.start();
        const auto result=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"try scanning entries");const auto attempts=f.searches+f.lists;
        f.runtime.requestGuidance();
        check("lifetime_attempt_and_question_budgets_cannot_be_reset",result.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && !f.runtime.pendingUserDecision() && attempts==4 && f.searches+f.lists==attempts && f.count("VERIFIED_SUCCESS")==0);
    }
    {
        Fixture f;f.start();f.failInterpretations=1;const auto id=f.runtime.pendingUserDecision()->questionId;const auto plans=f.plans;
        const auto unavailable=f.runtime.answerUser(id,"try scanning entries");
        check("answer_interpreter_preserves_typed_provider_failure_and_question",unavailable.outcome==RuntimeOutcome::EXTERNAL_UNAVAILABLE && unavailable.cause==RuntimeCause::LLM_TIMEOUT && f.runtime.pendingUserDecision()->questionId==id && f.plans==plans && f.records.empty());
        RuntimeSupervisor supervisor(f.runtime);std::istringstream in("R\n");std::ostringstream out;const auto action=supervisor.readControl(in,out,unavailable);
        const auto resumed=f.runtime.retryBlockedDependency();check("dependency_retry_reinterprets_cached_answer_without_workflow_restart",action==SupervisorAction::RETRY && resumed.cause==RuntimeCause::TARGET_CONFIRMATION && f.decompositions==1 && f.prelude==1 && f.interpretations==2);
    }
    {
        Fixture f;f.invalidProof=true;f.start();const auto paused=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"try scanning entries");
        RuntimeSupervisor supervisor(f.runtime);std::istringstream input("A\nmaybe another folder\n");std::ostringstream output;
        const auto action=supervisor.readControl(input,output,paused);
        check("evidence_pause_can_request_guidance_separately_from_continue",action==SupervisorAction::ANSWER && f.runtime.pendingUserDecision() && f.runtime.pendingUserDecision()->reason=="STRATEGY_GUIDANCE" && supervisor.answer()=="maybe another folder");
    }
    {
        Fixture f;f.invalidProof=true;f.start();const auto result=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"try scanning entries");
        check("candidate_discovery_alone_does_not_create_verified_recovery",result.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && f.count("FAILED")==1 && f.count("VERIFIED_SUCCESS")==0 && f.launches==0);
    }
    {
        int plans=0,inspections=0;json records=json::array();
        Branch3Runtime runtime([](const auto&,const auto&){return reply({{"steps",json::array({step("step_1","Inspect the selected resource")})}});},{},[&](const auto&,const auto&){
            if(++plans==1)return op("step_1","filesystem.inspect",{{"target","C:/fixture/known.txt"}});
            if(plans==2)return reply({{"status","ask_user"},{"message","Which observed resource?"}});
            return reply({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","execution"},{"operation","filesystem.inspect"},{"field","exists"},{"equals",true}}}});
        },[&](const auto& operation){++inspections;return okay(operation,{{"exists",true},{"path","C:/fixture/known.txt"}});},[](const auto&,const auto&){return reply({{"decision","PROVIDE_INFORMATION"},{"information","Use the inspected resource"}});});
        runtime.setRecoverySink([&](const auto& record){records.push_back(record);});runtime.start("Inspect the selected resource");
        const auto result=runtime.answerUser(runtime.pendingUserDecision()->questionId,"Use the inspected resource");
        bool success=false;for(const auto& record:records)success |= record["status"]=="VERIFIED_SUCCESS";
        check("guidance_can_unlock_retained_evidence_without_replaying_observation",result.success && inspections==1 && plans==3 && success);
    }
    {
        Fixture f;f.runtime.setRecoverySink([](const auto&){throw std::runtime_error("test sink unavailable");});f.start();const auto result=f.runtime.answerUser(f.runtime.pendingUserDecision()->questionId,"try scanning entries");check("learning_log_failure_does_not_change_verifier_or_workflow",result.cause==RuntimeCause::TARGET_CONFIRMATION && f.launches==0);
    }
    {
        const auto root=std::filesystem::temp_directory_path()/("NovaRecoveryLog_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if(!std::filesystem::create_directory(root))return 2;const auto log=root/"episodes.jsonl";std::ofstream(log,std::ios::binary)<<std::string(1024*1024,'x');
        const bool wrote=StrategyRecovery::append(log,{{"status","FAILED"},{"episode_id","fixture"}});
        check("local_recovery_log_rotates_at_bounded_size",wrote && std::filesystem::exists(root/"episodes.jsonl.1") && std::filesystem::file_size(log)<1024);
        std::filesystem::remove_all(root);
    }
    std::cout<<"Failures: "<<failed<<'\n';return failed?1:0;
}
