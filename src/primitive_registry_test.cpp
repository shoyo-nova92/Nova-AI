#include "primitive_registry.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <vector>
using namespace std;

namespace {
int passed=0, total=0;
void check(const string& name, bool ok) {
    ++total; cout << "[" << name << "] " << (ok ? "PASS" : "FAIL") << '\n';
    if (ok) ++passed;
}
}

int main() {
    const vector<pair<string,PrimitiveOperationType>> expected = {
        {"filesystem.create",PrimitiveOperationType::FILESYSTEM_CREATE},
        {"filesystem.create_file",PrimitiveOperationType::FILESYSTEM_CREATE_FILE},
        {"filesystem.delete",PrimitiveOperationType::FILESYSTEM_DELETE},
        {"filesystem.move",PrimitiveOperationType::FILESYSTEM_MOVE},
        {"filesystem.copy",PrimitiveOperationType::FILESYSTEM_COPY},
        {"filesystem.rename",PrimitiveOperationType::FILESYSTEM_RENAME},
        {"filesystem.list_directory",PrimitiveOperationType::FILESYSTEM_LIST_DIRECTORY},
        {"filesystem.search",PrimitiveOperationType::FILESYSTEM_SEARCH},
        {"filesystem.inspect",PrimitiveOperationType::FILESYSTEM_INSPECT},
        {"application.launch",PrimitiveOperationType::APPLICATION_LAUNCH},
        {"application.close",PrimitiveOperationType::APPLICATION_CLOSE},
        {"application.focus",PrimitiveOperationType::APPLICATION_FOCUS},
        {"text.write",PrimitiveOperationType::TEXT_WRITE},
        {"keyboard.press",PrimitiveOperationType::KEYBOARD_PRESS},
        {"keyboard.type",PrimitiveOperationType::KEYBOARD_TYPE},
        {"mouse.click",PrimitiveOperationType::MOUSE_CLICK},
        {"mouse.move",PrimitiveOperationType::MOUSE_MOVE},
        {"browser.navigate",PrimitiveOperationType::BROWSER_NAVIGATE},
        {"browser.wait_for",PrimitiveOperationType::BROWSER_WAIT_FOR},
        {"browser.observe",PrimitiveOperationType::BROWSER_OBSERVE},
        {"browser.click",PrimitiveOperationType::BROWSER_CLICK},
        {"browser.type",PrimitiveOperationType::BROWSER_TYPE}
    };

    const auto names=PrimitiveRegistry::getSupportedNames();
    check("EXPECTED_PRIMITIVE_COUNT",names.size()==22);
    check("STABLE_SORTED_NAMES",is_sorted(names.begin(),names.end()));

    bool types=true, schemas=true, metadata=true, uniqueFields=true, outputs=true;
    for (const auto& [name, expectedType] : expected) {
        PrimitiveDefinition def{};
        PrimitiveOperationType actual{};
        bool found=PrimitiveRegistry::isSupported(name)
            && PrimitiveRegistry::resolveType(name,actual)
            && PrimitiveRegistry::resolveDefinition(name,def);
        check("REGISTERED_"+name,found);
        if (!found) { types=schemas=metadata=outputs=false; continue; }
        types=types && actual==expectedType && def.type==expectedType;
        metadata=metadata && !def.description.empty() && !def.capability.empty();
        schemas=schemas && def.parameters.has_value() && !def.parameters->fields.empty();
        outputs=outputs && !def.outputs.empty();
        if (def.parameters) {
            set<string> seen;
            for (const auto& f : def.parameters->fields) {
                if (f.name.empty() || f.description.empty() || !seen.insert(f.name).second)
                    uniqueFields=false;
            }
        }
        for (const auto& o : def.outputs)
            if (o.name.empty() || o.type.empty() || o.description.empty()) outputs=false;
    }
    check("ALL_ENUM_MAPPINGS_CORRECT",types);
    check("ALL_SCHEMAS_DEFINED",schemas);
    check("ALL_DESCRIPTIONS_AND_CAPABILITIES_DEFINED",metadata);
    check("UNIQUE_PARAMETER_NAMES",uniqueFields);
    check("ALL_OUTPUT_CONTRACTS_DEFINED",outputs);

    PrimitiveDefinition browserNavigate{};
    bool browserNavigateOk = PrimitiveRegistry::resolveDefinition("browser.navigate", browserNavigate)
        && browserNavigate.parameters && browserNavigate.parameters->fields.size() == 4;
    if (browserNavigateOk) {
        const auto& fields = browserNavigate.parameters->fields;
        browserNavigateOk = fields[0].name == "url" && fields[0].type == PrimitiveParameterType::URL
            && fields[0].requirement == ParameterRequirement::REQUIRED
            && fields[1].name == "ready_condition" && fields[1].type == PrimitiveParameterType::ENUM
            && fields[2].name == "selector" && fields[2].requirement == ParameterRequirement::OPTIONAL
            && fields[3].name == "timeout_ms" && fields[3].hasMinimum && fields[3].minimum == 1000
            && fields[3].hasMaximum && fields[3].maximum == 120000;
    }
    check("BROWSER_NAVIGATE_SCHEMA_VALIDATED", browserNavigateOk);

    PrimitiveDefinition browserWait{};
    bool browserWaitOk = PrimitiveRegistry::resolveDefinition("browser.wait_for", browserWait)
        && browserWait.parameters && browserWait.parameters->fields.size() == 3;
    check("BROWSER_WAIT_SCHEMA_VALIDATED", browserWaitOk);

    PrimitiveOperationType unused{};
    PrimitiveDefinition missing{};
    check("UNKNOWN_PRIMITIVE_REJECTED",
        !PrimitiveRegistry::isSupported("system.destroy_everything")
        && !PrimitiveRegistry::resolveType("system.destroy_everything",unused)
        && !PrimitiveRegistry::resolveDefinition("system.destroy_everything",missing));

    PrimitiveDefinition createFile{};
    bool createFileOk=PrimitiveRegistry::resolveDefinition("filesystem.create_file",createFile)
        && createFile.parameters && createFile.parameters->fields.size()==3;
    check("CREATE_FILE_SCHEMA",createFileOk);

    PrimitiveDefinition click{};
    bool clickOk=PrimitiveRegistry::resolveDefinition("mouse.click",click) && click.parameters;
    if (clickOk) {
        const auto& f=click.parameters->fields;
        clickOk=f.size()==4 && f[0].name=="x" && f[1].name=="y"
            && f[2].name=="button" && f[3].name=="clickCount";
    }
    check("MOUSE_CLICK_SCHEMA",clickOk);

    cout << "\nPRIMITIVE REGISTRY TEST\nRegistered: " << names.size()
         << "\nPassed: " << passed << " / " << total << '\n';
    cout << (passed==total ? "RESULT: PASS\n" : "RESULT: FAIL\n");
    return passed==total ? 0 : 1;
}
