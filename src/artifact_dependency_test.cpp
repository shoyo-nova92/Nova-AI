#include "branch3_runtime.hpp"
#include "runtime_supervisor.hpp"
#include "artifact_binding.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <chrono>
using nlohmann::json;
namespace fs=std::filesystem;
LLMResponse response(json value){return {true,value.dump(),""};}
json artifact(const std::string& id,const std::string& kind,const std::string& name,json location,std::vector<std::string> deps={}){
    return {{"id",id},{"description","Create exactly the requested artifact"},{"depends_on",deps},{"content_requirement",kind=="file_content"?"required":"unspecified"},
        {"postcondition",{{"kind",kind},{"name",name},{"location",location}}}};
}
LLMResponse operation(const std::string& id,const std::string& name,json parameters){return response({{"status","operation"},{"step_id",id},{"operation",{{"name",name},{"parameters",parameters}}}});}
json output(const std::string& step){return {{"step_id",step},{"output","path"}};}
std::string read(const fs::path& path){std::ifstream input(path,std::ios::binary);return {(std::istreambuf_iterator<char>(input)),{}};}
int main(){
    int failures=0;auto check=[&](const std::string& name,bool okay){std::cout<<name<<": "<<(okay?"PASS":"FAIL")<<'\n';if(!okay)++failures;};
    const auto root=fs::temp_directory_path()/("NovaArtifactDependency_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if(!fs::create_directory(root))return 2;
    {
        int decompositions=0,plans=0,folders=0,files=0;bool absentBeforeExecution=false;std::string producerPath,childParent;
        const auto steps=json::array({artifact("step_1","directory_exists","A",Utf8::path(root)),artifact("step_2","file_content","B.txt","A")});
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return response({{"steps",steps}});},{},[&](const auto&,const auto&){
            if(++plans==1)return operation("step_1","filesystem.create",{{"name","A"},{"location",Utf8::path(root)}});
            return operation("step_2","filesystem.create_file",{{"name","B.txt"},{"location","A"},{"content","hello nova"}});
        },[&](const PrimitiveOperation& op){
            if(op.name=="filesystem.create"){++folders;absentBeforeExecution=!fs::exists(root/"A");}
            else{++files;childParent=op.parameters.location;}
            auto result=PrimitiveExecutionEngine{}.execute(op);
            if(op.name=="filesystem.create") {
                // Equivalent canonical backend spelling must be consumed directly.
                producerPath=result.resolvedPath;std::replace(producerPath.begin(),producerPath.end(),'\\','/');
                std::transform(producerPath.begin(),producerPath.end(),producerPath.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
                result.resolvedPath=producerPath;auto facts=json::parse(result.factsJson);facts["path"]=producerPath;result.factsJson=facts.dump();
            }
            return result;
        });
        const auto result=runtime.start("Create folder A in the existing location and create B.txt inside it containing hello nova");
        check("bare_new_directory_reference_is_not_an_environment_lookup",result.success && absentBeforeExecution && result.decomposedPlan.valid);
        const auto& child=result.decomposedPlan.steps.at(1);
        check("consumer_has_typed_producer_reference_and_dependency",child.artifactParentReference && child.artifactParentReference->type==ContextReferenceType::PREVIOUS_STEP_OUTPUT && child.artifactParentReference->sourceStepId=="step_1" && std::find(child.dependencies.begin(),child.dependencies.end(),"step_1")!=child.dependencies.end());
        check("parent_is_bound_from_verified_backend_output",childParent==producerPath && !producerPath.empty());
        check("folder_and_populated_file_created_once",folders==1 && files==1 && plans==2 && decompositions==1 && read(root/"A"/"B.txt")=="hello nova");
        check("normal_artifact_chain_has_no_recovery_question",!runtime.pendingUserDecision() && result.verifiedProgress.size()==2);
        RuntimeSupervisor supervisor(runtime);std::ostringstream menu;supervisor.handleOutcome(result,menu);
        check("completed_workflow_has_only_new_request_or_quit",menu.str().find("[A]")==std::string::npos && menu.str().find("[C]")==std::string::npos && !runtime.controls().guidance && !runtime.controls().reobserve);
    }
    {
        int plans=0,folders=0,files=0;const auto steps=json::array({artifact("step_1","directory_exists","Outer",Utf8::path(root)),artifact("step_2","directory_exists","Inner",output("step_1")),artifact("step_3","file_content","C.txt",output("step_2"))});
        const std::string payload="Exact populated content\nCafé — आशा";
        Branch3Runtime runtime([&](const auto&,const auto&){return response({{"steps",steps}});},{},[&](const auto&,const auto&){switch(++plans){
            case 1:return operation("step_1","filesystem.create",{{"name","Outer"},{"location",Utf8::path(root)}});
            case 2:return operation("step_2","filesystem.create",{{"name","Inner"},{"location","unresolved future directory"}});
            default:return operation("step_3","filesystem.create_file",{{"name","C.txt"},{"location",output("step_2")},{"content",payload}});
        }},[&](const auto& op){if(op.name=="filesystem.create")++folders;else ++files;return PrimitiveExecutionEngine{}.execute(op);});
        const auto result=runtime.start("Create Outer then Inner inside Outer then populated C.txt inside Inner");
        check("recursive_producer_to_consumer_chaining",result.success && folders==2 && files==1 && read(root/"Outer"/"Inner"/"C.txt")==payload && !runtime.pendingUserDecision());
    }
    {
        int plans=0,folders=0,files=0;const auto steps=json::array({artifact("step_1","directory_exists","Project",Utf8::path(root)),artifact("step_2","file_content","one.txt",output("step_1")),artifact("step_3","file_content","two.txt","Project",{"step_1"})});
        Branch3Runtime runtime([&](const auto&,const auto&){return response({{"steps",steps}});},{},[&](const auto&,const auto&){++plans;return plans==1?operation("step_1","filesystem.create",{{"name","Project"},{"location",Utf8::path(root)}}):operation(plans==2?"step_2":"step_3","filesystem.create_file",{{"name",plans==2?"one.txt":"two.txt"},{"location","Project"},{"content",plans==2?"first":"second"}});},[&](const auto& op){if(op.name=="filesystem.create")++folders;else ++files;return PrimitiveExecutionEngine{}.execute(op);});
        check("several_files_share_one_verified_directory_output",runtime.start("Create a project directory and two populated files").success && folders==1 && files==2 && read(root/"Project"/"one.txt")=="first" && read(root/"Project"/"two.txt")=="second");
    }
    for(const auto& producer:std::vector<std::string>{"step_2","step_9","step_1"}) {
        int decompositions=0,executions=0;auto steps=json::array({artifact("step_1","file_content","not-directory.txt",Utf8::path(root)),artifact("step_2","file_content","blocked.txt",output(producer))});
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return response({{"steps",steps}});},{},[](const auto&,const auto&){return operation("step_1","filesystem.create_file",{{"name","not-directory.txt"},{"location","C:/fixture"},{"content","test"}});},[&](const auto&){++executions;return ExecutionResult{};});
        const auto result=runtime.start("Reject an invalid producer reference");std::ostringstream menu;RuntimeSupervisor supervisor(runtime);supervisor.handleOutcome(result,menu);
        check("invalid_ref_rejected_before_execution_"+producer,result.cause==RuntimeCause::INVALID_PLAN && executions==0 && decompositions==2);
        check("invalid_plan_does_not_advertise_guidance_or_continue_"+producer,menu.str().find("[A]")==std::string::npos && menu.str().find("[C]")==std::string::npos && !runtime.controls().answer && !runtime.controls().guidance && !runtime.controls().reobserve);
        std::istringstream input("A\nC\nV\n");std::ostringstream output;check("invalid_menu_actions_cannot_construct_fake_recovery_"+producer,supervisor.readControl(input,output,result)==SupervisorAction::NEW_REQUEST && output.str().find("No incomplete retained workflow")==std::string::npos && executions==0);
    }
    {
        int executions=0;const auto steps=json::array({artifact("step_1","directory_exists","Safe",Utf8::path(root)),artifact("step_2","file_content","../escape.txt",output("step_1"))});
        Branch3Runtime runtime([&](const auto&,const auto&){return response({{"steps",steps}});},{},[](const auto&,const auto&){return operation("step_1","filesystem.create",{{"name","Safe"},{"location","C:/fixture"}});},[&](const auto&){++executions;return ExecutionResult{};});
        const auto result=runtime.start("Reject a traversing artifact reference");std::ostringstream menu;RuntimeSupervisor supervisor(runtime);supervisor.handleOutcome(result,menu);
        check("invalid_graph_with_allocated_kernel_has_no_recovery_actions",result.cause==RuntimeCause::INVALID_PLAN && executions==0 && menu.str().find("[A]")==std::string::npos && menu.str().find("[C]")==std::string::npos);
        runtime.continueSession();runtime.requestGuidance();check("invalid_graph_cannot_restart_through_continue_or_guidance",executions==0 && !runtime.pendingUserDecision());
    }
    {
        int calls=0,executions=0;const auto steps=json::array({artifact("step_1","directory_exists","Unverified",Utf8::path(root)),artifact("step_2","file_content","never.txt",output("step_1"))});
        Branch3Runtime runtime([&](const auto&,const auto&){return response({{"steps",steps}});},{},[&](const auto&,const auto&){++calls;return operation("step_1","filesystem.create",{{"name","Unverified"},{"location",Utf8::path(root)}});},[&](const auto& operation){++executions;ExecutionResult result{};result.status=ExecutionStatus::SUCCESS;result.action=operation.name;result.resolvedPath=Utf8::path(root/"Unverified");return result;});
        const auto result=runtime.start("Never consume an unverified producer");check("dispatch_without_artifact_proof_cannot_bind_a_parent",!result.success && executions==1 && !fs::exists(root/"Unverified") && result.verifiedProgress.empty());
    }
    {
        int calls=0;Branch3Runtime runtime([&](const auto&,const auto&){++calls;return LLMResponse{false,"","fixture timeout",LLMFailureCategory::TIMEOUT};},{},{});
        const auto result=runtime.start("Provider failure during planning");std::ostringstream menu;RuntimeSupervisor supervisor(runtime);supervisor.handleOutcome(result,menu);
        check("planning_dependency_retry_is_distinct_from_runtime_recovery",calls==1 && menu.str().find("[R]")!=std::string::npos && menu.str().find("[A]")==std::string::npos && menu.str().find("[C]")==std::string::npos && runtime.controls().retry);
    }
    {
        int decompositions=0,plans=0;auto discover=artifact("step_1","directory_exists","unused",Utf8::path(root));discover.erase("postcondition");discover["postcondition"]={{"source","filesystem"},{"kind","search_has_results"}};
        Branch3Runtime runtime([&](const auto&,const auto&){++decompositions;return response({{"steps",json::array({discover})}});},{},[&](const auto&,const auto&){++plans;return plans==2?operation("step_1","filesystem.list_directory",{{"location",Utf8::path(root)}}):operation("step_1","filesystem.search",{{"location",Utf8::path(root)},{"query","clue"}});},[&](const auto& op){ExecutionResult out{};out.status=ExecutionStatus::SUCCESS;out.factsJson=json{{"category","FILESYSTEM_SEARCH_RESULT"},{"results",plans>2?json::array({json{{"path",Utf8::path(root/"found.txt")},{"name","found.txt"},{"candidate_id","candidate_1"},{"match_score",0.94}}}):json::array()},{"clear_winner",true}}.dump();return out;},[](const auto&,const auto&){return response({{"decision","PROVIDE_INFORMATION"},{"information","use another clue"}});});
        StrategyRecovery::Policy policy;policy.autonomousAttempts=2;runtime.setRecoveryPolicy(policy);
        const auto result=runtime.start("Find the requested resource");std::ostringstream menu;RuntimeSupervisor supervisor(runtime);supervisor.handleOutcome(result,menu);
        check("real_strategy_guidance_still_advertises_answer",result.cause==RuntimeCause::STRATEGY_GUIDANCE && menu.str().find("[A] Provide a clue or alternative method")!=std::string::npos && runtime.controls().answer);
        std::istringstream input("use another clue\n");std::ostringstream output;const auto action=supervisor.readControl(input,output,result);
        const auto completed=runtime.answerUser(supervisor.questionId(),supervisor.answer());check("valid_guidance_accepts_free_form_answer_on_retained_graph",action==SupervisorAction::ANSWER && completed.success && decompositions==1);
    }
    fs::remove_all(root);std::cout<<"Failures: "<<failures<<'\n';return failures?1:0;
}
