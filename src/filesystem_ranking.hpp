#pragma once
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <set>
#include <cmath>

namespace FilesystemRanking {
using nlohmann::json;
// Ranking confidence is not execution safety. Require both coverage and separation.
inline constexpr double clearScore=0.85, clearMargin=0.15, minimumScore=0.40;
inline std::string normalize(const std::string& text) {
    std::string out;bool space=true;
    for(unsigned char c:text) {
        if(std::isalnum(c) || c>=128) {out+=static_cast<char>(std::tolower(c));space=false;}
        else if(!space){out+=' ';space=true;}
    }
    if(!out.empty() && out.back()==' ')out.pop_back();return out;
}
inline std::set<std::string> tokens(const std::string& text) {
    std::istringstream in(normalize(text));std::set<std::string> out;std::string word;
    while(in>>word)out.insert(word);return out;
}
inline double similarity(const std::string& a,const std::string& b) {
    if(a.empty() || b.empty())return 0;
    std::vector<size_t> row(b.size()+1);for(size_t j=0;j<row.size();++j)row[j]=j;
    for(size_t i=1;i<=a.size();++i){size_t diagonal=row[0];row[0]=i;for(size_t j=1;j<=b.size();++j){auto old=row[j];row[j]=std::min({row[j]+1,row[j-1]+1,diagonal+(a[i-1]!=b[j-1])});diagonal=old;}}
    return 1.0-static_cast<double>(row.back())/std::max(a.size(),b.size());
}
inline json rank(const std::string& query,const std::string& name) {
    const auto q=normalize(query),n=normalize(name);const auto qt=tokens(query),nt=tokens(name);
    double coverage=0;for(const auto& token:qt)if(nt.contains(token))coverage+=1;
    if(!qt.empty())coverage/=qt.size();
    json reasons=json::array();double score=0;
    if(q==n && !q.empty()){score=1;reasons.push_back("normalized_exact_match");}
    if(!q.empty() && n.find(q)!=std::string::npos){score=std::max(score,0.94);reasons.push_back("substring_match");}
    auto compactQ=q,compactN=n;compactQ.erase(std::remove(compactQ.begin(),compactQ.end(),' '),compactQ.end());compactN.erase(std::remove(compactN.begin(),compactN.end(),' '),compactN.end());
    if(compactQ.size()>=3 && compactN.find(compactQ)!=std::string::npos){score=std::max(score,0.88);reasons.push_back("separator_insensitive_match");}
    if(coverage>0){score=std::max(score,0.92*coverage);reasons.push_back("token_match");if(coverage==1)reasons.push_back("token_order_independent_match");}
    const double fuzzy=similarity(q,n);if(fuzzy>=0.65){score=std::max(score,0.80*fuzzy);reasons.push_back("fuzzy_edit_match");}
    return {{"match_score",score},{"match_reasons",reasons},{"exact_name_match",normalize(query)==normalize(name) && query==name}};
}
inline bool clear(const json& results) {
    return !results.empty() && results[0].value("match_score",0.0)>=clearScore &&
        (results.size()==1 || results[0].value("match_score",0.0)-results[1].value("match_score",0.0)>=clearMargin);
}
}
