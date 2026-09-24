#include "application_resolver.hpp"

#include <iostream>

int main()
{
    ApplicationResolver resolver;

    ApplicationResolverResult result =
        resolver.resolve("chrome");

    std::cout
        << "[APPLICATION RESOLVER TEST]"
        << std::endl;

    std::cout
        << "Success: "
        << (result.success ? "true" : "false")
        << std::endl;

    std::cout
        << "Application: "
        << result.application
        << std::endl;

    std::cout
        << "Display Name: "
        << (
            result.displayName.empty()
            ? "[none]"
            : result.displayName
        )
        << std::endl;

    std::cout
        << "App ID: "
        << (
            result.appId.empty()
            ? "[none]"
            : result.appId
        )
        << std::endl;

    std::cout
        << "Message: "
        << result.message
        << std::endl;

    return 0;
}