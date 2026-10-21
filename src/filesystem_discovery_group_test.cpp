#include "primitive_execution_engine.hpp"
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {
PrimitiveOperation operation(PrimitiveOperationType type, const std::string& name) {
    PrimitiveOperation result{};
    result.type = type;
    result.name = name;
    result.resolved = true;
    return result;
}
}

int main() {
    // Read-only coverage: this target never creates, modifies, or deletes data.
    PrimitiveExecutionEngine engine;
    const std::string root = fs::current_path().string();

    auto inspect = operation(PrimitiveOperationType::FILESYSTEM_INSPECT, "filesystem.inspect");
    inspect.parameters.target = root;
    const ExecutionResult inspectResult = engine.execute(inspect);

    auto list = operation(PrimitiveOperationType::FILESYSTEM_LIST_DIRECTORY, "filesystem.list_directory");
    list.parameters.location = root;
    list.parameters.maxResults = "5";
    const ExecutionResult listResult = engine.execute(list);

    auto search = operation(PrimitiveOperationType::FILESYSTEM_SEARCH, "filesystem.search");
    search.parameters.location = root;
    search.parameters.query = "definitely-not-a-nova-test-match";
    search.parameters.maxResults = "1";
    const ExecutionResult searchResult = engine.execute(search);

    const bool pass = inspectResult.status == ExecutionStatus::SUCCESS &&
        listResult.status == ExecutionStatus::SUCCESS &&
        searchResult.status == ExecutionStatus::SUCCESS &&
        !inspectResult.resolvedPath.empty();
    std::cout << (pass ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return pass ? 0 : 1;
}
