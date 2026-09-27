#include "primitive_registry.hpp"

#include <unordered_map>

namespace
{
    const unordered_map<string, PrimitiveOperationType>
        PRIMITIVE_REGISTRY =
    {
        {
            "filesystem.create",
            PrimitiveOperationType::FILESYSTEM_CREATE
        },

        {
            "filesystem.create_file",
            PrimitiveOperationType::FILESYSTEM_CREATE_FILE
        },

        {
            "filesystem.delete",
            PrimitiveOperationType::FILESYSTEM_DELETE
        },

        {
            "filesystem.move",
            PrimitiveOperationType::FILESYSTEM_MOVE
        },

        {
            "filesystem.copy",
            PrimitiveOperationType::FILESYSTEM_COPY
        },

        {
            "filesystem.rename",
            PrimitiveOperationType::FILESYSTEM_RENAME
        },

        {
            "application.launch",
            PrimitiveOperationType::APPLICATION_LAUNCH
        },

        {
            "application.close",
            PrimitiveOperationType::APPLICATION_CLOSE
        },

        {
            "application.focus",
            PrimitiveOperationType::APPLICATION_FOCUS
        },

        {
            "text.write",
            PrimitiveOperationType::TEXT_WRITE
        },

        {
            "keyboard.press",
            PrimitiveOperationType::KEYBOARD_PRESS
        },

        {
            "keyboard.type",
            PrimitiveOperationType::KEYBOARD_TYPE
        },

        {
            "mouse.click",
            PrimitiveOperationType::MOUSE_CLICK
        },

        {
            "mouse.move",
            PrimitiveOperationType::MOUSE_MOVE
        }
    };
}

bool PrimitiveRegistry::isSupported(
    const string& primitiveName
)
{
    return PRIMITIVE_REGISTRY.contains(
        primitiveName
    );
}

bool PrimitiveRegistry::resolveType(
    const string& primitiveName,
    PrimitiveOperationType& type
)
{
    auto iterator =
        PRIMITIVE_REGISTRY.find(
            primitiveName
        );

    if (iterator == PRIMITIVE_REGISTRY.end())
    {
        return false;
    }

    type = iterator->second;

    return true;
}

vector<string>
PrimitiveRegistry::getSupportedNames()
{
    vector<string> names;

    for (const auto& entry :
         PRIMITIVE_REGISTRY)
    {
        names.push_back(entry.first);
    }

    return names;
}