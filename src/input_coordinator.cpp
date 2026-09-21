#include "input_coordinator.hpp"

#include "keyboard_input.hpp"
#include "part_a_bridge.hpp"

#include <iostream>

using namespace std;

InputEvent InputCoordinator::waitForInput()
{
    cout << endl;
    cout << "============================================================"
         << endl;
    cout << "NOVA"
         << endl;
    cout << "============================================================"
         << endl;

    cout << "[INPUT] Starting unified input pipeline..."
         << endl;

    // --------------------------------------------------------
    // PART A — WAKE WORD / LISTENING
    // --------------------------------------------------------

    PartABridge partABridge;

    PartAResult result =
        partABridge.waitForText();

    // --------------------------------------------------------
    // KEYBOARD OVERRIDE
    // --------------------------------------------------------

    if (
        result.success &&
        result.keyboardRequest
    )
    {
        cout << endl;
        cout << "[INPUT] Switching to keyboard input."
             << endl;

        KeyboardInput keyboardInput;

        InputEvent keyboardEvent =
            keyboardInput.waitForInput();

        return keyboardEvent;
    }

    // --------------------------------------------------------
    // SPEECH INPUT
    // --------------------------------------------------------

    if (
        result.success &&
        !result.keyboardRequest
    )
    {
        InputEvent event;

        event.source =
            InputSource::SPEECH;

        event.rawText =
            result.rawText;

        cout << endl;
        cout << "[INPUT] Speech input received."
             << endl;

        return event;
    }

    // --------------------------------------------------------
    // FAILURE
    // --------------------------------------------------------

    cout << endl;
    cout << "[INPUT] Input pipeline failed."
         << endl;

    InputEvent event;

    event.source =
        InputSource::SPEECH;

    event.rawText =
        "";

    return event;
}