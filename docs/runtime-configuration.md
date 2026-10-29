# Using and configuring Nova

Start `build-runtime-supervisor/Nova.exe`. Press V then Enter for immediate text input, or S for voice. Voice models load only when voice is selected. After a task, V starts another typed request. `--text` starts directly at typed input. For multiline requests, enter `:paste`, paste the request, and finish with `:end` on its own line; payload lines are preserved.

All external actions use the live primitive registry and the same observed-state runtime. The legacy phrase parser is not used by the executable's request dispatcher. Missing files are observable facts; unsupported capabilities and ambiguous targets should produce a useful explanation rather than guessed actions.

Provider configuration comes from environment variables:

- `OPENROUTER_API_KEY`: credential, never printed in telemetry.
- `NOVA_MODEL`: default model. Existing free-model default is retained unless configured.
- `NOVA_ROUTER_MODEL`, `NOVA_DECOMPOSER_MODEL`, `NOVA_PLANNER_MODEL`, `NOVA_CONVERSATION_MODEL`: optional role overrides.
- `NOVA_FALLBACK_MODEL` or role-specific `NOVA_PLANNER_FALLBACK_MODEL` (same naming for other roles): optional fallback, only used for dependency failures. No fallback model is chosen or purchased automatically.
- `NOVA_LLM_TIMEOUT_SECONDS`: 5–300 seconds, default 90. Connect timeout is configured separately by `NOVA_LLM_CONNECT_TIMEOUT_SECONDS` (1–120, default 10).
- `NOVA_MAX_OUTPUT_TOKENS`: output bound, default 8192. Allow enough output for generated file contents.

Telemetry prints role, selected model, request/response byte counts, latency, HTTP status and failure category. It does not print API keys, provider account identifiers or entire prompts. Normal runtime logs still contain the user's requested task, as before.

Retry (R) retries an unavailable dependency using retained goal/progress. Continue (C) re-observes state with a fresh bounded planning budget. The workflow kernel owns dependencies, three-valued conditions, evidence, step recovery and completion. The planner receives one runtime-selected READY step and cannot run another step. Required steps finish only as VERIFIED or validly SKIPPED; a final planner `done` is neither needed nor authoritative.

FactStore records structured execution/observation evidence with stable IDs. Artifact verification checks backend-verified kind and identity. Semantic interpretation must propose a structured predicate supported by scoped facts from the selected step or its verified transitive ancestors; human log prose is not proof. File creation uses host readback, copy compares bytes, and rename/move check resulting paths. Browser dispatch alone cannot prove submission or download completion. Superseded browser observations cannot prove current state, and editable drafts are excluded from readable-page evidence. Successful verification clears the affected step's active recovery cause while retaining its history.

False conditions justify NOT_APPLICABLE/SKIPPED; unknown conditions block instead of silently skipping. The kernel detects cycles and invalid references before effects. Operation/verification events stream immediately, and logs append across restarts. The planning budget is bounded to 18–128 iterations and retains verified progress on continuation. See [workflow-kernel.md](workflow-kernel.md) for invariants and the deterministic validation gate.

## Browser sessions

Without an explicitly connected browser, Nova uses its labeled isolated persistent profile. It does not inherit ordinary Chrome/Edge cookies and does not pretend otherwise.

`NOVA_BROWSER_CDP_URL` can point to an explicitly enabled **loopback** Chromium CDP endpoint, for example `http://127.0.0.1:9222`. The browser must already expose that endpoint. A configured endpoint that cannot be reached fails visibly; it does not silently switch accounts/profiles. Nova neither modifies your browser settings nor copies your active user profile. A browser companion extension for attaching arbitrary normal windows remains a separate integration, not an implemented capability.

In an attached session, Nova opens a blank controlled tab initially. `browser.tabs` lists origins/IDs; an explicit local filter reveals relevant matching tab URLs/titles. `browser.select_tab` requires a returned ID. Navigation reuses an exact matching URL or opens a new tab rather than overwriting an unrelated selected user tab. Unfiltered desktop observation omits window titles and document contents and includes OS locale/region and configured known-folder paths.

Browser observations include bounded page text, observed links, focus, editability and blocking-dialog evidence. An ordinary click refuses declared download controls and cancels unexpected download events; cancellation happens after the event, so it is not proof that no network bytes started transferring. Only explicit `browser.download` saves a transfer to an authorized existing directory, returns its verified file path and byte count, and refuses overwrite. Downloads never execute automatically. Popup and download effects count independently from DOM changes.

Limits remain explicit: existing-browser control needs an actual connection, authentication/CAPTCHA needs user input, external providers can be unavailable, and installed OS capabilities bound what Nova can execute. The runtime must report these limits honestly, not hide them or claim completion.
