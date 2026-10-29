#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <set>
#include <map>
#include <cmath>

// Single vocabulary/schema used by decomposition, graph admission, planning,
// and semantic compilation. Action names are not semantic predicates.
namespace PredicateRegistry {
using nlohmann::json;
inline const std::set<std::string> browserKinds={"url_equals","url_contains","title_equals","title_contains","text_contains","element_exists","element_matches","link_exists","input_equals","download_complete"};
inline const std::set<std::string> filesystemKinds={"path_exists","path_missing","search_has_results","search_result_matches"};
inline const std::set<std::string> applicationKinds={"process_exists","window_exists","window_visible","window_title_contains","target_observed","foreground"};
inline const std::set<std::string> artifactKinds={"directory_exists","file_content"};
inline const std::map<std::string,std::string> browserFields={{"role","string"},{"text","string"},{"accessible_name","string"},{"href","string"},{"value","scalar"},{"enabled","boolean"},{"visible","boolean"}};
inline const std::map<std::string,std::string> filesystemFields={{"path","string"},{"name","string"},{"name_contains","string"},{"extension","string"},{"entry_type","string"},{"parent_path","string"},{"search_root","string"},{"search_roots","array"},{"root_semantics","string"},{"scope_strength","string"},{"scope_provenance","string"},{"query","string"},{"search_complete","boolean"},{"modified_within_days","number"}};
inline const std::map<std::string,std::string> applicationFields={{"process_id","integer"},{"process_name","string"},{"window_handle","string"},{"target_path","string"}};
inline std::string kindOf(const json& proof) {
    auto kind=proof.value("kind",std::string{});const auto prefix=proof.value("source",std::string{})+".";
    if(kind.starts_with(prefix))kind.erase(0,prefix.size());return kind;
}
inline const std::map<std::string,std::string>& fields(const std::string& source) {
    return source=="browser"?browserFields:source=="filesystem"?filesystemFields:applicationFields;
}
inline json constraints(const json& proof) {
    json out=proof.value("match",json::object());
    for(const auto& [field,type]:fields(proof.at("source").get<std::string>()))if(proof.contains(field))out[field]=proof[field];return out;
}
inline bool scalarKind(const std::string& source,const std::string& kind) {
    return source=="browser" && kind!="element_exists" && kind!="element_matches" && kind!="link_exists" || source=="application" && kind=="window_title_contains";
}
inline bool validateSemantic(const json& proof,std::string& error) {
    error.clear();auto invalid=[&](const std::string& why){error=why;return false;};
    if(!proof.is_object() || !proof.contains("source") || !proof["source"].is_string() || !proof.contains("kind") || !proof["kind"].is_string())return invalid("Semantic proof requires string source and kind");
    const auto source=proof["source"].get<std::string>(),kind=kindOf(proof);
    const auto* kinds=source=="browser"?&browserKinds:source=="filesystem"?&filesystemKinds:source=="application"?&applicationKinds:nullptr;
    if(!kinds || !kinds->contains(kind))return invalid("Unsupported verification predicate: "+source+"."+kind);
    const auto& supported=fields(source);std::set<std::string> allowed={"source","kind","match"};
    if(scalarKind(source,kind))allowed.insert("value");
    for(const auto& [field,type]:supported)allowed.insert(field);
    for(const auto& [key,value]:proof.items())if(!allowed.contains(key))return invalid("Unsupported semantic constraint: "+key+" (ephemeral IDs cannot be proof)");
    if(proof.contains("match") && !proof["match"].is_object())return invalid("match must be an object of stable fields");
    const auto match=constraints(proof);
    for(const auto& [key,value]:match.items()) {
        if(!supported.contains(key))return invalid("Unsupported semantic match field: "+key);
        const auto type=supported.at(key);
        if(type=="array"?!value.is_array():type=="string"?!value.is_string():type=="boolean"?!value.is_boolean():type=="integer"?!value.is_number_integer():type=="number"?!value.is_number():!value.is_primitive())return invalid("Invalid typed semantic match field: "+key);
        if(proof.contains("match") && proof["match"].contains(key) && proof.contains(key) && proof["match"][key]!=proof[key])return invalid("Conflicting semantic match field: "+key);
        if(key=="modified_within_days" && (!std::isfinite(value.get<double>()) || value.get<double>()<0 || value.get<double>()>36500))return invalid("modified_within_days must be from 0 to 36500");
        if(key=="search_roots") {
            if(value.empty() || value.size()>16)return invalid("search_roots requires 1 to 16 individually declared roots");
            for(const auto& root:value)if(!root.is_string() || root.get<std::string>().empty())return invalid("Each search root must be a non-empty string");
        }
        if(key=="root_semantics" && value!="ANY")return invalid("Only ANY root semantics is supported");
        if(key=="scope_strength" && value!="REQUIRED" && value!="PREFERRED")return invalid("Invalid discovery scope strength");
        if(key=="scope_provenance" && value!="USER_CONSTRAINT" && value!="USER_HINT" && value!="INFERRED")return invalid("Invalid discovery scope provenance");
        if(key=="entry_type" && value!="file" && value!="directory" && value!="other")return invalid("Invalid filesystem entry_type");
    }
    if(scalarKind(source,kind) && (!proof.contains("value") || !proof["value"].is_string() || proof["value"].get<std::string>().empty()))return invalid("Predicate requires a non-empty string value");
    if(source=="application" && match.empty())return invalid("Application proof must identify an observed process/window/target");
    if(source=="application" && kind=="target_observed" && (!match.contains("target_path") || match["target_path"].get<std::string>().empty()))return invalid("target_observed requires exact target_path");
    if(source=="browser" && kind=="element_matches" && match.empty())return invalid("element_matches requires stable constraints");
    if(source=="filesystem") {
        if(proof.contains("value"))return invalid("Filesystem predicates use named stable constraints, not value");
        if(kind=="path_exists" || kind=="path_missing") {
            if(match.size()!=1 || !match.contains("path") || match["path"].get<std::string>().empty())return invalid("Filesystem path predicate requires only exact path");
        }else if(kind=="search_has_results" && !match.empty())return invalid("search_has_results accepts no additional constraints");
        else if(kind=="search_result_matches" && match.empty())return invalid("Search result predicate requires stable constraints");
        if(match.contains("search_root") && match.contains("search_roots"))return invalid("Use search_root or search_roots, not both");
        if(match.contains("search_roots") && match.value("root_semantics",std::string{})!="ANY")return invalid("search_roots requires root_semantics ANY");
        if((match.contains("root_semantics") || match.contains("scope_strength") || match.contains("scope_provenance")) && !match.contains("search_root") && !match.contains("search_roots"))return invalid("Scope metadata requires discovery roots");
        if(match.contains("query") && match["query"].get<std::string>().empty())return invalid("query cannot be empty");
    }
    return true;
}
// Legacy artifact declarations are accepted without source. Qualified semantic
// names are spelling aliases only; execution.X is deliberately never translated.
inline bool declaration(const json& input,json& canonical,std::string& error) {
    if(!input.is_object()){error="Postcondition must be an object";return false;}
    canonical=input;
    if(!canonical.contains("kind") || !canonical["kind"].is_string()){error="Postcondition requires a string kind";return false;}
    if(!canonical.contains("source")) {
        const auto name=canonical["kind"].get<std::string>();const auto dot=name.find('.');
        if(dot!=std::string::npos){canonical["source"]=name.substr(0,dot);canonical["kind"]=name.substr(dot+1);}
        else if(artifactKinds.contains(name))canonical["source"]="artifact";
    }
    if(canonical.value("source",std::string{})=="artifact") {
        if(!artifactKinds.contains(canonical["kind"].get<std::string>())){error="Unsupported artifact postcondition";return false;}
        for(const auto& [key,value]:canonical.items())if(key!="source" && key!="kind" && key!="name" && key!="location"){error="Unsupported artifact constraint: "+key;return false;}
        if(!canonical.contains("name") || !canonical["name"].is_string() || canonical["name"].get<std::string>().find_first_not_of(" \t\r\n")==std::string::npos || !canonical.contains("location")){error="Artifact postcondition requires name and location";return false;}
        const auto& location=canonical["location"];
        if(location.is_object()) {
            if(location.size()!=2 || !location.contains("step_id") || !location["step_id"].is_string() || location["step_id"].get<std::string>().empty() || !location.contains("output") || location["output"]!="path") {error="Artifact location reference requires step_id and output=path";return false;}
        }else if(!location.is_string() || location.get<std::string>().find_first_not_of(" \t\r\n")==std::string::npos){error="Artifact location must be a location string or producer path reference";return false;}
        return true;
    }
    return validateSemantic(canonical,error);
}
inline json catalog() {
    json result=json::array();
    for(const auto source:{"filesystem","browser","application"}) {
        const auto& kinds=std::string(source)=="filesystem"?filesystemKinds:std::string(source)=="browser"?browserKinds:applicationKinds;
        for(const auto& kind:kinds) {
            json props=json::object();
            for(const auto& [field,type]:fields(source))props[field]=type=="scalar"?json{{"type",json::array({"string","number","boolean","null"})}}:json{{"type",type}};
            if(std::string(source)=="filesystem") {
                if(kind=="path_exists" || kind=="path_missing")props={{"path",{{"type","string"},{"minLength",1}}}};
                else if(kind=="search_has_results")props=json::object();
                else {
                    props["search_roots"]={{"type","array"},{"minItems",1},{"maxItems",16},{"items",{{"type","string"},{"minLength",1}}}};
                    props["root_semantics"]={{"const","ANY"}};
                    props["scope_strength"]={{"enum",json::array({"REQUIRED","PREFERRED"})},{"default","REQUIRED"}};
                    props["scope_provenance"]={{"enum",json::array({"USER_CONSTRAINT","USER_HINT","INFERRED"})}};
                    props["modified_within_days"]={{"type","number"},{"minimum",0},{"maximum",36500}};
                    props["query"]={{"type","string"},{"minLength",1}};
                    props["entry_type"]={{"enum",json::array({"file","directory","other"})}};
                }
            }
            json schema={{"type","object"},{"additionalProperties",false},{"required",json::array({"source","kind"})},
                {"properties",{{"source",{{"const",source}}},{"kind",{{"const",kind}}},{"match",{{"type","object"},{"additionalProperties",false},{"properties",props}}}}}};
            for(const auto& [field,spec]:props.items())schema["properties"][field]=spec;
            if(scalarKind(source,kind)) {schema["properties"]["value"]={{"type","string"},{"minLength",1}};schema["required"].push_back("value");}
            if(std::string(source)=="application" || kind=="element_matches" || kind=="search_result_matches") {
                schema["anyOf"]=json::array({json{{"required",json::array({"match"})},{"properties",{{"match",{{"minProperties",1}}}}}}});
                for(const auto& [field,spec]:props.items())schema["anyOf"].push_back({{"required",json::array({field})}});
            }
            if(kind=="target_observed") {
                schema["properties"]["target_path"]["minLength"]=1;schema["properties"]["match"]["properties"]["target_path"]["minLength"]=1;
            }
            // Explicit requirements also apply when constraints use top-level shorthand.
            if(kind=="path_exists" || kind=="path_missing" || kind=="target_observed") {
                const auto field=kind=="target_observed"?"target_path":"path";
                schema["anyOf"]=json::array({json{{"required",json::array({field})}},json{{"required",json::array({"match"})},{"properties",{{"match",{{"required",json::array({field})}}}}}}});
            }
            result.push_back({{"name",std::string(source)+"."+kind},{"schema",schema}});
        }
    }
    for(const auto& kind:artifactKinds)result.push_back({{"name","artifact."+kind},{"schema",{{"type","object"},{"additionalProperties",false},{"required",json::array({"kind","name","location"})},{"properties",{{"source",{{"const","artifact"}}},{"kind",{{"const",kind}}},{"name",{{"type","string"},{"minLength",1}}},{"location",{{"oneOf",json::array({json{{"type","string"},{"minLength",1}},json{{"type","object"},{"additionalProperties",false},{"required",json::array({"step_id","output"})},{"properties",{{"step_id",{{"type","string"},{"minLength",1}}},{"output",{{"const","path"}}}}}}})}}}}}}}});
    return result;
}
}
