#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "part_a_bridge.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <iostream>
#include <atomic>

using namespace std;

PartAResult PartABridge::waitForText(WakeGreeting greeting)
{
    static std::atomic<unsigned> sequence{0};
    const auto identity="Local\\NovaVoice_"+std::to_string(GetCurrentProcessId())+"_"+std::to_string(++sequence);
    struct Events{
        HANDLE ready,cancel;
        ~Events(){if(ready)CloseHandle(ready);if(cancel)CloseHandle(cancel);}
    } events{CreateEventA(nullptr,TRUE,FALSE,(identity+"_ready").c_str()),CreateEventA(nullptr,TRUE,FALSE,(identity+"_cancel").c_str())};
    if(!events.ready || !events.cancel)return {false,false,"","Could not initialize voice handoff events."};
    PartAResult result;

    result.success = false;
    result.keyboardRequest = false;
    result.rawText = "";
    result.message = "";

    cout << unitbuf;
    _putenv("PYTHONUNBUFFERED=1");

    cout
        << "[BRIDGE] Starting Python Part A..."
        << endl;

    string command =
        "cmd.exe /C "
        "\""
        ".venv-win\\Scripts\\python.exe "
        "-u "
        "python\\part_a_runtime.py --greeting-event "+identity+"_ready --cancel-event "+identity+"_cancel"
        "\"";

    FILE* pipe = _popen(
        command.c_str(),
        "r"
    );

    cout
        << "[BRIDGE] _popen returned."
        << endl;

    if (pipe == nullptr)
    {
        result.message =
            "Failed to start Part A runtime.";

        cout
            << "[BRIDGE] ERROR: "
            << result.message
            << endl;

        return result;
    }

    cout
        << "[BRIDGE] Python process started."
        << endl;

    cout
        << "[BRIDGE] Waiting for Python output..."
        << endl;

    char buffer[4096];

    while (
        fgets(
            buffer,
            sizeof(buffer),
            pipe
        ) != nullptr
    )
    {
        string line(buffer);

        cout
            << "[PYTHON] "
            << line
            << flush;

        // ----------------------------------------------------
        // KEYBOARD OVERRIDE REQUEST
        // ----------------------------------------------------

        const string keyboardPrefix =
            "[KEYBOARD_REQUEST]";

        if(line.find("[WAKE_READY]")!=string::npos){
            const auto cancelled=[&]{return WaitForSingleObject(events.cancel,0)==WAIT_OBJECT_0;};
            try{if(greeting && !cancelled())greeting(cancelled);}
            catch(const std::exception& error){std::cerr<<"[VOICE] Greeting unavailable: "<<error.what()<<'\n';}
            SetEvent(events.ready);
            continue;
        }

        if(line.find("[QUIT_REQUEST]")!=string::npos){
            result.success=true;result.rawText="quit";result.message="User quit while waiting for voice input.";break;
        }

        if (
            line.find(
                keyboardPrefix
            ) != string::npos
        )
        {
            result.success = true;
            result.keyboardRequest = true;
            result.rawText = "";
            result.message =
                "Part A requested keyboard input.";

            cout
                << "[BRIDGE] KEYBOARD_REQUEST received."
                << endl;

            break;
        }

        // ----------------------------------------------------
        // SPEECH TEXT
        // ----------------------------------------------------

        const string textPrefix =
            "[TEXT_READY] ";

        size_t position =
            line.find(textPrefix);

        if (
            position != string::npos
        )
        {
            string text =
                line.substr(
                    position + textPrefix.length()
                );

            while (
                !text.empty()
                &&
                (
                    text.back() == '\n'
                    ||
                    text.back() == '\r'
                )
            )
            {
                text.pop_back();
            }

            result.success = true;
            result.keyboardRequest = false;
            result.rawText = text;
            result.message =
                "Part A produced text.";

            cout
                << "[BRIDGE] TEXT_READY received."
                << endl;

            // Continue reading until Python exits cleanly.
            continue;
        }
    }

    cout
        << "[BRIDGE] Python process ended."
        << endl;

    _pclose(pipe);

    if (result.success)
    {
        return result;
    }

    result.success = false;
    result.keyboardRequest = false;
    result.rawText = "";
    result.message =
        "Part A ended without producing text.";

    return result;
}
