#include "branch3_runtime.hpp"
#include "utf8_text.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
using nlohmann::json;
namespace fs=std::filesystem;
LLMResponse reply(json j){return {true,j.dump(),""};}
json step(std::string id,std::string intent,std::vector<std::string> deps={}){return {{"id",id},{"description",intent},{"depends_on",deps},{"condition",nullptr}};}
json artifact(std::string id,std::string name,fs::path parent,bool directory=false,std::vector<std::string> deps={}){auto s=step(id,"Establish requested artifact",deps);s["postcondition"]={{"kind",directory?"directory_exists":"file_content"},{"name",name},{"location",Utf8::path(parent)}};if(!directory)s["content_requirement"]="required";return s;}
LLMResponse op(std::string id,std::string name,json params){return reply({{"status","operation"},{"step_id",id},{"operation",{{"name",name},{"parameters",params}}}});}
LLMResponse satisfied(std::string id,std::string operation,std::string path,std::string field="operation_succeeded",json equals=true,std::string answer="Verified requested outcome"){return reply({{"status","satisfied"},{"step_id",id},{"verification",{{"source","execution"},{"operation",operation},{"path",path},{"field",field},{"equals",equals}}},{"answer",answer}});}
LLMResponse browserProof(std::string id,std::string kind,std::string value,std::string answer="Observed requested browser outcome"){return reply({{"status","satisfied"},{"step_id",id},{"verification",{{"source","browser"},{"kind",kind},{"value",value}}},{"answer",answer}});}
GoalDecomposerLLM guide(json steps){return [steps](const std::string&,const std::string&){return reply({{"steps",steps}});};}
ExecutionResult success(const PrimitiveOperation& op,json facts=json::object()){ExecutionResult r{};r.action=op.name;r.status=ExecutionStatus::SUCCESS;r.reason=ExecutionFailureReason::NONE;r.factsJson=facts.dump();return r;}
std::string read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
int main(){
 int failures=0;auto test=[&](std::string name,bool okay){std::cout<<name<<": "<<(okay?"PASS":"FAIL")<<'\n';if(!okay)++failures;};
 const auto root=fs::temp_directory_path()/("NovaGolden_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 if(fs::exists(root))return 2;fs::create_directory(root);
 {
 Branch3Runtime r(guide(json::array({artifact("step_1","populated.txt",root)})),{},[&](const auto&,const auto&){return op("step_1","filesystem.create_file",{{"name","populated.txt"},{"location",Utf8::path(root)},{"content","Populated exactly"}});});
 test("01_create_populated_file",r.run("Create the populated file").success && read(root/"populated.txt")=="Populated exactly");
 }
 {
 int calls=0;Branch3Runtime r(guide(json::array({artifact("step_1","package",root,true),artifact("step_2","note.txt",root/"package",false,{"step_1"})})),{},[&](const auto&,const auto&){return ++calls==1?op("step_1","filesystem.create",{{"name","package"},{"location",Utf8::path(root)}}):op("step_2","filesystem.create_file",{{"name","note.txt"},{"location",Utf8::path(root/"package")},{"content","Note"}});});
 test("02_create_directory_and_file",r.run("Create a directory and populated file").success && calls==2 && read(root/"package"/"note.txt")=="Note");
 }
 {
 int calls=0;const auto path=Utf8::path(root/"populated.txt");Branch3Runtime r(guide(json::array({step("step_1","Inspect the existing file")})),{},[&](const auto&,const auto&){return ++calls==1?op("step_1","filesystem.inspect",{{"target",path}}):satisfied("step_1","filesystem.inspect",path,"exists",true);});
 test("03_inspect_existing_file",r.run("Inspect existing file").success && calls==2);
 }
 {
 auto missing=step("step_1","Inspect missing file"),present=step("step_2","Use existing resource",{"step_1"}),report=step("step_3","Report missing resource",{"step_1"});present["condition"]={{"step_id","step_1"},{"field","exists"},{"equals",true}};report["condition"]={{"step_id","step_1"},{"field","exists"},{"equals",false}};
 int calls=0;const auto path=Utf8::path(root/"missing.txt");Branch3Runtime r(guide(json::array({missing,present,report})),{},[&](const auto&,const auto&){++calls;if(calls==1)return op("step_1","filesystem.inspect",{{"target",path}});return satisfied(calls==2?"step_1":"step_3","filesystem.inspect",path,"exists",false,"File is missing; no replacement created.");});
 const auto result=r.run("Inspect; open only if present; otherwise report missing");test("04_missing_file_conditional",result.success && calls==3 && !fs::exists(root/"missing.txt") && result.answer.find("missing")!=std::string::npos);
 }
 {
 auto steps=json::array({artifact("step_1","draft.txt",root),artifact("step_2","backup.txt",root,false,{"step_1"}),artifact("step_3","final.txt",root,false,{"step_2"}),step("step_4","Report verified copy and rename",{"step_3"})});int calls=0;
 Branch3Runtime r(guide(steps),{},[&](const auto&,const auto&){switch(++calls){case 1:return op("step_1","filesystem.create_file",{{"name","draft.txt"},{"location",Utf8::path(root)},{"content","Exact copy payload"}});case 2:return op("step_2","filesystem.copy",{{"source",Utf8::path(root/"draft.txt")},{"destination",Utf8::path(root/"backup.txt")}});case 3:return op("step_3","filesystem.rename",{{"target",Utf8::path(root/"draft.txt")},{"newName","final.txt"}});default:return satisfied("step_4","filesystem.copy",Utf8::path(root/"backup.txt"),"source_bytes_equal",true,"final.txt and backup.txt verified identical.");}});
 const auto result=r.run("Create, copy, rename and verify");test("05_create_copy_rename_verify",result.success && calls==4 && read(root/"final.txt")==read(root/"backup.txt") && !fs::exists(root/"draft.txt"));
 }
 {
 auto steps=json::array({step("step_1","First observation"),step("step_2","Second observation"),step("step_3","Join both observations",{"step_1","step_2"})});int calls=0;bool selected=true;
 Branch3Runtime r(guide(steps),{},[&](const auto&,const std::string& prompt){const int n=++calls;const std::string id=n<=2?"step_1":n<=4?"step_2":"step_3";selected &= prompt.find("CURRENT_READY_STEP:\n"+id)!=std::string::npos;return n==1 || n==3?op(id,"filesystem.inspect",{{"target",Utf8::path(root)}}):satisfied(id,"filesystem.inspect",Utf8::path(root),"exists",true);});
 test("06_multiple_dependencies",r.run("Observe independent prerequisites then join").success && calls==5 && selected);
 }
 {
 int calls=0,executions=0;Branch3Runtime r(guide(json::array({step("step_1","Repair an observation")})),{},[&](const auto&,const auto&){return ++calls<=2?op("step_1","filesystem.inspect",{{"target",Utf8::path(root)}}):satisfied("step_1","filesystem.inspect",Utf8::path(root),"exists",true);},[&](const PrimitiveOperation& operation){auto result=success(operation,{{"exists",true},{"path",Utf8::path(root)}});result.resolvedPath=Utf8::path(root);if(++executions==1){result.status=ExecutionStatus::FAILED;result.failureCategory="target_not_found";result.message="Repairable fixture failure";}return result;});
 const auto result=r.run("Repair failed primitive");test("07_failed_primitive_repaired",result.success && executions==2 && result.lastExecutionIssue.empty());
 }
 {
 int calls=0;Branch3Runtime r(guide(json::array({step("step_1","Observe after provider recovery")})),{},[&](const auto&,const auto&){if(++calls<=2)return LLMResponse{false,"","Fixture timeout",LLMFailureCategory::TIMEOUT};return calls==3?op("step_1","filesystem.inspect",{{"target",Utf8::path(root)}}):satisfied("step_1","filesystem.inspect",Utf8::path(root),"exists",true);});
 const auto paused=r.run("Recover planner dependency");const auto resumed=r.retryBlockedDependency();test("08_planner_timeout_recovery",paused.cause==RuntimeCause::LLM_TIMEOUT && resumed.success && resumed.operationResults.size()==1);
 }
 {
 int calls=0;Branch3Runtime r(guide(json::array({step("step_1","Navigate"),step("step_2","Extract answer",{"step_1"})})),{},[&](const auto&,const auto&){++calls;if(calls==1)return op("step_1","browser.navigate",{{"url","https://fixture.invalid/"},{"ready_condition","load"}});return calls==2?browserProof("step_1","url_equals","https://fixture.invalid/"):browserProof("step_2","text_contains","Version 7.2","Version 7.2, source https://fixture.invalid/");},[&](const PrimitiveOperation& operation){auto result=success(operation);if(operation.name=="browser.observe")result.browserData=json{{"url","https://fixture.invalid/"},{"readable_text","Version 7.2"},{"auth_required",false}}.dump();return result;});
 const auto result=r.run("Navigate and extract answer");test("09_browser_navigation_and_answer",result.success && calls==3 && result.answer.find("Version 7.2")!=std::string::npos);
 }
 {
 int calls=0,types=0,submits=0;const std::string payload="Literal message";Branch3Runtime r(guide(json::array({step("step_1","Input and submit requested payload")})),{},[&](const auto&,const auto&){switch(++calls){case 1:return op("step_1","browser.navigate",{{"url","https://fixture.invalid/form"},{"ready_condition","load"}});case 2:return op("step_1","browser.type",{{"element_id","input"},{"content",payload}});case 3:return op("step_1","browser.click",{{"element_id","submit"}});default:return browserProof("step_1","text_contains","Recorded: Literal message");}},[&](const PrimitiveOperation& operation){auto result=success(operation);if(operation.name=="browser.type" && operation.parameters.content==payload)++types;if(operation.name=="browser.click")++submits;if(operation.name=="browser.observe")result.browserData=json{{"url","https://fixture.invalid/form"},{"readable_text",submits?"Recorded: Literal message":"Form"},{"auth_required",false},{"focus",{{"editable",true},{"value",types?payload:""}}}}.dump();return result;});
 test("10_browser_input_and_submission",r.run("Input exact payload and submit").success && types==1 && submits==1);
 }
 {
 int calls=0;const auto path=Utf8::path(root/"download.bin");Branch3Runtime r(guide(json::array({step("step_1","Complete authorized download")})),{},[&](const auto&,const auto&){if(++calls==1)return op("step_1","browser.navigate",{{"url","https://fixture.invalid/file"},{"ready_condition","load"}});return calls==2?op("step_1","browser.download",{{"element_id","download"},{"location",Utf8::path(root)}}):satisfied("step_1","browser.download",path,"verified_kind","download_complete");},[&](const PrimitiveOperation& operation){auto result=success(operation);if(operation.name=="browser.download"){result.postconditionVerified=true;result.verifiedKind="download_complete";result.resolvedPath=path;}if(operation.name=="browser.observe")result.browserData=json{{"url","https://fixture.invalid/file"},{"readable_text","Download finished"},{"auth_required",false}}.dump();return result;});
 test("11_browser_download_completion",r.run("Download authorized file").success && calls==3);
 }
 {
 int calls=0;Branch3Runtime r(guide(json::array({step("step_1","Launch registered application")})),{},[&](const auto&,const auto&){return ++calls==1?op("step_1","application.launch",{{"application","Fixture Application"}}):satisfied("step_1","application.launch","","process_id",42);},[&](const PrimitiveOperation& operation){return success(operation,{{"process_id",42},{"window_observed",true}});});
 test("12_application_launch_evidence",r.run("Launch registered application").success && calls==2);
 }
 {
 int effects=0;Branch3Runtime r(guide(json::array({step("step_1","Resolve ambiguous target")})),{},[](const auto&,const auto&){return reply({{"status","ask_user"},{"message","Which of the two matching targets?"}});},[&](const PrimitiveOperation& operation){++effects;return success(operation);});
 const auto result=r.run("Use ambiguous resource");test("13_ambiguous_request",result.outcome==RuntimeOutcome::NEEDS_USER && effects==0 && result.message.find("Which")!=std::string::npos);
 }
 {
 auto a=step("step_1","Observe"),b=step("step_2","Only when present",{"step_1"});b["condition"]={{"step_id","step_1"},{"field","exists"},{"equals",true}};int calls=0;
 const auto path=Utf8::path(root/"absent.txt");Branch3Runtime r(guide(json::array({a,b})),{},[&](const auto&,const auto&){return ++calls==1?op("step_1","filesystem.inspect",{{"target",path}}):satisfied("step_1","filesystem.inspect",path,"exists",false);});
 const auto result=r.run("Execute only if observed present");test("14_false_branch_skipped",result.success && calls==2 && result.operationResults.size()==1 && result.stateLog.find("[NOT_APPLICABLE]")!=std::string::npos);
 }
 {
 const std::string literal="Café — आशा ₹ 😀\nCase, punctuation: KEEP!";Branch3Runtime r(guide(json::array({artifact("step_1","literal.txt",root)})),{},[&](const auto&,const auto&){return op("step_1","filesystem.create_file",{{"name","literal.txt"},{"location",Utf8::path(root)},{"content",literal}});});
 test("15_exact_literal_payload",r.run("Preserve exact Unicode literal payload").success && read(root/"literal.txt")==literal);
 }
 fs::remove_all(root);std::cout<<"Fixed acceptance scenarios: 15; Failures: "<<failures<<". Browser/application adapters simulated; filesystem effects real. No network or LLM.\n";return failures?1:0;
}
