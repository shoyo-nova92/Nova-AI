#include "utf8_text.hpp"
#include "input_normalizer.hpp"
#include "branch3_runtime.hpp"
#include "state_manager.hpp"
#include "semantic_evidence.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using nlohmann::json;
namespace fs=std::filesystem;
LLMResponse response(json value) { return {true,value.dump(),""}; }
int main() {
    int failures=0,checks=0;
    auto check=[&](const std::string& name,bool ok){++checks;if(!ok)++failures;std::cout<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';};
    const std::string unicode="😀’ — नमस्ते; exact Unicode";
    for (const auto limit:{240,768,2000,2048,8000,16000}) {
        for (int offset=0;offset<6;++offset) {
            auto input=std::string(limit-offset,'a')+unicode;
            const auto out=Utf8::truncate(input,limit);
            check("UTF8_BOUNDARY_"+std::to_string(limit)+"_"+std::to_string(offset),out.size()<=limit && Utf8::invalidOffset(out)==std::string::npos && json::parse(json(out).dump())==out);
        }
    }
    for (const std::string malformed:{std::string("\xe2\x80"),std::string("\x80"),std::string("\xc0\xaf"),std::string("\xed\xa0\x80")}) {
        bool caught=false;
        try { Utf8::dump(json{{"field",malformed}},"catalog"); } catch(const Utf8::Error& error) { caught=std::string(error.what()).find("catalog.field")!=std::string::npos; }
        check("MALFORMED_TEXT_TYPED_FIELD",caught);
        check("LLM_ENCODING_FAILS_BEFORE_NETWORK",LLMClient{}.generate("system",malformed).failureCategory==LLMFailureCategory::INVALID_UTF8);
    }
    const std::string raw="  create a file with exactly \"Nova, please keep Case and “quotes” नमस्ते\"  ";
    const auto normalized=InputNormalizer{}.normalize(raw);
    check("NORMALIZATION_PRESERVES_LITERAL_PAYLOAD",normalized.originalText==raw && normalized.normalizedText==raw.substr(2,raw.size()-4));
    check("BLANK_REQUEST_NEEDS_INPUT",!InputNormalizer{}.normalize(" \t\n").success);
    StateManager state("goal"); state.addHistory("Failed path: fabricated");
    check("FAILURE_HISTORY_NOT_VERIFICATION_EVIDENCE",!state.hasEvidence("fabricated"));
    for(int i=0;i<3;++i) { state.recordFailure("copy|same-arguments|reason=target_not_found"); state.recordSuccess("inspect|different-arguments"); }
    check("UNRELATED_INSPECTION_CANNOT_RESET_FAILED_MUTATION",state.repeatedFailure("copy|same-arguments"));
    state.recordSuccess("copy|same-arguments");
    check("MATCHING_SUCCESS_RESOLVES_RETRY_HISTORY",!state.repeatedFailure("copy|same-arguments"));

    auto provider=[](LLMFailureCategory category) {
        Branch3Runtime runtime([=](const std::string&,const std::string&){return LLMResponse{false,"","Dependency failed",category};},{});
        return runtime.start("Inspect a resource");
    };
    check("DECOMPOSER_TIMEOUT_RETAINED",provider(LLMFailureCategory::TIMEOUT).cause==RuntimeCause::LLM_TIMEOUT);
    check("DECOMPOSER_RATE_LIMIT_RETAINED",provider(LLMFailureCategory::RATE_LIMIT).cause==RuntimeCause::LLM_RATE_LIMIT);
    check("DECOMPOSER_CONNECTION_CAUSE_RETAINED",provider(LLMFailureCategory::CONNECTION_ERROR).cause==RuntimeCause::LLM_CONNECTION_ERROR);
    check("DECOMPOSER_CONFIGURATION_CAUSE_RETAINED",provider(LLMFailureCategory::CONFIGURATION_ERROR).cause==RuntimeCause::LLM_CONFIGURATION_ERROR);

    int plans=0,types=0,clicks=0; bool submitted=false; bool earlyRejected=false;
    const auto steps=json::array({json{{"id","step_1"},{"description","Enter requested payload"}},json{{"id","step_2"},{"description","Submit and verify recorded payload"},{"depends_on",json::array({"step_1"})}}});
    auto operation=[](std::string name,json args,std::string step="step_1") { return response({{"status","operation"},{"step_id",step},{"operation",{{"name",name},{"parameters",args}}}}); };
    Branch3Runtime runtime([&](const std::string&,const std::string&){return response({{"steps",steps}});},{},
        [&](const std::string&,const std::string& context) {
            ++plans;
            if(plans==1) return operation("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            if(plans==2) return operation("browser.type",{{"element_id","current"},{"content","payload"}}); // backend failure, then repair
            if(plans==3) return operation("browser.type",{{"element_id","current"},{"content","payload"}});
            if(plans==4) return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","browser"},{"kind","input_equals"},{"value","payload"}}},{"message","Input value matches"}});
            if(plans==5) return response({{"status","done"},{"answer","Wrong premature completion"}});
            if(plans==6) { earlyRejected=context.find("Scheduling and whole-goal completion are owned by the kernel")!=std::string::npos; return operation("browser.click",{{"element_id","current"}},"step_2"); }
            if(plans==7) return response({{"status","satisfied"},{"step_id","step_2"},{"verification",{{"source","browser"},{"kind","text_contains"},{"value","Recorded payload"}}},{"message","Submission appears in result"},{"answer","Payload recorded and verified."}});
            return response({{"status","done"},{"answer","Payload recorded and verified."}});
        },[&](const PrimitiveOperation& op) {
            ExecutionResult result{};result.status=ExecutionStatus::SUCCESS;result.message="Operation succeeded";
            if(op.name=="browser.type") { if(++types==1) {result.status=ExecutionStatus::FAILED;result.failureCategory="invalid_plan";result.message="Missing target";} }
            if(op.name=="browser.click") {++clicks;submitted=true;}
            if(op.name=="browser.observe") result.browserData=json{{"url","https://example.org/"},{"visible_text",submitted?"Recorded payload":"Form"},{"readable_text",submitted?"Recorded payload":"Form"},{"auth_required",false},{"focus",{{"value",types>=2?"payload":""},{"editable",true}}},{"elements",json::array()}}.dump();
            return result;
        });
    const auto done=runtime.run("Enter payload and submit it");
    check("RECOVERED_TYPE_DOES_NOT_STICK",done.success && done.lastExecutionIssue.empty());
    check("PENDING_SUBMISSION_PREVENTS_DONE",earlyRejected && clicks==1 && submitted && plans==7);
    check("ANSWER_DELIVERED",done.answer=="Payload recorded and verified.");
    check("RESUME_DIAGNOSTICS_PRESERVED",done.stateLog.find("[OPERATION] step=step_2")!=std::string::npos);

    const auto root=fs::temp_directory_path()/("NovaRegression_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(fs::exists(root)) return 2;
    fs::create_directory(root);
    const auto source=root/Utf8::toPath("नोट.txt"); std::ofstream(source,std::ios::binary)<<unicode;
    PrimitiveExecutionEngine engine;
    PrimitiveOperation copy{};copy.name="filesystem.copy";copy.type=PrimitiveOperationType::FILESYSTEM_COPY;copy.resolved=true;
    copy.parameters.source=Utf8::path(source);copy.parameters.destination="backup.txt";
    const auto copied=engine.execute(copy);
    std::ifstream read(root/"backup.txt",std::ios::binary);const std::string bytes((std::istreambuf_iterator<char>(read)),{});read.close();
    check("UNICODE_SAME_FOLDER_COPY_VERIFIED",copied.status==ExecutionStatus::SUCCESS && bytes==unicode && copied.resolvedPath==Utf8::path(root/"backup.txt"));
    check("COPY_COLLISION_PRESERVED",engine.execute(copy).status!=ExecutionStatus::SUCCESS);
    copy.parameters.destination="../escape.txt";check("RELATIVE_TRAVERSAL_REJECTED",engine.execute(copy).status!=ExecutionStatus::SUCCESS);
    PrimitiveOperation inspect{};inspect.name="filesystem.inspect";inspect.type=PrimitiveOperationType::FILESYSTEM_INSPECT;inspect.resolved=true;inspect.parameters.target=Utf8::path(root/"missing.txt");
    const auto missing=engine.execute(inspect);
    check("MISSING_PATH_IS_OBSERVABLE_FACT",missing.status==ExecutionStatus::SUCCESS && missing.message.find("exists=false")!=std::string::npos && !fs::exists(root/"missing.txt"));

    auto artifactGuide=[&](const std::string& name) {
        return [&,name](const std::string&,const std::string&) {
            return response({{"steps",json::array({json{{"id","step_1"},{"description","Establish requested artifact"},
                {"postcondition",{{"kind","file_content"},{"name",name},{"location",Utf8::path(root)}}},{"content_requirement","required"}}})}});
        };
    };
    Branch3Runtime copiedArtifact(artifactGuide("artifact-copy.txt"),{},[&](const std::string&,const std::string&) {
        return operation("filesystem.copy",{{"source",Utf8::path(source)},{"destination","artifact-copy.txt"}});
    });
    std::string streamed;
    copiedArtifact.setEventSink([&](const std::string& event){streamed+=event;});
    const auto copiedGoal=copiedArtifact.run("Copy the requested source to its new filename");
    check("COPY_ESTABLISHES_ARTIFACT_POSTCONDITION",copiedGoal.success && copiedGoal.verifiedProgress.size()==1);
    check("EVENT_SINK_RETAINS_DIAGNOSTIC_LOG",!streamed.empty() && streamed==copiedGoal.stateLog);
    Branch3Runtime renamedArtifact(artifactGuide("artifact-final.txt"),{},[&](const std::string&,const std::string&) {
        return operation("filesystem.rename",{{"target",Utf8::path(root/"artifact-copy.txt")},{"newName","artifact-final.txt"}});
    });
    check("RENAME_ESTABLISHES_ARTIFACT_POSTCONDITION",renamedArtifact.run("Rename the existing artifact").success && !fs::exists(root/"artifact-copy.txt"));

    PrimitiveOperation unicodeFile{};unicodeFile.name="filesystem.create_file";unicodeFile.type=PrimitiveOperationType::FILESYSTEM_CREATE_FILE;unicodeFile.resolved=true;
    unicodeFile.parameters.name="आशा.txt";unicodeFile.parameters.location=Utf8::path(root);unicodeFile.parameters.content=unicode;
    const auto unicodeCreated=engine.execute(unicodeFile);
    check("UNICODE_FILENAME_REPORTED_WITH_VALID_UTF8",unicodeCreated.status==ExecutionStatus::SUCCESS && unicodeCreated.resolvedPath==Utf8::path(root/Utf8::toPath("आशा.txt")) && Utf8::invalidOffset(unicodeCreated.message)==std::string::npos);

    const auto checkGuide=json::array({json{{"id","step_1"},{"description","Observe whether the requested path exists"}}});
    int proofCalls=0;
    Branch3Runtime structured([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string&) {
            if (++proofCalls==1) return operation("filesystem.inspect",{{"target",inspect.parameters.target}});
            if (proofCalls==2) return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","execution"},{"operation","filesystem.inspect"},{"path",inspect.parameters.target}}},{"message","The inspection observed the requested missing path"}});
            return response({{"status","done"},{"answer","The requested path is missing; no file was created."}});
        });
    check("STRUCTURED_EXECUTION_PROOF_ACCEPTS_REAL_PATH",structured.run("Inspect the missing path and explain").success && proofCalls==2);
    auto unrelatedGuide=checkGuide;unrelatedGuide.push_back({{"id","step_2"},{"description","A different required step"}});
    int wrongCalls=0;
    Branch3Runtime wrongProof([&](const std::string&,const std::string&){return response({{"steps",unrelatedGuide}});},{},
        [&](const std::string&,const std::string&) {
            if (++wrongCalls==1) return operation("filesystem.inspect",{{"target",inspect.parameters.target}});
            return response({{"status","satisfied"},{"step_id","step_2"},{"verification",{{"source","execution"},{"operation","filesystem.inspect"},{"path",inspect.parameters.target}}}});
        });
    check("OTHER_STEP_EXECUTION_CANNOT_PROVE_COMPLETION",!wrongProof.run("Perform two different steps").success);
    const auto ancestorSteps=json::array({
        json{{"id","step_1"},{"description","Copy source to the requested artifact"},{"postcondition",{{"kind","file_content"},{"name","ancestor-copy.txt"},{"location",Utf8::path(root)}}},{"content_requirement","required"}},
        json{{"id","step_2"},{"description","Rename the verified copy"},{"depends_on",json::array({"step_1"})},{"postcondition",{{"kind","file_content"},{"name","ancestor-final.txt"},{"location",Utf8::path(root)}}},{"content_requirement","required"}},
        json{{"id","step_3"},{"description","Report the verified copy and rename chain"},{"depends_on",json::array({"step_2"})}}});
    int ancestorCalls=0;
    Branch3Runtime ancestor([&](const std::string&,const std::string&){return response({{"steps",ancestorSteps}});},{},
        [&](const std::string&,const std::string&) {
            if (++ancestorCalls==1) return operation("filesystem.copy",{{"source",Utf8::path(source)},{"destination","ancestor-copy.txt"}});
            if (ancestorCalls==2) return operation("filesystem.rename",{{"target",Utf8::path(root/"ancestor-copy.txt")},{"newName","ancestor-final.txt"}},"step_2");
            if (ancestorCalls==3) return response({{"status","satisfied"},{"step_id","step_3"},{"verification",{{"source","execution"},{"operation","filesystem.copy"},{"path",Utf8::path(root/"ancestor-copy.txt")}}}});
            return response({{"status","done"},{"answer","The copy was byte-verified and renamed successfully."}});
        });
    const auto ancestorResult=ancestor.run("Copy, rename and report verified execution");
    check("TRANSITIVE_VERIFIED_DEPENDENCY_PROOF_ACCEPTED",ancestorResult.success && ancestorCalls==3);
    auto conditionalScenario=[&](const fs::path& target,bool exists) {
        const auto steps=json::array({
            json{{"id","step_1"},{"description","Inspect the requested path"}},
            json{{"id","step_2"},{"description","Use the resource only if it exists"},{"depends_on",json::array({"step_1"})},{"condition",{{"step_id","step_1"},{"field","exists"},{"equals",true}}}},
            json{{"id","step_3"},{"description","Report the absent resource"},{"depends_on",json::array({"step_1"})},{"condition",{{"step_id","step_1"},{"field","exists"},{"equals",false}}}},
            json{{"id","step_4"},{"description","Report the final observed outcome"},{"depends_on",json::array({"step_2","step_3"})}}});
        int plans=0,executions=0;
        auto factProof=[&](const std::string& id) {return response({{"status","satisfied"},{"step_id",id},{"verification",{{"source","execution"},{"operation","filesystem.inspect"},{"path",Utf8::path(target)},{"field","exists"},{"equals",exists}}}});};
        Branch3Runtime conditional([&](const std::string&,const std::string&){return response({{"steps",steps}});},{},
            [&](const std::string&,const std::string&) {
                ++plans;
                if (plans==1) return operation("filesystem.inspect",{{"target",Utf8::path(target)}});
                if (plans==2) return factProof("step_1");
                if (plans==3) return operation("filesystem.inspect",{{"target",Utf8::path(target)}},"step_2");
                if (plans==4) return factProof(exists?"step_2":"step_3");
                if (plans==5) return factProof("step_4");
                return response({{"status","done"},{"answer",exists?"Resource exists and was inspected.":"Resource is missing; no file was created."}});
            },[&](const PrimitiveOperation& op){++executions;return engine.execute(op);});
        const auto outcome=conditional.run("Inspect and take the applicable conditional branch");
        check(exists?"TRUE_BRANCH_RUNS_FALSE_BRANCH_SKIPPED":"FALSE_BRANCH_CANNOT_BE_EXECUTED",outcome.success && executions==(exists?2:1));
        check(exists?"EXISTING_BRANCH_HAS_NOT_APPLICABLE_EVIDENCE":"MISSING_BRANCH_HAS_NOT_APPLICABLE_EVIDENCE",outcome.stateLog.find("[NOT_APPLICABLE]")!=std::string::npos);
    };
    conditionalScenario(root/"missing.txt",false);
    conditionalScenario(source,true);
    auto orderedGuide=checkGuide;
    orderedGuide.push_back({{"id","step_2"},{"description","Report the observation"},{"depends_on",json::array({"step_1"})}});
    int orderedCalls=0;
    bool schedulingFeedback=false;
    Branch3Runtime ordered([&](const std::string&,const std::string&){return response({{"steps",orderedGuide}});},{},
        [&](const std::string&,const std::string& prompt) {
            ++orderedCalls;
            if (orderedCalls==1) return operation("filesystem.inspect",{{"target",inspect.parameters.target}});
            if (orderedCalls==3) schedulingFeedback=prompt.find("Planner must advance only step_1")!=std::string::npos && prompt.find("CURRENT_READY_STEP:\nstep_1")!=std::string::npos;
            if (orderedCalls<=4) return response({{"status","satisfied"},{"step_id",orderedCalls==3?"step_1":"step_2"},{"verification",{{"source","execution"},{"operation","filesystem.inspect"},{"path",inspect.parameters.target}}}});
            return response({{"status","done"},{"answer","The requested path is missing."}});
        });
    const auto orderedResult=ordered.run("Inspect the requested resource and report its observed status");
    check("DEPENDENT_VERIFICATION_EXPLAINS_UNMET_PREREQUISITE",schedulingFeedback);
    check("OUT_OF_ORDER_PROOF_RECOVERS_WITHOUT_REEXECUTION",orderedResult.success && orderedResult.operationResults.size()==1 && orderedCalls==4);
    int falseSubmitPlans=0;
    Branch3Runtime falseSubmit([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string&) {
            if (++falseSubmitPlans==1) return operation("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","browser"},{"kind","text_contains"},{"value","unsent draft"}}}});
        },[&](const PrimitiveOperation& op) {
            ExecutionResult result{};result.action=op.name;result.status=ExecutionStatus::SUCCESS;
            if (op.name=="browser.observe") result.browserData=json{{"url","https://example.org/"},{"visible_text","Form unsent draft"},{"readable_text","Form"},{"auth_required",false},{"elements",json::array()}}.dump();
            return result;
        });
    check("EDITABLE_DRAFT_IS_NOT_SUBMISSION_EVIDENCE",!falseSubmit.run("Verify a completed submission").success);
    int nullableCalls=0;
    GoalDecomposer nullable([&](const std::string&,const std::string&){++nullableCalls;return response({{"steps",json::array({json{{"id","step_1"},{"description","Inspect a resource"},{"condition",nullptr},{"postcondition",nullptr}}})}});});
    check("OPTIONAL_NULL_FIELDS_ARE_ABSENT",nullable.decompose("Inspect a resource").valid && nullableCalls==1);
    int repairs=0;bool structuralFeedback=false;
    GoalDecomposer repaired([&](const std::string&,const std::string& prompt) {
        if(++repairs==1)return response({{"steps",json::array({json{{"id","step_1"},{"description","Inspect"},{"condition",{{"step_id","missing"},{"field","exists"},{"equals",true}}}}})}});
        structuralFeedback=prompt.find("Invalid condition for step_1")!=std::string::npos;
        return response({{"steps",checkGuide}});
    });
    check("CANDIDATE_SCHEMA_REPAIR_IS_BOUNDED_AND_STRUCTURAL",repaired.decompose("Inspect a resource").valid && repairs==2 && structuralFeedback);
    int exceptionPlans=0,exceptionExecutions=0;
    Branch3Runtime exceptionRepair([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string&) {
            if(++exceptionPlans<=2)return operation("filesystem.inspect",{{"target",Utf8::path(root)}});
            return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","execution"},{"operation","filesystem.inspect"},{"path",Utf8::path(root)},{"field","exists"},{"equals",true}}}});
        },[&](const PrimitiveOperation& op)->ExecutionResult {
            if(++exceptionExecutions==1)throw std::runtime_error("Fixture adapter exception");
            return engine.execute(op);
        });
    check("EXECUTOR_EXCEPTION_CAN_BE_REPAIRED_WITHOUT_RUNNING_STATE_LEAK",exceptionRepair.run("Inspect and recover an adapter failure").success && exceptionExecutions==2);
    int dispatchPlans=0;
    Branch3Runtime dispatchOnly([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string&) {
            if(++dispatchPlans==1)return operation("browser.navigate",{{"url","https://example.org/"},{"ready_condition","load"}});
            if(dispatchPlans==2)return operation("browser.download",{{"element_id","download"}});
            return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","execution"},{"operation","browser.download"},{"path",""}}}});
        },[&](const PrimitiveOperation& op) {
            ExecutionResult result{};result.status=ExecutionStatus::SUCCESS;
            if(op.name=="browser.observe")result.browserData=json{{"url","https://example.org/"},{"readable_text","Download pending"},{"auth_required",false}}.dump();return result;
        });
    check("DOWNLOAD_DISPATCH_WITHOUT_COMPLETION_EVIDENCE_REJECTED",!dispatchOnly.run("Verify a completed download").success);
    int semanticPlans=0;size_t largestContext=0;
    json semanticPage={{"url","https://reference.example.test/"},{"title","Release index"},{"readable_text","Current stable release: 7.4"},{"visible_text","Current stable release: 7.4"},{"auth_required",false},{"elements",json::array()}};
    for(int i=0;i<80;++i)semanticPage["elements"].push_back({{"id","current-"+std::to_string(i)},{"role","link"},{"text","Release "+std::to_string(i)},{"href","https://reference.example.test/"+std::to_string(i)},{"visible",true},{"enabled",true}});
    Branch3Runtime semanticBrowser([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string& prompt) {
            largestContext=std::max(largestContext,prompt.size());
            if(++semanticPlans==1)return operation("browser.navigate",{{"url","https://reference.example.test/"},{"ready_condition","load"}});
            return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","browser"},{"kind","link_exists"},{"href","https://reference.example.test/79"}}},{"answer","Observed release destination."}});
        },[&](const PrimitiveOperation& op) {
            ExecutionResult result{};result.status=ExecutionStatus::SUCCESS;if(op.name=="browser.observe")result.browserData=semanticPage.dump();return result;
        });
    check("BROWSER_SEMANTIC_LINK_CROSSES_RUNTIME_BOUNDARY",semanticBrowser.run("Find and verify the observed release destination").success && semanticPlans==2);
    check("BROWSER_PLANNER_CONTEXT_AVOIDS_DUPLICATE_DOM",largestContext<60000);
    int appPlans=0,appLaunches=0,appObservations=0;const auto document=Utf8::path(root/"document.txt");
    auto applicationProof=[&](){return response({{"status","satisfied"},{"step_id","step_1"},{"verification",{{"source","application"},{"kind","target_observed"},{"process_id",42},{"target_path",document}}},{"answer","Document observed in the requested process."}});};
    auto appExecutor=[&](const PrimitiveOperation& op) {
        ExecutionResult result{};result.status=ExecutionStatus::SUCCESS;
        if(op.name=="application.launch"){++appLaunches;result.factsJson=json{{"process_id",42},{"target_path",document},{"dispatch_accepted",true}}.dump();}
        if(op.name=="application.observe") {
            ++appObservations;
            result.factsJson=json{{"processes",json::array({json{{"process_id",42},{"process_name","editor.exe"}}})},{"windows",json::array({json{{"process_id",42},{"window_visible",true},{"window_handle","500"},{"window_title","Document"},{"target_path",document},{"target_observed",true},{"target_evidence",{{"association","exact_document_path"},{"source","document_or_tab_help_text"},{"observed_path",document}}}}})}}.dump();
        }
        return result;
    };
    Branch3Runtime semanticApplication([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string&) {
            ++appPlans;
            if(appPlans==1)return operation("application.launch",{{"application","Registered Editor"},{"target",document}});
            if(appPlans==3)return operation("application.observe",{{"process_id",42},{"target",document}});
            return applicationProof();
        },appExecutor);
    const auto appOutcome=semanticApplication.run("Open and observe the requested document");
    check("APPLICATION_DISPATCH_REJECTED_THEN_OBSERVED_PROOF_ACCEPTED",appOutcome.success && appPlans==4 && appLaunches==1 && appObservations==1);
    appPlans=0;
    Branch3Runtime unsupportedTarget([&](const std::string&,const std::string&){return response({{"steps",checkGuide}});},{},
        [&](const std::string&,const std::string&) {if(++appPlans==1)return operation("application.observe",{{"process_id",42},{"target",document}});return applicationProof();},
        [&](const PrimitiveOperation& op) {
            auto result=appExecutor(op);auto facts=json::parse(result.factsJson);facts["windows"][0]["target_observed"]=false;facts["windows"][0].erase("target_evidence");result.factsJson=facts.dump();return result;
        });
    const auto unsupportedOutcome=unsupportedTarget.run("Confirm the exact requested document is open");
    check("UNOBSERVABLE_DOCUMENT_ASSOCIATION_REMAINS_EXPLICITLY_UNSUPPORTED",!unsupportedOutcome.success && unsupportedOutcome.cause==RuntimeCause::EVIDENCE_UNAVAILABLE && appPlans<=3 && unsupportedOutcome.message.find("target/window document association")!=std::string::npos);
    fs::remove_all(root);
    std::cout<<"Checks: "<<checks<<" Failures: "<<failures<<'\n';
    return failures?1:0;
}
