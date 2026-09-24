#include "part_a_bridge.hpp"

#include <cstdio>
#include <string>
#include <iostream>

using namespace std;

PartAResult PartABridge::waitForText()
{
    PartAResult result;

    result.success = false;
    result.keyboardRequest = false;
    result.rawText = "";
    result.message = "";

    cout
        << "[BRIDGE] Starting Python Part A..."
        << endl;

    string command =
        "cmd.exe /C "
        "\""
        ".venv-win\\Scripts\\python.exe "
        "-u "
        "python\\part_a_runtime.py"
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
            << line;

        // ----------------------------------------------------
        // KEYBOARD OVERRIDE REQUEST
        // ----------------------------------------------------

        const string keyboardPrefix =
            "[KEYBOARD_REQUEST]";

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