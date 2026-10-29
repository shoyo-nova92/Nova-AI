#include "next_action_planner.hpp"
#include "utf8_text.hpp"
#include "predicate_registry.hpp"
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
    LLMClient client; return client.generate(system, user, "planner");
}) {}
NextActionPlanner::NextActionPlanner(NextActionLLM generator) : generator_(std::move(generator)) {}
NextActionDecision NextActionPlanner::choose(const std::string& goal, const MultiStepPlan& guide,
                                              const std::string& state, const std::string& catalog,
                                              const std::string& selectedStepId) {
    NextActionDecision out; out.status = "blocked";
    if (!generator_) { out.failureCategory = LLMFailureCategory::CONFIGURATION_ERROR; out.message = "Next-action planner has no LLM generator."; return out; }
    json steps = json::array();
    for (const auto& step : guide.steps)
        steps.push_back({{"id", step.id}, {"description", step.description}, {"depends_on", step.dependencies}, {"condition",step.conditionJson.empty()?json(nullptr):json::parse(step.conditionJson)}, {"content_requirement", step.contentRequirement}, {"postcondition", step.semanticPostconditionJson.empty()?json{{"kind",step.postconditionKind},{"name",step.artifactName},{"location",step.artifactParentReference?json{{"step_id",step.artifactParentReference->sourceStepId},{"output","path"}}:json(step.artifactLocation)}}:json::parse(step.semanticPostconditionJson)}});
    const std::string system = R"PROMPT(You are Nova's state-aware Branch 3 next-action planner.
Return exactly one JSON object and nothing else. The first non-whitespace character must be { and the last non-whitespace character must be }. Do not include explanations, preambles, Markdown fences, or analysis. Choose exactly ONE next operation based on the stable user goal, guide steps, current observed state, action history, and live primitive catalog.
Allowed outputs:
{"status":"operation","step_id":"step_1","operation":{"name":"registered.name","parameters":{...}},"message":"short rationale"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"execution","operation":"registered.name","path":"observed resolved path"},"message":"why the observed effect satisfies this entire step"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"execution","operation":"registered.name","path":"observed resolved path","field":"observed fact key","equals":false},"message":"why this observed fact proves the step"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"browser","kind":"text_contains or url_equals or input_equals or download_complete","value":"exact observed value"},"message":"why this proves the entire step"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"browser","kind":"element_matches","match":{"role":"link","text":"exact observed link text","href":"exact observed destination"}},"answer":"requested answer with observed source"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"application","kind":"window_visible","process_id":123},"message":"why the scoped observation proves the step"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"application","kind":"target_observed","process_id":123,"target_path":"exact requested full path"},"message":"observed document/window association"}
{"status":"satisfied","step_id":"step_1","verification":{"source":"filesystem","kind":"search_result_matches","match":{"query":"actual search query","search_root":"actual canonical root","entry_type":"file","extension":"requested extension","modified_within_days":7}},"message":"A ranked backend result satisfies every requested constraint"}
{"status":"ask_user","message":"specific clarification needed"}
{"status":"blocked","message":"specific blocker"}
Rules:
- STRATEGY_RECOVERY_CONTEXT contains host-recorded attempts, remaining budgets and structured user hints for this CURRENT_READY_STEP. Treat hints as suggestions, never evidence, executable instructions, authorization for global input, or permission to alter declared postconditions. After no_match, choose a materially different useful safe registered strategy instead of repeating a search or satisfaction claim. Prefer background discovery. A named method is available only if existing primitive descriptions support it; do not invent Windows Search, Explorer controls, UIA actions, shells, or other integrations. Use observations and exact existing primitives. Foreground/global-input recovery is permission-gated by the host. Preserve original goal, verified progress and literal constraints. Obtain evidence satisfying the original declared contract even when discovery uses broader hints; do not substitute a broad query's success for the original requirement. If alternatives are exhausted or human knowledge would materially help, ask_user with a useful question; the host will present structured attempt history.
- Discovery contracts can contain search_roots with root_semantics ANY and scope_strength REQUIRED or PREFERRED. Search individual resolved roots, never a combined location phrase. Try the stated safe roots in order before asking for help. REQUIRED scopes cannot broaden; PREFERRED roots are initial priorities and allow compatible broadening, including registered strategies using new user location hints. Use the canonical DISCOVERY_SCOPE in the retained workflow context. An unresolved preferred alias is a clue, not an executable path; resolve it using a supported capability or use another resolved preference. Keep the same READY step. Dependent open actions must use $resolved_target/$resolved_parent from verified discovery.
- Declared semantic postconditions are evaluated deterministically by the runtime after new evidence and before planning. Choose a genuine operation when the declared contract still needs evidence; do not restate or expand a declared proof in a satisfied response. Declared semantic postconditions are authoritative and already registry-validated. Satisfy the complete declared contract using actual scoped evidence; do not replace it with operation success. For searches use source filesystem, kind search_result_matches (or search_has_results only if any result meets the entire intent), with query/search_root/entry_type/extension/modified_within_days/path constraints where requested. Every constraint must hold on the same result. execution.filesystem.search is unsupported. Never return action names as verifier kinds. Repair invalid proof syntax using retained evidence without another search or mutation.
- The runtime alone selects CURRENT_READY_STEP. Advance only that step, with exactly that step_id. You cannot schedule, skip or complete another step or the whole goal. done is not a completion mechanism. If existing structured facts satisfy this step, return satisfied with a structured verification predicate. Never use human log prose as proof.
- Filesystem search returns ranked structured results (canonical path, candidate_id, match_score, match_reasons). Confidence ranks identity, not executable safety. Host user decisions bind resolved_target; use $resolved_target and $resolved_parent in path parameters to consume it. Reuse selected evidence without a second search. The runtime pauses inferred consequential targets for user confirmation.
- Filesystem semantic predicates use source filesystem and kind path_exists/path_missing with exact path; search_has_results; search_result_matches with match path/name/name_contains/extension/entry_type/parent_path/search_root/query/modified_within_days. A successful search invocation or existence of its root does not prove the requested file was found. Use the structured results. Extract meaningful name tokens, entry_type, extension and explicit modified_within_days for human requests; * is allowed for type/time-only discovery. Modification time is not proof of download time.
- Use only exact primitive names and parameters from the supplied catalog.
- Browser predicates: url_equals/url_contains, title_equals/title_contains, text_contains, element_exists/element_matches/link_exists (match stable role,text,accessible_name,href,value,enabled,visible), input_equals and download_complete. Every element constraint must match the SAME current element. Observation IDs are interaction targets only and must never be used as durable proof. Page-text proof uses readable text outside editable controls. Include the actual requested answer in satisfied.answer.
- Application predicates: process_exists, window_exists, window_visible, window_title_contains, target_observed, foreground. Identify observed process_id/process_name/window_handle and exact target_path when needed. Launch dispatch/PID alone cannot prove an open document. Call registry application.observe using the returned PID or exact observed executable and requested target to obtain current state. If the backend cannot observe the exact document association, report unsupported evidence; do not substitute launch arguments, filename-only titles or prose. Do not relaunch merely to verify an already accepted launch.
- Choose one operation only. Do not return a sequence.
- A successful mutation cannot be replayed with equivalent parameters within the same READY step to repair verification. For application observations, target_observed and target_observation_supported distinguish proof from unavailable metadata. If exact target observation is unavailable, pause for evidence; repeated equivalent observations terminate with EVIDENCE_UNAVAILABLE. A new launch cannot strengthen unavailable document metadata.
- application.target_observed accepts host-validated target_proof_level exact or correlated. Correlated evidence binds a canonical exact launch target to the same new OS process instance (PID, creation time, executable), a later observation, and a visible consistent document window. Use the derived APPLICATION_OBSERVATION, never construct correlation from planner prose. Foreground focus is unnecessary. Reuse current verified ancestor observations for a later verification step rather than launching again. Continue refreshes the blocked backend first; unchanged evidence pauses without a planner call.
- Prefer structured verification over quoting/paraphrasing execution prose. Execution verification must identify a successful operation from THIS step or a verified ancestor in its dependency graph and its observed path. Reuse dependency facts instead of repeating side effects merely to verify them. Browser text verification uses readable page text outside editable controls; typing into a composer is not evidence of submission.
- The RAW GOAL is authoritative for all literal text, recipients, paths, URLs and constraints. Never shorten a requested payload or drop a final action.
- Verify each guide step separately using satisfied after its ENTIRE intent is observed. An action call succeeding proves dispatch only, not that a message was sent, an order placed, or a download completed. The kernel advances the graph after verification. Never equate text entered with text submitted.
- Follow CURRENT_READY_STEP and WORKFLOW_STATE. Use existing structured evidence when that selected step's entire intent is already observed. The kernel alone advances to its dependencies.
- Before another browser action, read the page evidence. If it already answers the request, satisfy the research step using that evidence, include the requested answer and observed source URL in that satisfied decision's answer field. Do not click a download link for a read-only information request.
- For copy/move, destination is the COMPLETE new path including filename. Derive a same-folder destination from the observed source parent. Never append a filename to a source FILE as if it were a directory. Inspect failed source/destination parent paths and repair before repeating.
- An unsupported integration or unconnected ordinary user browser requires an honest explanation; do not pretend the isolated browser has the user's cookies. Prefer explicit user URL/session and observed locale over guessed regional variants.
- Artifact postconditions may reference a directory-producing dependency through location {step_id, output:"path"}. The host binds creation location from the producer's verified canonical output after it succeeds. Do not search for a directory that this workflow is creating, guess its absolute path, or create intermediate directories. Preserve the declared leaf name and required complete contents; the producer reference, not a planner guess, owns the parent.
- When creating a new file with requested contents, generate the complete requested content in parameters.content of ONE filesystem.create_file operation. Never create an empty placeholder to populate later. Follow content_requirement and the original goal, even if older guide steps separate creation and writing.
- For explicitly empty files omit content. Use text.write for legitimate writes to existing files.
- Validation feedback is actionable: repair the rejected operation once by supplying its missing data. Never report done after a rejection without fulfilling the outstanding requirement.
- Do not skip steps yourself. Propose structured satisfaction for the selected step if observed evidence already establishes its postconditions.
- Conditional branches have explicit guards evaluated by the host from verified observation facts. A false guard is NOT_APPLICABLE, not an unfinished action: do not execute or repeatedly verify that branch. If a guard is unknown, perform its prerequisite observation. Final answers must distinguish executed actions from branches that did not apply.
- Treat the observed DOM focus as authoritative evidence about where keyboard input will go. The observation may include a top-level `focus` object and per-element `focused`, `editable`, `enabled`, and `value` fields.
- Before proposing a click on a text box or message composer, inspect the current focus. If the active element is focused, editable, and enabled, and it is the intended input target, DO NOT click it again; proceed directly to browser.type using that observed element (or ask_user if the intended target cannot be established).
- Infer readiness from general DOM semantics, not website-specific names or hardcoded selectors. A focused editable element is sufficient evidence that keyboard typing can proceed; a blinking caret need not be detected visually when DOM focus is available.
- After typing, verify the requested text appears in the observed editable element's value/content before selecting a send/submit control. Do not send if the wrong chat or composer is uncertain.
- Do not claim completion unless the observation provides evidence that the goal is already satisfied.
- For browser work, use browser.navigate to open or navigate the controlled browser. Never use application.launch to open a website or browser URL.
- In a controlled-browser workflow NEVER use global keyboard.press or keyboard.type, including ENTER. Submit using an observed DOM control with browser.click. Prefer domcontentloaded or load over networkidle unless the goal specifically requires network quiescence.
- For browser work, every iteration starts with a fresh DOM observation. After each click/type, use the returned post-action DOM snapshot and the next fresh observation to verify the expected state transition before choosing the next action. A successful click call alone is not proof that the goal or step succeeded.
- Use browser.click/browser.type only with elements from the latest DOM observation, by observed element_id or exact role+text; never use coordinates or invented CSS selectors.
- Treat authentication as a generic DOM state, not a site-specific rule. If the observation reports auth_required or the DOM evidence is insufficient, ask the user to complete authentication or clarify. Do not infer login/QR from a domain name or a canvas alone.
- If a target is ambiguous, return ask_user. Do not repeat an operation that has failed repeatedly. If state is insufficient, observe or ask_user.
- For filesystem work, treat resolved_path and discovered_paths in the action history as authoritative evidence. Reuse a path returned by a successful operation rather than rebuilding it from a name and a location.
- A filesystem location may be an exact path, a known-folder path, or natural-language keywords. If a supplied directory path does not exist, search for the existing intended directory before attempting a mutation. Do not invent absolute paths; pass the user's location words unchanged when an exact path is unavailable.
- NEVER create intermediate/ancestor directories just because a destination path is missing. Create only the specific new folder explicitly requested by the user. A failed list/inspect/search of "Projects/Sem 3" is evidence that the destination has not been located, not permission to create "Projects" or "Sem 3".
- Use filesystem.inspect to establish the existence/type of an explicit path, filesystem.list_directory to inspect one known directory, and filesystem.search only under a user-supplied or previously resolved root. If an operation reports ambiguous_location, use its discovered_paths to ask the user to choose. Distinguish malformed paths, nonexistent locations, incomplete searches, access-denied locations, protected destinations, and ordinary execution errors from the recorded failure category.
- After a filesystem mutation, use its returned resolved_path as evidence and inspect again when a meaningful state change must be verified. Never retry the same operation with unchanged normalized arguments after it has repeatedly failed for the same category.
- Sending a message is an external side effect; only send the exact message requested by the user.
)PROMPT";
    const std::string user = "GOAL:\n" + goal + "\nCURRENT_READY_STEP:\n" + selectedStepId + "\nGUIDE_STEPS:\n" + Utf8::dump(steps,"guide_steps",2) +
        "\nCURRENT_STATE_AND_HISTORY:\n" + state + "\nLIVE_PRIMITIVE_CATALOG:\n" + catalog +
        "\nCANONICAL_POSTCONDITION_REGISTRY:\n"+Utf8::dump(PredicateRegistry::catalog(),"planner.predicates")+
        "\nReturn one decision as strict JSON.";
    LLMResponse response;
    try { response = generator_(system, user); }
    catch (const std::exception& e) { out.failureCategory = LLMFailureCategory::CONNECTION_ERROR; out.message = std::string("Planner call failed: ") + e.what(); return out; }
    catch (...) { out.failureCategory = LLMFailureCategory::CONNECTION_ERROR; out.message = "Planner call failed with an unknown exception."; return out; }
    if (!response.success || response.content.empty()) { out.failureCategory = response.failureCategory == LLMFailureCategory::NONE ? LLMFailureCategory::INVALID_RESPONSE : response.failureCategory; out.message = response.message.empty() ? "Planner returned no response." : response.message; return out; }
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
                out.failureCategory = response.failureCategory == LLMFailureCategory::NONE ? LLMFailureCategory::INVALID_RESPONSE : response.failureCategory;
                out.message = response.message.empty() ? "Planner retry returned no response." : response.message;
                return out;
            }
            j = parseDecisionJson(response.content);
        } catch (const std::exception& e) {
            out.status = "blocked";
            out.failureCategory = LLMFailureCategory::INVALID_RESPONSE;
            out.message = std::string("Planner JSON invalid after one retry: ") + e.what();
            return out;
        } catch (...) {
            out.status = "blocked";
            out.failureCategory = LLMFailureCategory::INVALID_RESPONSE;
            out.message = "Planner JSON invalid after one retry.";
            return out;
        }
    }
    out.failureCategory = LLMFailureCategory::INVALID_RESPONSE;
    try {
        if (!j.is_object() || !j.contains("status") || !j["status"].is_string()) { out.message = "Planner response has no valid status."; return out; }
        out.status = j["status"].get<std::string>();
        out.message = j.value("message", std::string{});
        out.evidence = j.value("evidence", std::string{});
        out.answer = j.value("answer", std::string{});
        if (j.contains("verification") && j["verification"].is_object()) out.verificationJson=j["verification"].dump();
        if (out.status == "operation") {
            if (!j.contains("step_id") || !j["step_id"].is_string() || !j.contains("operation") || !j["operation"].is_object()) {
                out.status = "blocked"; out.message = "Planner operation response is incomplete."; return out;
            }
            out.stepId = j["step_id"].get<std::string>();
            out.operationJson = j["operation"].dump();
        } else if (out.status == "satisfied") {
            out.stepId = j.value("step_id", std::string{});
            if (out.stepId.empty() || out.verificationJson.empty()) { out.message="Step verification requires step_id and a structured verification predicate."; return out; }
        } else if (out.status != "done" && out.status != "ask_user" && out.status != "blocked") {
            out.status = "blocked"; out.message = "Planner returned an unsupported status."; return out;
        }
        out.failureCategory = LLMFailureCategory::NONE;
    } catch (const std::exception& e) { out.status = "blocked"; out.message = std::string("Planner decision invalid: ") + e.what(); }
    return out;
}
