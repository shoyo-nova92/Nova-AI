# Workflow kernel contract

This milestone replaces Branch 3's independent scheduling, verification and failure ledgers. Existing UTF-8, filesystem, provider and browser fixes are retained. Live QA is paused until deterministic kernel and integration suites pass. No new user-facing capabilities are in scope.

## Model

- Workflow: a validated directed acyclic graph of stable, unique step IDs; a required flag identifies work necessary for completion.
- Step: intent, dependencies, optional Condition, expected Postconditions, StepStatus, evidence references and StepRecoveryState.
- Dependency: an edge to a prerequisite. A verified prerequisite is satisfied. A condition-false prerequisite is satisfied only as a valid skip; an unknown condition is never satisfied.
- Condition: a typed fact comparison tied to a producing ancestor step, with TRUE, FALSE or UNKNOWN evaluation. JSON null means absent. Unknown stays blocked; it never means false.
- StepStatus: PENDING, READY, RUNNING, VERIFIED, SKIPPED, RETRYABLE, BLOCKED or FAILED. Only VERIFIED and a justified SKIPPED are terminal success.
- Evidence: immutable ID, producing step, producing operation, typed category and structured JSON facts. Human-readable log text is not verification evidence. Current observations and historical effects remain distinct.
- Postcondition: a deterministic predicate over structured evidence. Artifact requirements match verified backend kind and target identity. Semantic steps require a structured evidence predicate; the LLM may interpret intent but cannot invent supporting facts or mark a step complete.
- RecoveryCause: typed runtime/provider category, stable backend code, message and retryability. Each step owns its cause and attempt count; successful verification clears that step's active blocker, retaining historical evidence.

## Invariants

1. Only the scheduler selects READY work. All required dependencies must be VERIFIED or validly SKIPPED before RUNNING.
2. A planner response cannot run, verify or skip another step. Mismatched IDs are rejected before execution.
3. Conditions query typed facts from verified producing steps. Unknown facts block applicability; false facts justify skipping.
4. VERIFIED requires PostconditionVerifier acceptance of FactStore evidence. Planner prose and planner done are never evidence.
5. FactStore references may cross the full transitive dependency closure, while unrelated and invalidated current-state evidence cannot prove a step.
6. A repaired step's valid verification removes its active recovery cause. Old failures cannot poison completion.
7. Completion is computed exclusively by the graph: every required step is VERIFIED or validly SKIPPED. No final LLM done is needed or authoritative.
8. Provider categories survive decomposition, planning, recovery and the user-facing result unchanged.
9. Registry names and schemas are the only external action vocabulary across routing, candidate-graph generation, planning, validation and dispatch.
10. Invalid references, duplicate IDs and cycles are rejected before effects. No READY work means complete or an exact blocked/failed dependency, never an invitation for the LLM to reschedule.
11. A continuation refreshes evidence and retries eligible work; it does not erase verified effects or silently replay completed mutations.
12. Browser dispatch alone proves neither submission nor a completed download. Semantic effects must be supported by typed observation/effect facts.

## Components and integration

WorkflowGraph validates and owns state. WorkflowScheduler evaluates dependencies and conditions and selects stable graph-order work. FactStore owns immutable structured evidence and ancestor-scoped queries. PostconditionVerifier evaluates predicates and commits verification. StepRecoveryState owns active typed causes. Branch 3 adapts backend results into evidence, requests a primitive for the selected step, validates it through the canonical registry, executes it and presents kernel outcomes.

## Validation gate

Run deterministic graph permutations and generated DAGs without network, voice, browser or desktop access. Include invalid graphs, three-valued conditions, non-ready planner attempts, transitive evidence, repairs, typed provider failures and completion without done. Then run existing regressions. Maintain 15 fixed acceptance scenarios with explicit offline adapter/live boundaries; a live-only test is skipped with a concrete reason rather than reported passing from a mock.

## Implementation map

- src/workflow_kernel.hpp and src/workflow_kernel.cpp: explicit model, DAG validation, deterministic scheduling, three-valued conditions, scoped FactStore, postcondition checks and step-local recovery.
- src/branch3_runtime.cpp: adapters and orchestration around Kernel; removed separate verification/skip sets, unresolved-operation map, local condition evaluator, local transitive verifier and planner-done completion logic.
- src/next_action_planner.hpp/.cpp: explicit runtime-selected step contract; structured satisfaction only; no prose-evidence or final-done instruction.
- src/goal_decomposer.hpp/.cpp: canonical registry context, nullable optional fields, bounded candidate-schema repair with structural diagnostics. The candidate remains subject to graph validation.
- src/primitive_execution_engine.cpp: structured inspect text/truncation facts and observed application activation/focus/window metadata. Accepted dispatch is distinguished from observed window/process state.
- src/runtime_outcome.hpp: explicit unsupported-capability, provider connection and configuration causes.
- src/workflow_kernel_test.cpp: generated DAGs and invariant cases without an LLM or platform effects.
- src/workflow_acceptance_test.cpp: the 15 fixed scenarios; frozen source checksum and boundaries in docs/workflow-acceptance.md.
- Existing runtime/reliability test sources: migrated obsolete final-done/prose fixtures to the new contract, preserving behavioral assertions; added nullable/schema/exception/provider/download-boundary cases.
- CMakeLists.txt: shared WorkflowKernel library and check-workflow gate.

## Evidence boundary

Artifact postconditions are fixed by the candidate's requested identity and backend-verified effect kind. Other intents carry an explicit INTERPRETED requirement: the model may propose semantic predicates, but cannot add facts, set statuses, choose another step or declare goal completion. The verifier checks those predicates against current, scoped structured facts. Natural-language interpretation remains model-dependent; the kernel does not claim to prove arbitrary semantic equivalence of a proposed predicate to every possible user request.

## Verified milestone results — 2026-10-28

Final executable: build-runtime-supervisor/Nova.exe, 20:48:44, 32,564,043 bytes. Deterministic gate: 5/5 suites passed; 250 generated DAGs / 29,439 checks; 15/15 fixed acceptance scenarios; existing runtime reliability 48/48, Branch 3 integration 27/27 and reliability regression 79/79. Native backend checks: filesystem manipulation 34/34, discovery pass, canonical registry 38/38, application lifecycle 12/12; local Edge/browser tests 16/16.

Live sign-off remains pending. Three frozen user prompts were attempted only after that gate: missing-file routing timed out, handover decomposition timed out, and browser navigation succeeded before a malformed planner response. Exact typed causes were retained. These are external provider blocks, not live passes. No source patch was added in response. Nova quit cleanly. Retained-graph retry was not run because the retry instruction arrived after clean exit; no retained state was invented. Daily-browser attachment, authentication-dependent flows and installer execution were not tested or expanded in this milestone. See logs.md and logs/qa-kernel-*.txt for exact prompts, timings and outcomes.

Additional existing offline contracts: decomposer15/15, mapper15/15, path policy5/5 passed. No deterministic test was skipped or left failing.

Update from subsequent user manual QA: conditional workflow, nine-step filesystem DAG and provider recovery passed live. Browser/application semantic evidence failures are addressed separately in docs/semantic-evidence.md; scheduling invariants remain frozen.
