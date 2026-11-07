#pragma once
#ifdef _WIN32
#include <windows.h>
#include <future>
#include <iostream>
#include <thread>
#include <functional>
#include "runtime_control.hpp"

// Message-only global hotkey: no foreground window changes, input stealing,
// keyboard injection or application state mutation on this listener thread.
class GlobalPauseHotkey {
    std::thread thread_;
    DWORD threadId_=0;
public:
    explicit GlobalPauseHotkey(std::function<void()> onPause={}){
        std::promise<DWORD> ready;auto result=ready.get_future();
        thread_=std::thread([signal=std::move(ready),onPause=std::move(onPause)]()mutable{
            MSG message{};PeekMessageW(&message,nullptr,0,0,PM_NOREMOVE);
            if(!RegisterHotKey(nullptr,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'P')){signal.set_value(0);return;}
            signal.set_value(GetCurrentThreadId());
            while(GetMessageW(&message,nullptr,0,0)>0)if(message.message==WM_HOTKEY){
                if(onPause)onPause();
                if(RuntimeControl::active()){
                    RuntimeControl::requestPause();
                    std::cout<<"\n[PAUSE_REQUESTED] Cancelling model thinking. An action already dispatched will settle before guidance input.\n";
                }
            }
            UnregisterHotKey(nullptr,1);
        });
        threadId_=result.get();
        if(threadId_)std::cout<<"[CONTROL] Ctrl+Alt+P pauses Nova from the browser or terminal.\n";
        else std::cout<<"[CONTROL] Ctrl+Alt+P is unavailable (shortcut registration failed).\n";
    }
    ~GlobalPauseHotkey(){if(threadId_)PostThreadMessageW(threadId_,WM_QUIT,0,0);if(thread_.joinable())thread_.join();}
    bool available()const{return threadId_!=0;}
    GlobalPauseHotkey(const GlobalPauseHotkey&)=delete;
    GlobalPauseHotkey& operator=(const GlobalPauseHotkey&)=delete;
};
#else
class GlobalPauseHotkey {};
#endif
