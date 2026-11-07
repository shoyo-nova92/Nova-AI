#pragma once
#include <windows.h>
#include <shlobj.h>
#ifdef OPTIONAL
#undef OPTIONAL
#endif
#include <map>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "utf8_text.hpp"

inline std::string processImageName(DWORD pid) {
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if (!process) return {};
    wchar_t path[32768]{}; DWORD length=32768;
    const bool ok=QueryFullProcessImageNameW(process,0,path,&length);
    CloseHandle(process);
    return ok ? Utf8::path(std::filesystem::path(path).filename()) : std::string{};
}
inline nlohmann::json environmentSnapshot() {
    using nlohmann::json;
    std::map<std::string,unsigned> counts;
    EnumWindows([](HWND window,LPARAM data)->BOOL {
        if (!IsWindowVisible(window) || GetWindow(window,GW_OWNER)) return TRUE;
        DWORD pid=0; GetWindowThreadProcessId(window,&pid);
        const auto image=processImageName(pid);
        auto& items=*reinterpret_cast<std::map<std::string,unsigned>*>(data);
        if (!image.empty() && (items.contains(image) || items.size()<40)) ++items[image];
        return TRUE;
    },reinterpret_cast<LPARAM>(&counts));
    DWORD foregroundPid=0; GetWindowThreadProcessId(GetForegroundWindow(),&foregroundPid);
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{},region[16]{};
    GetUserDefaultLocaleName(locale,LOCALE_NAME_MAX_LENGTH);
    GetGeoInfoW(GetUserGeoID(GEOCLASS_NATION),GEO_ISO2,region,16,0);
    json out={{"foreground_application",processImageName(foregroundPid)},
        {"locale",Utf8::path(std::filesystem::path(locale))},{"region",Utf8::path(std::filesystem::path(region))},
        {"visible_applications",json::array()},{"known_folders",json::object()},
        {"privacy","Window titles, document contents, credentials and unrelated tab URLs omitted."}};
    for (const auto& [name,count]:counts) out["visible_applications"].push_back({{"application",name},{"window_count",count}});
    for (const auto& [name,id]:std::initializer_list<std::pair<const char*,const KNOWNFOLDERID*>>{
        {"desktop",&FOLDERID_Desktop},{"documents",&FOLDERID_Documents},{"downloads",&FOLDERID_Downloads}}) {
        PWSTR path=nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(*id,0,nullptr,&path))) { out["known_folders"][name]=Utf8::path(std::filesystem::path(path)); CoTaskMemFree(path); }
    }
    return out;
}
