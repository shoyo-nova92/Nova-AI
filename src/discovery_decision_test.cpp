#include "branch3_runtime.hpp"
#include "runtime_supervisor.hpp"
#include "filesystem_ranking.hpp"
#include "semantic_evidence.hpp"
#include "application_correlation.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
using nlohmann::json;
namespace fs=std::filesystem;
LLMResponse reply(json j){return {true,j.dump(),""};}
LLMResponse action(std::string step,std::string name,json params){return reply({{"status","operation"},{"step_id",step},{"operation",{{"name",name},{"parameters",params}}}});}
LLMResponse proof(std::string step,json p){return reply({{"status","satisfied"},{"step_id",step},{"verification",p}});}
ExecutionResult success(const PrimitiveOperation& op,json facts=json::object()){ExecutionResult r{};r.action=op.name;r.status=ExecutionStatus::SUCCESS;r.factsJson=facts.dump();return r;}
int main(){
 int failures=0;auto check=[&](const char* name,bool okay){std::cout<<name<<": "<<(okay?"PASS":"FAIL")<<'\n';if(!okay)++failures;};
 check("token_order_independent",FilesystemRanking::rank("tracker comet","Comet Tracker - Setup.exe")["match_score"].get<double>()>=0.9);
 check("joined_tokens_are_discoverable",FilesystemRanking::rank("comet tracker","CometTrackerOld.exe")["match_score"].get<double>()>=0.8);
 check("punctuation_normalized",FilesystemRanking::rank("comet tracker","COMET_Tracker-Setup.exe")["match_score"].get<double>()>=0.9);
 check("close_scores_are_ambiguous",!FilesystemRanking::clear(json::array({json{{"match_score",0.94}},json{{"match_score",0.93}}})));
 const std::string first="C:/fixture/Comet Tracker - Setup.exe",second="C:/fixture/Comet Tracker Old.exe";
 auto candidates=[&](bool close){return json::array({json{{"candidate_id","candidate_1"},{"path",first},{"name","Comet Tracker - Setup.exe"},{"match_score",0.94},{"extension",".exe"},{"entry_type","file"},{"parent_path","C:/fixture"}},json{{"candidate_id","candidate_2"},{"path",second},{"name","Comet Tracker Old.exe"},{"match_score",close?0.93:0.54},{"extension",".exe"},{"entry_type","file"},{"parent_path","C:/fixture"}}});};
 json steps=json::array({json{{"id","step_1"},{"description","Find requested file"}},json{{"id","step_2"},{"description","Execute selected file"},{"depends_on",json::array({"step_1"})}}});
 for(bool close:{false,true}) {
  int decompositions=0,plans=0,searches=0,launches=0;std::string launched;bool selectedInLedger=false;
  Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply({{"steps",steps}});},{},[&](const auto&,const std::string& state){
   ++plans;if(plans==1)return action("step_1","filesystem.search",{{"location","C:/fixture"},{"query","comet tracker"}});
   selectedInLedger |= state.find("user_confirmed")!=std::string::npos;
   if(plans==2)return proof("step_1",{{"source","filesystem"},{"kind","search_result_matches"},{"extension",".exe"}});
   if(plans==3)return action("step_2","application.launch",{{"target","$resolved_target"}});
   return proof("step_2",{{"source","execution"},{"operation","application.launch"},{"field","operation_succeeded"},{"equals",true}});
  },[&](const PrimitiveOperation& op){
   if(op.name=="filesystem.search"){++searches;return success(op,{{"category","FILESYSTEM_SEARCH_RESULT"},{"query","comet tracker"},{"search_root","C:/fixture"},{"results",candidates(close)},{"clear_winner",!close}});}
   if(op.name=="application.launch"){++launches;launched=op.parameters.target;}return success(op);
  },[](const auto&,const std::string& prompt){return reply({{"decision","SELECT_CANDIDATE"},{"candidate_id",prompt.find("no the older one")!=std::string::npos?"candidate_2":"invented"}});});
  auto paused=runtime.start("Find the comet tracker and run it");auto pending=runtime.pendingUserDecision();
  check(close?"ambiguous_search_pauses":"consequential_inferred_target_pauses",pending && paused.outcome==RuntimeOutcome::NEEDS_USER && launches==0);
  const auto beforePlans=plans;
  if(close){runtime.answerUser(pending->questionId,"yes");check("ambiguous_yes_cannot_pick_candidate",launches==0 && plans==beforePlans);}
  runtime.continueSession();check("continue_cannot_answer_question",plans==beforePlans && launches==0);
  runtime.answerUser("stale-question","1");check("stale_question_rejected",plans==beforePlans && launches==0);
  runtime.answerUser(pending->questionId,"unrecognized response");check("invented_candidate_rejected",plans==beforePlans && launches==0 && runtime.pendingUserDecision().has_value());
  runtime.answerUser(pending->questionId,"show me the others");check("show_matches_retains_workflow",plans==beforePlans && searches==1);
  RuntimeSupervisor supervisor(runtime);std::istringstream input(close?"A\nthe second one\n":"A\nno the older one\n");std::ostringstream output;
  const auto control=supervisor.readControl(input,output,paused);
  check("terminal_answer_maps_active_question",control==SupervisorAction::ANSWER && supervisor.questionId()==pending->questionId && output.str().find("[2] Use")!=std::string::npos);
  const auto done=runtime.answerUser(supervisor.questionId(),supervisor.answer());
  check("selection_resumes_without_routing_search_or_decomposition",done.success && decompositions==1 && searches==1 && launches==1 && launched==second && !runtime.pendingUserDecision());
  check("selected_identity_is_structured_evidence",selectedInLedger);
 }
 {
  int plans=0,launches=0;Branch3Runtime runtime([&](const auto&,const auto&){return reply({{"steps",steps}});},{},[&](const auto&,const auto&){
   if(++plans==1)return action("step_1","filesystem.search",{{"location","C:/fixture"},{"query","comet tracker"}});
   if(plans==2)return proof("step_1",{{"source","filesystem"},{"kind","search_has_results"}});
   if(plans==3)return action("step_2","application.launch",{{"target",first}});
   return proof("step_2",{{"source","execution"},{"operation","application.launch"}});
  },[&](const auto& op){if(op.name=="filesystem.search")return success(op,{{"category","FILESYSTEM_SEARCH_RESULT"},{"results",candidates(false)},{"clear_winner",true}});++launches;return success(op);});
  check("explicit_canonical_target_needs_no_extra_confirmation",runtime.start("Run "+first).success && launches==1 && !runtime.pendingUserDecision());
 }
 {
  int plans=0;Branch3Runtime runtime([&](const auto&,const auto&){return reply({{"steps",json::array({steps[0]})}});},{},[&](const auto&,const auto&){if(++plans==1)return action("step_1","filesystem.search",{{"location","C:/fixture"},{"query","comet tracker"}});return proof("step_1",{{"source","filesystem"},{"kind","search_has_results"}});},[&](const auto& op){return success(op,{{"category","FILESYSTEM_SEARCH_RESULT"},{"results",candidates(false)},{"clear_winner",true}});});
  check("clear_read_only_discovery_has_no_confirmation",runtime.start("Find comet tracker").success && !runtime.pendingUserDecision());
 }
 {
  int searches=0,plans=0,decompositions=0;
  Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply({{"steps",json::array({steps[0]})}});},{},[&](const auto&,const auto&){++plans;if(plans==1)return action("step_1","filesystem.search",{{"location","C:/fixture"},{"query","comet"}});return proof("step_1",{{"source","filesystem"},{"kind","search_has_results"}});},[&](const auto& op){++searches;return success(op,{{"category","FILESYSTEM_SEARCH_RESULT"},{"results",candidates(true)},{"clear_winner",false}});});
  runtime.start("Find comet");const auto old=runtime.pendingUserDecision()->questionId;runtime.answerUser(old,"search again");
  check("explicit_search_again_resumes_same_step",searches==2 && plans==1 && decompositions==1 && runtime.pendingUserDecision()->questionId!=old);
  check("refined_choice_finishes_original_search_step",runtime.answerUser(runtime.pendingUserDecision()->questionId,"1").success && searches==2);
 }
 {
  int calls=0;Branch3Runtime runtime([&](const auto&,const auto&){return reply({{"steps",json::array({steps[0]})}});},{},[&](const auto&,const auto&){if(++calls==1)return reply({{"status","ask_user"},{"message","Which resource?"}});if(calls==2)return action("step_1","filesystem.inspect",{{"target","C:/fixture/exact.txt"}});return proof("step_1",{{"source","filesystem"},{"kind","path_exists"},{"path","C:/fixture/exact.txt"}});},[](const auto& op){return success(op,{{"path","C:/fixture/exact.txt"},{"exists",true}});});
  runtime.start("Inspect the resource");check("missing_information_has_retained_question",runtime.pendingUserDecision()->reason=="MISSING_INFORMATION");
  check("information_answer_resumes_same_workflow",runtime.answerUser(runtime.pendingUserDecision()->questionId,"Use exact.txt").success);
 }
 {
  workflow::Evidence evidence;evidence.operation="filesystem.search";evidence.facts={{"operation_succeeded",true},{"category","FILESYSTEM_SEARCH_RESULT"},{"results",json::array()}};std::string error;
  check("successful_empty_search_does_not_prove_result",!SemanticEvidence::matches(evidence,{{"source","filesystem"},{"kind","search_has_results"}},error));
  evidence.facts["results"]=candidates(false);
  check("search_predicate_matches_same_row",!SemanticEvidence::matches(evidence,{{"source","filesystem"},{"kind","search_result_matches"},{"path",first},{"name","Comet Tracker Old.exe"}},error));
  check("canonical_search_predicate",SemanticEvidence::matches(evidence,{{"source","filesystem"},{"kind","search_result_matches"},{"path",first},{"parent_path","C:/fixture"},{"extension",".exe"},{"entry_type","file"}},error));
  workflow::FactStore store;evidence.stepId="search";auto oldId=store.add(evidence).id;
  workflow::Evidence selection;selection.stepId="search";selection.operation="user.select_candidate";selection.facts={{"search_evidence_id",oldId}};auto selectedId=store.add(selection).id;
  const auto newer=store.add(evidence).id;
  check("refinement_supersedes_old_search_and_selection_currency",!store.find(oldId)->current && !store.find(selectedId)->current && store.find(newer)->current && store.find(oldId)->facts["results"].size()==2);
  check("unknown_search_constraint_rejected",SemanticEvidence::compile({{"source","filesystem"},{"kind","search_result_matches"},{"match",{{"invented",true}}}},error).empty());
  evidence.operation="filesystem.inspect";evidence.facts={{"operation_succeeded",true},{"path",first},{"exists",false}};
  check("path_missing_is_explicit_observation",SemanticEvidence::matches(evidence,{{"source","filesystem"},{"kind","path_missing"},{"path",first}},error));
 }
 {
  // Only test-owned temporary files; no user folders, installers, applications, or network.
  const auto root=fs::temp_directory_path()/("NovaDiscovery_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directory(root);
  std::ofstream(root/"Comet Tracker - Setup.exe")<<"inert test fixture";std::ofstream(root/"Other.pdf")<<"fixture";
  PrimitiveOperation search{};search.name="filesystem.search";search.type=PrimitiveOperationType::FILESYSTEM_SEARCH;search.resolved=true;
  search.parameters.location=Utf8::path(root);search.parameters.query="tracker comet";search.parameters.extension=".exe";
  const auto found=PrimitiveExecutionEngine{}.execute(search);const auto facts=json::parse(found.factsJson);
  check("backend_ranked_search_metadata",found.status==ExecutionStatus::SUCCESS && facts["results"].size()==1 && facts["clear_winner"]==true && facts["results"][0].contains("modified_time_ticks") && facts["results"][0]["match_reasons"].size()>=3);
  workflow::Evidence observed;observed.operation="filesystem.search";observed.facts=facts;observed.facts["operation_succeeded"]=true;std::string error;
  const json predicate={{"source","filesystem"},{"kind","search_result_matches"},{"match",{{"query","tracker comet"},{"search_root",Utf8::path(root)+"/"},{"entry_type","file"},{"extension","exe"}}}};
  check("actual_backend_evidence_verifies_undotted_extension_and_root_separator",SemanticEvidence::matches(observed,predicate,error));
  search.parameters.query="*";search.parameters.extension=".pdf";search.parameters.modifiedWithinDays="1";
  const auto recent=json::parse(PrimitiveExecutionEngine{}.execute(search).factsJson);check("generic_type_and_recency_search",recent["results"].size()==1 && recent["results"][0]["name"]=="Other.pdf");
  fs::remove_all(root);
 }
 std::cout<<"Failures: "<<failures<<'\n';return failures?1:0;
}
