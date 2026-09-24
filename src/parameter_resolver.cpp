#include "parameter_resolver.hpp"

using namespace std;

namespace
{
    string stripWrappingQuotes(
        const string& value
    )
    {
        if (value.size() >= 2)
        {
            char first = value.front();
            char last = value.back();

            if (
                (first == '"' && last == '"') ||
                (first == '\'' && last == '\'')
            )
            {
                return value.substr(
                    1,
                    value.size() - 2
                );
            }
        }

        return value;
    }
}

ParameterResolverResult ParameterResolver::resolve(
    const string& input,
    const Action& action,
    const Capability& capability
)
{
    ParameterResolverResult result;

    result.success = false;
    result.parameters.name = "";
    result.parameters.location = "";
    result.parameters.application = "";
    result.message = "";

    if (input.empty())
    {
        result.message =
            "Cannot resolve parameters from empty input.";

        return result;
    }

    if (!capability.supported)
    {
        result.message =
            "Cannot resolve parameters for unsupported capability.";

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
        const string nameMarker =
            "called ";

        size_t namePosition =
            input.find(nameMarker);

        if (
            namePosition != string::npos
        )
        {
            size_t nameStart =
                namePosition + nameMarker.length();

            size_t nameEnd =
                input.find(
                    " on ",
                    nameStart
                );

            if (
                nameEnd != string::npos
            )
            {
                result.parameters.name =
                    stripWrappingQuotes(
                        input.substr(
                            nameStart,
                            nameEnd - nameStart
                        )
                    );

                result.parameters.location =
                    "desktop";

                if (
                    result.parameters.name.empty()
                )
                {
                    result.message =
                        "Folder name cannot be empty.";

                    return result;
                }

                result.success = true;

                result.message =
                    "Folder parameters resolved successfully.";

                return result;
            }
        }

        result.message =
            "Unable to resolve folder name and location.";

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
        const string openPrefix =
            "open ";

        const string launchPrefix =
            "launch ";

        const string closePrefix =
            "close ";

        if (
            input.rfind(
                openPrefix,
                0
            ) == 0
        )
        {
            result.parameters.application =
                input.substr(
                    openPrefix.length()
                );
        }
        else if (
            input.rfind(
                launchPrefix,
                0
            ) == 0
        )
        {
            result.parameters.application =
                input.substr(
                    launchPrefix.length()
                );
        }
        else if (
            input.rfind(
                closePrefix,
                0
            ) == 0
        )
        {
            result.parameters.application =
                input.substr(
                    closePrefix.length()
                );
        }

        result.parameters.application =
            stripWrappingQuotes(
                result.parameters.application
            );

        if (
            result.parameters.application.empty()
        )
        {
            result.message =
                "Unable to resolve application parameter.";

            return result;
        }

        result.success = true;

        result.message =
            "Application parameter resolved successfully.";

        return result;
    }

    // --------------------------------------------------------
    // UNSUPPORTED ACTION
    // --------------------------------------------------------

    result.message =
        "No parameter resolution rule exists for requested action.";

    return result;
}