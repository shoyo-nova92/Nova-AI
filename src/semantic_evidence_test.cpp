#include "semantic_evidence.hpp"
#include "application_correlation.hpp"
#include <iostream>
using nlohmann::json;
namespace wf=workflow;
int main(){int checks=0,failures=0;auto check=[&](std::string name,bool okay){++checks;if(!okay)++failures;std::cout<<name<<": "<<(okay?"PASS":"FAIL")<<'\n';};
 json page={{"url","https://docs.example.test/releases/"},{"title","Product releases"},{"readable_text","Stable release 7.4"},{"visible_text","Stable release 7.4 unsent draft"},{"elements",json::array({json{{"id","ephemeral-1"},{"role","link"},{"text","Release 7.4"},{"accessible_name","Stable download"},{"href","https://docs.example.test/releases/7.4"},{"enabled",true},{"visible",true},{"value",nullptr}},json{{"id","ephemeral-2"},{"role","textbox"},{"value","unsent draft"},{"editable",true},{"visible",true},{"enabled",true}}})}};
 auto observed=SemanticEvidence::browserObservation("a",page.dump());
 auto matches=[&](json proof){std::string error;return SemanticEvidence::matches(observed,proof,error);};
 for(const auto& item:std::vector<std::pair<std::string,std::string>>{{"url_equals","https://docs.example.test/releases/"},{"url_contains","/releases/"},{"title_equals","Product releases"},{"title_contains","releases"},{"text_contains","Stable release 7.4"},{"input_equals","unsent draft"}})check(item.first,matches({{"source","browser"},{"kind",item.first},{"value",item.second}}));
 json proof={{"source","browser"},{"kind","element_matches"},{"match",{{"role","link"},{"text","Release 7.4"},{"href","https://docs.example.test/releases/7.4"},{"enabled",true},{"visible",true},{"value",nullptr}}}};
 check("LINK_TEXT_AND_HREF_CROSS_BOUNDARY",matches(proof));
 check("FLAT_ELEMENT_SCHEMA",matches({{"source","browser"},{"kind","element_matches"},{"role","link"},{"accessible_name","Stable download"}}));
 check("ELEMENT_EXISTS",matches({{"source","browser"},{"kind","element_exists"},{"match",{{"role","textbox"}}}}));
 check("LINK_EXISTS",matches({{"source","browser"},{"kind","link_exists"},{"href","https://docs.example.test/releases/7.4"}}));
 check("DOTTED_GENERIC_KIND",matches({{"source","browser"},{"kind","browser.title_contains"},{"value","Product"}}));
 auto wrong=proof;wrong["match"]["href"]="https://wrong.example/";check("WRONG_HREF_REJECTED",!matches(wrong));
 wrong=proof;wrong["match"]["role"]="textbox";check("CROSS_ELEMENT_CONSTRAINTS_REJECTED",!matches(wrong));
 wrong=proof;wrong["match"]["enabled"]=false;check("WRONG_BOOLEAN_REJECTED",!matches(wrong));
 check("DRAFT_NOT_SUBMISSION",!matches({{"source","browser"},{"kind","text_contains"},{"value","unsent draft"}}));
 check("OBSERVATION_IDS_REMOVED_FROM_DURABLE_FACTS",!observed.facts["elements"][0].contains("id"));
 std::string error;wrong=proof;wrong["match"]["id"]="ephemeral-1";check("IDS_NOT_ALLOWED_AS_PROOF",SemanticEvidence::compile(wrong,error).empty());
 wrong=proof;wrong["match"]["enabled"]="true";check("UNTYPED_BOOLEAN_NOT_ACCEPTED",SemanticEvidence::compile(wrong,error).empty());
 wrong=proof;wrong["kind"]="invented";check("UNKNOWN_PREDICATE_EXPLICIT",SemanticEvidence::compile(wrong,error).empty() && error.find("Unsupported verification predicate")!=std::string::npos);
 json app={{"processes",json::array({json{{"process_id",42},{"process_name","editor.exe"}}})},{"windows",json::array({json{{"process_id",42},{"process_name","editor.exe"},{"window_handle","12345"},{"window_visible",true},{"window_title","Review document — Editor"},{"foreground",true},{"target_path","C:/work/review.txt"},{"target_observed",true},{"target_evidence",{{"association","exact_document_path"},{"source","document_or_tab_help_text"},{"observed_path","C:/work/review.txt"}}}}})}};
 auto appEvidence=SemanticEvidence::applicationObservation("a",app);
 auto appMatches=[&](json p){std::string e;return SemanticEvidence::matches(appEvidence,p,e);};
 for(const auto& kind:{"process_exists","window_exists","window_visible","foreground"})check(std::string("APPLICATION_")+kind,appMatches({{"source","application"},{"kind",kind},{"process_id",42}}));
 check("APPLICATION_TITLE_CONTAINS",appMatches({{"source","application"},{"kind","window_title_contains"},{"process_name","editor.exe"},{"value","Review"}}));
 json targetProof={{"source","application"},{"kind","target_observed"},{"process_id",42},{"target_path","c:\\work\\review.txt"}};
 check("EXACT_OBSERVED_TARGET_ASSOCIATION",appMatches(targetProof));
 json dispatchFacts={{"operation_succeeded",true},{"dispatch_accepted",true},{"target_resolved",true},
    {"canonical_requested_target","C:/work/review.txt"},{"target_path","C:/work/review.txt"},
    {"runtime_session","fixture"},{"launch_id","launch-1"},{"launch_started_100ns",100},{"launch_completed_100ns",110},
    {"process_id",42},{"process_created_100ns",105},{"process_image_path","C:/bin/editor.exe"},{"new_process_instance",true}};
 auto launch=appEvidence;launch.id="dispatch-1";launch.category=wf::EvidenceCategory::APPLICATION_DISPATCH;launch.operation="application.launch";launch.facts=dispatchFacts;
 auto correlationFacts=app;correlationFacts["runtime_session"]="fixture";correlationFacts["observation_started_100ns"]=120;
 correlationFacts["target_requested"]="C:/work/review.txt";correlationFacts["target_observed"]=false;correlationFacts["target_observation_supported"]=false;
 auto& candidate=correlationFacts["windows"][0];candidate["window_title"]="review.txt - Editor";candidate["foreground"]=false;
 candidate["process_created_100ns"]=105;candidate["process_image_path"]="C:/bin/editor.exe";
 candidate["target_observed"]=false;candidate.erase("target_path");candidate.erase("target_evidence");
 auto correlate=[&](const json& raw,const wf::Evidence& d) {
    return SemanticEvidence::applicationObservation("a",ApplicationCorrelation::enrich(raw,{&d},{&d}));
 };
 auto correlated=correlate(correlationFacts,launch);
 check("STRONG_CORRELATION_ACCEPTS_GENERIC_TARGET_PROOF",SemanticEvidence::matches(correlated,targetProof,error));
 check("CORRELATION_DIAGNOSTIC_AND_DISPATCH_PROVENANCE",correlated.facts["target_proof_level"]=="correlated" &&
    correlated.facts["windows"][0]["target_evidence"]["dispatch_evidence_id"]=="dispatch-1");
 check("DERIVATION_DOES_NOT_MUTATE_SOURCE_FACTS",correlationFacts["windows"][0]["target_observed"]==false && launch.facts==dispatchFacts);
 auto rejectCorrelation=[&](std::string name,json raw,wf::Evidence d) {
    auto derived=correlate(raw,d);check(name,!SemanticEvidence::matches(derived,targetProof,error) && derived.facts["target_proof_level"]=="unavailable");
 };
 auto badLaunch=launch;badLaunch.facts.erase("process_created_100ns");rejectCorrelation("MISSING_PROCESS_EPOCH_REJECTED",correlationFacts,badLaunch);
 badLaunch=launch;badLaunch.facts["new_process_instance"]=false;rejectCorrelation("REUSED_PROCESS_REJECTED",correlationFacts,badLaunch);
 badLaunch=launch;badLaunch.facts["process_created_100ns"]=99;rejectCorrelation("PREEXISTING_PROCESS_REJECTED",correlationFacts,badLaunch);
 badLaunch=launch;badLaunch.facts["operation_succeeded"]=false;rejectCorrelation("FAILED_DISPATCH_REJECTED",correlationFacts,badLaunch);
 badLaunch=launch;badLaunch.facts["canonical_requested_target"]="C:/different/review.txt";rejectCorrelation("SAME_BASENAME_DIFFERENT_CANONICAL_TARGET_REJECTED",correlationFacts,badLaunch);
 badLaunch=launch;badLaunch.facts["runtime_session"]="other";rejectCorrelation("OTHER_LAUNCH_SESSION_REJECTED",correlationFacts,badLaunch);
 auto badObservation=correlationFacts;badObservation["observation_started_100ns"]=109;rejectCorrelation("PRE_DISPATCH_OBSERVATION_REJECTED",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"][0]["process_created_100ns"]=106;rejectCorrelation("PID_REUSE_REJECTED",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"][0]["process_id"]=43;rejectCorrelation("OTHER_PID_REJECTED_FOR_CORRELATION",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"][0]["process_image_path"]="C:/other/editor.exe";rejectCorrelation("DIFFERENT_EXECUTABLE_PATH_REJECTED",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"][0]["window_visible"]=false;rejectCorrelation("INVISIBLE_WINDOW_REJECTED_FOR_CORRELATION",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"][0]["window_title"]="different-review.txt - Editor";rejectCorrelation("TITLE_SUBSTRING_IS_NOT_DOCUMENT_IDENTITY",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"][0]["window_title"]="C:/different/review.txt";rejectCorrelation("CONTRADICTING_FULL_PATH_TITLE_REJECTED",badObservation,launch);
 badObservation=correlationFacts;badObservation["windows"].push_back(badObservation["windows"][0]);rejectCorrelation("DUPLICATE_DOCUMENT_WINDOWS_ARE_AMBIGUOUS",badObservation,launch);
 auto documentLabel=correlationFacts;documentLabel["windows"][0]["window_title"]="Editor";
 documentLabel["windows"][0]["document_controls"]=json::array({json{{"name","review.txt"},{"help_text","Document"}}});
 auto labelled=SemanticEvidence::applicationObservation("a",ApplicationCorrelation::enrich(documentLabel,{&launch},{&launch}));
 check("UIA_LABEL_CORROBORATES_TRUSTED_DISPATCH",SemanticEvidence::matches(labelled,targetProof,error));
 documentLabel["windows"][0]["document_controls"].push_back(documentLabel["windows"][0]["document_controls"][0]);
 rejectCorrelation("DUPLICATE_UIA_DOCUMENT_LABELS_REJECTED",documentLabel,launch);
 const auto diag=ApplicationCorrelation::diagnostic(correlationFacts,{&launch});
 check("CORRELATION_DIAGNOSTICS_ARE_STRUCTURED_AND_BOUNDED",diag.contains("dispatch_pid") && diag.contains("process_instance_match") && diag.contains("failure_reason") && diag.dump().size()<5000);
 auto conflict=launch;conflict.id="dispatch-2";conflict.facts["target_path"]="C:/different/review.txt";
 auto ambiguous=SemanticEvidence::applicationObservation("a",ApplicationCorrelation::enrich(correlationFacts,{&launch},{&launch,&conflict}));
 check("CONFLICTING_DISPATCH_TO_SAME_INSTANCE_REJECTED",!SemanticEvidence::matches(ambiguous,targetProof,error));
 auto noDispatch=SemanticEvidence::applicationObservation("a",ApplicationCorrelation::enrich(correlationFacts,{},{}));
 check("BASENAME_AND_PID_WITHOUT_DISPATCH_CONTEXT_REJECTED",!SemanticEvidence::matches(noDispatch,targetProof,error));
 auto directFacts=app;directFacts["target_requested"]="C:/work/review.txt";
 auto direct=ApplicationCorrelation::enrich(directFacts,{},{});check("DIRECT_EXACT_REMAINS_STRONGEST",direct["target_proof_level"]=="exact");
 auto dispatch=appEvidence;dispatch.category=wf::EvidenceCategory::APPLICATION_DISPATCH;dispatch.facts["operation_succeeded"]=true;check("DISPATCH_NOT_DOCUMENT_OBSERVATION",!SemanticEvidence::matches(dispatch,targetProof,error));
 wrong=targetProof;wrong["process_id"]=99;check("OTHER_PROCESS_REJECTED",!appMatches(wrong));
 wrong=targetProof;wrong["target_path"]="C:/work/other.txt";check("OTHER_DOCUMENT_REJECTED",!appMatches(wrong));
 auto weak=appEvidence;weak.facts["windows"][0].erase("target_evidence");check("CLAIM_WITHOUT_OBSERVED_ASSOCIATION_REJECTED",!SemanticEvidence::matches(weak,targetProof,error));
 weak=appEvidence;weak.facts["windows"][0]["target_evidence"]["observed_path"]="C:/work/other.txt";check("OBSERVED_PATH_MUST_MATCH_REQUESTED_PATH",!SemanticEvidence::matches(weak,targetProof,error));
 weak=appEvidence;weak.facts["windows"][0]["target_observed"]=false;check("BASENAME_TITLE_CANNOT_PROVE_TARGET",!SemanticEvidence::matches(weak,targetProof,error));
 std::vector<const wf::Evidence*> available={&dispatch};check("BOUNDED_UNSUPPORTED_TARGET_DIAGNOSTIC",SemanticEvidence::diagnostic(targetProof,available).find("target/window document association")!=std::string::npos && SemanticEvidence::diagnostic(targetProof,available).size()<=1200);
 wf::Step step;step.id="a";step.intent="Verify semantic state";
 wf::Kernel kernel({step});kernel.next();auto old=kernel.facts.add(observed).id;page["url"]="https://docs.example.test/new/";auto newer=kernel.facts.add(SemanticEvidence::browserObservation("a",page.dump())).id;
 auto predicates=SemanticEvidence::compile(proof,error);
 check("SUPERSEDED_BROWSER_OBSERVATION_REJECTED",!kernel.verify("a",{old},predicates,error));
 check("CURRENT_SEMANTIC_FACT_VERIFIES",kernel.verify("a",{newer},predicates,error) && kernel.complete());
 wf::Kernel appKernel({step});appKernel.next();auto previous=appKernel.facts.add(appEvidence).id;appKernel.facts.add(dispatch);
 check("APPLICATION_DISPATCH_INVALIDATES_PREVIOUS_OBSERVATION",!appKernel.facts.find(previous)->current);
 auto fresh=appKernel.facts.add(appEvidence).id;check("CURRENT_APPLICATION_PROOF_VERIFIES",appKernel.verify("a",{fresh},SemanticEvidence::compile(targetProof,error),error));
 json large=page;large["readable_text"]=std::string(40000,'x');large["visible_text"]=large["readable_text"];large["elements"]=json::array();
 for(int i=0;i<100;++i)large["elements"].push_back({{"id","interaction-"+std::to_string(i)},{"role","link"},{"text","Release "+std::to_string(i)},{"href","https://docs.example.test/"+std::to_string(i)},{"visible",true},{"enabled",true}});
 auto full=SemanticEvidence::browserObservation("a",large.dump());auto presentation=SemanticEvidence::plannerBrowser(large,"Release 99");
 check("FACTSTORE_RETAINS_ELEMENTS_OUTSIDE_PRESENTATION_LIMIT",full.facts["elements"].size()==100 && full.facts["readable_text"].get<std::string>().size()==40000);
 check("PLANNER_PRESENTATION_BOUNDED",presentation.dump().size()<=10000);
 check("PLANNER_INTERACTION_IDS_REMAIN",presentation["elements"][0].contains("id"));
 check("TAIL_ELEMENT_CAN_BE_VERIFIED",SemanticEvidence::matches(full,{{"source","browser"},{"kind","link_exists"},{"href","https://docs.example.test/99"}},error));
 std::cout<<"Semantic evidence checks: "<<checks<<" Failures: "<<failures<<'\n';return failures?1:0;
}
