#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <vector>
#include <cstdlib>
#include <stdexcept>
// Read-only Notion observations and explicit outbound voice dispatch. No task
// title, status name, weekday, date phrase, contact alias or page layout rules.
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <iomanip>
#include <ctime>
#include "runtime_control.hpp"
#include "utf8_text.hpp"
namespace ConnectedServices {
using nlohmann::json;
inline std::string env(const char* name,std::string fallback={}){const auto* value=std::getenv(name);return value&&*value?value:fallback;}
inline json unavailable(const std::string& category,const std::string& message){return {{"success",false},{"category",category},{"message",message}};}
inline std::string notionId(std::string value){
    const auto query=value.find('?');if(query!=std::string::npos)value.erase(query);
    std::smatch match;const std::regex pattern("([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}|[0-9a-fA-F]{32})$");
    if(!std::regex_search(value,match,pattern))throw std::runtime_error("Use an observed Notion UUID or page URL, not an invented resource name.");
    std::string id=match[1];id.erase(std::remove(id.begin(),id.end(),'-'),id.end());return id;
}
inline size_t receive(void* data,size_t size,size_t count,void* output){auto& text=*static_cast<std::string*>(output);const auto bytes=size*count;if(text.size()+bytes>8*1024*1024)return 0;text.append(static_cast<char*>(data),bytes);return bytes;}
inline std::string escape(CURL* curl,const std::string& text){char* value=curl_easy_escape(curl,text.data(),static_cast<int>(text.size()));if(!value)throw std::runtime_error("Cannot encode service request");std::string result(value);curl_free(value);return result;}
inline json http(const std::string& url,const std::string& method,const std::string& body,const std::vector<std::string>& headers,const std::string& basic={}){
    auto* curl=curl_easy_init();if(!curl)return unavailable("transport","HTTP initialization unavailable");
    curl_slist* list=nullptr;for(const auto& item:headers)list=curl_slist_append(list,item.c_str());std::string response;
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str());curl_easy_setopt(curl,CURLOPT_HTTPHEADER,list);curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,receive);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,8L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,30L);
    // Never follow a redirect carrying credentials to another host.
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,+[](void*,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int{return RuntimeControl::isPauseRequested()?1:0;});
    if(!basic.empty()){curl_easy_setopt(curl,CURLOPT_HTTPAUTH,CURLAUTH_BASIC);curl_easy_setopt(curl,CURLOPT_USERPWD,basic.c_str());}
    if(method=="POST"){curl_easy_setopt(curl,CURLOPT_POST,1L);curl_easy_setopt(curl,CURLOPT_POSTFIELDS,body.c_str());curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,static_cast<long>(body.size()));}
    const auto code=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_slist_free_all(list);curl_easy_cleanup(curl);
    if(code!=CURLE_OK)return unavailable(code==CURLE_ABORTED_BY_CALLBACK?"cancelled":"transport","Service request did not return a confirmed result; do not blindly repeat a write.");
    auto parsed=json::parse(response,nullptr,false);
    if(status<200 || status>=300){auto failure=json{{"success",false},{"category",status==429?"rate_limit":status==401||status==403?"authentication":status==404?"not_found":"provider"},{"http_status",status},{"message","Service returned HTTP "+std::to_string(status)+"; check connection permissions or quota."}};if(parsed.is_object()){failure["provider_code"]=parsed.contains("code")?parsed["code"]:json();failure["provider_message"]=parsed.value("message",std::string{});}return failure;}
    if(parsed.is_discarded())return unavailable("invalid_response","Expected a JSON service response");
    return {{"success",true},{"http_status",status},{"data",parsed}};
}
inline std::string endpoint(const char* overrideName,const std::string& official){
    auto value=env(overrideName,official);
    // Overrides exist solely for repeatable local HTTP tests; never exfiltrate a
    // configured provider token to an arbitrary planner-selected host.
    if(value!=official && !std::regex_match(value,std::regex("http://127[.]0[.]0[.]1:[0-9]{1,5}(/[A-Za-z0-9_./-]*)?")))throw std::runtime_error("Service endpoint override must be a loopback test server without URL userinfo");
    while(value.ends_with('/'))value.pop_back();return value;
}
inline json sources(){
    const auto path=Utf8::toPath(env("NOVA_SOURCES_FILE","config/personal_sources.json"));std::ifstream input(path);
    if(!input)return unavailable("configuration","Configure personal sources in config/personal_sources.json or NOVA_SOURCES_FILE.");
    json value;input>>value;
    return {{"success",true},{"sources",value},{"notion_configured",!env("NOTION_API_KEY").empty()},{"phone_configured",!env("TWILIO_ACCOUNT_SID").empty() && !env("TWILIO_AUTH_TOKEN").empty() && !env("NOVA_PHONE_TO").empty() && !env("TWILIO_FROM_NUMBER").empty()},{"calls_enabled",env("NOVA_CALLS_ENABLED")=="1"}};
}
inline json clock(){
    auto zone=env("NOVA_TIMEZONE");
    auto root=std::filesystem::current_path();
    while(!std::filesystem::exists(root/"python/personal_clock.py") && root!=root.parent_path())root=root.parent_path();
    if(!std::filesystem::exists(root/"python/personal_clock.py"))return unavailable("configuration","Cannot locate Nova's personal_clock.py helper.");
    if(zone.empty()){
        const auto configured=Utf8::toPath(env("NOVA_SOURCES_FILE",Utf8::path(root/"config/personal_sources.json")));
        std::ifstream file(configured);if(file){auto config=json::parse(file,nullptr,false);if(config.is_object())zone=config.value("timezone",std::string{});}
    }
    if(!std::regex_match(zone,std::regex("[A-Za-z][A-Za-z0-9_+/-]{0,100}")))return unavailable("configuration","Set an IANA timezone in config/personal_sources.json or NOVA_TIMEZONE; no host-local/UTC guess is made.");
    const auto executable=root/".venv-win/Scripts/python.exe",script=root/"python/personal_clock.py";
    const auto command="\"\""+Utf8::path(executable)+"\" -u \""+Utf8::path(script)+"\" --timezone "+zone+"\"";
    FILE* pipe=_popen(command.c_str(),"r");if(!pipe)return unavailable("configuration","Cannot start timezone clock helper");
    std::string output;char buffer[2048];while(fgets(buffer,sizeof(buffer),pipe))if(output.size()<65536)output+=buffer;const auto exit=_pclose(pipe);
    auto result=json::parse(output,nullptr,false);if(exit || !result.is_object())return unavailable("configuration","Timezone clock helper unavailable. Install tzdata in Nova's Python environment.");return result;
}
inline json notion(const std::string& operation,const std::string& target,const std::string& kind,const std::string& cursor,const std::string& content){
    const auto token=env("NOTION_API_KEY");if(token.empty())return unavailable("configuration","NOTION_API_KEY is missing. Connect/share the requested pages with a Notion integration.");
    const auto id=notionId(target),base=endpoint("NOVA_NOTION_ENDPOINT","https://api.notion.com/v1");
    const std::vector<std::string> headers={"Authorization: Bearer "+token,"Notion-Version: 2025-09-03","Content-Type: application/json"};
    const auto readResource=[&](const std::string& preferred){
        std::vector<std::string> kinds;if(preferred!="auto" && !preferred.empty())kinds.push_back(preferred);
        for(const auto* candidate:{"page","database","data_source","block"})if(std::find(kinds.begin(),kinds.end(),candidate)==kinds.end())kinds.push_back(candidate);
        json observed;
        for(const auto& resource:kinds){
            const auto route=resource=="page"?"pages":resource=="database"?"databases":resource=="data_source"?"data_sources":"blocks";
            observed=http(base+"/"+route+"/"+id,"GET","",headers);
            observed["source_id"]=id;
            if(observed.value("success",false)){observed["resolved_kind"]=resource;return observed;}
            const auto status=observed.value("http_status",0);if(status!=400 && status!=404)return observed;
        }return observed;
    };
    std::string url,method="GET",body;
    if(operation=="notion.read"){
        if(!kind.empty() && kind!="auto" && kind!="page" && kind!="database" && kind!="data_source" && kind!="block")throw std::runtime_error("Read kind must be auto, page, database, data_source or block");
        return readResource(kind);
    }else if(operation=="notion.children"){
        url=base+"/blocks/"+id+"/children?page_size=100";
        if(!cursor.empty()){auto* encoder=curl_easy_init();url+="&start_cursor="+escape(encoder,cursor);curl_easy_cleanup(encoder);}
    }else if(operation=="notion.query"){
        url=base+"/data_sources/"+id+"/query";method="POST";auto query=content.empty()?json::object():json::parse(content);if(!query.is_object())throw std::runtime_error("Query content must be a JSON object");
        for(auto it=query.begin();it!=query.end();++it)if(it.key()!="filter" && it.key()!="sorts")throw std::runtime_error("Only observed-schema filter and sorts are accepted in query content");
        query["page_size"]=100;if(!cursor.empty())query["start_cursor"]=cursor;body=query.dump();
    }else throw std::runtime_error("Unknown Notion primitive");
    auto result=http(url,method,body,headers);result["source_id"]=id;
    if(operation=="notion.query" && result.value("http_status",0)==404){
        auto database=http(base+"/databases/"+id,"GET","",headers);
        if(database.value("success",false) && database["data"].contains("data_sources")){
            const auto& choices=database["data"]["data_sources"];
            if(choices.is_array() && choices.size()==1){
                const auto resolved=notionId(choices[0].at("id").get<std::string>());
                result=http(base+"/data_sources/"+resolved+"/query",method,body,headers);result["source_id"]=resolved;result["resolved_from_database"]=id;
            }else {result=unavailable("resource_selection_required","This database contains multiple data sources. Inspect their schemas and query each relevant source; do not choose an arbitrary first source.");result["database"]=database["data"];}
        }
    }
    if(result.value("success",false))result["interpretation_guidance"]="Preserve original dates, properties, status groups, checkbox values, table cells and has_children. Follow next_cursor while has_more. Traverse child pages/databases and inspect data source schemas. Deduplicate views by data source ID. Determine relevance, completed versus pending and recurrence semantically from observed content and user timezone; never assume title/property words encode completion.";
    return result;
}
inline std::string xml(const std::string& value){std::string out;for(char c:value){if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='>')out+="&gt;";else if(c=='\"')out+="&quot;";else if(c=='\'')out+="&apos;";else out+=c;}return out;}
inline json call(const std::string& content,const std::string& requestId,const std::string& confirmed){
    if(env("NOVA_CALLS_ENABLED")!="1")return unavailable("configuration","Calling is disabled. Configure Twilio and your own verified NOVA_PHONE_TO, then set NOVA_CALLS_ENABLED=1.");
    if(confirmed!="true" && confirmed!="1")return unavailable("confirmation_required","Call only when the user explicitly asks for a phone call; confirm unresolved intent first.");
    if(content.empty() || content.size()>12000)return unavailable("invalid_input","Provide the factual readout as nonempty text, at most 12000 UTF-8 bytes.");
    if(!std::regex_match(requestId,std::regex("[A-Za-z0-9_-]{8,100}")))return unavailable("invalid_input","Supply a stable unique request_id (8..100 letters/digits/underscore/hyphen), reused for retries of this same call intent.");
    const auto sid=env("TWILIO_ACCOUNT_SID"),token=env("TWILIO_AUTH_TOKEN"),from=env("TWILIO_FROM_NUMBER"),to=env("NOVA_PHONE_TO");
    if(!std::regex_match(sid,std::regex("AC[0-9a-fA-F]{32}")) || token.empty() || !std::regex_match(from,std::regex("\\+[1-9][0-9]{6,14}")) || !std::regex_match(to,std::regex("\\+[1-9][0-9]{6,14}")))return unavailable("configuration","Configure Twilio Account SID, auth token, voice-capable from number and your own verified destination in E.164 format.");
    const auto root=Utf8::toPath(env("NOVA_CALL_LEDGER","logs/calls"));std::filesystem::create_directories(root);const auto receipt=root/(requestId+".json");
    // Reserve before any write. A crash/timeout is ambiguous, never permission
    // to create a duplicate real phone call. Concurrent processes cannot reserve
    // the same request twice (CREATE_NEW).
    HANDLE reservation=CreateFileW(receipt.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(reservation==INVALID_HANDLE_VALUE){
        std::ifstream file(receipt);json previous=json::parse(file,nullptr,false);
        if(previous.is_object() && previous.value("content",std::string{})==content && previous.value("to",std::string{})==to && previous.value("success",false)){previous.erase("content");previous.erase("to");previous["reused_receipt"]=true;return previous;}
        return unavailable("ambiguous_dispatch","This call request was already reserved. Inspect its receipt/provider before creating another call; request IDs must not be changed just to retry.");
    }
    const auto pending=json{{"success",false},{"state","dispatch_pending"},{"content",content},{"to",to}}.dump();DWORD written=0;const bool saved=WriteFile(reservation,pending.data(),static_cast<DWORD>(pending.size()),&written,nullptr) && written==pending.size();CloseHandle(reservation);
    if(!saved)return unavailable("storage","Cannot persist the call reservation; no call dispatched.");
    auto* encoder=curl_easy_init();if(!encoder)return unavailable("transport","Cannot encode call request");
    const auto twiml="<Response><Say voice=\""+xml(env("NOVA_CALL_VOICE","Polly.Joanna"))+"\">"+xml(content)+"</Say></Response>";
    const auto body="To="+escape(encoder,to)+"&From="+escape(encoder,from)+"&Twiml="+escape(encoder,twiml);curl_easy_cleanup(encoder);
    auto result=http(endpoint("NOVA_TWILIO_ENDPOINT","https://api.twilio.com/2010-04-01")+"/Accounts/"+sid+"/Calls.json","POST",body,{"Content-Type: application/x-www-form-urlencoded"},sid+":"+token);
    if(result.value("success",false)){
        const auto data=result.at("data");result={{"success",true},{"call_sid",data.value("sid",std::string{})},{"status",data.value("status",std::string{})},{"assurance","provider accepted the call; not proof that you answered or heard it"}};
        if(result["call_sid"].get<std::string>().empty())result=unavailable("ambiguous_dispatch","Provider response omitted a call SID; inspect provider history before retrying.");
    }
    auto stored=result;stored["content"]=content;stored["to"]=to;std::ofstream output(receipt,std::ios::trunc|std::ios::binary);output<<stored.dump();
    return result;
}
}
