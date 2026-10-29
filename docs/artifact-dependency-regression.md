# Artifact dependency and supervisor regression fix

## Root cause 1: future artifact treated as an existing location

Decomposition accepted the consumer's artifact location as a string, but the runtime's `workflowSteps` adapter immediately passed every artifact parent to `artifactParent`. A bare name such as `NovaRecoveryQA` therefore went through existing-location resolution before the producer step could run. The empty resolution threw while constructing the kernel, explaining why the old log labelled the failure as decomposition even though the decomposer had returned a valid logical plan. Filesystem creation and produced-artifact evidence were never reached.

Artifact locations now accept a typed producer output reference:

```json
{"step_id":"step_1","output":"path"}
```

The plan preserves it using the existing `ContextReferenceType::PREVIOUS_STEP_OUTPUT` representation. An unambiguous bare parent name matching a previously declared directory artifact is normalized to the same reference. An explicit dependency edge is retained or added. Unknown, forward/self, non-directory, and ambiguous parent references are rejected rather than guessed.

Graph construction records the logical parent reference and leaf name without resolving a future directory or inserting an existence fact. Once the producer is VERIFIED, binding reads the absolute path from the current immutable directory-artifact evidence that actually supported its verification. Both the consumer's creation parameter and its artifact postcondition resolve through that same verified output. Canonical root resolution and normal backend mutation authorization remain in force. Consumers cannot bind from dispatch success alone, a skipped/unverified producer, an unrelated step, or an arbitrary planner path.

This supports directory → populated file, directory → directory → populated file, and several files sharing one produced directory. The scheduler continues to own READY and dependency ordering. Runtime diagnostics now identify graph-construction failures separately from decomposition failures.

## Root cause 2: allocated session mistaken for actionable workflow

`hasActiveSession()` meant only that a session object existed. `start` allocated that object before decomposition/graph construction, and the supervisor used that allocation check to advertise A and C. Thus an INVALID_PLAN with no runnable graph displayed recovery actions that `requestGuidance` correctly could not perform.

The runtime now exposes separate control availability for a valid pending answer, a runnable retained guidance step, a scoped re-observation path, and a legitimate dependency retry. Rendering and key dispatch use those checks. Structurally invalid workflows offer V/Q, with no fake guidance question or Continue action. Existing provider failures can still use their existing dependency retry, and genuine strategy questions continue to offer A and accept direct free-form answers. Continue no longer restarts decomposition when no kernel was constructed.

Recovery budgets, guidance interpretation, recovery log schema, permissions, target confirmation, search ranking, and FactStore truth rules were not changed. The artifact postcondition representation was extended specifically to support verified producer-path binding.

## Offline validation

All 17 registered offline suites passed with `cmake --build build-runtime-supervisor --target check-workflow -j 4`:

- RuntimeReliability: PASS
- Branch3RuntimeIntegration: PASS
- ReliabilityRegression: PASS
- WorkflowKernel: PASS
- SemanticEvidence: PASS
- WorkflowAcceptance: PASS
- DiscoveryDecision: PASS
- PostconditionContract: PASS
- GoalDecomposerContract: PASS
- RecoveryGuidance: PASS
- ArtifactDependency: PASS
- PrimitiveMapperContract: PASS
- FilesystemPathPolicy: PASS
- FilesystemLocationResolver: PASS
- FilesystemDiscovery: PASS
- FilesystemCreationOffline: PASS
- FilesystemManipulationOffline: PASS

The new artifact suite verifies a nonexistent named parent, typed reference normalization, dependency insertion, canonical backend-output spelling, exact populated content, exactly-once creation, recursive chaining, shared parents, unverified-producer rejection, invalid reference/graph admission, no A/C for invalid plans, legitimate provider retry, and valid strategy-guidance interaction.

The two legacy filesystem mutation suites gained `--offline` mode so their positive mutations use unique temporary directories. Manual Desktop mode remains available. The creation suite's offline invalid-parent case uses an existing file as the invalid directory parent, avoiding broad missing-location discovery outside the fixture. Both FAILED and NOT_EXECUTED are accepted as valid rejection states; no-creation and preservation assertions remain. Unrelated live application, input-control, and live planning executables were not run.

## Targeted live Test 1

The production build `build-discovery-decision/Nova.exe` succeeded. Before the live run, `C:\Users\shour\Desktop\NovaRecoveryQA` did not exist.

The exact requested input was submitted through Nova's production keyboard → router → decomposer → runtime path:

```text
Create a folder called NovaRecoveryQA on my Desktop and create hello.txt inside it containing "hello nova".
```

On 2026-10-29 at 22:36:00 local time, Nova reported `COMPLETED / NONE`:

- step_1: one `filesystem.create`, SUCCESS, VERIFIED.
- step_2: one `filesystem.create_file`, SUCCESS, VERIFIED.
- The declared child parent was `step_1`; the dispatched child location was the producer's verified canonical Desktop directory path.
- No recovery question occurred. The completed menu showed V/Q.

Independent physical verification confirmed:

```text
C:\Users\shour\Desktop\NovaRecoveryQA\hello.txt
10 bytes, exactly: hello nova
```

Nova exited cleanly through Q. A fresh startup/quit smoke check also exited successfully. The folder and file remain in place. Tests 2–30 were not run.

Captured evidence:

- `build-discovery-decision/live-qa-test1.log`
- `build-discovery-decision/live-qa-test1-files.json`
- `build-discovery-decision/live-qa-restart.log`

## Files changed

- `src/artifact_binding.hpp` — verified producer-directory output binding (new).
- `src/artifact_dependency_test.cpp` — artifact and invalid-menu regressions (new).
- `src/multistep_plan.hpp`, `src/goal_decomposer.cpp` — typed producer references, safe normalization, and dependency validation.
- `src/predicate_registry.hpp` — artifact-location reference schema.
- `src/workflow_kernel.hpp`, `src/workflow_kernel.cpp` — deferred artifact-path metadata and verification; scheduler/FactStore semantics unchanged.
- `src/branch3_runtime.hpp`, `src/branch3_runtime.cpp` — runtime binding, creation identity checks, and control availability.
- `src/next_action_planner.cpp` — producer-reference context and guidance.
- `src/runtime_supervisor.cpp` — availability-based menus and dispatch.
- `src/filesystem_creation_group_test.cpp`, `src/filesystem_manipulation_group_test.cpp` — temporary-fixture offline mode.
- `CMakeLists.txt` — regression and offline-suite registration.
- `docs/artifact-dependency-regression.md` — this report.
