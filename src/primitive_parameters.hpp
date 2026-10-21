#pragma once
#include <string>
using namespace std;

struct PrimitiveParameters {
    // Existing compatibility fields
    string name, location, application, target, content;
    // Filesystem-specific
    string source, destination, newName, recursive, overwrite;
    string query, maxResults, entryType;
    // Keyboard-specific
    string key, modifiers, repeatCount;
    // Mouse-specific
    string x, y, button, clickCount;
    // Browser automation
    string url, readyCondition, selector, timeoutMs;
    string role, text, elementId, submit;
};
