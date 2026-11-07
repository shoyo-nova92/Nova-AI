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

    if (!getline(cin, event.rawText)) event.rawText = "quit";
    if (event.rawText == ":paste") {
        cout << "Paste your request. Finish with :end on a line by itself.\n";
        event.rawText.clear();
        string line;
        while (getline(cin,line) && line != ":end") {
            if (!event.rawText.empty()) event.rawText += '\n';
            event.rawText += line;
        }
    }

    return event;
}
