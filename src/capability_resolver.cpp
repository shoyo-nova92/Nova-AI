#include "capability_resolver.hpp"

using namespace std;

Capability CapabilityResolver::resolve(
    const Action& action
)
{
    Capability result;

    result.supported = false;
    result.name = "";
    result.message = "";

    if (action.name.empty())
    {
        result.message =
            "Cannot resolve capability for empty action.";

        return result;
    }

    // --------------------------------------------------------
    // CAPABILITY — FILESYSTEM
    // --------------------------------------------------------

    if (
        action.name ==
        "create_folder"
    )
    {
        result.supported = true;

        result.name =
            "filesystem";

        result.message =
            "Capability resolved as filesystem.";

        return result;
    }

    // --------------------------------------------------------
    // CAPABILITY — APPLICATION CONTROL
    // --------------------------------------------------------

    if (
        action.name ==
        "open_app" ||
        action.name ==
        "close_app"
    )
    {
        result.supported = true;

        result.name =
            "application_control";

        result.message =
            "Capability resolved as application_control.";

        return result;
    }

    // --------------------------------------------------------
    // UNSUPPORTED ACTION
    // --------------------------------------------------------

    result.message =
        "No capability available for requested action.";

    return result;
}

    Capability CapabilityResolver::resolve(
        const PrimitiveOperation& operation
    )
    {
        Capability result;

        result.supported = false;
        result.name = "";
        result.message = "";

        if (operation.name.empty())
        {
            result.message =
                "Cannot resolve capability for empty primitive.";

            return result;
        }

        // --------------------------------------------------------
        // FILESYSTEM PRIMITIVES
        // --------------------------------------------------------

        if (
            operation.name ==
                "filesystem.create"
            ||
            operation.name ==
                "filesystem.create_file"
            ||
            operation.name ==
                "filesystem.delete"
            ||
            operation.name ==
                "filesystem.move"
            ||
            operation.name ==
                "filesystem.copy"
            ||
            operation.name ==
                "filesystem.rename"
        )
        {
            result.supported = true;

            result.name =
                "filesystem";

            result.message =
                "Primitive capability resolved as filesystem.";

            return result;
        }

        // --------------------------------------------------------
        // APPLICATION CONTROL PRIMITIVES
        // --------------------------------------------------------

        if (
            operation.name ==
                "application.launch"
            ||
            operation.name ==
                "application.close"
            ||
            operation.name ==
                "application.focus"
        )
        {
            result.supported = true;

            result.name =
                "application_control";

            result.message =
                "Primitive capability resolved as application_control.";

            return result;
        }

        // --------------------------------------------------------
        // TEXT INPUT
        // --------------------------------------------------------

        if (
            operation.name ==
                "text.write"
        )
        {
            result.supported = true;

            result.name =
                "text_input";

            result.message =
                "Primitive capability resolved as text_input.";

            return result;
        }

        // --------------------------------------------------------
        // KEYBOARD
        // --------------------------------------------------------

        if (
            operation.name ==
                "keyboard.press"
            ||
            operation.name ==
                "keyboard.type"
        )
        {
            result.supported = true;

            result.name =
                "keyboard_control";

            result.message =
                "Primitive capability resolved as keyboard_control.";

            return result;
        }

        // --------------------------------------------------------
        // MOUSE
        // --------------------------------------------------------

        if (
            operation.name ==
                "mouse.click"
            ||
            operation.name ==
                "mouse.move"
        )
        {
            result.supported = true;

            result.name =
                "mouse_control";

            result.message =
                "Primitive capability resolved as mouse_control.";

            return result;
        }

        // --------------------------------------------------------
        // UNSUPPORTED PRIMITIVE
        // --------------------------------------------------------

        result.message =
            "No capability available for requested primitive.";

        return result;
    }