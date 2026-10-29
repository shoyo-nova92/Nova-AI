# Generic semantic evidence — engineering handover

The user's six manual tests and logs/runtime.log (20:59–21:26 on 2026-10-28) corroborate the scope: conditional workflow COMPLETED, nine-step handover COMPLETED after retry, browser/application steps stopped at unsupported structured evidence. Conversation and detailed intermediate recovery observations are from the supplied manual QA report; runtime.log does not retain every console/DOM event. These are user results, not new Codex live tests.

## Changes

- src/semantic_evidence.hpp/.cpp: stable browser/application evidence adapters, typed proof validation, semantic matchers, bounded missing-evidence diagnostics and separate planner presentation.
- src/workflow_kernel.hpp/.cpp: added semantic predicate dispatch and application dispatch/observation categories; invalidates superseded application observations. DAG validation, scheduling, dependencies, conditions, step transitions and completion algorithms remain unchanged.
- src/branch3_runtime.cpp: preserves full stable browser facts instead of using display compaction as the evidence store. Sends one bounded browser projection to the planner, without duplicating the DOM in the evidence ledger. Integrates application.observe evidence.
- src/application_observation.hpp: read-only, scoped Windows process/window/UI Automation observation. No focusing, launching or closing as part of observation. Accessibility connections/transactions use bounded timeouts and unsupported providers remain unavailable.
- src/primitive_execution_engine.cpp: canonical application.observe dispatch; launch records requested/resolved targets, actual OS-returned process IDs where available and launch mechanism. ShellExecuteEx keeps the dispatch/PID distinction without claiming an open document.
- src/primitive_operation.hpp, src/primitive_parameters.hpp, src/primitive_mapper.cpp, src/primitive_registry.cpp: canonical application.observe schema and process/window parameter storage. No command grammar or app aliases.
- src/browser_state.hpp and python/browser_agent.py: preserve/emit observed visibility alongside existing link/text/accessibility fields.
- src/next_action_planner.cpp: documented structured semantic proof shapes and explicit dispatch-versus-document guidance. Provider/model selection, deadlines, fallback and telemetry are unchanged.
- src/semantic_evidence_test.cpp, src/reliability_regression_test.cpp, src/primitive_registry_test.cpp and CMakeLists.txt: offline coverage and gate wiring. The fixed 15-scenario acceptance source/checksum is unchanged.

## Evidence and predicates

Application verification pauses with `EVIDENCE_UNAVAILABLE` when a scoped window observation supplies neither direct exact proof nor sufficient launch/process correlation, or when two equivalent observations produce no new state. Observation success remains distinct from process/window existence and target proof. Structured observations include `process_exists`, `window_exists`, `window_visible`, `target_observed`, `target_observation_supported`, `target_proof_level` (`exact`, `correlated`, or `unavailable`), and a reason when the target is unavailable. The support flag describes the current evidence, not a permanent application capability. An empty window scope is allowed another observation to accommodate startup/delegated activation.

Successful mutations are remembered per workflow step and equivalent operations are blocked before execution; verification retries cannot replay them. The guard survives Continue and provider retries. Failed operations remain repairable, and separate workflow steps retain independent action histories. Continue refreshes the last scoped application observation before the planner. An unchanged semantic fingerprint immediately retains `EVIDENCE_UNAVAILABLE` without any LLM call; timestamp, diagnostic, and foreground-only changes do not count as changed target evidence. Failed refreshes retain the original evidence blocker. New relevant evidence allows planning to resume. Provider Retry remains a separate dependency path. Kernel scheduling and completion rules are unchanged. Regression coverage uses injected executors and planners; no live applications are launched.

New categories: APPLICATION_DISPATCH and APPLICATION_OBSERVATION. Browser observations remain BROWSER_OBSERVATION; historical action effects remain distinct. Legacy APPLICATION_EFFECT is retained for compatibility and cannot prove new application observation predicates.

Browser predicates: url_equals, url_contains, title_equals, title_contains, text_contains, element_exists, element_matches and link_exists. Existing input_equals and download_complete remain available. A dotted kind such as browser.link_exists is also accepted. Element constraints use role, text, accessible_name, href, value, enabled and visible; all constraints must match the same element. Generated element IDs are stripped from stored semantic facts and rejected as proof, while bounded planner interaction context retains immediate-action IDs.

Application predicates: process_exists, window_exists, window_visible, window_title_contains, target_observed and foreground. They require scoped observed process/window/target identity and APPLICATION_OBSERVATION evidence. Dispatch, launch arguments, a PID or a basename-only title cannot prove an open document.

Example proof shapes:

```json
{"source":"browser","kind":"element_matches","match":{"role":"link","text":"Observed release","href":"https://reference.example.test/release"}}
```

```json
{"source":"application","kind":"target_observed","process_id":42,"target_path":"C:\\work\\review.txt"}
```

New browser observations invalidate older browser observations. Fresh application observations and successful application dispatches invalidate older application observations. Verification still applies currentness and producer/dependency scoping before semantic matching. Planner prose is never evidence.

FactStore retains exact backend facts, including elements beyond the planner's presentation limit. Planner projection is bounded to 10 KB, with relevant text, stable element semantics, focus, immediate interaction IDs and browser effects where they fit. The ledger refers to this projection rather than repeating complete browser facts. Missing/unsupported proof diagnostics are bounded and identify the missing observed relationship.

## Limits and manual handover

Direct `exact` proof requires a scoped window title or document/tab accessibility Name/HelpText exposing the exact full path, canonically matching the requested target. The backend records the actual observed path and source, and the verifier checks both.

`correlated` proof requires a successful exact canonical requested/resolved target dispatch, an OS-returned PID, matching process creation time and executable image path, a process created during that dispatch, matching runtime session and launch identity, a subsequent observation, and a visible window with a consistent whole document label. Window labels may contain a generic application suffix; substring matches are insufficient. Duplicate matching windows and conflicting target dispatches to the same instance are rejected. Foreground focus is unnecessary. The runtime derives this evidence only from the same step or verified ancestors, before adding the new observation to FactStore. The observation retains the supporting dispatch evidence ID and context; the original dispatch and observations remain separate immutable records. The semantic matcher rechecks the typed correlation conditions. Generic `application.target_observed` accepts either proof level and retains its diagnostic level.

Reused processes, absent/unqueryable process identity, delegated activations without a securely bound window, and insufficient document identity remain `unavailable`. No application-specific aliases or filename exceptions are used. UI Automation document text/value, launch command lines and filename-only matches are not substitutes. Observations are point-in-time snapshots, not a guarantee about unobserved later changes.

Restart Nova's Python browser-agent process before manual browser retesting if an older service is still running: an already-running python/browser_agent.py retains its imported observer code. No existing service/browser was stopped or operated by Codex for this patch.

## Validation

Build: build-runtime-supervisor/Nova.exe, 2026-10-28 21:46:59, 35,684,043 bytes. Six CTest suites passed: 250 generated DAGs/29,439 kernel checks; unchanged 15/15 fixed acceptance scenarios; runtime reliability48/48; Branch3 integration27/27; reliability regression83/83; semantic evidence40/40. Canonical registry39/39 (27 primitives); nine pure Python validation tests passed; browser_agent.py syntax compiled.

All validation was offline: fake browser/application adapters, real temporary filesystem fixtures and pure validators. Native process/window/UI Automation code was compiled and linked, not executed against daily applications. Live prompts, public sites, messaging accounts, native application lifecycle tests and Playwright browser integration were deliberately not run per the brief. No live PASS is claimed. The user performs manual acceptance on this build.

Outputs: logs/semantic-final-build-and-tests.log, logs/semantic-final-unit.log, logs/semantic-registry.log and logs/semantic-python-unit.log.
