# QA_Tester — Nova hardtest log

Session date: 2026-10-25 (Asia/Calcutta). Target: 10 realistic task runs, from simple to complex. Existing source changes preserved.

## Environment and startup
- Project: `C:\Users\shour\Desktop\Nova`.
- Executable: `build-runtime-supervisor/Nova.exe` (existing build).
- Actual interactive ConPTY terminal launched through `C:\msys64\usr\bin\bash.exe`, with `MSYSTEM=UCRT64`, PATH `/ucrt64/bin:/usr/bin:/c/Windows/System32:/c/Windows`, working directory `/c/Users/shour/Desktop/Nova`.
- Tool's direct shell selection did not honor Bash syntax; explicit bash executable launch works. First explicit launch omitted Windows System32 from PATH, causing `cmd.exe` lookup failure; corrected harness configuration and quit that launch. Neither setup attempt counts as a task run.
- Live Nova startup: `[BRIDGE] Python process started`, `[CUDA] Runtime DLL paths configured`, `[INIT] OpenWakeWord: READY`, `[INIT] Loading Whisper Large-v3...`.
- V is handled by Python's real `msvcrt` console keyboard reader. No mocks or runtime source edits used.

## Run 01 — Greeting and capability explanation
- Input: `Hi Nova! Tell me in one sentence what you can help me do on this computer.`
- Expected: One helpful sentence, no side effects.
- Actual normalized input: `Tell me in one sentence what you can help me do on this computer.`
- Actual response: `I can answer questions, write and debug code, analyze files and data, automate repetitive tasks, summarize documents, brainstorm ideas, and troubleshoot technical issues—all through natural conversation.`
- Runtime: `Status: COMPLETED`, `Cause: NONE`, `Reason: LLM response received.`
- Verification: Response is one sentence and relevant. No file mutation requested.
- Verdict: PASS.
- Input-path observation: It takes TWO V presses: one bypasses wake word and starts LISTENING; a second skips speech and emits KEYBOARD_REQUEST. Models load before either key is processed (~60 seconds on first launch).

### Repeated input startup observation
Choosing V for a new request starts another Python Part A process and reloads both speech models before keyboard input. Second load started 23:53:31; output subsequently reached Whisper READY, real V bypass, real V keyboard override, and `[NOVA INPUT]`. This creates substantial latency for a keyboard-only task. Exact second-ready timestamp was not captured; do not infer elapsed runtime from the interruption between tool calls. Run 02 prompt submitted only after keyboard readiness. Session continued across a user/tool turn interruption; no prompt was lost or silently counted.

## Run 02 — Real-world bill arithmetic
- Input: `I am splitting a restaurant bill of 1840 rupees between 4 friends with a 10 percent tip. What should each person pay? Show the arithmetic.`
- Expected: Tip 184, total 2024, per person 506 rupees, with explanation.
- Actual: Returned tip `1840 x 0.10 = 184`, total `1840 + 184 = 2024`, per person `2024 / 4 = 506`, and final answer 506 rupees.
- Runtime: `Status: COMPLETED`, `Cause: NONE`, `Reason: LLM response received.`
- Independent verification: (1840 * 1.10) / 4 = 506.
- Verdict: PASS. Terminal prints raw Markdown/LaTeX; answer remains readable.

### Environment adjustment after Run 02
Quit Nova cleanly and restarted same binary with `HF_HUB_OFFLINE=1` and `TRANSFORMERS_OFFLINE=1` to use existing cached speech models and avoid online model metadata/download checks. OPENROUTER remains live; this does not mock LLM responses, skip voice initialization, or alter project code. All following requests still use actual V/V keyboard bypass.

### Keyboard-only startup latency issue (QA-001)
- Reproduction: Finish a task, choose V for new request, then V/V to request typed input.
- Observed: Every request starts a new Python process and loads OpenWakeWord + CUDA Whisper Large-v3 before it checks keyboard bypass. Restart with cached-model offline settings began 2026-10-26 00:01:54 and still printed only `Loading Whisper Large-v3...` at 00:03:51 (at least 117 seconds before keyboard readiness).
- Resource context: At 00:03:35, Windows reported 381668 KB free physical RAM of 24785960 KB; model process working set fluctuated from ~650MB to ~1.4GB. Resource pressure may contribute; causal attribution is unconfirmed.
- Impact: Simple typed tasks are blocked on heavyweight voice initialization even when voice will be bypassed.
- Severity: Medium usability/performance; reproducible architecture behavior. No fabricated task verdicts; these waits do not count toward 10 inputs.

## Run 03 — Create a named project folder
- Input: `Create a folder named QA_Test_20261026 inside C:\Users\shour\Desktop\Nova.`
- Expected: Create that empty directory.
- Actual normalized input preserved the request. Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Reason: Unable to determine requested action.`
- Independent verification: `Test-Path` for requested directory = False; no folder created.
- Verdict: FAIL (QA-002). A clear single directory creation request was not mapped to an action. Absolute location and exact name were supplied. No clarification question was asked.
- Reproduction: Submit the exact input above through keyboard; inspect directory existence.
- Severity: High core functionality failure for this request; root routing/parser diagnosis remains unconfirmed.

### QA-002 source evidence
Read-only inspection of `src/action_parser.cpp` shows case-sensitive `input.find("create a folder")` / `input.find("make a folder")`. Run 03 normalized input begins uppercase `Create a folder`, which does not match. This explains the observed error in the source snapshot (existing binary behavior is consistent). Normal sentence capitalization causes a supported single action to fail.

## Run 04 — Dependent folder creation and exact-text note
- Input: `Create a folder called QA_Test_20261026 inside C:\Users\shour\Desktop\Nova, then create Notes.txt inside that folder containing exactly "Meeting: Friday 10:30 AM; Budget = Rs. 1250; Owner: Asha.".`
- Expected: Create folder and file, preserving exact requested case, punctuation, time, and amount.
- Actual: OpenRouter HTTP 429 before normalization; error `Rate limit exceeded: free-models-per-day`; limit=50, remaining=0, reset Unix-ms=1792972800000. Personal provider identifiers omitted from this log.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Independent verification: No requested directory or note created (prior run already confirmed folder absent; failed before any action stage).
- Verdict: BLOCKED by external daily provider quota. Task functionality cannot be evaluated from this run.
- QA-003: Although provider response explicitly identifies rate-limit, top-level result reports `Cause: UNKNOWN`; recovery does not expose reset or actionable retry timing. Severity: Medium diagnostics/recovery.
- Startup latency: New request began approximately 00:05:45; input accepted 00:10:35. Cached offline model loading still costs several minutes before keyboard-only requests.

Provider reset converts to 2026-10-26 05:30:00 Asia/Calcutta. Continuing the requested 10 actual submitted task prompts; subsequent provider-blocked runs are explicitly BLOCKED, never task passes/fails. No credits, account settings, or models changed.

## Run 05 — Scoped file discovery and missing-directory behavior
- Input: `List the files in C:\Users\shour\Desktop\Nova\QA_Test_20261026 and find any text file whose name contains Notes. If the folder is missing, tell me clearly rather than searching elsewhere.`
- Expected: Confine discovery to explicit directory; report that it is missing, without inventing results or searching unrelated locations.
- Actual accepted: 2026-10-26 00:18:41. OpenRouter returned HTTP 429 `Rate limit exceeded: free-models-per-day`, remaining=0, reset=05:30 IST.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Verification: No normalization, planner, filesystem execution, or discovery output occurred; therefore missing-directory behavior was not reached. Requested QA directory remains absent.
- Verdict: BLOCKED (provider quota); no claim about discovery correctness. QA-003 repeats.
- Input startup took approximately 7 minutes before actual V/V keyboard readiness, worsening QA-001. GPU was idle (141/8151 MiB, 0%) while host model working set grew to ~3GB; underlying cause not proven.

## Run 06 — Multi-step copy/rename with dependencies
- Input: `Inside C:\Users\shour\Desktop\Nova, create a folder named QA_Workflow_20261026, create draft.txt there with exactly "Project Phoenix - Review v1", copy it to backup.txt, then rename draft.txt to final.txt. Keep the backup and show me the final filenames.`
- Expected: Ordered creation, exact content in final.txt and backup.txt, draft.txt absent, final names reported.
- Actual accepted: 2026-10-26 00:19:39. HTTP 429 `Rate limit exceeded: free-models-per-day`, remaining=0; same provider reset.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Independent verification: Requested workflow directory does not exist; no filesystem steps executed.
- Verdict: BLOCKED (provider quota). Multi-step ordering/copy/rename correctness remains untested. QA-003 repeats.
- Startup improved to under a minute this time, demonstrating latency varies greatly; cause unconfirmed.

## Run 07 — Missing-file error handling without accidental creation
- Input: `Open C:\Users\shour\Desktop\Nova\QA_MISSING_20261026.txt in Notepad. If it does not exist, tell me it is missing and do not create an empty replacement.`
- Expected: Explain missing file; do not launch a misleading blank replacement or create a new file.
- Actual accepted: 2026-10-26 00:20:06. HTTP 429 `Rate limit exceeded: free-models-per-day`, remaining=0.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Independent verification: Requested path remains nonexistent. Execution failed before application/file handling, so graceful missing-file behavior cannot be assessed.
- Verdict: BLOCKED (provider quota). QA-003 repeats; absence of unwanted file is observed, not evidence that the requested handler works.

## Run 08 — Ambiguous natural follow-up
- Input: `Move that file into the project folder and give it a better name.`
- Expected: Ask which source file, destination folder, and desired filename are meant; avoid guessing or moving unrelated data. Previous requested file does not exist, so it is not a valid actionable referent.
- Actual accepted: 2026-10-26 00:20:29. HTTP 429 `Rate limit exceeded: free-models-per-day`, remaining=0.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Verification: No normalized request, routing, move primitive, or clarification output occurred; no execution stage reached.
- Verdict: BLOCKED (provider quota). Ambiguity handling remains unassessed. QA-003 repeats.

## Run 09 — Read-only official-site browser research
- Input: `Open the official Python website in the browser, find the current stable Python release, then tell me its version number and the official download page link. Do not download or install anything.`
- Expected: Browse official public Python pages, identify stable release from observed evidence, return version + source link without download/install.
- Actual accepted: 2026-10-26 00:20:56. HTTP 429 `Rate limit exceeded: free-models-per-day`, remaining=0.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Verification: No browser action or research response emitted; task blocked before planning. Current Python version was neither guessed nor independently substituted for Nova's result.
- Verdict: BLOCKED (provider quota). Browser functionality unassessed. QA-003 repeats.

## Run 10 — Complex handover package with exact content, copies, and manifest
- Input: `Set up a small client handover package in C:\Users\shour\Desktop\Nova\QA_Handover_20261026: create Docs and Archive subfolders, put a README.txt in Docs with exactly "Client: Acme; Status: REVIEW; Contact: Asha", copy README.txt into Archive as README-backup.txt, and create manifest.txt in the package root listing those two relative file paths on separate lines. Verify both copies have identical contents and report anything you could not complete. Do not overwrite any existing files.`
- Expected: Create root + two subfolders, exact README content, matching backup, two-line relative-path manifest, independent verification and honest completion report, no overwrite.
- Actual accepted: 2026-10-26 00:21:21. HTTP 429 `Rate limit exceeded: free-models-per-day`, remaining=0.
- Runtime: `Status: FAILED`, `Cause: UNKNOWN`, `Blocked at: Input normalization`, `Reason: Input normalization LLM failed: OpenRouter returned HTTP status 429.`
- Independent verification: Requested package root does not exist; no files created. Planner and filesystem execution were never reached.
- Verdict: BLOCKED (provider quota). Complex workflow behavior unassessed. QA-003 repeats.

## Session result
Exactly 10 distinct realistic user-task prompts were ACTUALLY entered into the real Nova executable in an interactive UCRT64 Bash/Windows ConPTY session; each used real V/V voice bypass. No mocks, unit-test substitutes, or runtime source changes.

- PASS: 2 (runs 01–02).
- FAIL: 1 (run 03, uppercase single-action parsing).
- BLOCKED: 7 (runs 04–10, provider daily quota exhausted).
- Product findings: QA-001 heavyweight repeated voice-model initialization before typed input; QA-002 case-sensitive create-folder parser; QA-003 top-level UNKNOWN cause for explicit provider rate-limit and limited recovery information.
- External limitation: OpenRouter daily free-model quota 50, remaining 0, reset 2026-10-26 05:30 IST. No credits purchased, model changed, or reset awaited.
- Functional coverage limitation: Later diverse inputs tested input acceptance and provider-failure behavior; their filesystem/browser/ambiguity workflows did NOT execute and must be retested when quota is available.
- Stop condition: 10 actual runs reached; no recurring watcher or timer remains armed. Nova quit with Q after the final result.

# Retry session — 2026-10-26 evening (Asia/Calcutta)
User reports quota restored and authorizes retrying previously blocked runs 04–10. Original results remain above. Preflight 21:45:01: no Nova/Python process active; QA_Test_20261026, QA_Workflow_20261026, QA_Handover_20261026, QA_MISSING_20261026.txt all absent. Same existing executable and real UCRT64 ConPTY V/V input path; cached speech-model offline environment retained. No provider/model/account changes.

## Retry 04 — Dependent folder and exact-text note
- Exact original input: `Create a folder called QA_Test_20261026 inside C:\Users\shour\Desktop\Nova, then create Notes.txt inside that folder containing exactly "Meeting: Friday 10:30 AM; Budget = Rs. 1250; Owner: Asha.".`
- Accepted 2026-10-26 21:45:46; completed 21:46:17. Quota is available: normalization, planning, and real execution ran.
- Observed: `filesystem.create : SUCCESS`, `filesystem.create_file : SUCCESS`; planner content present=true bytes=57; runtime `Status: COMPLETED`, `Cause: NONE`, `Reason: All requested postconditions verified.`
- Independent filesystem verification: QA_Test_20261026 directory exists. Notes.txt content is exactly `Meeting: Friday 10:30 AM; Budget = Rs. 1250; Owner: Asha.` (57 characters), case-sensitive equality=True, no trailing newline or extra quotation marks.
- Verdict: PASS. Real output satisfies exact content and dependent creation requirements. Original blocked result remains historical.

## Retry 05 — Scoped list/search
- Exact original input: `List the files in C:\Users\shour\Desktop\Nova\QA_Test_20261026 and find any text file whose name contains Notes. If the folder is missing, tell me clearly rather than searching elsewhere.`
- Accepted 2026-10-26 21:47:24. Prerequisite differs from first session: Retry04 successfully created directory + Notes.txt, so expected list/search now includes Notes.txt.
- Actual normalized text preserved the full request. Runtime `Status: FAILED`, `Cause: UNKNOWN`, `Reason: Unable to determine requested action.` No discovery output or planner execution emitted.
- Independent filesystem verification: Directory exists and contains exactly Notes.txt (57 bytes). Requested target is concrete and available.
- Verdict: FAIL (QA-004). Clear supported-catalog list/search request cannot be executed through root routing/single-action path; user gets generic action parsing failure instead of listing/matching existing file. Exact router classification is not printed, so routing attribution is inferred from parser error; no source changes made.
- Severity: High for file discovery functionality. Reproduce with exact input after creating target fixture as Retry04.

## Retry 06 — Multi-step copy/rename (in progress)
- Exact original input: `Inside C:\Users\shour\Desktop\Nova, create a folder named QA_Workflow_20261026, create draft.txt there with exactly "Project Phoenix - Review v1", copy it to backup.txt, then rename draft.txt to final.txt. Keep the backup and show me the final filenames.`
- Accepted 2026-10-26 21:48:32; normalization preserved content. First attempt ended 21:49:40 with `Status: FAILED`, `Cause: INVALID_PLAN`, `Blocked at: Goal decomposition`, `Reason: Goal decomposition failed: OpenRouter request failed: Timeout was reached`.
- No requested directory created. External provider timeout occurred before execution, so task verdict pending recovery.
- QA-005 diagnostic issue: Transport timeout is labeled INVALID_PLAN; this is not evidence of invalid user goal or invalid returned plan.
- Pressing offered R once to test recovery of the same authorized task, without retyping or changing prompt.

### Retry 06 recovery outcome and continuation
- Offered R recovered from provider timeout: created QA_Workflow_20261026 and draft.txt, then stopped 21:52:36 with `Status: PARTIAL_SUCCESS`, `Cause: ITERATION_LIMIT`, `Reason: Reached the bounded limit of 12 iterations. Continue to re-observe and resume the same goal.`
- Verified progress printed step_1 folder, step_2 file. Last issue: `filesystem.copy [target_not_found]: Copy destination parent does not exist.` Blocked at `step_3: Copy draft.txt to backup.txt in the same folder`.
- Independent directory listing contains only draft.txt (27 bytes), although the requested same parent directory exists. No backup.txt or final.txt.
- Exact content check: actual JSON string `"Project Phoenix - Review v1"`, length=27, expected length=27, case-sensitive exact equality=True. No extra newline. File content matches exactly.
- QA-006: Copy workflow stalled to iteration limit reporting missing parent for a same-directory copy after successfully creating that directory; operation parameters are not printed in resume output, so whether planner arguments or path resolution caused it is unconfirmed.
- Pressing offered C once to test state-preserving recovery; this is continuation of same task.

### Retry 06 final verdict
- C continuation ended 21:54:08: `Status: NEEDS_USER`, `Cause: UNKNOWN`, `Reason: The same operation failed three times; user intervention required.` Same `filesystem.copy [target_not_found]: Copy destination parent does not exist.`
- Final independent state: QA_Workflow_20261026 contains draft.txt only, exact content correct; backup.txt and final.txt absent. No filenames summary generated.
- Verdict: FAIL / partial completion (QA-006), after one R for provider timeout and one C for execution recovery. Runtime honestly did not claim full completion and retained folder/file progress; recovery could not complete same-directory copy/rename. Moving to next original test rather than indefinitely retrying unchanged failure.

## Retry 07 — Missing-file request
- Exact original input: `Open C:\Users\shour\Desktop\Nova\QA_MISSING_20261026.txt in Notepad. If it does not exist, tell me it is missing and do not create an empty replacement.`
- Accepted 2026-10-26 21:55:18. Normalized text preserved the request.
- Actual: `Status: FAILED`, `Cause: UNKNOWN`, `Reason: Unable to determine requested action.` No missing-file explanation or application execution.
- Independent verification: QA_MISSING_20261026.txt remains absent. No unwanted replacement created, but filesystem check/error handling did not execute.
- Verdict: FAIL for graceful missing-file handling; generic parser failure instead of meaningful missing-file report. Consistent with QA-002 case-sensitive action parsing (`Open` versus lowercase parser matches), not a new independently isolated path-policy defect.

## Retry 08 — Ambiguous natural follow-up
- Exact original input: `Move that file into the project folder and give it a better name.`
- Accepted 2026-10-26 22:04:19. Normalized text unchanged.
- Actual: `Status: NEEDS_USER`, `Cause: UNKNOWN`, `Reason: Please clarify the request in a new input.` No move/rename operation executed.
- Expected: Ask for source/destination/name rather than guessing. Nova correctly pauses for clarification; question is generic instead of identifying missing fields.
- Verification: No execution/planner mutation output. Prior QA_Workflow draft and QA_Test note remain in place; nonexistent QA_MISSING target remains absent.
- Verdict: PASS for safe ambiguity handling, with usability observation that clarification could name required details.

## Retry 09 — Official-site browser research
- Exact original input: `Open the official Python website in the browser, find the current stable Python release, then tell me its version number and the official download page link. Do not download or install anything.`
- Accepted 2026-10-26 22:04:59; paused 22:07:25.
- Real browser actions: navigate python.org SUCCESS; click Downloads SUCCESS; attempted version/download link FAILED stale_element; runtime re-observed; three subsequent clicks labeled Python 3.15.2 SUCCESS but did not achieve meaningful transition.
- Final: `Status: NEEDS_USER`, `Cause: NO_STATE_CHANGE`, `Reason: Browser actions produced no meaningful state change twice.` Last error retained old stale-element message; no final version + download-page answer given to user.
- Independent read-only observation via Nova's local browser service AFTER pause: actual URL https://www.python.org/downloads/, title Download Python | Python.org, DOM ready complete, page_kind content, auth_required=False. Visible page explicitly states standalone installer for Python 3.15.2 and lists Python 3.15.2 Oct.25,2026. Required version evidence was available on the page.
- No new file appeared in normal Downloads directory during task. This does not establish whether any temporary browser download started; no installation was requested or observed.
- Verdict: FAIL / partial browser progress (QA-007). Browser reaches relevant official evidence but loops clicking release labels until no-state-change guard, then asks user instead of reporting available version + source page link. Stale-element recovery did re-observe and continue safely; it did not complete the task.
- Observation limitation: Local observer does not include element hrefs; exact clicked destinations are absent from console output. We did not replace Nova's answer with independent research or fabricate a release URL.

Actual file was actually downloaded 3 times in my drive. it just did nto see it.

## Retry session interrupted for analysis
User changed direction to analysis before Retry10 was submitted. Six original prompts (04–09) were actually retried; Retry10 was NOT entered. Final V/V startup had reached keyboard input (no Part A Python process remained) and QA-owned Nova accepted q, stopping cleanly. No additional task was triggered.

Retry outcomes: 04 PASS; 05 FAIL generic action parsing; 06 FAIL/partial copy/rename after provider-timeout recovery; 07 FAIL generic action parsing; 08 PASS safe clarification; 09 FAIL/partial browser loop. Two passes and four functional failures/partial failures among six retries; no quota block in this retry session. Original Run10 remains provider-blocked and un-retried.

Artifacts left for inspection: QA_Test_20261026/Notes.txt exact requested57chars; QA_Workflow_20261026/draft.txt exact requested27chars. Backup/final files absent; QA_MISSING_20261026.txt absent. No source modifications made. Browser service had already exited by final process check; unrelated VSCode Python process was left untouched.


## Analysis after retry session
Scope: targeted read-only source analysis against actual outcomes. Six retries completed (04-09): two passes, four failures/partial completions. Retry10 not submitted. No quota failure during this session. No source fixes made.

1. Routing/capability mismatch (confirmed): main.cpp sends SINGLE_ACTION requests to SingleActionPipeline, whose ActionParser only recognizes create-folder/open-app/close-app phrases. Registry discovery capabilities are therefore unreachable through this route. Retry05 list/search hits parser failure despite an existing Notes.txt. The router classification is not directly logged; the unique parser error plus dispatch code establishes that the narrow path was reached. Making parsing case-insensitive alone will not add list/search/file-open semantics.
2. Case-sensitive matching (confirmed): action_parser.cpp uses input.find on lowercase literal phrases without normalizing input case. Capitalized Create/Open survive normalization and fail. Only normalize command matching, preserving original case-sensitive contents/names/paths.
3. Copy/planner path recovery (observed failure, precise cause unproven): Retry06 creates correct folder and exact-content draft, then repeatedly fails destination-parent check in primitive_execution_engine.cpp:736. Requested parent exists, but actual attempted destination is absent from logs; cannot establish whether planner generated the wrong path or resolver changed it. Log raw/normalized source+destination+resolved parent, then validate against successful prior resolved_path before retrying. Same failure eventually stops safely, but cannot repair itself.
4. Browser evidence-to-answer completion (observed failure): Retry09 reaches official Downloads page yet repeats release-label clicks and stops NO_STATE_CHANGE instead of answering. Stop guard is useful; successful navigation is not the failing primitive. Observer omits hrefs, truncates visible text to2400chars, and BrowserState further retains2000chars/24elements, which can limit evidence available to the planner. Need evidence extraction and an explicit version+source answer, with completion checked before redundant clicks. Exact click destinations and which text survived compaction were not captured, so do not claim a fully isolated planner root cause.
5. Error categories lost (confirmed): LLMClient already supplies typed failureCategory. Input normalization returns only message to main's default UNKNOWN. GoalDecomposer drops structured failure category and Branch3Runtime::start labels any invalid decomposition INVALID_PLAN, including provider timeout. Preserve transport categories end-to-end so HTTP429 and timeout remain distinct from malformed plans.
6. Keyboard startup latency (confirmed): every waitForInput creates a fresh Part A process; Python constructs Whisper before checking msvcrt V input. V/V cannot skip initialization. Earlier loads took seconds to minutes. Offer direct keyboard input before model construction, and initialize voice lazily/reuse it.
7. Observability impedes diagnosis (confirmed): branch stateLog is printed after start returns, and resumed.stateLog is not printed in main's R/C path. Execution messages omit failing copy destination and browser href. Stream bounded structured events with operation arguments, resolved paths, attempt, category and verification result; redact secrets.

Fix priority: unify routing with actual supported registry and case-insensitive command recognition; add operation-level evidence; repair copy path construction/recovery and browser answer completion; preserve typed dependency failures; remove heavyweight voice startup from keyboard path. Re-test original prompts after each relevant fix.
Working evidence: greeting/arithmetic, multi-step folder+exact-content file creation, safe ambiguity pause, retained progress and honest incomplete status, and basic controlled-browser navigation/stale-reference recovery. End-to-end copy/rename, scoped discovery, conditional missing-file response and browser research answer remain broken in observed tests. Handover package remains unassessed after interruption.
# Fixes verification — 2026-10-27 (Asia/Calcutta)
QA_Tester live regression session, six new realistic prompts with fresh QA_Fixed_* names. Preflight23:41:39 all four fixture roots/paths absent. Built executable build-runtime-supervisor/Nova.exe timestamp23:40:37; no source edits by QA. Real UCRT64 ConPTY, V menu routes directly to keyboard before voice initialization. Each result independently checked. Historical records preserved; embedded three-download report at line192 has unverified authorship/transfer details and is not rewritten as established fact.

## Fixed regression 01 — Uppercase request and exact Unicode file
- Input: `Create a folder named QA_Fixed_Text_20261027 inside C:\Users\shour\Desktop\Nova, then create Note.txt inside it containing exactly "Client: Café; Owner: आशा; Status: REVIEW; Budget: ₹1250.". Preserve every character and do not add a newline.`
- Keyboard: first V+Enter immediately opened [NOVA INPUT], without Python/Whisper initialization.
- Accepted23:42:09; completed23:42:51 (~42sec task execution). Input preserved exactly, including accents, Devanagari and rupee symbol.
- Observed: router/decomposer/planner HTTP200; registry filesystem.create and create_file SUCCESS. Runtime COMPLETED/NONE, All requested postconditions verified.
- Independent verification: Note.txt UTF8 bytes=65; case-sensitive equality to exact quoted payload=True, no newline added. Directory and file exist.
- Verdict: PASS. Uppercase command reached generic registry runtime; exact Unicode and direct keyboard path work.
- Raw terminal output: logs/qa-fixed-01.txt (terminal wrapping/control artifacts retained).

## Fixed regression 02 — Scoped discovery with answer
- Input: `List the files in C:\Users\shour\Desktop\Nova\QA_Fixed_Text_20261027 and find any text file whose name contains Note. Tell me the matching filename and its full path; do not search elsewhere.`
- Accepted23:43:19; completed23:44:38 (~79sec, router32sec/decomposer16sec included).
- Actual: registry filesystem.list_directory SUCCESS; explicit satisfied step evidence references discovered Note.txt; planner done; COMPLETED/NONE. Final Nova answer gives Filename Note.txt and exact full path C:\Users\shour\Desktop\Nova\QA_Fixed_Text_20261027\Note.txt.
- Independent listing: exactly Note.txt65bytes in requested folder. No broader search operation emitted.
- Verdict: PASS. Previously unreachable discovery now runs through registry and reports observed filename/path.
- Raw transcript logs/qa-fixed-02.txt.

## Fixed regression 03 — Copy/rename exact workflow (in progress)
- Input: `Inside C:\Users\shour\Desktop\Nova, create a folder named QA_Fixed_Workflow_20261027, create draft.txt there with exactly "Project Phoenix - Review v2", copy it to backup.txt in the same folder, then rename draft.txt to final.txt. Keep the backup, verify both files have identical content, and show me the final filenames.`
- Accepted23:45:03; first pause23:48:41. Generic create/create_file/copy all SUCCESS with correct absolute source/destination; prior missing-parent bug did not recur.
- Actual pause: PARTIAL_SUCCESS / VALIDATION_ERROR, Step verification lacks concrete current evidence, pending step_3 copy. Planner emitted satisfied three times based on successful copy but proof rejected; rename not reached.
- Genuine independent progress: folder contains draft.txt and backup.txt27bytes each. Pressing offered C once to re-observe/continue; no unchanged mutation failure repeated.

### Fixed regression 03 — Outcome recovered after usage interruption
- On 2026-10-28 resume, previous PTY session41417 no longer exists (`Unknown process id`), and no Nova process is active. Full final console result could not be recovered; do not infer a COMPLETED status.
- Durable runtime log records last result exactly: `[2026-10-27 23:53:04] [INFO] [RUNTIME_RESUME] [status=EXTERNAL_UNAVAILABLE] Provider returned an incomplete or malformed text response.`
- Last directly observed state before interruption (23:50:23): final.txt and backup.txt existed, draft absent, runtime still planning verification/answer.
- Independent filesystem verification on resume: final.txt and backup.txt each27bytes; both exactly `Project Phoenix - Review v2`, identical contents=True, draft.txt absent.
- Verdict: Artifact operations PASS (create/copy/rename/exact contents), end-to-end verification/answer BLOCKED by malformed provider response after earlier proof-validation pause. A final user-facing filename answer was not observed. No blanket task PASS claimed.
- Raw first-pause transcript remains logs/qa-fixed-03-first-pause.txt. Later provider result is sourced from logs/runtime.log, not reconstructed console output. No new tests launched; waiting for newly instrumented build READY.

# Final build verification — 2026-10-28
Four remaining live workflows on the new generic-capability build. Previous Unicode/exact-file and scoped discovery passes carry forward. Fresh QA_Verified_* paths checked absent before launch; no reuse/overwrite of prior partial fixtures. Real UCRT64 ConPTY and V keyboard entry; isolated browser expected (no CDP attachment configured). Each event captured incrementally in logs/qa-verified-*.txt where possible, preserving exact prompts and observed outcomes. No source edits by QA.

## Verified regression 01 — Fresh full copy/rename workflow
- Input: `Inside C:\Users\shour\Desktop\Nova, create a folder named QA_Verified_Workflow_20261028, create draft.txt there with exactly "Project Phoenix - Review v3", copy it to backup.txt in the same folder, then rename draft.txt to final.txt. Keep the backup, verify both files have identical content, and show me the final filenames.`
- Accepted16:16:42; paused16:20:40 (~238sec). Create folder/file, copy and rename all SUCCESS with correct streamed absolute parameters. Host copy reports source_bytes_equal=true; rename source_absent=true. Verified progress correctly contains steps1–4.
- Remaining verification step5 emitted structured execution proof referencing filesystem.copy + backup absolute path three times. Runtime rejected proof and ended PARTIAL_SUCCESS/VALIDATION_ERROR, Step verification lacks concrete current evidence. No final filenames answer was emitted.
- Independent filesystem verification: final.txt and backup.txt each contain exactly Project Phoenix - Review v3, SHA256 hashes match, draft.txt absent. Actual create/copy/rename behavior PASS.
- End-to-end verdict: FAIL / partial due to verification ledger rejection after successful effects, not old destination-parent failure. Exact guide dependencies are not printed; source verifiedDecision currently admits only the current step or direct dependency execution, so a proof referring to transitive earlier copy may be rejected (candidate cause, not conclusively isolated).
- Raw incremental transcript logs/qa-verified-workflow.txt. No additional C pressed because identical proof already rejected three times; awaiting root's generic proof fix while proceeding independent tests unless told otherwise.

## Verified regression 02 — Conditional missing file
- Input: `Open C:\Users\shour\Desktop\Nova\QA_Verified_MISSING_20261028.txt in Notepad if it exists. If it does not exist, tell me clearly that it is missing and do not create a replacement.`
- Accepted16:21:11; paused16:22:31 (~80sec). This request was already active when root asked to quit for new build; no further tasks sent.
- Observed: filesystem.inspect SUCCESS; planner explicitly observed exists=false, satisfied step1 check and step3 missing-report/no-create branch using structured exact-path inspection proof. Planner done twice correctly described file missing/no replacement.
- Actual runtime result: PARTIAL_SUCCESS/VALIDATION_ERROR, Completion rejected: required logical steps remain unverified. Only step1 and step3 are listed; step2 remains unverified (precise guide text not printed). Likely unused conditional-open branch treated as mandatory, but confirm with new guide logging.
- Independent verification: requested path still absent; no create_file or application.launch operation emitted. Safety/no-replacement behavior PASS; end-to-end status/report FAIL because conditional completion remains rejected despite correct missing-file reasoning. No final [NOVA] answer printed.
- Raw transcript logs/qa-verified-missing.txt. No C on known ledger issue. Q cleanly stopped Nova16:22:58; remaining browser/handover not submitted in this build. Hold for root newbuild READY.

# Conditional/ancestor verification build — resumed live QA
New executable expected timestamp2026-10-28 16:31:13 size30359515. Retest conditional missing path, verify existing v3 workflow without mutation, read-only official browser research, then fresh handover. Existing transcripts retained; no source changes. Fresh launch uses real UCRT64 V keyboard path.


## Usage-limit resume — generic workflow fixes, 2026-10-28
- Build completed: build-runtime-supervisor/Nova.exe, timestamp 16:31:13, 30,359,515 bytes. Build log: logs/build-conditional-proof.log.
- Fixed reproduced proof rejection by allowing execution evidence from verified transitive dependencies, retaining operation/path checks and typed fact constraints.
- Fixed mandatory inactive branches with structured guide conditions against actual backend facts. False branches are recorded NOT_APPLICABLE and cannot dispatch. No user-phrase/app/site handlers added.
- Verification: CTest 3/3 PASS; ReliabilityRegressionTest 71 checks / 0 failures; filesystem manipulation 34/34 PASS; filesystem discovery PASS; Python browser integration 16/16 PASS. Outputs: logs/runtime-regression.log, logs/reliability-checks-final.log, logs/filesystem-final.log, logs/discovery-final.log, logs/browser-final.log.
- QA_Tester resumed actual UCRT64 tests on this binary. Live outcomes will be appended separately; offline passes do not establish live task completion.
## Conditional build regression 01 — Missing-file branch scheduling
- Exact input: `Open C:\Users\shour\Desktop\Nova\QA_Verified_MISSING_20261028.txt in Notepad if it exists. If it does not exist, tell me clearly that it is missing and do not create a replacement.`
- Accepted16:35:02; paused16:37:08 (~126sec). Guide now correctly defines step1 check, step2 open guarded exists=true, step3 report guarded exists=false.
- Actual: filesystem.inspect step1 SUCCESS with exists=false; planner prematurely returned done, then satisfied step3 three times referencing that inspection, without satisfying prerequisite step1. Final PARTIAL_SUCCESS/VALIDATION_ERROR, Step verification lacks concrete current evidence; verified progress empty. No missing-file final [NOVA] answer emitted.
- Independent check: path remains absent; no application launch/create action emitted. Safe missing-file handling effects PASS, end-to-end task FAIL due to planner scheduling/ledger order. Conditional guide metadata itself is correct in this run.
- Raw incremental transcript logs/qa-conditional-missing.txt includes complete16:37:08 final result. No C repeated; proceeding independent regressions while root refines generic scheduling feedback.

## Conditional build regression 02 — Existing workflow read-only verification
- Input: `Inspect C:\Users\shour\Desktop\Nova\QA_Verified_Workflow_20261028. Confirm that final.txt and backup.txt both contain exactly "Project Phoenix - Review v3", that their contents are identical, and that draft.txt is absent. Do not create, rename, modify or delete anything. Report the filenames and verification results.`
- Accepted16:37:49. Router HTTP200 then NEEDS_USER/UNKNOWN, Blocked at Root routing: This request needs a capability or connection unavailable in the current registry. Please provide the missing integration or an alternative task.
- No filesystem or other action executed. Catalog inspected read-only: filesystem.inspect exposes existence/type, but no explicit read/hash/compare capability was found. Runtime honestly did not claim content verification.
- Independent checks still confirm both files exactly v3 text, matching contents, draft absent. This agent's checks are not substituted for Nova's execution.
- Verdict: BLOCKED / capability unavailable for independent content verification across sessions; task not completed. Does not exercise transitive proof matcher. Raw logs/qa-conditional-existing-workflow.txt.


## Follow-up scheduling and capability-discovery repair
- Live conditional test exposed planner satisfying a dependent report before verifying its inspection prerequisite. Host now supplies PENDING_STEP_SCHEDULING with READY/WAITING prerequisite IDs and successful-execution hints; rejection feedback names missing prerequisites and tells planner to verify them before repeating dependent proof.
- A new regression deliberately submits the dependent proof first, checks actionable prerequisite feedback, then completes without reexecuting the inspection. Regression total now73/73 PASS; CTest3/3 PASS; registry38/38 PASS.
- Existing-workflow read-only request was rejected by router because filesystem.inspect catalog underdescribed its implemented bounded UTF-8 preview. Updated catalog to advertise existence/type/size/text preview and truncation limit. No new command matcher or privileged special case was introduced.
- New executable pending clean QA shutdown; tests above are source-level verification, not live confirmation of these follow-ups.
## Conditional build regression 03 — Read-only browser scheduling
- Input: `Open the official Python website in the browser, find the current stable Python release, then tell me its version number and the official download page link. Do not download or install anything.`
- Accepted16:38:36; paused16:40:25 (~109sec). Guide5 steps includes open homepage, locate release, extract version/link, report.
- Actual browser.navigate https://www.python.org SUCCESS. Planner then repeatedly satisfied step2 using current-browser text_contains Latest: Python 3.15.2, but never satisfied prerequisite step1 open-page. Three rejected dependent proofs ended PARTIAL_SUCCESS/VALIDATION_ERROR, Step verification lacks concrete current evidence. No final answer emitted.
- This is generic readiness/ledger ordering failure, not earlier redundant-click/download behavior. No browser.click, browser.download or install action emitted in this run.
- Independent observation after pause: controlled URL https://www.python.org/, page content contains Latest: Python 3.15.2, downloads metadata empty. No completed transfer recorded by service. Empty metadata does not prove zero network bytes transferred; here no transfer action/event was observed.
- Verdict: FAIL / partial end-to-end due to unsatisfied predecessor, while basic browser navigation and evidence collection work. Raw logs/qa-conditional-browser.txt. Q for root readiness-ledger/catalog rebuild; fresh handover remains unsubmitted.

# Explicit scheduling ledger build — 2026-10-28
Build timestamp16:41:08 size30365721. Retest missing conditional with same prompt, fresh handover, browser read-only, then existing-content verification. Prior failures retained. No QA source edits; actual UCRT64 keyboard execution and incremental raw transcripts.



## Scheduling-feedback executable ready
- Nova rebuilt 2026-10-28 16:41:08, 30,365,721 bytes. CTest3/3 PASS on final scheduling/catalog source; regression73/73 and registry38/38 remain passing. Build logs/build-ready-scheduling.log. QA_Tester dispatched to retest actual missing-file, fresh handover, browser and read-only content workflows.
## Ready-ledger regression 01 — Conditional missing file PASS
- Same exact input: `Open C:\Users\shour\Desktop\Nova\QA_Verified_MISSING_20261028.txt in Notepad if it exists. If it does not exist, tell me clearly that it is missing and do not create a replacement.`
- Accepted16:41:51; completed16:42:52 (~61sec).
- Actual registry inspect SUCCESS; planner first satisfied prerequisite step1 via exists=false proof. Host marked guarded open step2 NOT_APPLICABLE. Planner then satisfied missing-report step3 and done. Runtime COMPLETED/NONE with final [NOVA] answer clearly says file is missing and no replacement created.
- Independent check: exact requested path remains absent. No create or application.launch action emitted. No C required.
- Verdict: PASS end-to-end. Explicit scheduling feedback plus conditional guard resolved prior proof/order failure. Raw logs/qa-ready-missing.txt.

## Ready-ledger regression 02 — Fresh client handover decomposition failure
- Input: `Set up a client handover package in C:\Users\shour\Desktop\Nova\QA_Verified_Handover_20261028. Create Docs and Archive subfolders. In Docs create draft.txt containing exactly "Client: Acme; Status: REVIEW; Contact: Asha". Copy that file into Archive as README-backup.txt, then rename Docs\draft.txt to README.txt. Create manifest.txt in the package root with exactly two lines: Docs\README.txt and Archive\README-backup.txt. Verify the two README files have identical contents and report the final relative paths. Do not overwrite existing files.`
- Accepted16:43:10; failed16:44:09 (~59sec). Router/decomposer HTTP200. No validated GUIDE or action emitted.
- Actual FAILED/INVALID_PLAN at Goal decomposition: Condition must reference an earlier observation step and a scalar fact comparison.
- Independent check: package root remains absent; no files or directories created. Exact invalid condition fields not printed, so cannot identify malformed model output versus overly strict parser from transcript alone.
- Verdict: FAIL / decomposition contract rejection, no functional handover effects tested. No C because no genuine progress and repeated controls are not a substitute for fixing contract handling. Raw logs/qa-ready-handover.txt.

## Ready-ledger regression 03 — Historical browser result before kernel milestone
- Same frozen official-Python read-only prompt accepted16:44:41; final16:46:17 PARTIAL_SUCCESS/VALIDATION_ERROR, Step verification lacks concrete current evidence.
- Step1 URL predicate verified; step2 text_contains Latest: Python 3.15.2 repeatedly rejected. Independent service observation after pause found that literal in visible_text at index1049; downloads metadata empty. No final answer observed. Raw logs/qa-ready-browser.txt.
- This failed run is retained as architecture/contract evidence. No additional workaround/control attempts performed before deterministic kernel replacement.

# Deterministic workflow-kernel live acceptance — 2026-10-28
User milestone: live QA only after deterministic gate. Root reports gate passed; executable checked timestamp20:48:44 size32564043. Reviewed docs/workflow-kernel.md: host-selected READY work, immutable evidence, deterministic completion. Frozen prompts only: missing conditional, original client handover, official Python read-only research. Preflight20:49:36 no Nova process; missing path absent, handover root absent. No source edits, random prompts or reactive workarounds by QA. Incremental raw logs qa-kernel-*.txt.

## Kernel live acceptance 01 — Frozen conditional missing-file prompt
- Exact input: `Open C:\Users\shour\Desktop\Nova\QA_Verified_MISSING_20261028.txt in Notepad if it exists. If it does not exist, tell me clearly that it is missing and do not create a replacement.`
- Accepted20:50:10. Router request ended after90011ms; HTTP200 was seen but complete response timed out. Actual EXTERNAL_UNAVAILABLE / LLM_TIMEOUT, blocked Root routing: Root Router LLM failed: Provider request failed: Timeout was reached.
- No validated graph or external action ran. Path remains absent. Kernel scheduling/conditional behavior was not exercised in this attempt; typed provider-failure propagation was observed correctly.
- Verdict: BLOCKED by live provider timeout, not deterministic kernel pass/fail. No unchanged retry/control used. Raw logs/qa-kernel-missing.txt.

## Kernel live acceptance 02 — Frozen client handover prompt
- Exact input: `Set up a client handover package in C:\Users\shour\Desktop\Nova\QA_Verified_Handover_20261028. Create Docs and Archive subfolders. In Docs create draft.txt containing exactly "Client: Acme; Status: REVIEW; Contact: Asha". Copy that file into Archive as README-backup.txt, then rename Docs\draft.txt to README.txt. Create manifest.txt in the package root with exactly two lines: Docs\README.txt and Archive\README-backup.txt. Verify the two README files have identical contents and report the final relative paths. Do not overwrite existing files.`
- Accepted20:52:03; provider failure20:53:44. Router HTTP20010755ms; decomposer HTTP200 response_bytes330 but timed out90010ms before usable graph.
- Actual EXTERNAL_UNAVAILABLE / LLM_TIMEOUT, blocked Goal decomposition: Provider request failed: Timeout was reached.
- Independent check: QA_Verified_Handover_20261028 remains absent; no primitives executed. Backend creation/copy/rename and kernel scheduling unexercised in this live attempt.
- Verdict: BLOCKED by external provider timeout. Typed dependency cause preserved; not a kernel functionality pass/fail. No R/C or alternate prompt used. Raw logs/qa-kernel-handover.txt.


# Workflow-kernel milestone — 2026-10-28
User replaced reactive live-failure patching with formalization, deterministic kernel, offline gate, then bounded reconnection. Useful prior UTF-8/filesystem/provider/browser fixes preserved. No browser attachment, locale, wake-word or new capability feature work added. Previous orphan QA Nova process30584 was stopped after confirming executable path; live QA paused until the kernel gate passed.

## Architecture and invariants
- Contract written first in docs/workflow-kernel.md: Workflow, Step, Dependency, three-valued Condition, StepStatus, Evidence, Postcondition and RecoveryCause.
- Added src/workflow_kernel.hpp/.cpp: WorkflowGraph, WorkflowScheduler, FactStore, PostconditionVerifier and StepRecoveryState. Kernel validates missing/duplicate/cyclic references before effects, selects READY steps, evaluates guards from typed facts, verifies scoped evidence and owns terminal completion.
- Branch3Runtime now adapts results and orchestrates the kernel. Removed its verifiedSteps/skippedSteps sets, operation issue map, condition evaluator, ad-hoc transitive verifier, repeated done-rejection completion mechanism and artifact-only completion shortcut. Human log prose no longer proves a step.
- Planner is called with a specific CURRENT_READY_STEP. A mismatched step ID cannot dispatch or verify anything. done cannot complete the workflow and is not required. A verified repair clears that step's active recovery state, retaining historical evidence.
- FactStore assigns stable evidence IDs; includes producing step/operation/category and typed facts. Superseded browser observations cannot establish current state. File inspect now exposes bounded text/preview-truncation as structured facts. Application adapters distinguish accepted dispatch from observed PID/window/focus evidence; no invented window claim from launch dispatch alone.
- Canonical registry feeds router, decomposition, planner, mapper validation and dispatch checks. Unsupported primitive gets explicit UNSUPPORTED_CAPABILITY before execution. Connection/configuration provider failures now retain distinct typed causes.
- Semantic interpretation remains the model's job for logical intent that cannot be expressed as an artifact postcondition; it must provide a structured fact predicate, which the kernel checks. No user-command/app/site/filename handlers added.

## Fixed validation gate and results
- Added check-workflow CMake target, run after architecture changes. CTest5/5 PASS.
- WorkflowKernelTest:250 generated DAG cases,29,439 checks,0 failures, plus joins/guards/unknown facts/transitive evidence/repairs/cycles/non-ready operations/current observation invalidation.
- WorkflowAcceptanceTest:15/15 fixed scenarios PASS. Real temporary filesystem effects; browser/application typed adapters simulated. No LLM/network/voice/live desktop used by this deterministic gate. Frozen scenario list and source SHA256 in docs/workflow-acceptance.md.
- Existing RuntimeReliability48/48 PASS; Branch3RuntimeIntegration27/27 PASS; ReliabilityRegression79/79 PASS. Old fixtures were migrated from prose evidence/final done assumptions to structured evidence and kernel-owned completion; behavior assertions retained.
- Platform checks after gate: filesystem manipulation34/34 PASS, discovery PASS, registry38/38 PASS, native application lifecycle12/12 PASS, Python local-browser16/16 PASS (installed Edge/local harmless pages).
- Built executable build-runtime-supervisor/Nova.exe timestamp20:48:44,32,564,043 bytes. No source changes after this gate; docs/logs only.
- Outputs: logs/kernel-final-build-and-tests.log, logs/kernel-tests.log, logs/workflow-acceptance-final.log, logs/reliability-checks-final.log, logs/application-final.log, logs/browser-final.log, logs/filesystem-final.log, logs/registry-final.log.
- All deterministic tests passed before live QA resumed. Frozen missing-file request then hit a90s router timeout before any workflow/effect: EXTERNAL_UNAVAILABLE/LLM_TIMEOUT correctly retained, not counted as a live pass. Remaining frozen live results appended by QA_Tester separately.
## Kernel live acceptance 03 — Frozen official Python read-only browser prompt
- Exact input: `Open the official Python website in the browser, find the current stable Python release, then tell me its version number and the official download page link. Do not download or install anything.`
- Accepted20:54:00; paused20:54:25 (~25sec). Router/decomposer HTTP200 valid. Kernel produced a3-step DAG and host selected ready=step_1; planner requested browser.navigate https://www.python.org, execution SUCCESS evidence=evidence_1.
- Subsequent planner response HTTP200 response_bytes184 latency492ms was INVALID_RESPONSE. Actual EXTERNAL_UNAVAILABLE / LLM_INVALID_RESPONSE, blocked step_1, Provider returned an incomplete or malformed text response.
- Independent browser observation after pause confirms URL https://www.python.org/, title Welcome to Python.org, controlled browser ready, downloads metadata empty. No click/download/install action emitted. No final release/version/source answer returned by Nova; independent observation is not substituted as Nova task completion.
- Verdict: BLOCKED by live provider invalid response after real successful navigation. Host-selected ready work and structured execution evidence were observed; full verification/report completion not exercised. No R/C or workaround used. Raw logs/qa-kernel-browser.txt.

## Kernel live acceptance session summary
Exactly the3 frozen prompts ran after the deterministic gate: missing prompt BLOCKED router timeout, handover BLOCKED decomposition timeout, browser BLOCKED invalid planner response after navigation. No functionality PASS claimed from provider-blocked attempts. Typed causes LLM_TIMEOUT and LLM_INVALID_RESPONSE surfaced with exact blocked stage; no unknown-category fallback observed.
No code edits or isolated fixes suggested by QA. Handover root and missing target remain absent. Clean Q at final pause; no recurring loop armed. Browser downloads metadata empty and no completed download observed; this is not proof that zero network bytes could ever start in an unexpected browser event.


## Final offline contract checks
- GoalDecomposerContractTest15/15 PASS; PrimitiveMapperContractTest15/15 PASS; FilesystemPathPolicyTest5/5 PASS on current source. Logs/decomposer-contract-final.log, logs/mapper-contract-final.log, logs/path-policy-final.log.
- Three frozen live attempts remain externally blocked (router timeout, decomposer timeout, invalid planner response after successful browser navigation). No source patch followed those outcomes. QA cleanly exited; retained-graph R was not run because its instruction arrived after exit. No reconstructed retry or completion is claimed.
- Kernel implementation and offline/platform gate are verified. Full live sign-off remains pending external provider recovery. No ignored deterministic failures and no paid-model substitution, command matcher or scenario-specific workaround introduced.

# Generic semantic-evidence patch — 2026-10-28
- Reviewed both new supplied briefs and runtime.log manual-session records. User manual results supersede earlier provider-blocked sign-off: conditional missing-file COMPLETED21:03:48, handover COMPLETED via RUNTIME_RESUME21:15:04 after decomposition timeout, browser PARTIAL_SUCCESS21:19:02, application PARTIAL_SUCCESS21:26:07. Both partials identify unsupported structured evidence for step_2. Conversation and detailed provider recovery PASS are attributed to the user's manual report, not reconstructed from missing console traces.
- Kernel scheduler/dependency/condition/completion algorithms frozen. Added generic semantic adapters/matchers, APPLICATION_DISPATCH/APPLICATION_OBSERVATION distinction, canonical read-only application.observe and exact observed full-path document association checks. No website/app/filename/user-phrase handlers and no provider/model changes.
- Browser FactStore retains full stable semantic facts; bounded planner projection is separate and DOM is not duplicated in the ledger. Stale observations remain unusable. Application dispatch/PID/arguments/basename-only titles cannot establish target_observed.
- Build Nova.exe21:46:59,35,684,043 bytes. CTest6/6 PASS; kernel250DAGs/29,439checks; fixed acceptance15/15 unchanged checksum; runtime48/48; Branch3integration27/27; regression83/83; semantic40/40; registry39/39 with27 primitives; pure Python validators9/9 and syntax check PASS.
- No user-facing live prompts, actual browser, daily apps or messaging accounts operated for validation. Native Windows observation was compiled, not live tested. Browser/application integration tests use injected fixtures; no manual PASS invented. Existing native application and Playwright live suites intentionally not run under the brief's no-live-QA instruction.
- Exact target/window association remains unsupported when Windows accessibility/window metadata does not expose the full document path; stronger claims are rejected. User manual validation remains required. If an older Nova browser-agent service is still running, restart that service to load the updated Python observer.
- Engineering handover/files/predicates/limits: docs/semantic-evidence.md. Test logs: logs/semantic-final-build-and-tests.log, logs/semantic-final-unit.log, logs/semantic-registry.log, logs/semantic-python-unit.log.
