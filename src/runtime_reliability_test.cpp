#include <curl/curl.h>
#include "branch3_runtime.hpp"
#include "runtime_supervisor.hpp"
#include "browser_state.hpp"
#include "application_state.hpp"
#include "application_correlation.hpp"
#include "state_manager.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
using nlohmann::json;
namespace fs = std::filesystem;
namespace {
LLMResponse reply(json value) { return {true, value.dump(), ""}; }
LLMResponse timeout() { return {false, "", "Planner request exceeded Nova's LLM request timeout.", LLMFailureCategory::TIMEOUT}; }
LLMResponse action(const std::string& name, json params, const std::string& step="step_1") {
    return reply({{"status","operation"},{"step_id",step},{"operation",{{"name",name},{"parameters",params}}}});
}
GoalDecomposerLLM guide(json steps, int* calls = nullptr) {
    return [=](const std::string&, const std::string&) { if (calls) ++*calls; return reply({{"steps", steps}}); };
}
json step(const std::string& id, const std::string& description) { return {{"id",id},{"description",description}}; }
json artifact(const std::string& id, const std::string& kind, const std::string& name, const fs::path& location) {
    auto item = step(id,"Create requested artifact");
    item["postcondition"] = {{"kind",kind},{"name",name},{"location",location.string()}};
    if (kind == "file_content") item["content_requirement"] = "required";
    return item;
}
json dom(const std::string& id="current", const std::string& url="https://example.org/", const std::string& value="") {
    return {{"url",url},{"title","Example"},{"page_kind","content"},{"ready_state","complete"},{"auth_required",false},
        {"visible_text","Search content"}, {"focus",{{"id",id},{"role","textbox"},{"value",value},{"editable",true},{"enabled",true},{"focused",true}}},
        {"elements",json::array({json{{"id",id},{"role","textbox"},{"text","Search"},{"value",value},{"editable",true},{"enabled",true}}})}};
}
ExecutionResult executed(const PrimitiveOperation& op, const std::string& category="", const json& snapshot=json::object()) {
    ExecutionResult result{};
    result.action = op.name;
    result.status = category.empty() ? ExecutionStatus::SUCCESS : ExecutionStatus::FAILED;
    result.reason = category.empty() ? ExecutionFailureReason::NONE : ExecutionFailureReason::EXECUTION_ERROR;
    result.failureCategory = category;
    result.message = category.empty() ? "Operation succeeded" : "Concrete execution problem";
    if (!snapshot.empty()) result.browserData = snapshot.dump();
    return result;
}
}
int main() {
    int checks=0, failures=0;
    auto check = [&](const std::string& label, bool condition) { ++checks; if (!condition) ++failures; std::cout << label << ": " << (condition ? "PASS" : "FAIL") << '\n'; };
    const fs::path root = fs::temp_directory_path() / ("NovaSupervisorTest_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (fs::exists(root)) return 2;
    fs::create_directory(root);
    const auto initial = json::array({artifact("step_1","directory_exists","Artifact",root),artifact("step_2","file_content","sample.data",root/"Artifact")});
    int planCalls=0;
    Branch3Runtime complete(guide(initial), {}, [&](const std::string&, const std::string&) {
        ++planCalls;
        if (planCalls==1) return action("filesystem.create",{{"name","Artifact"},{"location",root.string()}});
        if (planCalls==2) return action("filesystem.create_file",{{"name","sample.data"},{"location",(root/"Artifact").string()},{"content","requested implementation"}},"step_2");
        return timeout();
    });
    auto completed = complete.start("Create requested artifacts");
    check("VERIFIED_GOAL_COMPLETES_WITHOUT_FINAL_LLM",completed.outcome==RuntimeOutcome::COMPLETED && completed.success && planCalls==2 && completed.verifiedProgress.size()==2);
    check("TIMEOUT_AFTER_POSTCONDITIONS_CANNOT_REVERSE_COMPLETION",complete.continueSession().success && planCalls==2);

    int decompositions=0, calls=0;
    auto continuationSteps=initial;
    continuationSteps[1]["postcondition"]["name"]="continued.data";
    Branch3Runtime paused(guide(continuationSteps,&decompositions),{},[&](const std::string&,const std::string&) {
        ++calls;
        if (calls==1) return action("filesystem.create",{{"name","Artifact"},{"location",root.string()}});
        if (calls<=3) return timeout();
        return action("filesystem.create_file",{{"name","continued.data"},{"location",(root/"Artifact").string()},{"content","resumed"}},"step_2");
    });
    auto unavailable=paused.start("Create and populate");
    check("TIMEOUT_RETRIES_ONCE_PRESERVING_PROGRESS",unavailable.outcome==RuntimeOutcome::EXTERNAL_UNAVAILABLE && unavailable.cause==RuntimeCause::LLM_TIMEOUT && calls==3 && unavailable.verifiedProgress.size()==1 && paused.hasActiveSession());
    check("EXISTING_AUTHORIZED_DIRECTORY_SATISFIED",unavailable.operationResults.size()==1 && unavailable.operationResults[0].execution.failureCategory=="already_exists");
    auto resumed=paused.continueSession();
    check("CONTINUE_REUSES_DECOMPOSITION",resumed.success && decompositions==1 && calls==4 && resumed.operationResults.size()==2);

    Branch3Runtime collision(guide(json::array({artifact("step_1","file_content","sample.data",root/"Artifact")})),{},[&](const std::string&,const std::string&) {
        return action("filesystem.create_file",{{"name","sample.data"},{"location",(root/"Artifact").string()},{"content","different"}});
    });
    auto collided=collision.run("Create conflicting file");
    check("CONFLICTING_FILE_NOT_SATISFIED",!collided.success && collided.verifiedProgress.empty());
    std::ifstream content(root/"Artifact"/"sample.data"); std::string original; std::getline(content,original); content.close();
    check("COLLISION_DOES_NOT_OVERWRITE",original=="requested implementation");

    RuntimeSupervisor supervisor(paused);
    check("NEW_REQUEST_ABANDONS_SESSION",supervisor.control('V')==SupervisorAction::NEW_REQUEST && !paused.hasActiveSession());
    check("INVALID_MENU_CONTROL_NO_ACTION",supervisor.control('x')==SupervisorAction::INVALID && supervisor.running());
    check("QUIT_STOPS_SUPERVISOR",supervisor.control('Q')==SupervisorAction::QUIT && !supervisor.running());
    RuntimeSupervisor eofSupervisor(paused); std::istringstream eof; std::ostringstream terminal;
    check("EOF_SHUTS_DOWN_CLEANLY",eofSupervisor.readControl(eof,terminal,completed)==SupervisorAction::QUIT);

    const auto before=dom("old-id");
    check("FINGERPRINT_IGNORES_OBSERVATION_IDS",BrowserState::fingerprint(before.dump())==BrowserState::fingerprint(dom("new-id").dump()));
    check("FINGERPRINT_DETECTS_URL_CHANGE",BrowserState::fingerprint(before.dump())!=BrowserState::fingerprint(dom("new-id","https://example.org/results").dump()));
    check("FINGERPRINT_DETECTS_FOCUS_VALUE",BrowserState::fingerprint(before.dump())!=BrowserState::fingerprint(dom("new-id","https://example.org/","query").dump()));
    auto changed=before; changed["visible_text"]="Results available";
    check("FINGERPRINT_DETECTS_CONTENT_CHANGE",BrowserState::fingerprint(before.dump())!=BrowserState::fingerprint(changed.dump()));
    check("LEGACY_DOM_MARKER_PARSED",BrowserState::fingerprint("Browser DOM result: "+before.dump())==BrowserState::fingerprint(before.dump()));
    check("COMPACT_CONTEXT_PRESERVES_CURRENT_IDS",BrowserState::compact(before.dump()).find("old-id")!=std::string::npos);
    StateManager bounded("goal");
    for(int i=0;i<100;++i) {
        bounded.addHistory("Operation succeeded Browser DOM result: " + before.dump() + std::string(100000,'x'));
        bounded.setObservation(dom("current").dump());
    }
    check("HISTORY_HAS_NO_OLD_DOM_DUMPS",bounded.render().find("old-id")==std::string::npos && bounded.render().find("Browser DOM result:")==std::string::npos);
    check("PLANNER_CONTEXT_BOUNDED",bounded.render().size()<40000);

    auto wordingScenario = [&](const std::string& description, const std::string& failure) {
        int observations=0, planner=0, mutations=0; bool refreshed=false;
        Branch3Runtime browser(guide(json::array({step("step_1",description)})),{},[&](const std::string&, const std::string& context) {
            ++planner;
            if(planner==1) return action("browser.navigate",{{"url","https://example.org/"},{"ready_condition","domcontentloaded"}});
            if(planner==2) return action(failure=="browser_timeout" ? "browser.wait_for" : "browser.click",
                failure=="browser_timeout" ? json{{"ready_condition","load"}} : json{{"element_id","fresh-1"}});
            refreshed=context.find("fresh-2")!=std::string::npos;
            return timeout();
        },[&](const PrimitiveOperation& op) {
            if(op.name=="browser.observe") { ++observations; return executed(op,"",dom("fresh-"+std::to_string(observations), observations>=2 ? "https://example.org/results" : "https://example.org/")); }
            if(op.name=="browser.click" || op.name=="browser.wait_for") { ++mutations; return executed(op,failure); }
            return executed(op);
        });
        auto result=browser.run("Navigate and inspect results");
        check("BROWSER_RECOVERY_"+description+failure,result.outcome==RuntimeOutcome::EXTERNAL_UNAVAILABLE && result.cause==RuntimeCause::LLM_TIMEOUT && observations==2 && refreshed && mutations==1 && !result.lastExecutionIssue.empty() && !result.recovery.empty());
        return observations;
    };
    auto a=wordingScenario("Navigate to service","stale_element");
    auto b=wordingScenario("Navigate to service website","stale_element");
    check("OBSERVATION_INDEPENDENT_OF_WORDING",a==b);
    wordingScenario("Navigate to service","browser_timeout");

    check("CURL_TIMEOUT_CLASSIFICATION",classifyLLMFailure(CURLE_OPERATION_TIMEDOUT,0)==LLMFailureCategory::TIMEOUT);
    check("HTTP_429_CLASSIFICATION",classifyLLMFailure(CURLE_OK,429)==LLMFailureCategory::RATE_LIMIT);
    check("HTTP_AUTH_CLASSIFICATION",classifyLLMFailure(CURLE_OK,401)==LLMFailureCategory::AUTH_ERROR && classifyLLMFailure(CURLE_OK,403)==LLMFailureCategory::AUTH_ERROR);
    check("HTTP_PROVIDER_CLASSIFICATION",classifyLLMFailure(CURLE_OK,503)==LLMFailureCategory::PROVIDER_ERROR);
    check("CONNECTION_CLASSIFICATION",classifyLLMFailure(CURLE_COULDNT_CONNECT,0)==LLMFailureCategory::CONNECTION_ERROR);
    NextActionPlanner invalid([](const std::string&,const std::string&) { return LLMResponse{false,"","No usable response",LLMFailureCategory::INVALID_RESPONSE}; });
    MultiStepPlan dummy{};
    check("INVALID_RESPONSE_PRESERVES_TYPE",invalid.choose("goal",dummy,"state","catalog").failureCategory==LLMFailureCategory::INVALID_RESPONSE);


    for (const auto category : {LLMFailureCategory::RATE_LIMIT, LLMFailureCategory::AUTH_ERROR, LLMFailureCategory::PROVIDER_ERROR}) {
        int attempts=0;
        Branch3Runtime provider(guide(json::array({step("step_1","Inspect")})),{},[&](const std::string&,const std::string&) {
            ++attempts; return LLMResponse{false,"","Provider diagnostic",category};
        });
        const auto failure=provider.run("Inspect");
        check("PROVIDER_CATEGORY_AND_RETRY_BOUND_"+std::to_string(static_cast<int>(category)),
            failure.outcome==RuntimeOutcome::EXTERNAL_UNAVAILABLE &&
            attempts==(category==LLMFailureCategory::AUTH_ERROR ? 1 : 2) &&
            failure.cause==(category==LLMFailureCategory::RATE_LIMIT ? RuntimeCause::LLM_RATE_LIMIT :
                category==LLMFailureCategory::AUTH_ERROR ? RuntimeCause::LLM_AUTH_ERROR : RuntimeCause::LLM_PROVIDER_ERROR));
    }
    auto huge=dom("retained");
    huge["title"]=std::string(50000,'t'); huge["visible_text"]=std::string(100000,'v');
    for(int i=0;i<1000;++i) huge["elements"].push_back({{"id",std::to_string(i)},{"role","button"},{"text",std::string(5000,'x')}});
    bounded.setObservation(huge.dump());
    check("OVERSIZED_OBSERVATION_STILL_BOUNDED",bounded.render().size()<40000);

    int authPlans=0, authObservations=0, authDecompositions=0;
    Branch3Runtime authentication(guide(json::array({step("step_1","Open account")}),&authDecompositions),{},
        [&](const std::string&,const std::string&) {
            ++authPlans;
            if(authPlans==1) return action("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            return timeout();
        },[&](const PrimitiveOperation& op) {
            if(op.name=="browser.observe") {
                ++authObservations; auto page=dom("authentication-"+std::to_string(authObservations));
                page["auth_required"] = authObservations==1;
                return executed(op,"",page);
            }
            return executed(op);
        });
    const auto authPause=authentication.run("Open account");
    check("AUTH_REQUIRES_USER_WITHOUT_EXTRA_PLANNING",authPause.outcome==RuntimeOutcome::NEEDS_USER && authPause.cause==RuntimeCause::AUTH_REQUIRED && authPlans==1);
    RuntimeSupervisor authSupervisor(authentication);
    std::istringstream resumeMenu("C\n"); std::ostringstream menuOutput;
    check("CONTINUE_MENU_ACCEPTED",authSupervisor.readControl(resumeMenu,menuOutput,authPause)==SupervisorAction::CONTINUE);
    const auto authResumed=authentication.continueSession();
    check("CONTINUATION_REFRESHES_DOM_WITHOUT_DECOMPOSING",authResumed.cause==RuntimeCause::LLM_TIMEOUT && authObservations==2 && authDecompositions==1);
    std::istringstream retryMenu("R\n");
    check("RETRY_MENU_ACCEPTED",authSupervisor.readControl(retryMenu,menuOutput,authResumed)==SupervisorAction::RETRY);
    const auto retried=authentication.retryBlockedDependency();
    check("RETRY_PRESERVES_SESSION",retried.cause==RuntimeCause::LLM_TIMEOUT && authDecompositions==1 && authObservations==3);
    check("MENU_REPORTS_TYPED_CAUSE",menuOutput.str().find("LLM_TIMEOUT")!=std::string::npos);

    int keyExecutions=0, globalPlans=0;
    Branch3Runtime globalKeys(guide(json::array({step("step_1","Use page")})),{},
        [&](const std::string&,const std::string&) {
            if(++globalPlans==1) return action("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            return action("keyboard.press",{{"key","ENTER"}});
        },[&](const PrimitiveOperation& op) {
            if(op.name.starts_with("keyboard.")) ++keyExecutions;
            return op.name=="browser.observe" ? executed(op,"",dom()) : executed(op);
        });
    auto keys=globalKeys.run("Submit page");
    check("CONTROLLED_BROWSER_REJECTS_GLOBAL_KEYS",keys.cause==RuntimeCause::VALIDATION_ERROR && keyExecutions==0);

    int stalePlans=0, staleExecutes=0, staleObservations=0;
    Branch3Runtime staleLoop(guide(json::array({step("step_1","Use page")})),{},
        [&](const std::string&,const std::string&) {
            if(++stalePlans==1) return action("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            return action("browser.click",{{"element_id","old-reference"}});
        },[&](const PrimitiveOperation& op) {
            if(op.name=="browser.observe") return executed(op,"",dom("fresh-"+std::to_string(++staleObservations)));
            if(op.name=="browser.click") { ++staleExecutes; return executed(op,"stale_element"); }
            return executed(op);
        });
    auto staleBlocked=staleLoop.run("Use page");
    check("STALE_CLICK_NOT_BLINDLY_REPEATED",staleBlocked.cause==RuntimeCause::STALE_ELEMENT && staleExecutes==1 && staleObservations>=2);

    int unchangedPlans=0, unchangedClicks=0;
    Branch3Runtime noChange(guide(json::array({step("step_1","Use page")})),{},
        [&](const std::string&,const std::string&) {
            if(++unchangedPlans==1) return action("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            return action("browser.click",{{"role","button"},{"text","Observed control"}});
        },[&](const PrimitiveOperation& op) {
            if(op.name=="browser.observe") return executed(op,"",dom("regenerated-"+std::to_string(unchangedPlans)));
            if(op.name=="browser.click") ++unchangedClicks;
            return executed(op);
        });
    const auto stuck=noChange.run("Use page");
    check("SUCCESSFUL_CLICK_NOT_REPLAYED_FOR_VERIFICATION",stuck.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && unchangedClicks==1);

    int readinessPlans=0;
    Branch3Runtime readiness(guide(json::array({step("step_1","Wait for requested state")})),{},
        [&](const std::string&,const std::string&) {
            ++readinessPlans;
            if(readinessPlans==1) return action("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            if(readinessPlans==2) return action("browser.wait_for",{{"ready_condition","load"}});
            return reply({{"status","done"}});
        },[&](const PrimitiveOperation& op) {
            if(op.name=="browser.observe") return executed(op,"",dom());
            return executed(op,op.name=="browser.wait_for" ? "browser_timeout" : "");
        });
    check("UNCHANGED_TIMEOUT_NOT_FALSE_COMPLETION",!readiness.run("Wait for state").success);

    std::ofstream(root/"not_directory") << "conflict";
    Branch3Runtime directoryConflict(guide(json::array({artifact("step_1","directory_exists","not_directory",root)})),{},
        [&](const std::string&,const std::string&) { return action("filesystem.create",{{"name","not_directory"},{"location",root.string()}}); });
    check("EXISTING_FILE_CANNOT_SATISFY_DIRECTORY",!directoryConflict.run("Ensure directory").success);


    int prohibitedWrites=0;
    Branch3Runtime overwriteGuard(guide(json::array({artifact("step_1","file_content","sample.data",root/"Artifact")})),{},
        [&](const std::string&,const std::string&) { return action("text.write",{{"target",(root/"Artifact"/"sample.data").string()},{"content","overwrite"}}); },
        [&](const PrimitiveOperation& op) { ++prohibitedWrites; return executed(op); });
    check("NEW_ARTIFACT_CANNOT_BYPASS_COLLISION_WITH_TEXT_WRITE",overwriteGuard.run("Create a new artifact").cause==RuntimeCause::ALREADY_EXISTS && prohibitedWrites==0);

    std::ofstream(root/"existing.data") << "old";
    int writePlans=0;
    Branch3Runtime legitimateWrite(guide(json::array({step("step_1","Write requested text into the existing artifact")})),{},
        [&](const std::string&,const std::string&) {
            if(++writePlans==1) return action("text.write",{{"target",(root/"existing.data").string()},{"content","new"}});
            if(writePlans==2) return reply({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","execution"},{"operation","text.write"},{"path",(root/"existing.data").string()}}},{"message","Write verified by execution"}});
            return reply({{"status","done"},{"message","Requested content written to the existing file"}});
        });
    check("LEGITIMATE_EXISTING_FILE_WRITE_UNCHANGED",legitimateWrite.run("Update the existing artifact").success);

    // Injected executors only: no real desktop applications or provider calls.
    const std::string document="C:\\EvidenceFixture\\requested.data";
    json window={{"process_id",77},{"process_name","editor.exe"},{"window_handle","123"},
        {"window_title","requested.data"},{"window_visible",true},{"target_observed",false}};
    json appFacts={{"processes",json::array({{{"process_id",77},{"process_name","editor.exe"}}})},
        {"windows",json::array({window})},{"target_requested",document},{"target_observed",false},{"target_observation_supported",false}};
    auto observationResult=[&](const PrimitiveOperation& op,const json& facts) {
        auto value=executed(op);value.factsJson=facts.dump();return value;
    };
    auto noisyFacts=appFacts;noisyFacts["observation_token"]="new";noisyFacts["windows"][0]["diagnostic"]="new prose";
    check("APP_FINGERPRINT_IGNORES_DIAGNOSTIC_CHURN",ApplicationState::fingerprint(appFacts)==ApplicationState::fingerprint(noisyFacts));
    noisyFacts["windows"][0]["window_visible"]=false;
    check("APP_FINGERPRINT_DETECTS_VISIBLE_STATE",ApplicationState::fingerprint(appFacts)!=ApplicationState::fingerprint(noisyFacts));

    int appPlans=0,launches=0,appObservations=0;
    Branch3Runtime unavailableApplication(guide(json::array({step("step_1","Open and verify requested document")})),{},
        [&](const std::string&,const std::string&) {
            if(++appPlans==1)return action("application.launch",{{"application","editor"},{"target",document}});
            if(appPlans==2)return action("application.observe",{{"process_id",77},{"target",document}});
            return action("application.launch",{{"application","EDITOR"},{"target","c:/evidencefixture/requested.data"}});
        },[&](const PrimitiveOperation& op) {
            if(op.name=="application.launch"){++launches;return executed(op);}
            ++appObservations;return observationResult(op,appFacts);
        });
    auto evidencePause=unavailableApplication.run("Open and verify document");
    check("UNAVAILABLE_TARGET_STOPS_BEFORE_MORE_PLANNING",evidencePause.cause==RuntimeCause::EVIDENCE_UNAVAILABLE &&
        !evidencePause.success && appPlans==2 && launches==1 && appObservations==1 && evidencePause.operationResults.size()==2);
    check("UNAVAILABLE_CAUSE_HAS_PUBLIC_NAME",std::string(causeName(evidencePause.cause))=="EVIDENCE_UNAVAILABLE");
    auto replayPause=unavailableApplication.continueSession();
    check("CONTINUE_REFRESHES_WITHOUT_PLANNER_OR_MUTATION",replayPause.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && launches==1 && appPlans==2 && appObservations==2);
    unavailableApplication.continueSession();
    check("REPEATED_CONTINUE_PRESERVES_TYPED_BLOCKER_WITHOUT_LLM",appPlans==2 && appObservations==3);
    unavailableApplication.retryBlockedDependency();
    check("DEPENDENCY_RETRY_DOES_NOT_RESUME_EVIDENCE_BLOCKER",appPlans==2 && appObservations==3);
    auto foregroundFacts=appFacts;foregroundFacts["windows"][0]["foreground"]=true;
    auto backgroundFacts=foregroundFacts;backgroundFacts["windows"][0]["foreground"]=false;
    check("FOCUS_CHANGES_IRRELEVANT_TO_TARGET_REFRESH",ApplicationState::fingerprint(foregroundFacts)==ApplicationState::fingerprint(backgroundFacts));

    int stableObservations=0;
    auto unknownFacts=appFacts;unknownFacts.erase("target_observation_supported");
    Branch3Runtime stableApplication(guide(json::array({step("step_1","Verify document")})),{},
        [&](const std::string&,const std::string&) {return action("application.observe",{{"process_id",77},{"target",document}});},
        [&](const PrimitiveOperation& op) {
            auto facts=unknownFacts;facts["observation_token"]=std::to_string(++stableObservations);
            return observationResult(op,facts);
        });
    auto stablePause=stableApplication.run("Verify document");
    check("EQUIVALENT_APP_OBSERVATIONS_STOP_AT_TWO",stablePause.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && stableObservations==2);

    int startupObservations=0;
    auto noWindow=appFacts;noWindow["windows"]=json::array();
    check("EMPTY_WINDOW_SCOPE_IS_NOT_IMMEDIATE_UNAVAILABLE",!ApplicationState::targetUnavailable(noWindow));
    Branch3Runtime startup(guide(json::array({step("step_1","Observe starting application")})),{},
        [&](const std::string&,const std::string&) {return action("application.observe",{{"process_id",77},{"target",document}});},
        [&](const PrimitiveOperation& op) {++startupObservations;return observationResult(op,noWindow);});
    check("EMPTY_WINDOW_SCOPE_IS_BOUNDED",startup.run("Observe starting application").cause==RuntimeCause::EVIDENCE_UNAVAILABLE && startupObservations==2);

    int independentPlans=0,independentLaunches=0;
    auto nextAppStep=step("step_2","Launch again as a separate requested step");nextAppStep["depends_on"]=json::array({"step_1"});
    Branch3Runtime separateSteps(guide(json::array({step("step_1","Dispatch requested launch"),nextAppStep})),{},
        [&](const std::string&,const std::string&) {
            const int index=++independentPlans;const std::string id=index<=2?"step_1":"step_2";
            if(index==1 || index==3)return action("application.launch",{{"application","editor"},{"target",document}},id);
            return reply({{"status","satisfied"},{"step_id",id},{"verification",{{"source","execution"},{"operation","application.launch"},{"field","dispatch_accepted"},{"equals",true}}}});
        },[&](const PrimitiveOperation& op) {++independentLaunches;return executed(op);});
    check("SEPARATE_STEPS_CAN_REQUEST_SAME_MUTATION",separateSteps.run("Dispatch two requested launches").success && independentLaunches==2);

    int recoveryPlans=0,recoveryObservations=0;
    Branch3Runtime observableLater(guide(json::array({step("step_1","Verify document")})),{},
        [&](const std::string&,const std::string&) {
            if(++recoveryPlans==1)return action("application.observe",{{"process_id",77},{"target",document}});
            return reply({{"status","satisfied"},{"step_id","step_1"},{"verification",{
                {"source","application"},{"kind","target_observed"},{"process_id",77},{"target_path",document}}}});
        },[&](const PrimitiveOperation& op) {
            auto facts=appFacts;
            if(++recoveryObservations>1) {
                facts["target_observed"]=true;facts["target_observation_supported"]=true;
                facts["windows"][0]["target_observed"]=true;facts["windows"][0]["target_path"]=document;
                facts["windows"][0]["target_evidence"]={{"association","exact_document_path"},{"source","document_or_tab_help_text"},{"observed_path",document}};
            }
            return observationResult(op,facts);
        });
    check("APP_EVIDENCE_LIMIT_PAUSES",observableLater.run("Verify document").cause==RuntimeCause::EVIDENCE_UNAVAILABLE);
    check("CONTINUE_CAN_VERIFY_NEW_EXACT_EVIDENCE",observableLater.continueSession().success && recoveryObservations==2);

    auto launchContext=json{{"dispatch_accepted",true},{"target_resolved",true},{"canonical_requested_target",document},{"target_path",document},
        {"process_id",77},{"process_created_100ns",105},{"process_image_path","C:/bin/editor.exe"},
        {"launch_started_100ns",100},{"launch_completed_100ns",110},{"launch_id","fixture-launch"},{"new_process_instance",true}};
    auto correlatedObservation=appFacts;
    correlatedObservation["observation_started_100ns"]=120;
    correlatedObservation["windows"][0]["process_created_100ns"]=105;
    correlatedObservation["windows"][0]["process_image_path"]="C:/bin/editor.exe";
    correlatedObservation["windows"][0]["foreground"]=false;
    int lifecyclePlans=0,lifecycleLaunches=0,lifecycleCloses=0,lifecycleObservations=0;
    bool correlatedInLedger=false;
    auto verifyApp=step("step_2","Make sure requested document is actually open");verifyApp["depends_on"]=json::array({"step_1"});
    auto closeApp=step("step_3","Close and verify application is gone");closeApp["depends_on"]=json::array({"step_2"});
    Branch3Runtime lifecycle(guide(json::array({step("step_1","Open requested document"),verifyApp,closeApp})),{},
        [&](const std::string&,const std::string& context) {
            const auto index=++lifecyclePlans;
            if(index==1)return action("application.launch",{{"application","editor"},{"target",document}});
            if(index==2)return action("application.observe",{{"process_id",77},{"target",document}});
            if(index==3 || index==4) {
                correlatedInLedger=context.find("correlated_document_target")!=std::string::npos;
                return reply({{"status","satisfied"},{"step_id",index==3?"step_1":"step_2"},
                    {"verification",{{"source","application"},{"kind","target_observed"},{"process_id",77},{"target_path",document}}}});
            }
            if(index==5)return action("application.close",{{"application","editor"}},"step_3");
            if(index==6)return action("application.observe",{{"process_id",77}},"step_3");
            return reply({{"status","satisfied"},{"step_id","step_3"},{"verification",{
                {"source","execution"},{"operation","application.observe"},{"field","process_exists"},{"equals",false}}}});
        },[&](const PrimitiveOperation& op) {
            if(op.name=="application.launch"){++lifecycleLaunches;return observationResult(op,launchContext);}
            if(op.name=="application.close"){++lifecycleCloses;return executed(op);}
            ++lifecycleObservations;
            if(!lifecycleCloses)return observationResult(op,correlatedObservation);
            return observationResult(op,{{"processes",json::array()},{"windows",json::array()},{"process_exists",false},{"target_requested",""}});
        });
    const auto lifecycleResult=lifecycle.run("Open document, make sure it is open, then close it");
    check("CORRELATED_OPEN_REUSE_VERIFY_CLOSE_WORKFLOW",lifecycleResult.success && lifecycleLaunches==1 && lifecycleCloses==1 && lifecycleObservations==2 && lifecyclePlans==7 && correlatedInLedger);

    int delayedPlans=0,delayedObservations=0,delayedLaunches=0;
    Branch3Runtime delayedWindow(guide(json::array({step("step_1","Open and verify document")})),{},
        [&](const std::string&,const std::string&) {
            const auto index=++delayedPlans;
            if(index==1)return action("application.launch",{{"application","editor"},{"target",document}});
            if(index<=3)return action("application.observe",{{"process_id",77},{"target",document}});
            return reply({{"status","satisfied"},{"step_id","step_1"},{"verification",{
                {"source","application"},{"kind","target_observed"},{"process_id",77},{"target_path",document}}}});
        },[&](const PrimitiveOperation& op) {
            if(op.name=="application.launch"){++delayedLaunches;return observationResult(op,launchContext);}
            return observationResult(op,++delayedObservations<=2?noWindow:correlatedObservation);
        });
    check("WINDOW_STARTUP_LIMIT_RETAINS_LAUNCH_CONTEXT",delayedWindow.run("Open and verify").cause==RuntimeCause::EVIDENCE_UNAVAILABLE && delayedLaunches==1);
    check("CONTINUE_REFRESH_DERIVES_NEW_CORRELATION_BEFORE_PLANNER",delayedWindow.continueSession().success && delayedPlans==4 && delayedObservations==3 && delayedLaunches==1);

    int refreshFailurePlans=0,refreshAttempts=0;
    Branch3Runtime refreshFailure(guide(json::array({step("step_1","Verify application")})),{},
        [&](const std::string&,const std::string&) {++refreshFailurePlans;return action("application.observe",{{"process_id",77},{"target",document}});},
        [&](const PrimitiveOperation& op) {
            if(++refreshAttempts==1)return observationResult(op,appFacts);
            return executed(op,"access_denied");
        });
    refreshFailure.run("Verify application");
    check("FAILED_REFRESH_PRESERVES_EVIDENCE_BLOCKER_WITHOUT_LLM",refreshFailure.continueSession().cause==RuntimeCause::EVIDENCE_UNAVAILABLE && refreshFailurePlans==1 && refreshAttempts==2);

    int unrelatedPlans=0;
    Branch3Runtime unrelatedLaunch(guide(json::array({step("step_1","Dispatch launch"),step("step_2","Observe independent document")})),{},
        [&](const std::string&,const std::string&) {
            const auto index=++unrelatedPlans;
            if(index==1)return action("application.launch",{{"application","editor"},{"target",document}});
            if(index==2)return reply({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","execution"},{"operation","application.launch"},{"field","dispatch_accepted"},{"equals",true}}}});
            return action("application.observe",{{"process_id",77},{"target",document}},"step_2");
        },[&](const PrimitiveOperation& op) {return observationResult(op,op.name=="application.launch"?launchContext:correlatedObservation);});
    check("UNRELATED_STEP_DISPATCH_CANNOT_SUPPLY_CORRELATION",unrelatedLaunch.run("Independent steps").cause==RuntimeCause::EVIDENCE_UNAVAILABLE);

    int dispatchPlans=0,dispatches=0;
    Branch3Runtime failedDispatch(guide(json::array({step("step_1","Launch editor")})),{},
        [&](const std::string&,const std::string&) {
            if(++dispatchPlans<=2)return action("application.launch",{{"application","editor"},{"target",document}});
            return timeout();
        },[&](const PrimitiveOperation& op) {return executed(op,++dispatches==1?"target_not_found":"");});
    check("FAILED_MUTATION_CAN_BE_REPAIRED",failedDispatch.run("Launch editor").cause==RuntimeCause::LLM_TIMEOUT && dispatches==2);

    int inspections=0;
    Branch3Runtime exhausted(guide(json::array({step("step_1","Inspect environment")})),{},[&](const std::string&,const std::string&) { return action("filesystem.inspect",{{"target",root.string()}}); },
        [&](const PrimitiveOperation& op) { ++inspections; return executed(op); });
    auto limit=exhausted.run("Inspect environment");
    check("REPEATED_UNVERIFIED_OBSERVATION_REQUESTS_GUIDANCE",!limit.success && limit.cause==RuntimeCause::STRATEGY_GUIDANCE && inspections==2 && exhausted.pendingUserDecision().has_value());
    std::error_code ec;
    fs::remove_all(root,ec);
    check("FIXTURE_CLEANED",!ec && !fs::exists(root));
    std::cout << "Passed: " << checks-failures << " Failed: " << failures << '\n';
    return failures ? 1 : 0;
}
