#pragma once
#include "workflow_kernel.hpp"
#include "utf8_text.hpp"
#include <filesystem>

// A logical producer reference is not evidence of an existing directory.
// Bind only from the immutable evidence that verified the producer's output.
namespace ArtifactBinding {
struct Directory {
    bool okay=false;
    std::string path,evidenceId,error;
};
inline bool leaf(const std::string& name) {
    const auto path=Utf8::toPath(name);
    return !name.empty() && name!="." && name!=".." && name.find_first_of("/\\")==std::string::npos && !path.is_absolute() && !path.has_root_name();
}
inline Directory parent(const workflow::WorkflowGraph& graph,const workflow::FactStore& facts,
                        const std::string& consumer,const std::string& producer) {
    const auto* source=graph.find(producer);
    if(!source || !graph.ancestor(producer,consumer))return {false,{},{},"Artifact parent must reference a dependency producer"};
    if(source->status!=workflow::StepStatus::VERIFIED)return {false,{},{},"Artifact parent producer is not verified: "+producer};
    Directory result;
    for(const auto& id:source->evidenceIds) {
        const auto* evidence=facts.find(id);
        if(!evidence || !evidence->current || evidence->category!=workflow::EvidenceCategory::ARTIFACT ||
           evidence->facts.value("verified_kind",std::string{})!="directory_exists" || !evidence->facts.contains("path") || !evidence->facts["path"].is_string())continue;
        const auto path=evidence->facts["path"].get<std::string>();
        if(!Utf8::toPath(path).is_absolute())return {false,{},{},"Producer directory output is not an absolute backend path"};
        if(result.okay && !workflow::sameIdentity(result.path,path))return {false,{},{},"Producer has ambiguous directory output evidence"};
        result={true,path,id,{}};
    }
    if(!result.okay)result.error="No current verified directory output is available for "+producer;
    return result;
}
}
