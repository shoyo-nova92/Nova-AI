#pragma once
#include <string>
using namespace std;

struct PrimitiveParameters {
    // Existing compatibility fields
    string name, location, application, target, content;
    // Filesystem-specific
    string source, destination, newName, recursive, overwrite;
    // Keyboard-specific
    string key, modifiers, repeatCount;
    // Mouse-specific
    string x, y, button, clickCount;
};
