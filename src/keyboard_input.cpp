#include "keyboard_input.hpp"

#include <iostream>
#include <string>

using namespace std;

InputEvent KeyboardInput::waitForInput()
{
    InputEvent event;

    event.source = InputSource::KEYBOARD;

    cout << endl;
    cout << "[NOVA INPUT]" << endl;
    cout << "> ";

    getline(cin, event.rawText);

    return event;
}