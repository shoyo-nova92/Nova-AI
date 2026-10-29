# Interactive clarification and user-guided recovery

This milestone implements retained clarification, structured guidance, bounded recovery, and local recovery-episode recording. It does not implement learning, model updates, rules, or a strategy library.

## Runtime interaction

An active step keeps its operation attempts, typed outcomes, evidence references, search clues, hints, input mode, permission state, and remaining budget. Empty structured searches allow the planner to try materially different registered strategies before asking the user. Repeated equivalent filesystem observations and repeated execution failures also lead to guidance instead of an opaque planning-limit pause. Search identity fingerprints normalize query-token order, extension spelling, and location spelling for duplicate-attempt protection; search ranking itself is unchanged.

Default limits are three autonomous operations, eight operations per accepted guidance episode, 24 total recorded operations per step, three strategy questions, and eight retained hints. `setRecoveryPolicy` allows bounded configuration. Counts and history survive answers, dependency retries, and Continue; an answer does not replenish the lifetime budget.

`NEEDS_USER / STRATEGY_GUIDANCE` carries a `PendingUserDecision` with a stable question ID, current step, host-owned blocker, actual attempt history, useful information types, available primitive catalog, budget, and retained workflow snapshot. Questions are rendered deterministically from that data, including concise summaries of what actually ran. No model call is needed to invent a question or explain a blocker. Ordinary `MISSING_INFORMATION` questions also use a host-rendered question anchored to the current logical step.

The terminal accepts `[A] Answer` or direct free-form text for an active question. Existing evidence pauses also offer A (or direct text) to request a strategy question, while C retains its separate host-observation behavior. V abandons the old workflow and starts a new request; Q quits. Target ambiguity and target confirmation remain separate candidate decisions.

## Guidance interpretation

The configured answer interpreter receives the original goal, active question/reason, current step, actual attempts, evidence, primitive catalog, budget, and user answer. Its structured outputs are limited to:

- `ADD_SEARCH_HINT`: typed name fragment, query concept, extension, or modification-age hint.
- `ADD_LOCATION_HINT`: a bounded location clue.
- `SUGGEST_STRATEGY`: a short method identifier and a nonempty list of required registered primitives.
- `PROVIDE_INFORMATION`: bounded contextual information.
- `USER_CANNOT_CLARIFY`: keep the workflow paused without more planning.

The host validates action names, fields, types, ranges, lengths, and primitive availability. Additional executable-operation fields and unregistered required capabilities are rejected. A suggested method is planning context, not a new capability or an executable plan. The normal mapper, primitive validator, scheduler, target-confirmation gate, verifier, and mutation replay guard still own execution and truth.

Accepted hints enter the current step's strategy context. They do not enter FactStore as environmental proof and cannot rewrite a declared postcondition. A resumed planner may use the hint to select another supported method or obtain additional evidence. No root routing or goal decomposition runs merely because the user answered. Verified prerequisites, canonical targets, prior evidence, and successful-side-effect protection remain intact.

An interpreter transport/response failure retains both question and answer in memory with the typed provider cause. R retries that interpretation specifically, rather than restarting the workflow. A can replace the answer. Raw cached answers are bounded and never written to the recovery log.

## Foreground and global input

Recovery operations that can activate a native application, change focus, or send system-wide keyboard/mouse input pause with `USER_ACTION_REQUIRED`. A hint mentioning these primitives does not authorize input. The host requires an explicit affirmative answer to the permission question; an interpreter cannot invent permission from unrelated text. Consent is scoped to the current step and guidance generation, and resets on new guidance. Refusal executes no pending global-input operation and returns to strategy guidance.

The pending operation is retained and resumes through normal validation after permission. Ordinary controlled-browser DOM actions are still distinct from global keyboard/mouse input, and the existing controlled-browser keyboard prohibition remains intact.

## Recovery episodes

Production runs append a separate local journal at `logs/recovery-episodes.jsonl`. One rotated predecessor, `recovery-episodes.jsonl.1`, is retained. The writer caps each record at 16 KiB and each file at approximately 1 MiB. An unavailable log sink does not change verification or execution outcomes. Tests use injected in-memory sinks and test-owned temporary files.

Each episode has a stable ID, schema version, event sequence, step ID, goal class, typed initial problem, capability context, attempts before guidance, redacted guidance description, attempts after guidance, status, and observed goal result. Journal transitions distinguish:

- `USER_SUGGESTED`: valid guidance was received.
- `ATTEMPTED`: the method was tried, or retained evidence was evaluated after information resolved the question.
- `VERIFIED_SUCCESS`: the kernel deterministically verified the relevant step. This is never inferred from a suggestion, operation dispatch, candidate score, or planner prose alone.
- `FAILED`: guidance was unavailable, exhausted, declined, superseded, abandoned, or could not supply the required verification.

A successful recovery step can be recorded while later goal steps are pending. A later goal-completion event updates the same successful episode. Failed suggestions remain distinct from successful recoveries; there is no learned rule emitted from either.

Durable projections deliberately omit raw guidance, goal text, descriptions, paths, search queries, hint values, content, exception prose, full DOMs, screenshots, and conversation history. They retain registered operation names, validated hint types, input modes, typed outcomes, and verification status. The private projection functions and injectable sink form the redaction boundary. The runtime never reads these records back as authority or runtime memory.

## Verification and practical limits

The offline suite covers autonomous failures followed by guidance; factual question history; natural-answer interpretation; search/location/strategy hints; retained graph and verified steps; no new decomposition; registry rejection; separate target confirmation; canonical-path resume; success/failure journaling; proof-required success; retained-evidence recovery without replay; permission grant/refusal and stale/forged consent; malformed guidance; lifetime budgets; typed interpreter retry; optional guidance from evidence pauses; sink failure; raw-data exclusion; and bounded file rotation.

Validation uses ten offline suites under `check-workflow`, with native applications, browser sessions, and installers mocked. The production executable is built at `build-discovery-decision/Nova.exe`. No live QA was run.

The current registry has no dedicated Windows Search/index integration or generic UI Automation control-invocation primitive. A user can suggest such a method, but Nova must validate actual required primitives and choose a supported alternative or ask for another clue. A native UI method still needs scoped backend evidence before it can satisfy a postcondition; visible UI text or global-input dispatch does not create filesystem truth.

Guidance cannot silently relax literal goal constraints or verifier contracts. A broader discovery strategy must eventually obtain evidence that satisfies the original declared requirement. Explicitly changing the goal uses V. Strategies remain planner-proposed and bounded; this milestone adds no hardcoded application-specific fallback. Sessions and pending questions are retained in memory, not restored after a process restart.

## Files changed

- `src/strategy_recovery.hpp` — new bounded step context, input classification, privacy projections, and local writer.
- `src/recovery_guidance_test.cpp` — new mocked recovery/interaction/logging suite.
- `src/branch3_runtime.cpp`, `src/branch3_runtime.hpp` — retained recovery context, questions, interpretation, permissions, dependency retry, and episode lifecycle.
- `src/pending_user_decision.hpp`, `src/runtime_outcome.hpp` — structured context and clarification causes.
- `src/runtime_supervisor.cpp` — direct answers, optional guidance, and distinct dependency retry.
- `src/next_action_planner.cpp` — registry-grounded recovery and hint semantics.
- `src/runtime_reliability_test.cpp`, `src/postcondition_contract_test.cpp` — updated no-progress/empty-discovery expectations.
- `CMakeLists.txt` — offline test target and check dependency.
- `docs/interactive-recovery.md` — this report.

Kernel scheduling, FactStore immutability, search ranking, predicate truth, native application correlation, and provider model configuration were not changed by this milestone.
