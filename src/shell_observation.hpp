#pragma once
#include <windows.h>
#include <exdisp.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <servprov.h>
#include "application_observation.hpp"

// Read native Explorer views through the Shell namespace. PID/title and
// LocationURL guesses are deliberately not folder-identity evidence.
namespace ShellObservation {
inline nlohmann::json observe() {
    using nlohmann::json;
    json facts={{"category","SHELL_WINDOW_OBSERVATION"},{"windows",json::array()},
        {"observation_started_100ns",ApplicationObservation::now()},{"observer_available",false}};
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    IShellWindows* windows=nullptr;
    HRESULT status=CoCreateInstance(CLSID_ShellWindows,nullptr,CLSCTX_LOCAL_SERVER,IID_PPV_ARGS(&windows));
    if(SUCCEEDED(status) && windows) {
        facts["observer_available"]=true;long count=0;windows->get_Count(&count);
        for(long i=0;i<count && i<64;++i) {
            VARIANT index;VariantInit(&index);index.vt=VT_I4;index.lVal=i;
            IDispatch* dispatch=nullptr;IWebBrowserApp* app=nullptr;IServiceProvider* provider=nullptr;
            IShellBrowser* browser=nullptr;IShellView* view=nullptr;IFolderView* folderView=nullptr;IPersistFolder2* folder=nullptr;
            PIDLIST_ABSOLUTE pidl=nullptr;PWSTR raw=nullptr;SHANDLE_PTR handle=0;
            status=windows->Item(index,&dispatch);
            if(SUCCEEDED(status) && dispatch)status=dispatch->QueryInterface(IID_PPV_ARGS(&app));
            if(SUCCEEDED(status) && app)status=app->get_HWND(&handle);
            if(SUCCEEDED(status))status=dispatch->QueryInterface(IID_PPV_ARGS(&provider));
            if(SUCCEEDED(status))status=provider->QueryService(SID_STopLevelBrowser,IID_PPV_ARGS(&browser));
            if(SUCCEEDED(status))status=browser->QueryActiveShellView(&view);
            if(SUCCEEDED(status))status=view->QueryInterface(IID_PPV_ARGS(&folderView));
            if(SUCCEEDED(status))status=folderView->GetFolder(IID_PPV_ARGS(&folder));
            if(SUCCEEDED(status))status=folder->GetCurFolder(&pidl);
            if(SUCCEEDED(status))status=SHGetNameFromIDList(pidl,SIGDN_FILESYSPATH,&raw);
            if(SUCCEEDED(status) && raw && handle) {
                std::error_code error;const auto canonical=std::filesystem::canonical(std::filesystem::path(raw),error);
                const auto hwnd=reinterpret_cast<HWND>(static_cast<uintptr_t>(handle));
                if(!error && std::filesystem::is_directory(canonical,error) && !error)
                    facts["windows"].push_back({{"window_handle",std::to_string(static_cast<uintptr_t>(handle))},
                        {"canonical_location",Utf8::path(canonical)},{"visible",IsWindowVisible(hwnd)!=FALSE},{"source","native_shell_folder"}});
            }
            if(raw)CoTaskMemFree(raw);if(pidl)CoTaskMemFree(pidl);if(folder)folder->Release();if(folderView)folderView->Release();
            if(view)view->Release();if(browser)browser->Release();if(provider)provider->Release();if(app)app->Release();if(dispatch)dispatch->Release();
        }
        windows->Release();
    }
    facts["observation_completed_100ns"]=ApplicationObservation::now();
    if(SUCCEEDED(initialized))CoUninitialize();return facts;
}
}
