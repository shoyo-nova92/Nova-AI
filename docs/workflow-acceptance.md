# Fixed workflow acceptance scenarios

Frozen acceptance source: src/workflow_acceptance_test.cpp
SHA256: d1a1d97fed66f4bc6ef3237b777fe393be9bdc5821a9db73e0bf90db16dd4e61

Run every architecture change with: cmake --build build-runtime-supervisor --target Nova check-workflow --parallel 2

The gate runs kernel tests, the fixed 15 scenarios and the existing runtime suites. No OpenRouter, voice, public website or desktop state is used by the deterministic gate. Filesystem scenarios perform real operations in a fresh temporary fixture; browser/application scenarios use injected typed adapters and are explicitly labeled simulations. Separate Python browser tests exercise installed Edge on local pages and native application lifecycle tests exercise Windows.

01. Create a populated file with exact readback.
02. Create directory and dependent file.
03. Inspect an existing file.
04. Missing-file conditional, including JSON null optional metadata.
05. Create, copy, rename and verify via transitive evidence.
06. Multiple prerequisite join and host-selected READY step.
07. Failed primitive followed by successful repair.
08. Planner timeout, retained cause and recovery without replay.
09. Browser navigation followed by answer extraction.
10. Browser input and verified submission; draft text is insufficient.
11. Explicit download completion evidence.
12. Application launch with typed process evidence.
13. Ambiguous request asks for clarification without effects.
14. False conditional branch is validly skipped without execution.
15. Exact Unicode/literal payload preservation.

Live frozen prompts are recorded separately in logs.md. Provider failures are recorded as unavailable, never converted into simulated passes. Authentication, daily-profile attachment and installer execution are outside this milestone. A changed acceptance source requires an explicit reviewed reason; do not weaken expectations to accommodate a runtime defect.
