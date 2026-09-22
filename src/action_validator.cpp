#include "action_validator.hpp"

using namespace std;

ActionValidationResult ActionValidator::validate(
    const Action& action,
    const Capability& capability,
    const ActionParameters& parameters
)
{
    ActionValidationResult result;

    result.valid = false;
    result.message = "";

    // --------------------------------------------------------
    // BASIC VALIDATION
    // --------------------------------------------------------

    if (action.name.empty())
    {
        result.message =
            "Action is missing.";

        return result;
    }

    if (!capability.supported)
    {
        result.message =
            "Capability is unsupported.";

        return result;
    }

    // --------------------------------------------------------
    // CREATE FOLDER
    // --------------------------------------------------------

    if (
        action.name ==
        "create_folder"
    )
    {
        if (
            parameters.name.empty()
        )
        {
            result.message =
                "Folder name is required.";

            return result;
        }

        if (
            parameters.location.empty()
        )
        {
            result.message =
                "Folder location is required.";

            return result;
        }

        result.valid = true;

        result.message =
            "Create folder action is valid.";

        return result;
    }

    // --------------------------------------------------------
    // OPEN / CLOSE APPLICATION
    // --------------------------------------------------------

    if (
        action.name ==
        "open_app" ||
        action.name ==
        "close_app"
    )
    {
        if (
            parameters.application.empty()
        )
        {
            result.message =
                "Application parameter is required.";

            return result;
        }

        result.valid = true;

        result.message =
            "Application action is valid.";

        return result;
    }

    // --------------------------------------------------------
    // UNKNOWN ACTION
    // --------------------------------------------------------

    result.message =
        "No validation rule exists for requested action.";

    return result;
}