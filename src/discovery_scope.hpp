#pragma once
#include "predicate_registry.hpp"
#include "semantic_evidence.hpp"
#include "filesystem_path_policy.hpp"
#include "utf8_text.hpp"
#include <functional>
#include <stdexcept>
#include <algorithm>
#include <cctype>

// Scope is intent, not an executable path. Resolve each alternative separately;
// unresolved preferences remain clues for recovery, never filesystem parameters.
namespace DiscoveryScope {
using nlohmann::json;
using Resolver=std::function<std::string(const std::string&)>;
inline bool malformed(const std::string& value) {
    if(value.empty() || value.size()>32767)return true;
    for(unsigned char c:value)if(c<32 || c=='<' || c=='>' || c=='|' || c=='"' || c=='?' || c=='*')return true;
    const auto colon=value.find(':');
    if(colon!=std::string::npos && (colon!=1 || !std::isalpha(static_cast<unsigned char>(value[0])) || value.size()<3 || (value[2]!='/' && value[2]!='\\') || value.find(':',2)!=std::string::npos))return true;
    auto normalized=value;std::replace(normalized.begin(),normalized.end(),'\\','/');
    if(normalized.starts_with("//")) {
        const auto serverEnd=normalized.find('/',2);
        if(serverEnd==std::string::npos || serverEnd==2 || serverEnd+1==normalized.size() || normalized[serverEnd+1]=='/')return true;
    }else if(normalized.starts_with('/'))return true; // Windows drive-relative root
    return false;
}
inline std::string resolve(const std::string& clue) {
    if(malformed(clue))return {};
    const FilesystemPathPolicy policy;const auto path=policy.resolve(clue);
    if(!path.ok)return {};
    const auto canonical=policy.authorize(path.path,FilesystemAccess::DISCOVER);
    return canonical.ok?Utf8::path(canonical.path):std::string{};
}
inline json roots(const json& match) {
    return match.contains("search_roots")?match["search_roots"]:match.contains("search_root")?json::array({match["search_root"]}):json::array();
}
inline bool required(const json& match) {return match.value("scope_strength",std::string("REQUIRED"))=="REQUIRED";}
inline json bind(json proof,const Resolver& resolver=resolve) {
    std::string error;if(!PredicateRegistry::validateSemantic(proof,error))throw std::invalid_argument(error);
    auto match=PredicateRegistry::constraints(proof);json bound=json::array();
    for(const auto& item:roots(match)) {
        const auto clue=item.get<std::string>();
        if(malformed(clue))throw std::invalid_argument("Malformed discovery root: "+clue);
        auto path=resolver(clue);
        if(path.empty()) {
            if(required(match))throw std::invalid_argument("Cannot resolve required discovery root: "+clue);
            path=clue;
        }
        bool duplicate=false;
        for(const auto& prior:bound)duplicate |= prior==path || SemanticEvidence::samePath(prior.get<std::string>(),path);
        if(!duplicate)bound.push_back(path);
    }
    if(match.contains("search_roots"))match["search_roots"]=bound;
    else if(match.contains("search_root"))match["search_root"]=bound.at(0);
    for(const auto& [field,type]:PredicateRegistry::filesystemFields)proof.erase(field);
    proof["match"]=match;return proof;
}
inline bool permits(const json& match,const std::string& path) {
    if(!required(match) || roots(match).empty())return true;
    for(const auto& root:roots(match))if(SemanticEvidence::samePath(path,root.get<std::string>()) || SemanticEvidence::withinRoot(path,root.get<std::string>()))return true;
    return false;
}
}
