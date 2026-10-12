#include "parameter_resolver.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

namespace
{
    string resolvedJson(
        const string& name,
        const string& location,
        const string& application,
        const string& target,
        const string& content
    )
    {
        return string("{\"status\":\"resolved\",\"parameters\":{") +
            "\"name\":" + name + "," +
            "\"location\":" + location + "," +
            "\"application\":" + application + "," +
            "\"target\":" + target + "," +
            "\"content\":" + content + "}}";
    }

    string noneValue()
    {
        return "{\"type\":\"none\"}";
    }

    string literalValue(const string& value)
    {
        return "{\"type\":\"literal\",\"value\":\"" + value + "\"}";
    }

    string contextValue(const string& stepId, const string& output)
    {
        return "{\"type\":\"context_reference\",\"step_id\":\"" +
            stepId + "\",\"output\":\"" + output + "\"}";
    }

    ParameterResolver makeResolver(const string& response)
    {
        return ParameterResolver(
            [response](const string&, const string&)
            {
                return LLMResponse{true, response, ""};
            });
    }

    PrimitiveOperation createFileOperation()
    {
        PrimitiveOperation operation{};
        operation.type = PrimitiveOperationType::FILESYSTEM_CREATE_FILE;
        operation.name = "filesystem.create_file";
        operation.resolved = true;
        return operation;
    }

    Capability filesystemCapability()
    {
        Capability capability{};
        capability.supported = true;
        capability.name = "filesystem";
        capability.message = "Test capability.";
        return capability;
    }

    bool check(bool condition, const string& testName, int& passed, int& total)
    {
        ++total;
        cout << "[" << (condition ? "PASS" : "FAIL") << "] "
             << testName << endl;
        if (condition)
        {
            ++passed;
        }
        return condition;
    }
}

int main()
{
    int passed = 0;
    int total = 0;
    const Capability capability = filesystemCapability();

    {
        ParameterResolver resolver = makeResolver(resolvedJson(
            literalValue("Main.java"),
            contextValue("step_1", "path"),
            noneValue(),
            noneValue(),
            noneValue()));
        const ParameterResolverResult result = resolver.resolve(
            "Create Main.java inside that folder",
            createFileOperation(),
            capability);
        check(
            result.success &&
                result.parameters.name.type == ParameterValueType::LITERAL &&
                result.parameters.name.value == "Main.java" &&
                result.parameters.location.type == ParameterValueType::CONTEXT_REFERENCE &&
                result.parameters.location.reference.stepId == "step_1" &&
                result.parameters.location.reference.output == "path",
            "literal and context reference preserved",
            passed,
            total);
    }

    {
        ParameterResolver resolver = makeResolver(resolvedJson(
            literalValue("Main.java"),
            literalValue("desktop"),
            noneValue(),
            noneValue(),
            noneValue()));
        const ParameterResolverResult result = resolver.resolve(
            "Create Main.java",
            createFileOperation(),
            capability);
        check(
            result.success &&
                result.parameters.content.type == ParameterValueType::NONE,
            "optional content omitted for file creation",
            passed,
            total);
    }

    {
        ParameterResolver resolver = makeResolver(resolvedJson(
            noneValue(),
            literalValue("desktop"),
            noneValue(),
            noneValue(),
            noneValue()));
        const ParameterResolverResult result = resolver.resolve(
            "Create a file",
            createFileOperation(),
            capability);
        check(
            !result.success &&
                result.status == ParameterResolutionStatus::INVALID_RESPONSE &&
                result.message.find("Required parameter is missing: name") != string::npos,
            "required parameter missing is rejected",
            passed,
            total);
    }

    {
        const string response =
            "{\"status\":\"missing_required_parameter\","
            "\"question\":\"What should the file be named?\"}";
        ParameterResolver resolver = makeResolver(response);
        const ParameterResolverResult result = resolver.resolve(
            "Create a file",
            createFileOperation(),
            capability);
        check(
            !result.success &&
                result.status == ParameterResolutionStatus::MISSING_REQUIRED_PARAMETER &&
                !result.clarificationQuestion.empty(),
            "LLM missing-required response is preserved",
            passed,
            total);
    }

    {
        ParameterResolver resolver = makeResolver(resolvedJson(
            literalValue("Main.java"),
            literalValue("desktop"),
            literalValue("VS Code"),
            noneValue(),
            noneValue()));
        const ParameterResolverResult result = resolver.resolve(
            "Create Main.java",
            createFileOperation(),
            capability);
        check(
            !result.success &&
                result.status == ParameterResolutionStatus::INVALID_RESPONSE &&
                result.message.find("not applicable: application") != string::npos,
            "not-applicable parameter is rejected",
            passed,
            total);
    }

    {
        ParameterResolver resolver = makeResolver(
            "{\"status\":\"resolved\",\"parameters\":{broken}}");
        const ParameterResolverResult result = resolver.resolve(
            "Create Main.java",
            createFileOperation(),
            capability);
        check(
            !result.success &&
                result.status == ParameterResolutionStatus::INVALID_RESPONSE,
            "invalid LLM JSON is rejected",
            passed,
            total);
    }

    {
        ParameterResolver resolver = makeResolver(
            "{\"status\":\"ambiguous\",\"question\":\"Which folder?\"}");
        const ParameterResolverResult result = resolver.resolve(
            "Create Main.java inside that folder",
            createFileOperation(),
            capability);
        check(
            !result.success &&
                result.status == ParameterResolutionStatus::AMBIGUOUS,
            "ambiguous LLM response is surfaced",
            passed,
            total);
    }

    {
        ParameterResolver resolver = makeResolver(resolvedJson(
            literalValue("Main.java"),
            literalValue("desktop"),
            noneValue(),
            noneValue(),
            noneValue()));
        const ParameterResolverResult result = resolver.resolve(
            "Create a file called Main.java inside NovaProject",
            createFileOperation(),
            capability);
        check(
            result.success &&
                result.parameters.content.type == ParameterValueType::NONE,
            "file extension does not imply requested content",
            passed,
            total);
    }

    cout << "Passed: " << passed << " / " << total << endl;
    return passed == total ? 0 : 1;
}
