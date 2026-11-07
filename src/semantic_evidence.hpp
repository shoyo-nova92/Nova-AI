#pragma once
#include "workflow_kernel.hpp"
namespace SemanticEvidence {
bool samePath(const std::string& a,const std::string& b);
bool withinRoot(const std::string& path,const std::string& root);
workflow::Evidence browserObservation(const std::string& step,const std::string& raw);
workflow::Evidence applicationObservation(const std::string& step,const nlohmann::json& facts);
std::vector<workflow::Postcondition> compile(const nlohmann::json& proof,std::string& error);
bool matches(const workflow::Evidence& evidence,const nlohmann::json& proof,std::string& error);
std::string diagnostic(const nlohmann::json& proof,const std::vector<const workflow::Evidence*>& available);
nlohmann::json plannerBrowser(const nlohmann::json& source,const std::string& relevance,size_t budget=10000);
}
