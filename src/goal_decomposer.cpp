#include "goal_decomposer.hpp"
#include "llm_client.hpp"
#include "utf8_text.hpp"
#include "primitive_registry.hpp"
#include "predicate_registry.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_set>
#include <utility>
#include <algorithm>
#include <cctype>

using namespace std;
using json = nlohmann::json;

namespace
{
    bool isNonBlank(const string& value)
    {
        return value.find_first_not_of(" \t\r\n")
            != string::npos;
    }

    bool hasOnlyKeys(
        const json& object,
        const unordered_set<string>& allowed
    )
    {
        if (!object.is_object())
            return false;

        for (auto it = object.begin(); it != object.end(); ++it)
        {
            if (!allowed.contains(it.key()))
                return false;
        }

        return true;
    }
}

GoalDecomposer::GoalDecomposer()
    : generator_([](const string& systemPrompt, const string& userPrompt) {
          LLMClient client; return client.generate(systemPrompt, userPrompt, "decomposer");
      }) {}

GoalDecomposer::GoalDecomposer(GoalDecomposerLLM generator)
    : generator_(move(generator))
{
}

MultiStepPlan GoalDecomposer::decompose(const string& goal)
{
    auto plan=decomposeOnce(goal, "");
    if (!plan.valid && plan.failureCategory==LLMFailureCategory::NONE && isNonBlank(goal)) {
        const auto feedback=plan.message;
        plan=decomposeOnce(goal, feedback);
        if (!plan.valid && plan.failureCategory==LLMFailureCategory::NONE)
            plan.message="Plan remained invalid after one schema repair: "+plan.message;
    }
    return plan;
}

MultiStepPlan GoalDecomposer::decomposeOnce(const string& goal, const string& feedback)
{
    MultiStepPlan plan;

    plan.valid = false;
    plan.goal = goal;
    plan.steps.clear();
    plan.message = "";

    if (goal.empty())
    {
        plan.message = "Goal is empty.";
        return plan;
    }

    const string systemPrompt = R"(
You are Nova's Goal Decomposer.

Your ONLY responsibility is to decompose a user's high-level goal
into the smallest meaningful sequence of logical steps.

Do NOT:
- execute anything
- choose execution mechanisms
- invent capabilities
- resolve concrete system parameters
- produce shell commands
- produce code
- perform the requested actions
- decide whether Nova supports a capability

Each step must describe WHAT needs to happen, not HOW it will
be executed.

If one logical step may later require multiple basic operations,
keep it as one meaningful logical step.

Return STRICT JSON only.

Required format:

{
  "steps": [
    {
      "id": "step_1",
      "description": "..."
    },
    {
      "id": "step_2",
      "description": "...",
      "depends_on": ["step_1"]
    }
  ]
}

Rules:
- Declare semantic postconditions only from CANONICAL_POSTCONDITION_REGISTRY, using its exact source/kind/schema. Never use action names such as execution.filesystem.search, execution.application.launch, execution.browser.navigate or execution.filesystem.* as predicate kinds. Execution success is not semantic proof. If the needed observation identity cannot yet be specified, omit the postcondition for that step and let the planner supply a registry-valid observed proof later.
- Discovery location uncertainty belongs in a valid step, not an invalid path. Declare each logical root individually: match.search_roots:["documents","downloads"], root_semantics:"ANY". Never concatenate alternative locations into one path. scope_strength:"REQUIRED" means a hard location restriction (default); "PREFERRED" means search priority that permits compatible broadening. Set scope_provenance to USER_CONSTRAINT, USER_HINT, or INFERRED according to the interpreted language. Do not harden a tentative location hint into a required root. Unresolved preferred location clues may be progressively resolved by registered discovery operations. A discovered resource consumed by a dependent operation uses $resolved_target (or $resolved_parent); never invent a prose path placeholder. Keep one discovery step while searching alternatives.
- A logical discovery step must declare filesystem.search_result_matches with the requested query, search_root/location clue, entry_type, extension, and modified_within_days where explicitly constrained. Use search_has_results only when any non-empty result satisfies the entire request. Example postcondition: {"source":"filesystem","kind":"search_result_matches","match":{"query":"requested filename words","search_root":"user's root clue","entry_type":"file","extension":"requested extension"}}. Do not invent concrete paths or a numeric time window from vague recency alone; modification time does not establish download time. Preserve fuzzy recency as intent if it cannot be expressed reliably.
- Omit optional condition and postcondition fields when they do not apply. Do not turn a prohibition such as no overwrite into an invented branch or a reference to an unobserved fact.
- Represent every conditional branch with a structured condition, not only prose: condition: {"step_id":"earlier observation step","field":"observed fact key such as exists","equals":true or false or another scalar}. The branch applies only when that verified step's actual facts match; a false guard is marked not applicable without execution. For an inspect/open-if-present/otherwise-report task, give the opening branch condition exists=true and the missing-report branch condition exists=false, both referencing the inspection step. Do not require both branches to execute.
- For a logical step whose full intent is ensuring a new directory or new file with requested contents exists, include postcondition: {"kind":"directory_exists" or "file_content", "name":"exact requested leaf name", "location":"requested parent location"}. Preserve names and location clues. This must express the entire step, not a subtask. For browser and other tasks use only canonical semantic declarations when their constraints are known; otherwise omit. File contents are specified by the step description and content_requirement.
- When a step creates an artifact inside a directory produced by an earlier step, use the typed output reference in its artifact postcondition: location: {"step_id":"step_1","output":"path"}, and depend on that producer. Do not require the new parent to exist during planning, invent its canonical path, or search for it as an existing directory. This applies recursively to nested new directories and to several files sharing a new parent. Preserve the literal artifact leaf name and contents.
- For every file creation step include content_requirement: "required" if the user requests contents, or "empty_allowed" if no contents are requested (including explicitly empty files). Use "unspecified" for unrelated steps.
- Keep creation and requested population of a new file in ONE logical step. Preserve the complete content requirements in its description. Do not split creating a blank file from writing its requested contents.
- Step IDs must be unique.
- IDs must use the format step_1, step_2, step_3, etc.
- Keep steps in logical order.
- Use depends_on only when a step actually requires another step.
- Do not add unnecessary dependencies.
- Do not return markdown.
- Do not return explanations outside the JSON.

IMPORTANT DECOMPOSITION AND PRESERVATION RULES:

- Decompose ONLY what the user explicitly requested or what is
  logically unavoidable to fulfill that request.

- NEVER invent intermediate steps, resources, files, folders,
  directories, project structures, configurations, dependencies,
  or implementation conventions that the user did not request.

- Do not introduce standard project structures merely because they
  are conventional for a language, framework, IDE, or application.

- For example, if the user asks to create a Java project named
  "NovaJava" and add "Main.java", do NOT invent "src", "main",
  "java", Maven structure, Gradle structure, package directories,
  configuration files, or other project files unless the user
  explicitly requests them.

- Do not expand one requested action into additional implementation
  steps merely because those steps would commonly be used in practice.

- Every concrete user-provided name, filename, folder name,
  application name, path, location, or other explicit resource
  identifier MUST be preserved verbatim in the relevant step description.

- Do not replace concrete names with generic descriptions.

- Do not remove names such as "NovaJava", "Main.java", "Chrome", etc.

- Do not generalize a named resource into phrases such as
  "the project directory" when the concrete name is available.

- Preserve the user's requested level of abstraction.

- If the user requests one logical action, keep it as one logical
  step unless fulfilling it necessarily requires another explicitly
  requested logical action.

- Dependencies must represent actual logical prerequisites, not
  conventional workflow ordering.

- Step descriptions must contain enough information for deterministic
  downstream resolution of the information explicitly provided by
  the user.

- Do not infer additional user intent from domain conventions.

- The decomposer may restructure wording for clarity, but it must not
  add new user intent.)";

    json capabilities=json::array();
    for(const auto& [name,definition]:PrimitiveRegistry::getCatalog())
        capabilities.push_back({{"name",name},{"description",definition.description},{"capability",definition.capability}});
    const string userPrompt =
        "Decompose this user goal into logical steps:\n\n" + goal +
        "\nCANONICAL_POSTCONDITION_REGISTRY (only supported semantic predicate names and schemas):\n"+Utf8::dump(PredicateRegistry::catalog(),"decomposer.predicates")+
        "\nCANONICAL_CAPABILITY_REGISTRY (context only; do not assign primitives):\n"+Utf8::dump(capabilities,"decomposer.registry")+
        (feedback.empty() ? string{} : "\n\nYour previous plan failed schema validation: "+feedback+"\nRepair that structural error using the same original goal. Return only valid JSON. Preserve every requested name, literal payload and constraint; do not invent resources, observations or conditions.");

    LLMResponse llmResponse;

    if (generator_)
    {
        llmResponse = generator_(systemPrompt, userPrompt);
    }
    else
    {
        LLMClient client;
        llmResponse = client.generate(systemPrompt, userPrompt, "decomposer");
    }


    if (!llmResponse.success)
    {
        plan.failureCategory = llmResponse.failureCategory == LLMFailureCategory::NONE
            ? LLMFailureCategory::INVALID_RESPONSE : llmResponse.failureCategory;
        plan.valid = false;
        plan.steps.clear();

        plan.message = llmResponse.message.empty()
            ? "Goal decomposer LLM request failed."
            : llmResponse.message;

        return plan;
    }

    string response = llmResponse.content;

    if (response.empty())
    {
        plan.message = "Goal decomposer received an empty LLM response.";
        return plan;
    }

    try
    {
        const json parsed = json::parse(response);

        if (!parsed.is_object() ||
            !hasOnlyKeys(parsed, {"steps"}) ||
            !parsed.contains("steps") ||
            !parsed["steps"].is_array())
        {
            plan.message = "Response must contain only a steps array.";
            return plan;
        }

        const json& steps = parsed["steps"];

        if (steps.empty())
        {
            plan.message = "Goal decomposer returned no steps.";
            return plan;
        }

        if (steps.size() > 50)
        {
            plan.message = "Goal decomposer returned too many steps.";
            return plan;
        }

        unordered_set<string> knownIds;

        for (size_t i = 0; i < steps.size(); ++i)
        {
            const json& item = steps[i];

            if (!item.is_object() ||
                !hasOnlyKeys(item, {"id", "description", "depends_on", "content_requirement", "postcondition", "condition"}) ||
                !item.contains("id") ||
                !item["id"].is_string() ||
                !item.contains("description") ||
                !item["description"].is_string())
            {
                plan.message = "Step contains missing or unexpected fields.";
                plan.steps.clear();
                return plan;
            }

            const string expectedId = "step_" + to_string(i + 1);
            const string id = item["id"].get<string>();
            const string description = item["description"].get<string>();

            if (id != expectedId)
            {
                plan.message = "Step IDs must be sequential: step_1, step_2, etc.";
                plan.steps.clear();
                return plan;
            }

            if (!isNonBlank(description))
            {
                plan.message = "Step description cannot be blank.";
                plan.steps.clear();
                return plan;
            }

            MultiStepAction step{};
            step.id = id;
            step.description = description;
            step.resolved = false;
            if (item.contains("postcondition") && !item["postcondition"].is_null()) {
                json post;std::string error;
                if(!PredicateRegistry::declaration(item["postcondition"],post,error)) {
                    plan.message="Invalid postcondition for "+id+": "+error;plan.steps.clear();return plan;
                }
                if(post["source"]=="artifact") {
                    step.postconditionKind=post["kind"].get<string>();step.artifactName=post["name"].get<string>();
                    if(post["location"].is_object())step.artifactParentReference=ContextReference{ContextReferenceType::PREVIOUS_STEP_OUTPUT,"path",post["location"]["step_id"].get<string>(),false,{}};
                    else step.artifactLocation=post["location"].get<string>();
                }else step.semanticPostconditionJson=post.dump();
            }
            step.contentRequirement = item.value("content_requirement", step.postconditionKind == "file_content" ? string("required") : string("unspecified"));
            if (step.contentRequirement != "unspecified" && step.contentRequirement != "required" &&
                step.contentRequirement != "empty_allowed") {
                plan.message = "Invalid content_requirement.";
                plan.steps.clear();
                return plan;
            }

            // Goal decomposition must not assign execution primitives.
            step.operations.clear();
            step.semanticParameters.clear();
            step.contextReferences.clear();
            step.operation.clear();
            step.capability.clear();
            step.parameters.clear();

            if (item.contains("depends_on"))
            {
                const json& dependencies = item["depends_on"];

                if (!dependencies.is_array())
                {
                    plan.message = "depends_on must be an array.";
                    plan.steps.clear();
                    return plan;
                }

                unordered_set<string> seenDependencies;

                for (const auto& dependency : dependencies)
                {
                    if (!dependency.is_string())
                    {
                        plan.message = "Dependencies must be strings.";
                        plan.steps.clear();
                        return plan;
                    }

                    const string dependencyId =
                        dependency.get<string>();

                    // Dependencies must refer to earlier steps.
                    if (!knownIds.contains(dependencyId))
                    {
                        plan.message =
                            "Dependency must reference an earlier existing step: " +
                            dependencyId;

                        plan.steps.clear();
                        return plan;
                    }

                    if (!seenDependencies.insert(dependencyId).second)
                    {
                        plan.message =
                            "Duplicate dependency: " + dependencyId;

                        plan.steps.clear();
                        return plan;
                    }

                    step.dependencies.push_back(dependencyId);
                }
            }

            if (item.contains("condition") && !item["condition"].is_null()) {
                const auto& condition=item["condition"];
                if (!condition.is_object() || !hasOnlyKeys(condition,{"step_id","field","equals"}) ||
                    !condition.contains("step_id") || !condition["step_id"].is_string() ||
                    !condition.contains("field") || !condition["field"].is_string() ||
                    !condition.contains("equals") || !condition["equals"].is_primitive() ||
                    !knownIds.contains(condition["step_id"].get<string>()) || !isNonBlank(condition["field"].get<string>())) {
                    plan.message="Invalid condition for "+id+": "+Utf8::truncate(condition.dump(),512,"decomposer.condition")+". Condition must reference an earlier observation step and a scalar fact comparison.";plan.steps.clear();return plan;
                }
                const auto source=condition["step_id"].get<string>();
                if (find(step.dependencies.begin(),step.dependencies.end(),source)==step.dependencies.end()) step.dependencies.push_back(source);
                step.conditionJson=condition.dump();
            }
            // Normalize an unambiguous named planned directory before any environment lookup.
            // Explicit producer references and inferred bare-name references both add a real dependency.
            if(!step.postconditionKind.empty()) {
                if(!step.artifactParentReference && !step.artifactLocation.empty() && step.artifactLocation.find_first_of("/\\:")==string::npos) {
                    vector<string> matches;
                    for(const auto& prior:plan.steps)if(prior.postconditionKind=="directory_exists" && prior.artifactName.size()==step.artifactLocation.size() &&
                        equal(prior.artifactName.begin(),prior.artifactName.end(),step.artifactLocation.begin(),[](unsigned char a,unsigned char b){return tolower(a)==tolower(b);}))matches.push_back(prior.id);
                    if(matches.size()>1) {
                        vector<string> direct;for(const auto& producer:matches)if(find(step.dependencies.begin(),step.dependencies.end(),producer)!=step.dependencies.end())direct.push_back(producer);
                        if(direct.size()==1)matches=direct;
                    }
                    if(matches.size()>1){plan.message="Ambiguous planned artifact parent for "+id+"; use a typed producer reference";plan.steps.clear();return plan;}
                    if(matches.size()==1)step.artifactParentReference=ContextReference{ContextReferenceType::PREVIOUS_STEP_OUTPUT,"path",matches[0],false,{}};
                }
                if(step.artifactParentReference) {
                    const auto producer=step.artifactParentReference->sourceStepId;
                    const auto prior=find_if(plan.steps.begin(),plan.steps.end(),[&](const auto& value){return value.id==producer;});
                    if(prior==plan.steps.end() || prior->postconditionKind!="directory_exists") {plan.message="Artifact parent must reference an earlier directory-producing step: "+producer;plan.steps.clear();return plan;}
                    if(find(step.dependencies.begin(),step.dependencies.end(),producer)==step.dependencies.end())step.dependencies.push_back(producer);
                }
            }
            knownIds.insert(id);
            plan.steps.push_back(move(step));
        }

        plan.valid = true;
        plan.message = "Goal decomposed and validated successfully.";
        return plan;
    }
    catch (const json::exception& error)
    {
        plan.steps.clear();
        plan.message =
            string("Invalid JSON from goal decomposer: ") + error.what();
        return plan;
    }
}
