#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include "predicate_registry.hpp"
#include "filesystem_ranking.hpp"
#include "semantic_evidence.hpp"
#include "browser_state.hpp"
#include "utf8_text.hpp"
#include "application_correlation.hpp"
#include <algorithm>
#include <cctype>
#include <set>
#include <sstream>
#include <cwctype>
using nlohmann::json;
namespace wf=workflow;
namespace {
std::set<std::string> fieldNames(const std::string& source) {std::set<std::string> out;for(const auto& [name,type]:PredicateRegistry::fields(source))out.insert(name);return out;}
const auto elementFields=fieldNames("browser"),fsFields=fieldNames("filesystem"),appFields=fieldNames("application");
std::string kindOf(const json& proof) {return PredicateRegistry::kindOf(proof);}
json constraints(const json& proof,const std::set<std::string>& fields) {
    json result=proof.contains("match")?proof["match"]:json::object();
    for(const auto& field:fields)if(proof.contains(field))result[field]=proof[field];
    return result;
}
bool rowMatches(const json& row,const json& expected,bool application=false) {
    if(!row.is_object())return false;
    for(const auto& [key,value]:expected.items()) {
        if(!row.contains(key))return false;
        if(application && key=="target_path") {
            if(!value.is_string() || !row[key].is_string() || !wf::sameIdentity(row[key].get<std::string>(),value.get<std::string>()))return false;
        }else if(row[key]!=value)return false;
    }
    return true;
}
bool stringTest(const json& facts,const std::string& field,const json& value,bool contains) {
    if(!facts.contains(field) || !facts[field].is_string() || !value.is_string())return false;
    const auto text=facts[field].get<std::string>(),expected=value.get<std::string>();
    return contains?!expected.empty() && text.find(expected)!=std::string::npos:text==expected;
}
void stableElement(json& item) {
    if(!item.is_object())return;
    item.erase("id");item.erase("element_id");item.erase("observation_token");
}
std::string extensionIdentity(std::string value) {
    if(!value.empty() && value.front()!='.')value.insert(value.begin(),'.');
    std::transform(value.begin(),value.end(),value.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});return value;
}
// Search paths come from canonical backend records. Normalize their spelling
// lexically without consulting the live filesystem or resolving a new target.
std::wstring pathIdentity(std::string value) {
    std::replace(value.begin(),value.end(),'\\','/');
    if(value.starts_with("//")) {
        // libstdc++ on Windows does not consistently recognize UNC root names.
        // Keep server/share immutable and normalize only the path below it.
        const auto serverEnd=value.find('/',2);
        if(serverEnd==std::string::npos || serverEnd==2)return {};
        const auto shareEnd=value.find('/',serverEnd+1);
        const auto rootEnd=shareEnd==std::string::npos?value.size():shareEnd;
        if(rootEnd==serverEnd+1)return {};
        auto path=L"//"+Utf8::toPath(value.substr(2,rootEnd-2)).generic_wstring();
        if(shareEnd!=std::string::npos)path+=Utf8::toPath(value.substr(shareEnd)).lexically_normal().generic_wstring();
        while(!path.empty() && path.back()==L'/')path.pop_back();return path;
    }
    const auto parsed=Utf8::toPath(value);
    if(!parsed.is_absolute())return {};
    auto path=parsed.lexically_normal().generic_wstring();
    while(path.size()>1 && path.back()==L'/')path.pop_back();
    return path;
}
bool equalPathSpelling(const std::wstring& a,const std::wstring& b) {
#ifdef _WIN32
    return !a.empty() && a.size()==b.size() && CompareStringOrdinal(a.data(),static_cast<int>(a.size()),b.data(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL;
#else
    return !a.empty() && a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),[](wchar_t x,wchar_t y){return std::towlower(x)==std::towlower(y);});
#endif
}
bool sameFilesystemPath(const std::string& a,const std::string& b) {
    return equalPathSpelling(pathIdentity(a),pathIdentity(b));
}
bool underRoot(const std::string& path,const std::string& root) {
    const auto child=pathIdentity(path),parent=pathIdentity(root);
    return !parent.empty() && child.size()>parent.size() && child[parent.size()]==L'/' && equalPathSpelling(child.substr(0,parent.size()),parent);
}
bool searchMatches(const json& facts,const json& expected,std::string& error) {
    auto fail=[&](const std::string& reason){error=reason;return false;};
    // These constraints describe the enclosing search, not a candidate row.
    for(const auto field:{"query","search_complete"})if(expected.contains(field)) {
        if(!facts.contains(field))return fail(std::string("Missing search metadata: ")+field);
        if(std::string(field)=="query") {
            if(!facts[field].is_string() || FilesystemRanking::tokens(facts[field].get<std::string>())!=FilesystemRanking::tokens(expected[field].get<std::string>()))return fail("Search query does not match the declared query");
        }else if(!facts[field].is_boolean() || facts[field]!=expected[field])return fail("Search completeness does not satisfy the declared constraint");
    }
    const bool scoped=expected.contains("search_root") || expected.contains("search_roots");
    if(scoped) {
        if(!facts.contains("search_root") || !facts["search_root"].is_string())return fail("Missing canonical search root metadata");
        if(expected.value("scope_strength",std::string("REQUIRED"))=="REQUIRED") {
            const auto roots=expected.contains("search_roots")?expected["search_roots"]:json::array({expected["search_root"]});
            bool allowed=false;
            for(const auto& root:roots)allowed |= sameFilesystemPath(facts["search_root"].get<std::string>(),root.get<std::string>()) || underRoot(facts["search_root"].get<std::string>(),root.get<std::string>());
            if(!allowed)return fail("Search root is outside the declared required scope");
        }
    }
    if(!facts.contains("results") || !facts["results"].is_array() || facts["results"].empty())return fail("Search has no structured candidate results");
    std::string mismatch="No candidate satisfies every declared result constraint";
    for(const auto& row:facts["results"]) {
        if(!row.is_object()){mismatch="Search candidate is not a structured record";continue;}
        bool okay=true;
        for(const auto& [key,value]:expected.items()) {
            if(key=="root_semantics" || key=="scope_strength" || key=="scope_provenance")continue;
            if(key=="query") {
                okay=row.contains("match_score") && row["match_score"].is_number() && std::isfinite(row["match_score"].get<double>()) && row["match_score"].get<double>()>=FilesystemRanking::minimumScore;
            }else if(key=="search_complete")continue;
            else if(key=="search_root" || key=="search_roots") {
                okay=row.contains("path") && row["path"].is_string() && underRoot(row["path"].get<std::string>(),facts["search_root"].get<std::string>());
            }else if(key=="modified_within_days") {
                okay=row.contains("modified_age_days") && row["modified_age_days"].is_number() && std::isfinite(row["modified_age_days"].get<double>()) && row["modified_age_days"].get<double>()>=0 && row["modified_age_days"].get<double>()<=value.get<double>();
            }else {
                const auto field=key=="name_contains"?"name":key;
                if(!row.contains(field) || !row[field].is_string()){okay=false;}
                else {
                    const auto actual=row[field].get<std::string>(),wanted=value.get<std::string>();
                    if(key=="path" || key=="parent_path") {
                        okay=sameFilesystemPath(actual,wanted);
                        if(okay && key=="parent_path" && row.contains("path") && row["path"].is_string())okay=sameFilesystemPath(actual,Utf8::path(Utf8::toPath(row["path"].get<std::string>()).parent_path()));
                    }else if(key=="extension")okay=extensionIdentity(actual)==extensionIdentity(wanted);
                    else if(key=="name_contains")okay=!wanted.empty() && actual.find(wanted)!=std::string::npos;
                    else okay=actual==wanted;
                }
            }
            if(!okay){mismatch="Candidate constraint missing or mismatched: "+key;break;}
        }
        if(okay){error.clear();return true;}
    }
    return fail(mismatch);
}
}
namespace SemanticEvidence {
bool samePath(const std::string& a,const std::string& b) {return sameFilesystemPath(a,b);}
bool withinRoot(const std::string& path,const std::string& root) {return underRoot(path,root);}
wf::Evidence browserObservation(const std::string& step,const std::string& raw) {
    wf::Evidence evidence;evidence.stepId=step;evidence.operation="browser.observe";evidence.category=wf::EvidenceCategory::BROWSER_OBSERVATION;
    evidence.facts=BrowserState::snapshot(raw);if(!evidence.facts.is_object())evidence.facts=json::object();
    evidence.facts.erase("observation_token");evidence.facts.erase("element_id");
    if(evidence.facts.contains("elements") && evidence.facts["elements"].is_array())for(auto& element:evidence.facts["elements"])stableElement(element);
    if(evidence.facts.contains("focus"))stableElement(evidence.facts["focus"]);
    // Keep exact backend facts. Context compaction is a separate presentation operation.
    evidence.facts["input_values"]=json::array();evidence.facts["completed_download_paths"]=json::array();
    auto input=[&](const json& e){if(e.is_object() && e.value("editable",false) && e.contains("value"))evidence.facts["input_values"].push_back(e["value"]);};
    if(evidence.facts.contains("focus"))input(evidence.facts["focus"]);
    if(evidence.facts.contains("elements") && evidence.facts["elements"].is_array())for(const auto& e:evidence.facts["elements"])input(e);
    if(evidence.facts.contains("downloads") && evidence.facts["downloads"].is_array())for(const auto& d:evidence.facts["downloads"])
        if(d.value("status",std::string{})=="completed" && d.contains("path"))evidence.facts["completed_download_paths"].push_back(d["path"]);
    return evidence;
}
wf::Evidence applicationObservation(const std::string& step,const json& facts) {
    wf::Evidence e;e.stepId=step;e.operation="application.observe";e.category=wf::EvidenceCategory::APPLICATION_OBSERVATION;e.facts=facts;return e;
}
std::vector<wf::Postcondition> compile(const json& proof,std::string& error) {
    if(!PredicateRegistry::validateSemantic(proof,error))return {};
    wf::Postcondition p;p.kind=wf::PredicateKind::SEMANTIC;p.value=proof;return {p};
}
bool matches(const wf::Evidence& evidence,const json& proof,std::string& error) {
    if(compile(proof,error).empty())return false;
    if(!evidence.current)return false;
    const auto source=proof["source"].get<std::string>(),kind=kindOf(proof);const auto& facts=evidence.facts;
    if(source=="filesystem") {
        if(!facts.value("operation_succeeded",false))return false;
        const auto expected=constraints(proof,fsFields);
        if(kind=="path_exists" || kind=="path_missing")return evidence.operation=="filesystem.inspect" &&
            facts.contains("exists") && facts["exists"].is_boolean() && facts["exists"]==(kind=="path_exists") &&
            sameFilesystemPath(facts.value("path",std::string{}),expected["path"].get<std::string>());
        if(evidence.operation!="filesystem.search" || facts.value("category",std::string{})!="FILESYSTEM_SEARCH_RESULT" || !facts.contains("results") || !facts["results"].is_array())return false;
        if(kind=="search_has_results")return !facts["results"].empty();
        return searchMatches(facts,expected,error);
    }
    if(source=="browser") {
        if(evidence.category!=wf::EvidenceCategory::BROWSER_OBSERVATION)return false;
        if(kind.starts_with("url_"))return stringTest(facts,"url",proof["value"],kind=="url_contains");
        if(kind.starts_with("title_"))return stringTest(facts,"title",proof["value"],kind=="title_contains");
        if(kind=="text_contains")return stringTest(facts,"readable_text",proof["value"],true);
        if(kind=="input_equals" || kind=="download_complete") {
            const auto field=kind=="input_equals"?"input_values":"completed_download_paths";
            if(!facts.contains(field) || !facts[field].is_array())return false;
            return std::find(facts[field].begin(),facts[field].end(),proof["value"])!=facts[field].end();
        }
        auto expected=constraints(proof,elementFields);if(kind=="link_exists") {
            if(expected.contains("role") && expected["role"]!="link")return false;expected["role"]="link";
        }
        if(facts.contains("elements") && facts["elements"].is_array())for(const auto& e:facts["elements"])if(rowMatches(e,expected))return true;
    } else {
        if(evidence.category!=wf::EvidenceCategory::APPLICATION_OBSERVATION)return false;
        const auto expected=constraints(proof,appFields);const auto collection=kind=="process_exists"?"processes":"windows";
        if(facts.contains(collection) && facts[collection].is_array())for(const auto& row:facts[collection]) {
            if(!rowMatches(row,expected,true))continue;
            if(kind=="process_exists" || kind=="window_exists")return true;
            if(kind=="window_visible" && row.value("window_visible",false))return true;
            if(kind=="foreground" && row.value("foreground",false))return true;
            if(kind=="window_title_contains" && stringTest(row,"window_title",proof["value"],true))return true;
            if(kind=="target_observed" && row.value("target_observed",false) && row.contains("target_evidence") && row["target_evidence"].is_object()) {
                if(ApplicationCorrelation::correlated(facts,row,expected["target_path"].get<std::string>()))return true;
                const auto& association=row["target_evidence"];
                const auto origin=association.value("source",std::string{});
                if(association.value("association",std::string{})=="exact_document_path" &&
                   (origin=="window_title" || origin=="document_or_tab_name" || origin=="document_or_tab_help_text") &&
                   association.contains("observed_path") && association["observed_path"].is_string() &&
                   wf::sameIdentity(association["observed_path"].get<std::string>(),expected["target_path"].get<std::string>()))return true;
            }
        }
    }
    return false;
}
std::string diagnostic(const json& proof,const std::vector<const wf::Evidence*>& available) {
    const auto source=proof.value("source",std::string("unknown")),kind=kindOf(proof);std::set<std::string> fields;bool dispatch=false,observation=false;
    for(const auto* e:available) {
        dispatch |= e->category==wf::EvidenceCategory::APPLICATION_DISPATCH || e->category==wf::EvidenceCategory::APPLICATION_EFFECT;
        const bool applies=source=="browser"?e->category==wf::EvidenceCategory::BROWSER_OBSERVATION:source=="filesystem"?(e->operation=="filesystem.search" || e->operation=="filesystem.inspect"):e->category==wf::EvidenceCategory::APPLICATION_OBSERVATION;
        if(!applies)continue;observation=true;for(const auto& [key,value]:e->facts.items())fields.insert(key);
    }
    std::string out="Requested proof: "+source+"."+kind+". Available scoped current fields: ";for(const auto& field:fields)out+=field+", ";
    if(!observation)out+="none (a fresh "+source+" observation is required)";
    if(source=="application" && kind=="target_observed")out+=". Missing: observed exact target/window document association or validated launch/process/window correlation; dispatch arguments, PID, basename or launch success alone cannot prove it";
    else if(source=="filesystem") {
        std::string detail;
        for(const auto* e:available)if(e->current && e->operation=="filesystem.search") {matches(*e,proof,detail);if(!detail.empty())break;}
        out+=". Missing: "+(detail.empty()?std::string("matching typed current filesystem evidence"):detail);
    }else out+=". Missing: matching typed current observation satisfying every constraint";
    if(dispatch)out+=". Application dispatch evidence is available but is not observed document/window state";
    return Utf8::truncate(out,1200,"semantic.diagnostic");
}
json plannerBrowser(const json& source,const std::string& relevance,size_t budget) {
    json out=json::object();for(const auto* key:{"url","title","page_kind","ready_state","auth_required","session_mode"})if(source.contains(key))out[key]=source[key];
    auto excerpt=[&](const std::string& text,size_t limit) {
        size_t offset=0;std::istringstream words(relevance);std::string word;
        while(words>>word)if(word.size()>=4){auto found=text.find(word);if(found!=std::string::npos && found>limit/2){offset=found-limit/3;while(offset>0 && (static_cast<unsigned char>(text[offset])&0xC0)==0x80)--offset;break;}}
        return Utf8::truncate(text.substr(offset),limit,"planner.excerpt");
    };
    for(const auto* key:{"readable_text","visible_text"})if(source.contains(key) && source[key].is_string())out[key]=excerpt(source[key].get<std::string>(),1600);
    if(source.contains("focus"))out["focus"]=source["focus"];
    out["elements"]=json::array();std::vector<json> elements;
    if(source.contains("elements") && source["elements"].is_array())elements=source["elements"].get<std::vector<json>>();
    auto score=[&](const json& element) {
        int rank=element.value("focused",false)?100:element.value("editable",false)?5:0;std::string text=element.dump();std::istringstream words(relevance);std::string word;
        while(words>>word)if(word.size()>=4 && text.find(word)!=std::string::npos)rank+=10;return rank;
    };
    std::stable_sort(elements.begin(),elements.end(),[&](const json& a,const json& b){return score(a)>score(b);});
    for(const auto& element:elements) {if(out["elements"].size()>=24)break;out["elements"].push_back(element);if(out.dump().size()>budget){out["elements"].erase(out["elements"].size()-1);break;}}
    for(const auto* key:{"effects","downloads","dialogs"})if(source.contains(key)){auto candidate=out;candidate[key]=source[key];if(candidate.dump().size()<=budget)out=std::move(candidate);}
    // Oversized metadata must not defeat the presentation bound. FactStore remains untouched.
    if(out.dump().size()>budget)out=json{{"context_truncated",true},{"message","Observation exceeds presentation budget; exact structured facts remain in FactStore."}};
    return out;
}
}
