#include "goal_decomposer.hpp"
#include "llm_client.hpp"

#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

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

    LLMClient client;

    LLMResponse llmResponse = client.generate(
        systemPrompt,
        userPrompt
    );

    if (!llmResponse.success)
    {
        plan.message =
            "Goal decomposer LLM request failed: " +
            llmResponse.message;

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
        json parsed = json::parse(response);

        if (!parsed.contains("steps") ||
            !parsed["steps"].is_array())
        {
            plan.message =
                "Goal decomposer response does not contain a valid steps array.";

            return plan;
        }

        for (const auto& stepJson : parsed["steps"])
        {
            if (!stepJson.contains("id") ||
                !stepJson["id"].is_string())
            {
                plan.message =
                    "Goal decomposer returned a step without a valid id.";

                plan.steps.clear();
                return plan;
            }

            if (!stepJson.contains("description") ||
                !stepJson["description"].is_string())
            {
                plan.message =
                    "Goal decomposer returned a step without a valid description.";

                plan.steps.clear();
                return plan;
            }

            MultiStepAction step;

            step.id =
                stepJson["id"].get<string>();

            step.description =
                stepJson["description"].get<string>();

            step.operation = "";
            step.capability = "";
            step.parameters.clear();
            step.resolved = false;

            if (stepJson.contains("depends_on"))
            {
                if (!stepJson["depends_on"].is_array())
                {
                    plan.message =
                        "Goal decomposer returned an invalid depends_on field.";

                    plan.steps.clear();
                    return plan;
                }

                for (const auto& dependency :
                     stepJson["depends_on"])
                {
                    if (!dependency.is_string())
                    {
                        plan.message =
                            "Goal decomposer returned a non-string dependency.";

                        plan.steps.clear();
                        return plan;
                    }

                    step.dependencies.push_back(
                        dependency.get<string>()
                    );
                }
            }

            plan.steps.push_back(step);
        }

        if (plan.steps.empty())
        {
            plan.message =
                "Goal decomposer returned no steps.";

            return plan;
        }

        plan.valid = true;
        plan.message =
            "Goal decomposed successfully.";

        return plan;
    }
    catch (const json::exception& error)
    {
        plan.message =
            string("Invalid JSON from goal decomposer: ") +
            error.what();

        plan.steps.clear();

        return plan;
    }
}