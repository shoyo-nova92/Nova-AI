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