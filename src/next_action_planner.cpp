#include "next_action_planner.hpp"
#include <nlohmann/json.hpp>
#include <exception>
#include <utility>
#include <stdexcept>
using nlohmann::json;

namespace {
// LLMs occasionally prepend explanations or wrap JSON in Markdown despite the
// strict-JSON instruction. Accept the first complete top-level JSON object.
json parseDecisionJson(const std::string& content) {
    try {
        const auto parsed = json::parse(content);
        if (parsed.is_object()) return parsed;
    } catch (...) {
        // Try extracting a JSON object from surrounding prose/Markdown below.
    }

    std::size_t start = std::string::npos;
    int depth = 0;
    bool inString = false;
    bool escaped = false;
    for (std::size_t i = 0; i < content.size(); ++i) {
        const char c = content[i];
        if (start == std::string::npos) {
            if (c == '{') {
                start = i;
                depth = 1;
                inString = false;
                escaped = false;
            }
            continue;
        }
        if (inString) {
            if (escaped) escaped = false;
            else if (c == '\\') escaped = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') inString = true;
        else if (c == '{') ++depth;
        else if (c == '}' && --depth == 0) {
            const auto candidate = content.substr(start, i - start + 1);
            const auto parsed = json::parse(candidate);
            if (parsed.is_object()) return parsed;
            start = std::string::npos;
        }
    }
    throw std::runtime_error("response did not contain a complete JSON object");
}
}
NextActionPlanner::NextActionPlanner() : generator_([](const std::string& system, const std::string& user) {
    LLMClient client; return client.generate(system, user);
}) {}
NextActionPlanner::NextActionPlanner(NextActionLLM generator) : generator_(std::move(generator)) {}
NextActionDecision NextActionPlanner::choose(const std::string& goal, const MultiStepPlan& guide,
                                              const std::string& state, const std::string& catalog) {
    NextActionDecision out; out.status = "blocked";
    if (!generator_) { out.message = "Next-action planner has no LLM generator."; return out; }
    json steps = json::array();
    for (const auto& step : guide.steps)
        steps.push_back({{"id", step.id}, {"description", step.description}, {"depends_on", step.dependencies}});
    const std::string system = R"PROMPT(You are Nova's state-aware Branch 3 next-action planner.
Return exactly one JSON object and nothing else. The first non-whitespace character must be { and the last non-whitespace character must be }. Do not include explanations, preambles, Markdown fences, or analysis. Choose exactly ONE next operation based on the stable user goal, guide steps, current observed state, action history, and live primitive catalog.
Allowed outputs:
{"status":"operation","step_id":"step_1","operation":{"name":"registered.name","parameters":{...}},"message":"short rationale"}
{"status":"done","message":"evidence-based reason"}
{"status":"ask_user","message":"specific clarification needed"}
{"status":"blocked","message":"specific blocker"}
Rules:
- Use only exact primitive names and parameters from the supplied catalog.
- Choose one operation only. Do not return a sequence.
- Guide steps are guidance, not a rigid script. Skip actions already satisfied by observed state.
- Treat the observed DOM focus as authoritative evidence about where keyboard input will go. The observation may include a top-level `focus` object and per-element `focused`, `editable`, `enabled`, and `value` fields.
- Before proposing a click on a text box or message composer, inspect the current focus. If the active element is focused, editable, and enabled, and it is the intended input target, DO NOT click it again; proceed directly to browser.type using that observed element (or ask_user if the intended target cannot be established).
- Infer readiness from general DOM semantics, not website-specific names or hardcoded selectors. A focused editable element is sufficient evidence that keyboard typing can proceed; a blinking caret need not be detected visually when DOM focus is available.
- After typing, verify the requested text appears in the observed editable element's value/content before selecting a send/submit control. Do not send if the wrong chat or composer is uncertain.
- Do not claim completion unless the observation provides evidence that the goal is already satisfied.
- For browser work, use browser.navigate to open or navigate the controlled browser. Never use application.launch to open a website or browser URL.
- For browser work, every iteration starts with a fresh DOM observation. After each click/type, use the returned post-action DOM snapshot and the next fresh observation to verify the expected state transition before choosing the next action. A successful click call alone is not proof that the goal or step succeeded.
- Use browser.click/browser.type only with elements from the latest DOM observation, by observed element_id or exact role+text; never use coordinates or invented CSS selectors.
- Treat authentication as a generic DOM state, not a site-specific rule. If the observation reports auth_required or the DOM evidence is insufficient, ask the user to complete authentication or clarify. Do not infer login/QR from a domain name or a canvas alone.
- If a target is ambiguous, return ask_user. Do not repeat an operation that has failed repeatedly. If state is insufficient, observe or ask_user.
- For filesystem work, treat resolved_path and discovered_paths in the action history as authoritative evidence. Reuse a path returned by a successful operation rather than rebuilding it from a name and a location.
- Use filesystem.inspect to establish the existence/type of an explicit path, filesystem.list_directory to inspect one known directory, and filesystem.search only under a user-supplied or previously resolved root. Respect their bounded results.
- Do not guess a missing folder from its name. If a search yields multiple viable candidates, return ask_user. Distinguish malformed paths, nonexistent locations, access-denied locations, protected destinations, and ordinary execution errors from the recorded failure category.
- After a filesystem mutation, use its returned resolved_path as evidence and inspect again when a meaningful state change must be verified. Never retry the same operation with unchanged normalized arguments after it has repeatedly failed for the same category.
- Sending a message is an external side effect; only send the exact message requested by the user.
)PROMPT";
    const std::string user = "GOAL:\n" + goal + "\nGUIDE_STEPS:\n" + steps.dump(2) +
        "\nCURRENT_STATE_AND_HISTORY:\n" + state + "\nLIVE_PRIMITIVE_CATALOG:\n" + catalog +
        "\nReturn one decision as strict JSON.";
    LLMResponse response;
    try { response = generator_(system, user); }
    catch (const std::exception& e) { out.message = std::string("Planner call failed: ") + e.what(); return out; }
    catch (...) { out.message = "Planner call failed with an unknown exception."; return out; }
    if (!response.success || response.content.empty()) { out.message = response.message.empty() ? "Planner returned no response." : response.message; return out; }
    json j;
    try {
        j = parseDecisionJson(response.content);
    } catch (const std::exception&) {
        // One bounded retry is cheaper and safer than blocking Branch 3 on a
        // formatting-only failure. Preserve the same goal and observed state.
        const std::string retryUser = user +
            "\nFORMAT ERROR: your previous response was not a JSON object. "
            "Return only one valid JSON object using the allowed schema. No prose.";
        try {
            response = generator_(system, retryUser);
            if (!response.success || response.content.empty()) {
                out.message = response.message.empty() ? "Planner retry returned no response." : response.message;
                return out;
            }
            j = parseDecisionJson(response.content);
        } catch (const std::exception& e) {
            out.status = "blocked";
            out.message = std::string("Planner JSON invalid after one retry: ") + e.what();
            return out;
        } catch (...) {
            out.status = "blocked";
            out.message = "Planner JSON invalid after one retry.";
            return out;
        }
    }
    try {
        if (!j.is_object() || !j.contains("status") || !j["status"].is_string()) { out.message = "Planner response has no valid status."; return out; }
        out.status = j["status"].get<std::string>();
        out.message = j.value("message", std::string{});
        if (out.status == "operation") {
            if (!j.contains("step_id") || !j["step_id"].is_string() || !j.contains("operation") || !j["operation"].is_object()) {
                out.status = "blocked"; out.message = "Planner operation response is incomplete."; return out;
            }
            out.stepId = j["step_id"].get<std::string>();
            out.operationJson = j["operation"].dump();
        } else if (out.status != "done" && out.status != "ask_user" && out.status != "blocked") {
            out.status = "blocked"; out.message = "Planner returned an unsupported status.";
        }
    } catch (const std::exception& e) { out.status = "blocked"; out.message = std::string("Planner decision invalid: ") + e.what(); }
    return out;
}
