#include "primitive_registry.hpp"

#include <unordered_map>
#include <optional>

namespace
{
    const unordered_map<string, PrimitiveDefinition>
        PRIMITIVE_REGISTRY =
    {
        {
            "filesystem.create",
            {
                PrimitiveOperationType::FILESYSTEM_CREATE,
                PrimitiveParameterSchema{
                    ParameterRequirement::REQUIRED,
                    ParameterRequirement::REQUIRED,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::NOT_APPLICABLE
                }
            }
        },

        {
            "filesystem.create_file",
            {
                PrimitiveOperationType::FILESYSTEM_CREATE_FILE,
                PrimitiveParameterSchema{
                    ParameterRequirement::REQUIRED,
                    ParameterRequirement::REQUIRED,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::OPTIONAL
                }
            }
        },

        {
            "filesystem.delete",
            {PrimitiveOperationType::FILESYSTEM_DELETE, nullopt}
        },

        {
            "filesystem.move",
            {PrimitiveOperationType::FILESYSTEM_MOVE, nullopt}
        },

        {
            "filesystem.copy",
            {PrimitiveOperationType::FILESYSTEM_COPY, nullopt}
        },

        {
            "filesystem.rename",
            {PrimitiveOperationType::FILESYSTEM_RENAME, nullopt}
        },

        {
            "application.launch",
            {
                PrimitiveOperationType::APPLICATION_LAUNCH,
                PrimitiveParameterSchema{
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::REQUIRED,
                    ParameterRequirement::OPTIONAL,
                    ParameterRequirement::NOT_APPLICABLE
                }
            }
        },

        {
            "application.close",
            {PrimitiveOperationType::APPLICATION_CLOSE, nullopt}
        },

        {
            "application.focus",
            {PrimitiveOperationType::APPLICATION_FOCUS, nullopt}
        },

        {
            "text.write",
            {
                PrimitiveOperationType::TEXT_WRITE,
                PrimitiveParameterSchema{
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::NOT_APPLICABLE,
                    ParameterRequirement::REQUIRED,
                    ParameterRequirement::REQUIRED
                }
            }
        },

        {
            "keyboard.press",
            {PrimitiveOperationType::KEYBOARD_PRESS, nullopt}
        },

        {
            "keyboard.type",
            {PrimitiveOperationType::KEYBOARD_TYPE, nullopt}
        },

        {
            "mouse.click",
            {PrimitiveOperationType::MOUSE_CLICK, nullopt}
        },

        {
            "mouse.move",
            {PrimitiveOperationType::MOUSE_MOVE, nullopt}
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
    PrimitiveDefinition definition;

    if (!resolveDefinition(primitiveName, definition))
    {
        return false;
    }

    type = definition.type;

    return true;
}

bool PrimitiveRegistry::resolveDefinition(
    const string& primitiveName,
    PrimitiveDefinition& definition
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

    definition = iterator->second;

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