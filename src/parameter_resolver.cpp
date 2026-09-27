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


// ============================================================
// BRANCH 2
// ============================================================

ParameterResolverResult ParameterResolver::resolve(
    const string& input,
    const Action& action,
    const Capability& capability
)
{
    ParameterResolverResult result;

    result.success = false;
    result.status =
    ParameterResolutionStatus::FAILED;

    result.clarificationQuestion =
        "";

    result.parameters.name = "";
    result.parameters.location = "";
    result.parameters.application = "";
    result.parameters.target = "";
    result.parameters.content = "";

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
            input.find(
                nameMarker
            );

        if (
            namePosition != string::npos
        )
        {
            size_t nameStart =
                namePosition +
                nameMarker.length();

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

                result.success =
                    true;

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
        "open_app"
        ||
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

        result.success =
            true;

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


// ============================================================
// BRANCH 3
// ============================================================

ParameterResolverResult ParameterResolver::resolve(
    const string& stepDescription,
    const PrimitiveOperation& operation,
    const Capability& capability
)
{
    ParameterResolverResult result;

    result.success = false;
    result.status =
    ParameterResolutionStatus::FAILED;

    result.clarificationQuestion =
        "";

    result.parameters.name = "";
    result.parameters.location = "";
    result.parameters.application = "";
    result.parameters.target = "";
    result.parameters.content = "";

    result.message = "";

    // --------------------------------------------------------
    // BASIC VALIDATION
    // --------------------------------------------------------

    if (stepDescription.empty())
    {
        result.message =
            "Cannot resolve parameters from empty step description.";

        return result;
    }

    if (!operation.resolved)
    {
        result.message =
            "Cannot resolve parameters for unresolved primitive.";

        return result;
    }

    if (!capability.supported)
    {
        result.message =
            "Cannot resolve parameters for unsupported capability.";

        return result;
    }

        // --------------------------------------------------------
        // FILESYSTEM — CREATE FOLDER
        // --------------------------------------------------------

        if (
            operation.name ==
            "filesystem.create"
        )
        {
            string description =
                stepDescription;

            string folderName;

            // ----------------------------------------------------
            // Explicit:
            // "directory named NovaJava"
            // "folder called NovaJava"
            // ----------------------------------------------------

            const string namedMarkers[] =
            {
                " directory named ",
                " directory called ",
                " folder named ",
                " folder called ",
                " project named ",
                " project called "
            };

            for (const string& marker : namedMarkers)
            {
                size_t markerPosition =
                    description.find(marker);

                if (markerPosition != string::npos)
                {
                    size_t nameStart =
                        markerPosition + marker.length();

                    size_t nameEnd =
                        description.find(
                            " on ",
                            nameStart
                        );

                    if (nameEnd == string::npos)
                    {
                        nameEnd =
                            description.find(
                                " in ",
                                nameStart
                            );
                    }

                    if (nameEnd != string::npos)
                    {
                        folderName =
                            description.substr(
                                nameStart,
                                nameEnd - nameStart
                            );
                    }

                    break;
                }
            }
            folderName =
                stripWrappingQuotes(
                    folderName
                );

            if (!folderName.empty())
            {
                result.parameters.name =
                    folderName;

                result.parameters.location =
                    "desktop";

                result.success =
                    true;

                result.status =
                    ParameterResolutionStatus::RESOLVED;

                result.message =
                    "Primitive parameters resolved successfully.";

                return result;

                return result;
            }

            result.status =
                ParameterResolutionStatus::CLARIFICATION_REQUIRED;

            result.message =
                "Required folder name is missing.";

            result.clarificationQuestion =
                "What should the project folder be named?";

            return result;
            // ----------------------------------------------------
// Semantic:
// "Create the NovaJava project directory..."
// "Create the NovaJava directory..."
// ----------------------------------------------------

if (folderName.empty())
{
    const string createPrefix =
        "Create ";

    if (
        description.rfind(
            createPrefix,
            0
        ) == 0
    )
    {
        string remainder =
            description.substr(
                createPrefix.length()
            );

        // Remove leading article.
        if (
            remainder.rfind(
                "the ",
                0
            ) == 0
        )
        {
            remainder =
                remainder.substr(4);
        }

        if (
            remainder.rfind(
                "a ",
                0
            ) == 0
        )
        {
            remainder =
                remainder.substr(2);
        }

        // ------------------------------------------------
        // "NovaJava project directory"
        // -> "NovaJava"
        // ------------------------------------------------

        const string projectDirectoryMarker =
            " project directory";

        size_t markerPosition =
            remainder.find(
                projectDirectoryMarker
            );

        if (markerPosition != string::npos)
        {
            folderName =
                remainder.substr(
                    0,
                    markerPosition
                );
        }

        // ------------------------------------------------
        // "NovaJava directory"
        // -> "NovaJava"
        // ------------------------------------------------

        if (folderName.empty())
        {
            const string directoryMarker =
                " directory";

            markerPosition =
                remainder.find(
                    directoryMarker
                );

            if (markerPosition != string::npos)
            {
                folderName =
                    remainder.substr(
                        0,
                        markerPosition
                    );
            }
        }
    }
}
        }
        // --------------------------------------------------------
        // FILESYSTEM — CREATE FILE
// --------------------------------------------------------

if (
    operation.name ==
    "filesystem.create_file"
)
{
    string description =
        stepDescription;

    string fileName;

    // ----------------------------------------------------
    // FORM 1 / 2:
    // "file named Main.java"
    // "file called Main.java"
    // ----------------------------------------------------

    const string namedMarkers[] =
    {
        " file named ",
        " file called "
    };

    for (const string& marker : namedMarkers)
    {
        size_t markerPosition =
            description.find(marker);

        if (markerPosition != string::npos)
        {
            size_t nameStart =
                markerPosition + marker.length();

            size_t nameEnd =
                description.find(
                    " inside ",
                    nameStart
                );

            if (nameEnd == string::npos)
            {
                nameEnd =
                    description.find(
                        " in ",
                        nameStart
                    );
            }

            if (nameEnd == string::npos)
            {
                nameEnd =
                    description.find(
                        " with ",
                        nameStart
                    );
            }

            if (nameEnd != string::npos)
            {
                fileName =
                    description.substr(
                        nameStart,
                        nameEnd - nameStart
                    );
            }
            else
            {
                fileName =
                    description.substr(
                        nameStart
                    );
            }

            break;
        }
    }

    // ----------------------------------------------------
    // FORM 3:
    // "Create a Main.java file ..."
    // "Create a basic Main.java class file ..."
    //
    // Prefer an explicit filename extension.
    // ----------------------------------------------------

    if (fileName.empty())
    {
        const string extensions[] =
        {
            ".java",
            ".cpp",
            ".py",
            ".txt"
        };

        for (const string& extension : extensions)
        {
            size_t extensionPosition =
                description.find(extension);

            if (extensionPosition == string::npos)
            {
                continue;
            }

            size_t nameStart =
                description.rfind(
                    ' ',
                    extensionPosition
                );

            if (nameStart == string::npos)
            {
                nameStart = 0;
            }
            else
            {
                nameStart++;
            }

            size_t nameEnd =
                extensionPosition +
                extension.length();

            fileName =
                description.substr(
                    nameStart,
                    nameEnd - nameStart
                );

            break;
        }
    }

    fileName =
        stripWrappingQuotes(
            fileName
        );

    // ----------------------------------------------------
    // SUCCESS
    // ----------------------------------------------------

    if (!fileName.empty())
    {
        result.parameters.name =
            fileName;

        // ----------------------------------------------------
        // Resolve semantic file content when requested.
        // ----------------------------------------------------

        if (
            description.find(" with ") != string::npos ||
            description.find(" containing ") != string::npos ||
            description.find(" that contains ") != string::npos
        )
        {
            ContentResolutionResult contentResult =
                contentResolver.resolve(
                    description,
                    fileName
                );

            if (
                contentResult.status ==
                ContentResolutionStatus::CLARIFICATION_REQUIRED
            )
            {
                result.status =
                    ParameterResolutionStatus::CLARIFICATION_REQUIRED;

                result.message =
                    contentResult.message;

                result.clarificationQuestion =
                    contentResult.clarificationQuestion;

                return result;
            }

            if (
                contentResult.status ==
                ContentResolutionStatus::FAILED
            )
            {
                result.status =
                    ParameterResolutionStatus::FAILED;

                result.message =
                    contentResult.message;

                return result;
            }

            result.parameters.content =
                contentResult.content;
        }

        result.success =
            true;

        result.status =
            ParameterResolutionStatus::RESOLVED;

        result.message =
            "File parameters resolved successfully.";

        return result;
    }

    // ----------------------------------------------------
    // CLARIFICATION
    // ----------------------------------------------------

    result.status =
        ParameterResolutionStatus::CLARIFICATION_REQUIRED;

    result.message =
        "Required file name is missing.";

    result.clarificationQuestion =
        "What should the file be named?";

    return result;
}
    // --------------------------------------------------------
    // APPLICATION — LAUNCH
    // --------------------------------------------------------

    if (
        operation.name ==
        "application.launch"
    )
    {
        string description =
            stepDescription;

        // ----------------------------------------------------
        // 1. Resolve semantic form:
        //
        // "Open VS Code and navigate to the NovaJava project directory"
        //
        // The application is the text immediately after
        // "Open " and before the navigation clause.
        // ----------------------------------------------------

        const string openPrefix =
            "Open ";

        const string launchPrefix =
            "Launch ";

        if (
            description.rfind(
                openPrefix,
                0
            ) == 0
        )
        {
            string remainder =
                description.substr(
                    openPrefix.length()
                );

            const string navigateMarker =
                " and navigate to ";

            size_t navigatePosition =
                remainder.find(
                    navigateMarker
                );

            if (
                navigatePosition != string::npos
            )
            {
                result.parameters.application =
                    remainder.substr(
                        0,
                        navigatePosition
                    );

                result.parameters.target =
                    remainder.substr(
                        navigatePosition +
                        navigateMarker.length()
                    );

                result.parameters.application =
                    stripWrappingQuotes(
                        result.parameters.application
                    );

                result.parameters.target =
                    stripWrappingQuotes(
                        result.parameters.target
                    );

                // Remove trailing punctuation.
                while (
                    !result.parameters.target.empty()
                    &&
                    (
                        result.parameters.target.back() == '.'
                        ||
                        result.parameters.target.back() == '!'
                        ||
                        result.parameters.target.back() == '?'
                    )
                )
                {
                    result.parameters.target.pop_back();
                }

                // Remove trailing " folder" / " directory".
                const string folderSuffix =
                    " folder";

                const string directorySuffix =
                    " directory";

                if (
                    result.parameters.target.size() >=
                    folderSuffix.size()
                    &&
                    result.parameters.target.compare(
                        result.parameters.target.size() -
                        folderSuffix.size(),
                        folderSuffix.size(),
                        folderSuffix
                    ) == 0
                )
                {
                    result.parameters.target.erase(
                        result.parameters.target.size() -
                        folderSuffix.size()
                    );
                }
                else if (
                    result.parameters.target.size() >=
                    directorySuffix.size()
                    &&
                    result.parameters.target.compare(
                        result.parameters.target.size() -
                        directorySuffix.size(),
                        directorySuffix.size(),
                        directorySuffix
                    ) == 0
                )
                {
                    result.parameters.target.erase(
                        result.parameters.target.size() -
                        directorySuffix.size()
                    );
                }

                if (
                    !result.parameters.application.empty()
                    &&
                    !result.parameters.target.empty()
                )
                {
                    result.success =
                        true;

                    result.status =
                        ParameterResolutionStatus::RESOLVED;

                    result.message =
                        "Application launch parameters resolved successfully.";

                    return result;
                }
            }
        }

        // ----------------------------------------------------
        // 2. Existing "in" form:
        //
        // "Open the NovaJava project directory in VS Code"
        // ----------------------------------------------------

        const string inMarker =
            " in ";

        size_t inPosition =
            description.rfind(
                inMarker
            );

        if (inPosition != string::npos)
        {
            result.parameters.target =
                stripWrappingQuotes(
                    description.substr(
                        0,
                        inPosition
                    )
                );

            result.parameters.application =
                stripWrappingQuotes(
                    description.substr(
                        inPosition +
                        inMarker.length()
                    )
                );

            // Remove trailing punctuation.
            while (
                !result.parameters.application.empty()
                &&
                (
                    result.parameters.application.back() == '.'
                    ||
                    result.parameters.application.back() == '!'
                    ||
                    result.parameters.application.back() == '?'
                )
            )
            {
                result.parameters.application.pop_back();
            }

            // Remove "Open the ..." / "Open ..." from target.
            const string openThePrefix =
                "Open the ";

            const string openPrefix =
                "Open ";

            if (
                result.parameters.target.rfind(
                    openThePrefix,
                    0
                ) == 0
            )
            {
                result.parameters.target =
                    result.parameters.target.substr(
                        openThePrefix.length()
                    );
            }
            else if (
                result.parameters.target.rfind(
                    openPrefix,
                    0
                ) == 0
            )
            {
                result.parameters.target =
                    result.parameters.target.substr(
                        openPrefix.length()
                    );
            }

            // Remove trailing " folder" / " directory".
            const string folderSuffix =
                " folder";

            const string directorySuffix =
                " directory";

            if (
                result.parameters.target.size() >=
                folderSuffix.size()
                &&
                result.parameters.target.compare(
                    result.parameters.target.size() -
                    folderSuffix.size(),
                    folderSuffix.size(),
                    folderSuffix
                ) == 0
            )
            {
                result.parameters.target.erase(
                    result.parameters.target.size() -
                    folderSuffix.size()
                );
            }
            else if (
                result.parameters.target.size() >=
                directorySuffix.size()
                &&
                result.parameters.target.compare(
                    result.parameters.target.size() -
                    directorySuffix.size(),
                    directorySuffix.size(),
                    directorySuffix
                ) == 0
            )
            {
                result.parameters.target.erase(
                    result.parameters.target.size() -
                    directorySuffix.size()
                );
            }

            if (
                !result.parameters.application.empty()
                &&
                !result.parameters.target.empty()
            )
            {
                result.success =
                    true;

                result.status =
                    ParameterResolutionStatus::RESOLVED;

                result.message =
                    "Application launch parameters resolved successfully.";

                return result;
            }
        }

        // ----------------------------------------------------
        // 3. Simple form:
        //
        // "open VS Code"
        // "launch VS Code"
        // ----------------------------------------------------

        const string simpleOpenPrefix =
            "open ";

        const string simpleLaunchPrefix =
            "launch ";

        if (
            description.rfind(
                simpleOpenPrefix,
                0
            ) == 0
        )
        {
            result.parameters.application =
                description.substr(
                    simpleOpenPrefix.length()
                );
        }
        else if (
            description.rfind(
                simpleLaunchPrefix,
                0
            ) == 0
        )
        {
            result.parameters.application =
                description.substr(
                    simpleLaunchPrefix.length()
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

        result.success =
            true;

        result.status =
            ParameterResolutionStatus::RESOLVED;

        result.message =
            "Primitive parameters resolved successfully.";

        return result;
    }
    // --------------------------------------------------------
    // TEXT — WRITE
    // --------------------------------------------------------

    if (
        operation.name ==
        "text.write"
    )
    {
        string normalizedInput =
            stepDescription;

        const string writePrefix =
            "write ";

        if (
            normalizedInput.rfind(
                writePrefix,
                0
            ) == 0
        )
        {
            normalizedInput =
                normalizedInput.substr(
                    writePrefix.length()
                );
        }

        const string intoMarker =
            " into ";

        size_t intoPosition =
            normalizedInput.find(
                intoMarker
            );

        if (
            intoPosition != string::npos
        )
        {
            string content =
                normalizedInput.substr(
                    0,
                    intoPosition
                );

            string target =
                normalizedInput.substr(
                    intoPosition +
                    intoMarker.length()
                );

            content =
                stripWrappingQuotes(
                    content
                );

            target =
                stripWrappingQuotes(
                    target
                );

            if (
                content.empty()
            )
            {
                result.message =
                    "Unable to resolve text content.";

                return result;
            }

            if (
                target.empty()
            )
            {
                result.message =
                    "Unable to resolve text target.";

                return result;
            }

            result.parameters.content =
                content;

            result.parameters.target =
                target;

            result.success =
                true;

            result.message =
                "Text write parameters resolved successfully.";

            return result;
        }

        // --------------------------------------------------------
        // SEMANTIC CONTENT RESOLUTION
        // --------------------------------------------------------

        string target;

        const string targetMarkers[] =
        {
            " in ",
            " inside "
        };

        for (const string& marker : targetMarkers)
        {
            size_t markerPosition =
                stepDescription.rfind(marker);

            if (markerPosition != string::npos)
            {
                target =
                    stepDescription.substr(
                        markerPosition + marker.length()
                    );

                target =
                    stripWrappingQuotes(
                        target
                    );

                break;
            }
        }

        if (target.empty())
        {
            result.status =
                ParameterResolutionStatus::CLARIFICATION_REQUIRED;

            result.message =
                "Unable to determine the target file.";

            result.clarificationQuestion =
                "Which file should the content be written to?";

            return result;
        }

        ContentResolutionResult contentResult =
            contentResolver.resolve(
                stepDescription,
                target
            );

        if (
            contentResult.status ==
            ContentResolutionStatus::CLARIFICATION_REQUIRED
        )
        {
            result.status =
                ParameterResolutionStatus::CLARIFICATION_REQUIRED;

            result.message =
                contentResult.message;

            result.clarificationQuestion =
                contentResult.clarificationQuestion;

            return result;
        }

        if (
            contentResult.status ==
            ContentResolutionStatus::FAILED
        )
        {
            result.status =
                ParameterResolutionStatus::FAILED;

            result.message =
                contentResult.message;

            return result;
        }

        result.parameters.target =
            target;

        result.parameters.content =
            contentResult.content;

        result.success =
            true;

        result.status =
            ParameterResolutionStatus::RESOLVED;

        result.message =
            "Semantic text content resolved successfully.";

        return result;
    }

    // --------------------------------------------------------
    // UNSUPPORTED / NOT YET DEFINED
    // --------------------------------------------------------

    result.message =
        "No parameter resolution rule exists for requested primitive.";

        return result;
}

    // ============================================================
    // PARAMETER CONVERSION
    // ============================================================

    PrimitiveParameters ParameterResolver::toPrimitiveParameters(
        const ActionParameters& parameters
    )
    {
        PrimitiveParameters result;

        result.name =
            parameters.name;

        result.location =
            parameters.location;

        result.application =
            parameters.application;

        result.target =
            parameters.target;

        result.content =
            parameters.content;

        return result;
}
    bool ParameterResolver::resolveIntoOperation(
        const string& stepDescription,
        PrimitiveOperation& operation,
        const Capability& capability
    )
    {
        ParameterResolverResult result =
            resolve(
                stepDescription,
                operation,
                capability
            );

        if (!result.success)
        {
            return false;
        }

        operation.parameters =
            toPrimitiveParameters(
                result.parameters
            );

        return true;
    }