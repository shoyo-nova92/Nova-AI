#include <windows.h>

#include "application_resolver.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace
{
    std::string normalize(
        const std::string& value
    )
    {
        std::string result = value;

        std::transform(
            result.begin(),
            result.end(),
            result.begin(),
            [](
                unsigned char character
            )
            {
                return static_cast<char>(
                    std::tolower(character)
                );
            }
        );

        return result;
    }

    std::string escapePowerShell(
        const std::string& value
    )
    {
        std::string result;

        for (char character : value)
        {
            if (character == '\'')
            {
                result += "''";
            }
            else
            {
                result += character;
            }
        }

        return result;
    }
}

ApplicationResolverResult ApplicationResolver::resolve(
    const std::string& application
)
{
    ApplicationResolverResult result{};

    result.success = false;
    result.application = application;
    result.appId = "";
    result.displayName = "";
    result.message = "";

    if (application.empty())
    {
        result.message =
            "Application name is empty.";

        return result;
    }

    const std::string normalized =
        normalize(application);

    const std::string escaped =
        escapePowerShell(application);

    const std::string command =
        "powershell.exe -NoProfile -Command "
        "\""
        "$app = Get-StartApps | "
        "Where-Object { "
        "$_.Name -like '*"
        + escaped +
        "*' "
        "-or "
        "$_.AppID -like '*"
        + escaped +
        "*' "
        "} | Select-Object -First 1; "
        "if ($null -ne $app) { "
        "[Console]::WriteLine($app.Name); "
        "[Console]::WriteLine($app.AppID); "
        "}"
        "\"";

    FILE* pipe =
        _popen(
            command.c_str(),
            "r"
        );

    if (pipe == nullptr)
    {
        result.message =
            "Failed to query Windows Start Apps.";

        return result;
    }

    char buffer[1024];

    std::string output;

    while (
        fgets(
            buffer,
            sizeof(buffer),
            pipe
        ) != nullptr
    )
    {
        output += buffer;
    }

    const int exitCode =
        _pclose(pipe);

    if (exitCode != 0 || output.empty())
    {
        result.message =
            "Windows Start Apps could not resolve application: "
            + application;

        return result;
    }

    const std::size_t separator =
        output.find('\n');

    if (separator == std::string::npos)
    {
        result.message =
            "Windows returned an invalid application result.";

        return result;
    }

    result.displayName =
        output.substr(
            0,
            separator
        );

    result.appId =
        output.substr(
            separator + 1
        );

    while (
        !result.displayName.empty()
        &&
        (
            result.displayName.back() == '\r'
            ||
            result.displayName.back() == '\n'
        )
    )
    {
        result.displayName.pop_back();
    }

    while (
        !result.appId.empty()
        &&
        (
            result.appId.back() == '\r'
            ||
            result.appId.back() == '\n'
        )
    )
    {
        result.appId.pop_back();
    }

    if (
        result.displayName.empty()
        ||
        result.appId.empty()
    )
    {
        result.message =
            "Windows returned an incomplete application result.";

        return result;
    }

    result.success = true;

    result.message =
        "Application resolved through Windows Start Apps.";

    return result;
}