#include "workflow_kernel.hpp"
#include "primitive_registry.hpp"
#include <iostream>
#include <random>
#include <stdexcept>
using namespace workflow;
int checks=0,failures=0;
void check(bool result,const std::string& name) {++checks;if(!result){++failures;std::cout<<"FAIL "<<name<<'\n';}}
Step step(std::string id,std::vector<std::string> deps={}) {Step s;s.id=id;s.intent=id;s.dependencies=std::move(deps);return s;}
std::string fact(Kernel& kernel,const std::string& id,Json value={{"observed",true}},std::string operation="fixture.observe",EvidenceCategory category=EvidenceCategory::OBSERVATION) {
    Evidence e;e.stepId=id;e.operation=std::move(operation);e.facts=std::move(value);e.category=category;return kernel.facts.add(std::move(e)).id;
}
Postcondition factPredicate(std::string field="observed",Json value=true) {Postcondition p;p.field=std::move(field);p.value=std::move(value);return p;}
bool verify(Kernel& kernel,const std::string& id,const std::string& evidence,Postcondition p=factPredicate()) {std::string error;return kernel.verify(id,{evidence},{p},error);}
int main() {
    std::mt19937 random(20261028);
    // 250 reproducible DAGs, including shuffled storage order and many joins.
    for(int run=0;run<250;++run) {
        const int count=3+random()%22;
        std::vector<Step> steps;
        for(int i=0;i<count;++i) {
            std::vector<std::string> deps;
            for(int j=0;j<i;++j) if(random()%4==0) deps.push_back("s"+std::to_string(j));
            steps.push_back(step("s"+std::to_string(i),deps));
        }
        std::shuffle(steps.begin(),steps.end(),random);
        Kernel kernel(steps);check(kernel.graph.valid(),"generated DAG valid");
        int completed=0;std::string error;
        while(const auto* selected=kernel.next()) {
            const auto id=selected->id;
            check(kernel.graph.dependenciesSatisfied(*selected),"READY obeys all prerequisites");
            for(const auto& candidate:kernel.graph.steps()) if(!kernel.graph.dependenciesSatisfied(candidate))
                check(!kernel.begin(candidate.id,error),"non-ready cannot run");
            check(kernel.begin(id,error),"selected READY enters RUNNING");
            const auto evidence=fact(kernel,id);kernel.operationFinished(id);
            check(verify(kernel,id,evidence),"typed observation verifies current step");
            ++completed;
        }
        check(completed==count && kernel.complete(),"generated DAG completes without planner done");
    }
    for(bool exists:{false,true}) {
        auto a=step("inspect"),b=step("present",{"inspect"}),c=step("missing",{"inspect"}),d=step("join",{"present","missing"});
        b.condition=Condition{"inspect","exists",true};c.condition=Condition{"inspect","exists",false};
        Kernel kernel({d,c,b,a});std::string error;
        check(kernel.next()->id=="inspect","condition prerequisites schedule first");
        check(WorkflowScheduler::evaluate(*b.condition,kernel.graph,kernel.facts)==Truth::UNKNOWN,"unverified condition is UNKNOWN");
        const auto e=fact(kernel,"inspect",{{"exists",exists}});
        check(verify(kernel,"inspect",e,factPredicate("exists",exists)),"inspect fact verifies");
        const auto applies=exists?"present":"missing",skips=exists?"missing":"present";
        check(kernel.graph.find(skips)->status==StepStatus::SKIPPED && kernel.graph.find(skips)->validSkip,"false branch validly skipped");
        check(!kernel.begin(skips,error),"skipped branch cannot execute");
        check(kernel.next()->id==applies,"true branch READY");
        check(verify(kernel,applies,e,factPredicate("exists",exists)),"ancestor fact reusable");
        const auto answer=fact(kernel,"join");
        check(kernel.next()->id=="join" && verify(kernel,"join",answer) && kernel.complete(),"join accepts VERIFIED and valid SKIPPED");
    }
    {
        auto a=step("a"),b=step("b",{"a"});b.condition=Condition{"a","absent_fact",true};
        Kernel k({a,b});k.next();verify(k,"a",fact(k,"a"));
        check(!k.next() && !k.complete() && k.graph.find("b")->status==StepStatus::BLOCKED,"missing condition fact stays UNKNOWN");
        check(WorkflowScheduler::blocker(k.graph).find("condition UNKNOWN")!=std::string::npos,"unknown condition exact blocker");
    }
    {
        Kernel k({step("a"),step("b",{"a"}),step("c",{"b"})});std::string error;k.next();
        check(!k.begin("c",error),"A to B to C cannot start C");
        k.begin("a",error);k.recordFailure("a",{RuntimeCause::LLM_TIMEOUT,"timeout","provider timed out",true});
        check(k.graph.find("a")->recovery.active->category==RuntimeCause::LLM_TIMEOUT,"provider cause retained");
        k.retry();check(k.next()->id=="a","retry schedules same failed step");
        k.begin("a",error);auto earlier=fact(k,"a");k.operationFinished("a");
        check(verify(k,"a",earlier) && !k.graph.find("a")->recovery.active,"successful verification clears old failure");
        verify(k,"b",fact(k,"b"));
        check(verify(k,"c",earlier) && k.complete(),"transitive A evidence accepted by C");
    }
    {
        Kernel k({step("a"),step("b")});k.next();auto unrelated=fact(k,"a");
        check(!verify(k,"b",unrelated),"unrelated evidence rejected");
        auto failure=fact(k,"b",{{"observed",true}},"failed",EvidenceCategory::FAILURE);
        check(!verify(k,"b",failure),"failure prose cannot prove success");
        std::string error;check(!k.verify("b",{unrelated},{},error),"empty interpretation rejected");
        k.recordFailure("b",{RuntimeCause::ACCESS_DENIED,"access_denied","policy denied",false});verify(k,"a",unrelated);k.refresh();
        check(!k.begin("b",error) && WorkflowScheduler::blocker(k.graph).find("access_denied")!=std::string::npos,"blocked cause retained without LLM override");
    }
    {
        auto a=step("copy");Postcondition p;p.kind=PredicateKind::ARTIFACT;p.path="C:/fixture/backup.txt";p.value="file_content";a.postconditions={p};
        Kernel k({a,step("rename",{"copy"}),step("verify",{"rename"})});k.next();
        auto copy=fact(k,"copy",{{"path","C:\\fixture\\backup.txt"},{"verified_kind","file_content"},{"source_bytes_equal",true}},"filesystem.copy",EvidenceCategory::ARTIFACT);
        check(verify(k,"copy",copy),"copy establishes artifact postcondition");
        auto rename=fact(k,"rename",{{"observed",true}},"filesystem.rename");verify(k,"rename",rename);
        Postcondition source;source.operation="filesystem.copy";source.field="source_bytes_equal";source.value=true;
        check(verify(k,"verify",copy,source) && k.complete(),"copy rename later verify A evidence");
    }
    {
        Kernel k({step("a")});k.next();
        const auto old=fact(k,"a",{{"url","https://fixture/old"}},"browser.observe",EvidenceCategory::BROWSER_OBSERVATION);
        fact(k,"a",{{"url","https://fixture/new"}},"browser.observe",EvidenceCategory::BROWSER_OBSERVATION);
        check(!verify(k,"a",old,factPredicate("url","https://fixture/old")),"superseded browser observation cannot verify current state");
    }
    check(!WorkflowGraph({step("a",{"missing"})}).valid(),"missing prerequisite rejected");
    check(!WorkflowGraph({step("a",{"b"}),step("b",{"a"})}).valid(),"cycle rejected");
    check(!WorkflowGraph({step("a"),step("a")}).valid(),"duplicate ID rejected");
    check(!WorkflowGraph({}).valid(),"empty graph rejected");
    {
        auto b=step("b");b.condition=Condition{"a","exists",true};
        check(!WorkflowGraph({step("a"),b}).valid(),"unrelated condition producer rejected");
    }
    check(PrimitiveRegistry::isSupported("filesystem.inspect"),"canonical capability lookup");
    check(!PrimitiveRegistry::isSupported("invented.unsupported"),"unsupported capability explicit lookup miss");
    std::cout<<"Generated DAG cases: 250; Checks: "<<checks<<"; Failures: "<<failures<<'\n';return failures?1:0;
}
