#include "branch3_runtime.hpp"
#include "predicate_registry.hpp"
#include "semantic_evidence.hpp"
#include <iostream>
using nlohmann::json;
LLMResponse reply(json value){return {true,value.dump(),""};}
json step(const std::string& id,const std::string& intent,std::vector<std::string> deps={}){return {{"id",id},{"description",intent},{"depends_on",deps}};}
LLMResponse operation(const std::string& id,const std::string& name,json params){return reply({{"status","operation"},{"step_id",id},{"operation",{{"name",name},{"parameters",params}}}});}
LLMResponse satisfied(const std::string& id,json proof){return reply({{"status","satisfied"},{"step_id",id},{"verification",proof}});}
ExecutionResult success(const PrimitiveOperation& op,json facts=json::object()){ExecutionResult out{};out.status=ExecutionStatus::SUCCESS;out.action=op.name;out.factsJson=facts.dump();return out;}
int main(){
 int failures=0;auto check=[&](const std::string& name,bool okay){std::cout<<name<<": "<<(okay?"PASS":"FAIL")<<'\n';if(!okay)++failures;};
 const std::string root="C:/fixture",path=root+"/Nebula Tracker - Package.exe";
 const json searchProof={{"source","filesystem"},{"kind","search_result_matches"},{"match",{{"query","nebula tracker"},{"search_root",root},{"entry_type","file"},{"extension",".exe"},{"modified_within_days",14}}}};
 const json searchFacts={{"category","FILESYSTEM_SEARCH_RESULT"},{"query","nebula tracker"},{"search_root",root},{"clear_winner",true},
  {"results",json::array({json{{"candidate_id","candidate_1"},{"name","Nebula Tracker - Package.exe"},{"path",path},{"parent_path",root},{"entry_type","file"},{"extension",".exe"},{"modified_age_days",13.0},{"match_score",0.94},{"match_reasons",json::array({"token_match","extension_match","recency_match"})}}})}};
 auto discovery=step("step_1","Find matching executable in the specified root and time window");discovery["postcondition"]=searchProof;
 discovery["postcondition"]["match"]["extension"]="exe";discovery["postcondition"]["match"]["search_root"]="c:\\FIXTURE\\";
 auto steps=json::array({discovery,step("step_2","Open its containing directory",{"step_1"}),step("step_3","Execute the selected file",{"step_2"})});
 // The declared search verifies before any second planner call for that step.
 {
  int decompositions=0,plans=0,searches=0,opens=0,launches=0,searchPlans=0;bool schemas=false;std::string dispatched;
  Branch3Runtime runtime([&](const auto&,const std::string& prompt){++decompositions;schemas=prompt.find("CANONICAL_POSTCONDITION_REGISTRY")!=std::string::npos && prompt.find("modified_within_days")!=std::string::npos;return reply({{"steps",steps}});},{},[&](const auto&,const std::string& prompt){
   if(prompt.find("CURRENT_READY_STEP:\nstep_1")!=std::string::npos)++searchPlans;
   switch(++plans){
    case 1:return operation("step_1","filesystem.search",{{"location",root},{"query","nebula tracker"},{"entry_type","file"},{"extension",".exe"},{"modified_within_days",14}});
    case 2:return operation("step_2","application.launch",{{"target","$resolved_parent"}});
    case 3:return satisfied("step_2",{{"source","execution"},{"operation","application.launch"},{"field","operation_succeeded"},{"equals",true}});
    case 4:return operation("step_3","application.launch",{{"target","$resolved_target"}});
    default:return satisfied("step_3",{{"source","execution"},{"operation","application.launch"}});
   }
  },[&](const PrimitiveOperation& op){if(op.name=="filesystem.search"){++searches;return success(op,searchFacts);}if(op.parameters.target==root)++opens;else{++launches;dispatched=op.parameters.target;}return success(op);},[](const auto&,const auto&){return reply({{"decision","SELECT_CANDIDATE"},{"candidate_id","candidate_1"}});});
  const auto paused=runtime.start("Find the matching package, open its directory, and execute it");
  check("decomposition_receives_canonical_schema",schemas);
  check("declared_search_needs_no_planner_satisfaction",searchPlans==1 && plans==4 && paused.outcome==RuntimeOutcome::NEEDS_USER && paused.cause==RuntimeCause::TARGET_CONFIRMATION && searches==1 && opens==1 && launches==0);
  bool verified=false;for(const auto& progress:paused.verifiedProgress)verified |= progress.find("step_1: VERIFIED")!=std::string::npos;
  check("search_verified_before_execution_confirmation",verified && runtime.pendingUserDecision() && runtime.pendingUserDecision()->candidates[0]["path"]==path);
  const auto pending=runtime.pendingUserDecision();
  if(pending){const auto completed=runtime.answerUser(pending->questionId,"yeah that's it");check("natural_answer_resumes_retained_canonical_target",completed.success && decompositions==1 && searches==1 && opens==1 && launches==1 && dispatched==path);}
  else check("natural_answer_resumes_retained_canonical_target",false);
  check("proof_origin_is_diagnostic",paused.stateLog.find("origin=runtime")!=std::string::npos && paused.stateLog.find("contract=declared")!=std::string::npos);
 }
 for(bool exists:{false,true}) {
  int plans=0,executions=0;auto inspect=step("step_1","Observe exact path");inspect["postcondition"]={{"source","filesystem"},{"kind","path_exists"},{"path",path}};
  Branch3Runtime runtime([&](const auto&,const auto&){return reply({{"steps",json::array({inspect})}});},{},[&](const auto&,const auto&){++plans;return operation("step_1","filesystem.inspect",{{"target",path}});},[&](const auto& op){++executions;return success(op,{{"path",path},{"exists",exists}});});
  const auto result=runtime.start("Observe the requested exact path");
  check(exists?"declared_inspect_verifies_after_operation":"failed_declared_inspect_pauses_without_satisfaction_calls",plans==1 && executions==1 && (exists?result.success:!result.success && result.cause==RuntimeCause::EVIDENCE_UNAVAILABLE));
 }
 {
  int plans=0,observations=0;auto browser=step("step_1","Navigate to requested page");browser["postcondition"]={{"source","browser"},{"kind","url_equals"},{"value","https://fixture.invalid/"}};
  Branch3Runtime runtime([&](const auto&,const auto&){return reply({{"steps",json::array({browser})}});},{},[&](const auto&,const auto&){++plans;return operation("step_1","browser.navigate",{{"url","https://fixture.invalid/"},{"ready_condition","load"}});},[&](const auto& op){auto result=success(op);if(op.name=="browser.observe"){++observations;result.browserData=json{{"url","https://fixture.invalid/"},{"auth_required",false}}.dump();}return result;});
  check("declared_browser_contract_uses_fresh_host_observation_before_planner",runtime.start("Navigate to requested page").success && plans==1 && observations==1);
 }
 // Unknown/action-shaped declarations are rejected across the entire plan before execution.
 for(const auto legacy:{"execution.filesystem.search","execution.application.launch","execution.browser.navigate","execution.filesystem.delete","filesystem.invented"}) {
  int decompositions=0,plans=0,executions=0;auto bad=steps;bad[2]["postcondition"]={{"kind",legacy}};
  Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply({{"steps",bad}});},{},[&](const auto&,const auto&){++plans;return operation("step_1","filesystem.search",{{"location",root},{"query","nebula tracker"}});},[&](const auto& op){++executions;return success(op,searchFacts);});
  const auto rejected=runtime.start("Find and execute the requested file");
  check(std::string("invalid_declared_predicate_fails_before_any_operation_")+legacy,!rejected.success && rejected.cause==RuntimeCause::INVALID_PLAN && decompositions==2 && plans==0 && executions==0);
 }
 {
  int decompositions=0,executions=0,plans=0;bool feedback=false;
  Branch3Runtime runtime([&](const auto&,const std::string& prompt){++decompositions;auto only=discovery;if(decompositions==1)only["postcondition"]={{"kind","execution.filesystem.search"}};else feedback=prompt.find("Unsupported verification predicate: execution.filesystem.search")!=std::string::npos && executions==0;return reply({{"steps",json::array({only})}});},{},[&](const auto&,const auto&){if(++plans==1)return operation("step_1","filesystem.search",{{"location",root},{"query","nebula tracker"},{"entry_type","file"},{"extension",".exe"},{"modified_within_days",14}});return satisfied("step_1",searchProof);},[&](const auto& op){++executions;return success(op,searchFacts);});
  const auto completed=runtime.start("Find the matching recent executable");
  check("one_bounded_decomposition_repair_precedes_execution",completed.success && decompositions==2 && feedback && executions==1);
 }
 // Undeclared legacy guides get one proof-only repair; no second search or decomposition.
 {
  int decompositions=0,plans=0,searches=0;bool repair=false;
  Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return reply({{"steps",json::array({step("step_1","Find the requested file")})}});},{},[&](const auto&,const std::string& prompt){++plans;if(plans==1)return operation("step_1","filesystem.search",{{"location",root},{"query","nebula tracker"}});if(plans==2)return satisfied("step_1",{{"source","execution"},{"operation","filesystem.search"}});repair=prompt.find("Planner verification schema repair (one attempt)")!=std::string::npos;return satisfied("step_1",searchProof);},[&](const auto& op){++searches;return success(op,searchFacts);});
  check("legacy_planner_contract_repairs_from_existing_evidence",runtime.start("Find the requested file").success && decompositions==1 && searches==1 && plans==3 && repair);
 }
 {
  workflow::Evidence evidence;evidence.operation="filesystem.search";evidence.facts=searchFacts;evidence.facts["operation_succeeded"]=true;std::string error;
  check("complete_search_constraints_match_one_result",SemanticEvidence::matches(evidence,searchProof,error));
  auto canonical=searchProof;canonical["match"]["path"]=path;check("canonical_path_search_constraint",SemanticEvidence::matches(evidence,canonical,error));
  auto uppercase=evidence;uppercase.facts["results"][0]["extension"]=".EXE";check("extension_contract_follows_case_insensitive_search",SemanticEvidence::matches(uppercase,searchProof,error));
  auto reverse=searchProof;reverse["match"]["query"]="tracker nebula";check("query_contract_respects_token_order_independence",SemanticEvidence::matches(evidence,reverse,error));
  for(const auto field:{"entry_type","extension","search_root","query","modified_within_days","path"}) {
   auto wrong=canonical;if(std::string(field)=="modified_within_days")wrong["match"][field]=7;else wrong["match"][field]=std::string(field)=="entry_type"?"directory":"different";
   check(std::string("wrong_search_constraint_rejected_")+field,!SemanticEvidence::matches(evidence,wrong,error));
  }
  auto missing=evidence;missing.facts["results"][0].erase("modified_age_days");check("recency_requires_backend_metadata",!SemanticEvidence::matches(missing,searchProof,error));
  auto empty=evidence;empty.facts["results"]=json::array();check("successful_empty_search_cannot_prove_discovery",!SemanticEvidence::matches(empty,searchProof,error));
  auto shape=searchProof;shape["match"]["extension"]="EXE";shape["match"]["search_root"]="c:\\FIXTURE\\.\\";
  shape["match"]["parent_path"]="C:/Fixture/sub/../";shape["match"]["path"]="c:\\fixture\\Nebula Tracker - Package.exe";
  const auto before=evidence.facts.dump();
  check("extension_and_canonical_windows_path_spellings",SemanticEvidence::matches(evidence,shape,error));
  check("semantic_normalization_preserves_original_evidence",evidence.facts.dump()==before);
  auto strict=shape;strict["match"]["name"]="nebula tracker - package.exe";check("name_constraint_is_not_arbitrarily_case_folded",!SemanticEvidence::matches(evidence,strict,error));
  strict=shape;strict["match"]["extension"]="e-xe";check("arbitrary_extension_punctuation_is_not_weakened",!SemanticEvidence::matches(evidence,strict,error));
  auto separate=evidence;separate.facts["entry_type"]="file";separate.facts["extension"]=".exe";separate.facts["results"][0].erase("extension");
  check("candidate_constraints_cannot_use_flat_search_metadata",!SemanticEvidence::matches(separate,searchProof,error) && error.find("extension")!=std::string::npos);
  auto rootOnly=searchProof;rootOnly["match"]={{"search_root",root}};
  auto outside=evidence;outside.facts["results"][0]["path"]="C:/fixture-sibling/Nebula Tracker - Package.exe";
  check("root_membership_uses_path_components",!SemanticEvidence::matches(outside,rootOnly,error));
  auto nested=evidence;nested.facts["results"][0]["path"]=root+"/nested/Nebula Tracker - Package.exe";nested.facts["results"][0]["parent_path"]=root+"/nested";
  check("root_relationship_accepts_recursive_descendants",SemanticEvidence::matches(nested,rootOnly,error));
  auto unc=evidence;unc.facts["search_root"]="\\\\SERVER\\Share\\Folder";unc.facts["results"][0]["path"]="\\\\SERVER\\Share\\Folder\\Nebula Tracker - Package.exe";
  auto uncProof=rootOnly;uncProof["match"]["search_root"]="//server/share/folder/";
  check("unc_root_case_and_separator_normalization",SemanticEvidence::matches(unc,uncProof,error));
  auto unicode=evidence;unicode.facts["search_root"]="C:/Données";unicode.facts["results"][0]["path"]="C:/Données/Nebula Tracker - Package.exe";
  auto unicodeProof=rootOnly;unicodeProof["match"]["search_root"]="c:/DONNÉES/";
  check("unicode_windows_path_comparison_is_case_insensitive",SemanticEvidence::matches(unicode,unicodeProof,error));
  auto complete=searchProof;complete["match"]["search_complete"]=true;
  check("explicit_completeness_requires_search_metadata",!SemanticEvidence::matches(evidence,complete,error));
  auto completed=evidence;completed.facts["search_complete"]=true;check("completeness_is_read_from_enclosing_observation",SemanticEvidence::matches(completed,complete,error));
  completed.facts["search_complete"]=false;check("incomplete_search_cannot_prove_declared_completeness",!SemanticEvidence::matches(completed,complete,error));
  auto mixed=evidence;auto other=mixed.facts["results"][0];mixed.facts["results"][0]["extension"]=".pdf";other["entry_type"]="directory";mixed.facts["results"].push_back(other);
  check("candidate_constraints_must_hold_on_one_record",!SemanticEvidence::matches(mixed,searchProof,error));
  mixed.facts["results"].push_back(evidence.facts["results"][0]);check("a_later_candidate_can_satisfy_entire_contract",SemanticEvidence::matches(mixed,searchProof,error));
  workflow::Step declared;declared.id="one";workflow::Postcondition invalid;invalid.kind=workflow::PredicateKind::SEMANTIC;invalid.value={{"source","execution"},{"kind","filesystem.search"}};declared.postconditions={invalid};
  workflow::Kernel rejected({declared});check("graph_admission_rejects_unsupported_semantic_contract",!rejected.graph.valid() && !rejected.next());
  invalid.value=searchProof;declared.postconditions={invalid};workflow::Kernel accepted({declared});check("graph_admission_accepts_registered_semantic_contract",accepted.graph.valid());
 }
 for(const auto bad:{0,1,2,3,4}) {
  int plans=0,effects=0;auto wrongFacts=searchFacts;
  if(bad==0)wrongFacts["results"][0]["modified_age_days"]=20;
  if(bad==1)wrongFacts["query"]="different words";
  if(bad==2)wrongFacts["search_root"]="C:/other";
  if(bad==3)wrongFacts["results"][0]["extension"]=".pdf";
  if(bad==4)wrongFacts["results"]=json::array();
  Branch3Runtime runtime([&](const auto&,const auto&){return reply({{"steps",steps}});},{},[&](const auto&,const auto&){if(++plans==1)return operation("step_1","filesystem.search",{{"location",root},{"query","nebula tracker"}});return satisfied("step_1",{{"source","execution"},{"operation","filesystem.search"}});},[&](const auto& op){if(op.name!="filesystem.search")++effects;return success(op,wrongFacts);});
  const auto paused=runtime.start("Find, open and execute matching file");check("declared_contract_never_weakens_to_search_success_"+std::to_string(bad),!paused.success && effects==0 && (bad==4?runtime.pendingUserDecision().has_value() && paused.cause==RuntimeCause::STRATEGY_GUIDANCE:!runtime.pendingUserDecision() && plans==1 && paused.cause==RuntimeCause::EVIDENCE_UNAVAILABLE));
 }
 std::cout<<"Failures: "<<failures<<'\n';return failures?1:0;
}
