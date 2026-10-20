# Nova state-aware Branch 3 — first implementation

## What changed

- Branch 3 now decomposes the goal once, then runs an observe → plan one operation → registry/schema validate → execute → observe loop.
- The next-action planner returns exactly one operation or `done`, `ask_user`, or `blocked`.
- `PrimitiveMapper::mapStep()` validates one proposed operation through the existing registry/schema parsing path; the legacy whole-plan `map()` remains available for compatibility but is no longer used by Branch 3 runtime.
- Added a State Manager with iteration count, bounded recent history, state logging, and repeated-failure tracking.
- Added `browser.observe`, `browser.click`, and `browser.type` primitives through the existing browser agent and deterministic execution engine.
- Browser observation returns the current URL/title, page-kind hint, bounded visible page text, and at most 40 visible interactive elements. Element IDs are transient and should be refreshed after every action.
- Branch 2 was not intentionally changed. Existing legacy Branch 3 files have not been deleted.

## Build on Windows / MSYS2 UCRT64

Run from the Nova project root in the UCRT64 terminal:

```bash
cmake -S . -B build
cmake --build build --target Nova Branch3RuntimeIntegrationTest PrimitiveMapperContractTest -j
```

Run the Python browser-agent validation tests using the same Python environment configured for Nova:

```bash
python -m unittest -v python/browser_agent_test.py
```

Then run the C++ integration test executable produced by CMake (the exact path may vary by generator, commonly `build/Branch3RuntimeIntegrationTest.exe`).

## First manual test

1. Start Nova using the same launch method you normally use.
2. Ask it to open WhatsApp Web in Edge, wait for it to load, open the Yash chat, type `hi`, and send it.
3. Watch `[STATE-AWARE LOOP LOG]`. Each iteration should show the observation, planner decision, and validation/execution result.
4. If a QR/login screen appears, complete login in Nova's controlled browser profile and retry/resume the request.
5. If multiple contacts match Yash, Nova should ask for clarification rather than guess.

## Important limits of this handoff

- The Python validation tests pass in the packaging environment.
- A full Windows C++ build could not be run in this Linux packaging environment because the project uses Win32 APIs and the environment does not have the project's configured `nlohmann_json` dependency installed. Please run the build commands above before treating this as verified.
- Browser automation uses Nova's dedicated Playwright profile, not an unrelated everyday Edge profile. First-time login may require a QR scan.
- This first implementation uses the controlled browser as the observation source. Broader native-window observation and UI Automation remain follow-up work.
