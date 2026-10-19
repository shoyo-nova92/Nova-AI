#pragma once
#include <string>
#include "primitive_parameters.hpp"
using namespace std;

enum class PrimitiveOperationType {
    FILESYSTEM_CREATE, FILESYSTEM_CREATE_FILE, FILESYSTEM_DELETE,
    FILESYSTEM_MOVE, FILESYSTEM_COPY, FILESYSTEM_RENAME,
    APPLICATION_LAUNCH, APPLICATION_CLOSE, APPLICATION_FOCUS,
    TEXT_WRITE, KEYBOARD_PRESS, KEYBOARD_TYPE, MOUSE_CLICK, MOUSE_MOVE,
    BROWSER_NAVIGATE, BROWSER_WAIT_FOR
};

struct PrimitiveOperation {
    PrimitiveOperationType type{};
    string name;
    PrimitiveParameters parameters;
    bool resolved = false;
};

inline string primitiveOperationTypeToString(PrimitiveOperationType type) {
    switch (type) {
        case PrimitiveOperationType::FILESYSTEM_CREATE: return "filesystem.create";
        case PrimitiveOperationType::FILESYSTEM_CREATE_FILE: return "filesystem.create_file";
        case PrimitiveOperationType::FILESYSTEM_DELETE: return "filesystem.delete";
        case PrimitiveOperationType::FILESYSTEM_MOVE: return "filesystem.move";
        case PrimitiveOperationType::FILESYSTEM_COPY: return "filesystem.copy";
        case PrimitiveOperationType::FILESYSTEM_RENAME: return "filesystem.rename";
        case PrimitiveOperationType::APPLICATION_LAUNCH: return "application.launch";
        case PrimitiveOperationType::APPLICATION_CLOSE: return "application.close";
        case PrimitiveOperationType::APPLICATION_FOCUS: return "application.focus";
        case PrimitiveOperationType::TEXT_WRITE: return "text.write";
        case PrimitiveOperationType::KEYBOARD_PRESS: return "keyboard.press";
        case PrimitiveOperationType::KEYBOARD_TYPE: return "keyboard.type";
        case PrimitiveOperationType::MOUSE_CLICK: return "mouse.click";
        case PrimitiveOperationType::MOUSE_MOVE: return "mouse.move";
        case PrimitiveOperationType::BROWSER_NAVIGATE: return "browser.navigate";
        case PrimitiveOperationType::BROWSER_WAIT_FOR: return "browser.wait_for";
    }
    return "unknown";
}
