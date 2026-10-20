#include "goal_decomposer.hpp"
#include "llm_client.hpp"

#include <nlohmann/json.hpp>

#include <string>
#include <unordered_set>
#include <utility>

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
          LLMClient client; return client.generate(systemPrompt, userPrompt);
      }) {}

GoalDecomposer::GoalDecomposer(GoalDecomposerLLM generator)
    : generator_(move(generator))
{
}

MultiStepPlan GoalDecomposer::decompose(const string& goal)
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

    const string userPrompt =
        "Decompose this user goal into logical steps:\n\n" + goal;

    LLMResponse llmResponse;

    if (generator_)
    {
        llmResponse = generator_(systemPrompt, userPrompt);
    }
    else
    {
        LLMClient client;
        llmResponse = client.generate(systemPrompt, userPrompt);
    }


    if (!llmResponse.success)
    {
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
                !hasOnlyKeys(item, {"id", "description", "depends_on"}) ||
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