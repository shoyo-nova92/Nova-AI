#pragma once
#include <string>
#include "primitive_parameters.hpp"
using namespace std;

enum class PrimitiveOperationType {
    FILESYSTEM_CREATE, FILESYSTEM_CREATE_FILE, FILESYSTEM_DELETE,
    FILESYSTEM_MOVE, FILESYSTEM_COPY, FILESYSTEM_RENAME,
    FILESYSTEM_LIST_DIRECTORY, FILESYSTEM_SEARCH, FILESYSTEM_INSPECT,
    APPLICATION_LAUNCH, APPLICATION_CLOSE, APPLICATION_FOCUS,
    TEXT_WRITE, KEYBOARD_PRESS, KEYBOARD_TYPE, MOUSE_CLICK, MOUSE_MOVE,
    BROWSER_NAVIGATE, BROWSER_WAIT_FOR, BROWSER_OBSERVE, BROWSER_CLICK, BROWSER_TYPE,
    ENVIRONMENT_OBSERVE, BROWSER_DOWNLOAD, BROWSER_TABS, BROWSER_SELECT_TAB, APPLICATION_OBSERVE
};

struct PrimitiveOperation {
    PrimitiveOperationType type{};
    string name;
    PrimitiveParameters parameters;
    bool resolved = false;
};

inline string primitiveOperationTypeToString(PrimitiveOperationType type) {
    switch (type) {
        case PrimitiveOperationType::APPLICATION_OBSERVE: return "application.observe";
        case PrimitiveOperationType::FILESYSTEM_CREATE: return "filesystem.create";
        case PrimitiveOperationType::FILESYSTEM_CREATE_FILE: return "filesystem.create_file";
        case PrimitiveOperationType::FILESYSTEM_DELETE: return "filesystem.delete";
        case PrimitiveOperationType::FILESYSTEM_MOVE: return "filesystem.move";
        case PrimitiveOperationType::FILESYSTEM_COPY: return "filesystem.copy";
        case PrimitiveOperationType::FILESYSTEM_RENAME: return "filesystem.rename";
        case PrimitiveOperationType::FILESYSTEM_LIST_DIRECTORY: return "filesystem.list_directory";
        case PrimitiveOperationType::FILESYSTEM_SEARCH: return "filesystem.search";
        case PrimitiveOperationType::FILESYSTEM_INSPECT: return "filesystem.inspect";
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
        case PrimitiveOperationType::BROWSER_OBSERVE: return "browser.observe";
        case PrimitiveOperationType::BROWSER_CLICK: return "browser.click";
        case PrimitiveOperationType::BROWSER_TYPE: return "browser.type";
        case PrimitiveOperationType::ENVIRONMENT_OBSERVE: return "environment.observe";
        case PrimitiveOperationType::BROWSER_DOWNLOAD: return "browser.download";
        case PrimitiveOperationType::BROWSER_TABS: return "browser.tabs";
        case PrimitiveOperationType::BROWSER_SELECT_TAB: return "browser.select_tab";
    }
    return "unknown";
}
