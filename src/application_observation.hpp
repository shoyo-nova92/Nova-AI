#pragma once
#include <windows.h>
#include <psapi.h>
#include <uiautomationclient.h>
#include <nlohmann/json.hpp>
#include "utf8_text.hpp"
#include <filesystem>
#include <set>
#include <optional>
#include <atomic>

// Read-only, explicitly scoped Windows observation. No focusing, launching or closing.
namespace ApplicationObservation {
using nlohmann::json;
inline uint64_t fileTicks(const FILETIME& time) {
    return (static_cast<uint64_t>(time.dwHighDateTime)<<32)|time.dwLowDateTime;
}
inline uint64_t now() {FILETIME time{};GetSystemTimePreciseAsFileTime(&time);return fileTicks(time);}
inline json processIdentity(HANDLE process) {
    json facts=json::object();if(!process)return facts;
    const auto pid=GetProcessId(process);if(pid)facts["process_id"]=pid;
    FILETIME created{},exited{},kernel{},user{};
    if(GetProcessTimes(process,&created,&exited,&kernel,&user))facts["process_created_100ns"]=fileTicks(created);
    wchar_t image[32768]{};DWORD length=32768;
    if(QueryFullProcessImageNameW(process,0,image,&length)) {
        const auto path=std::filesystem::path(std::wstring(image,length));
        facts["process_image_path"]=Utf8::path(path);facts["process_name"]=Utf8::path(path.filename());
    }
    return facts;
}
inline json processIdentity(DWORD pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    auto facts=processIdentity(process);if(process)CloseHandle(process);return facts;
}
inline void recordLaunch(json& facts,HANDLE process,uint64_t started,const std::filesystem::path& requested) {
    static std::atomic<uint64_t> sequence{0};
    facts.update(processIdentity(process));
    facts["launch_started_100ns"]=started;facts["launch_completed_100ns"]=now();
    facts["launch_id"]=std::to_string(started)+":"+std::to_string(++sequence);
    const auto created=facts.value("process_created_100ns",uint64_t{});
    facts["new_process_instance"]=created && created>=started && created<=facts["launch_completed_100ns"].get<uint64_t>();
    if(!requested.empty()) {
        std::error_code error;const auto canonical=std::filesystem::canonical(requested,error);
        if(!error)facts["canonical_requested_target"]=Utf8::path(canonical);
        // resolved target is canonicalized independently of the requested spelling.
        if(facts.contains("target_path")) {
            const auto resolved=std::filesystem::canonical(Utf8::toPath(facts["target_path"].get<std::string>()),error);
            if(!error)facts["target_path"]=Utf8::path(resolved);
        }
    }
}
inline std::string processName(DWORD pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);if(!process)return {};
    wchar_t name[32768]{};DWORD length=32768;
    const bool okay=QueryFullProcessImageNameW(process,0,name,&length)!=FALSE;CloseHandle(process);
    return okay?Utf8::path(std::filesystem::path(std::wstring(name,length)).filename()):std::string{};
}
inline bool sameTarget(const std::wstring& observed,const std::filesystem::path& target) {
    if(observed.empty() || target.empty())return false;
    const std::filesystem::path value(observed);
    if(!value.is_absolute())return false;
    // Only actual full paths qualify; a basename or launch argument is insufficient.
    std::error_code error;const auto a=std::filesystem::weakly_canonical(value,error);if(error)return false;
    const auto b=std::filesystem::weakly_canonical(target,error);return !error && _wcsicmp(a.c_str(),b.c_str())==0;
}
inline json documentAssociation(IUIAutomation* automation,HWND hwnd,const std::filesystem::path& target) {
    json result={{"association","unavailable"},{"reason","Exact full document path is not exposed by the observed window/document metadata."}};if(target.empty())return result;
    wchar_t title[2048]{};GetWindowTextW(hwnd,title,2048);
    if(sameTarget(title,target))return {{"association","exact_document_path"},{"source","window_title"},{"observed_path",Utf8::path(std::filesystem::path(title))}};
    result["document_controls"]=json::array();
    if(!automation)return result;
    IUIAutomationElement* windowRoot=nullptr;
    if(SUCCEEDED(automation->ElementFromHandle(hwnd,&windowRoot)) && windowRoot) {
        for(const auto property:{UIA_NamePropertyId,UIA_HelpTextPropertyId}) {
            VARIANT value;VariantInit(&value);
            if(SUCCEEDED(windowRoot->GetCurrentPropertyValue(property,&value)) && value.vt==VT_BSTR && value.bstrVal)
                result[property==UIA_NamePropertyId?"uia_name":"uia_help_text"]=Utf8::truncate(Utf8::path(std::filesystem::path(std::wstring(value.bstrVal,SysStringLen(value.bstrVal)))),512,"uia.metadata");
            VariantClear(&value);
        }
        windowRoot->Release();
    }
    IUIAutomationElement* root=nullptr;IUIAutomationCondition *document=nullptr,*tab=nullptr,*condition=nullptr;IUIAutomationElementArray* elements=nullptr;
    VARIANT documentType;VariantInit(&documentType);documentType.vt=VT_I4;documentType.lVal=UIA_DocumentControlTypeId;
    VARIANT tabType;VariantInit(&tabType);tabType.vt=VT_I4;tabType.lVal=UIA_TabItemControlTypeId;
    HRESULT status=automation->ElementFromHandle(hwnd,&root);
    if(SUCCEEDED(status))status=automation->CreatePropertyCondition(UIA_ControlTypePropertyId,documentType,&document);
    if(SUCCEEDED(status))status=automation->CreatePropertyCondition(UIA_ControlTypePropertyId,tabType,&tab);
    if(SUCCEEDED(status))status=automation->CreateOrCondition(document,tab,&condition);
    if(SUCCEEDED(status))status=root->FindAll(TreeScope_Descendants,condition,&elements);
    if(SUCCEEDED(status) && elements) {
        int count=0;elements->get_Length(&count);
        for(int i=0;i<count && i<64 && result["association"]!="exact_document_path";++i) {
            IUIAutomationElement* element=nullptr;if(FAILED(elements->GetElement(i,&element)) || !element)continue;
            json control=json::object();
            for(const auto property:{UIA_NamePropertyId,UIA_HelpTextPropertyId}) {
                VARIANT value;VariantInit(&value);
                if(SUCCEEDED(element->GetCurrentPropertyValue(property,&value)) && value.vt==VT_BSTR && value.bstrVal && sameTarget(std::wstring(value.bstrVal,SysStringLen(value.bstrVal)),target))
                    result={{"association","exact_document_path"},{"source",property==UIA_NamePropertyId?"document_or_tab_name":"document_or_tab_help_text"},{"observed_path",Utf8::path(std::filesystem::path(std::wstring(value.bstrVal,SysStringLen(value.bstrVal))))}};
                if(value.vt==VT_BSTR && value.bstrVal)control[property==UIA_NamePropertyId?"name":"help_text"]=Utf8::truncate(Utf8::path(std::filesystem::path(std::wstring(value.bstrVal,SysStringLen(value.bstrVal)))),512,"uia.document");
                VariantClear(&value);
            }
            if(!result.contains("document_controls"))result["document_controls"]=json::array();
            result["document_controls"].push_back(std::move(control));
            element->Release();
        }
    }
    if(elements)elements->Release();if(condition)condition->Release();if(document)document->Release();if(tab)tab->Release();if(root)root->Release();
    return result;
}
inline json observe(std::optional<DWORD> pid,const std::string& executable,const std::filesystem::path& target,std::optional<uintptr_t> window) {
    json facts={{"category","APPLICATION_OBSERVATION"},{"processes",json::array()},{"windows",json::array()},{"target_requested",Utf8::path(target)},
        {"observation_started_100ns",now()}};
    std::set<DWORD> matches;
    auto inspect=[&](DWORD id) {
        const auto name=processName(id);if(name.empty())return;
        if(!executable.empty() && _wcsicmp(Utf8::toPath(name).c_str(),Utf8::toPath(executable).c_str())!=0)return;
        matches.insert(id);facts["processes"].push_back(processIdentity(id));
    };
    if(pid)inspect(*pid);else {DWORD ids[4096]{},bytes=0;if(EnumProcesses(ids,sizeof(ids),&bytes))for(size_t i=0;i<bytes/sizeof(DWORD) && matches.size()<32;++i)inspect(ids[i]);}
    const HRESULT initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);IUIAutomation* automation=nullptr;
    if(!target.empty())CoCreateInstance(CLSID_CUIAutomation8,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation));
    if(automation) {
        IUIAutomation2* bounded=nullptr;
        if(SUCCEEDED(automation->QueryInterface(IID_PPV_ARGS(&bounded)))) {
            const bool configured=SUCCEEDED(bounded->put_ConnectionTimeout(500)) && SUCCEEDED(bounded->put_TransactionTimeout(500));bounded->Release();
            if(!configured){automation->Release();automation=nullptr;}
        }else {automation->Release();automation=nullptr;}
    }
    facts["document_accessibility_available"]=automation!=nullptr;
    struct Context {json* facts;std::set<DWORD>* pids;const std::filesystem::path* target;IUIAutomation* automation;std::optional<uintptr_t> window;} context{&facts,&matches,&target,automation,window};
    EnumWindows([](HWND hwnd,LPARAM raw)->BOOL {
        auto& c=*reinterpret_cast<Context*>(raw);DWORD id=0;GetWindowThreadProcessId(hwnd,&id);
        if(!c.pids->contains(id) || (c.window && reinterpret_cast<uintptr_t>(hwnd)!=*c.window))return TRUE;
        if((*c.facts)["windows"].size()>=16)return FALSE;
        wchar_t title[2048]{};GetWindowTextW(hwnd,title,2048);
        auto association=documentAssociation(c.automation,hwnd,*c.target);
        json row={{"process_id",id},{"process_name",processName(id)},{"window_handle",std::to_string(reinterpret_cast<uintptr_t>(hwnd))},{"window_visible",IsWindowVisible(hwnd)!=FALSE},{"window_title",Utf8::path(std::filesystem::path(title))},{"foreground",GetForegroundWindow()==hwnd},{"target_observed",association["association"]=="exact_document_path"},{"target_evidence",association}};
        row.update(processIdentity(id));
        for(const auto key:{"uia_name","uia_help_text","document_controls"})if(association.contains(key))row[key]=association[key];
        row["target_proof_level"]=row["target_observed"]==true?"exact":"unavailable";
        if(!c.target->empty()) {
            row["target_observation_supported"]=row["target_observed"];
            if(row["target_observed"]==false)row["target_observation_reason"]=association["reason"];
        }
        if(row["target_observed"]==true)row["target_path"]=Utf8::path(*c.target);
        (*c.facts)["windows"].push_back(std::move(row));return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    facts["process_exists"]=!facts["processes"].empty();facts["window_exists"]=!facts["windows"].empty();
    facts["window_visible"]=false;facts["target_observed"]=false;
    for(const auto& row:facts["windows"]) {
        if(row.value("window_visible",false))facts["window_visible"]=true;
        if(row.value("target_observed",false))facts["target_observed"]=true;
    }
    if(!target.empty()) {
        facts["target_observation_supported"]=facts["target_observed"];
        if(facts["target_observed"]==false)facts["target_observation_reason"]=facts["windows"].empty()
            ? "No window was observed in the requested scope; the process may still be starting or may have delegated activation."
            : "Exact full document path is unavailable from the currently observed window/document metadata.";
    }
    facts["target_proof_level"]=facts["target_observed"]==true?"exact":"unavailable";
    facts["observation_completed_100ns"]=now();
    if(automation)automation->Release();if(SUCCEEDED(initialized))CoUninitialize();return facts;
}
}
