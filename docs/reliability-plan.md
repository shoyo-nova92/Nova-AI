# Nova reliability work — 2026-10-27

Evidence: logs.md and the supplied ChatGPT review. Implement generic workflow fixes, never app/site/command phrase handlers.

1. Preserve raw input; local non-destructive normalization; route all external goals through the registry-backed runtime. Text entry must not initialize voice models.
2. Validate UTF-8 at boundaries and truncate on complete code points; retain source field/offset in typed errors.
3. Preserve provider categories; configurable model/timeout and bounded fallback; metadata-only latency telemetry.
4. Track unresolved issues and verification by logical step; refuse premature completion; distinguish retry of a blocked dependency from continuation.
5. Include exact validated operation arguments/resolved paths in bounded diagnostics. Resolve a destination leaf relative to an explicit source parent, never invent missing ancestors.
6. Browser observations expose link destinations, dialogs, tabs and download effects. Read evidence and return an answer without needless clicks; require per-step evidence. Prevent unexpected download actions in ordinary clicks.
7. Observe a bounded title-free environment inventory and locale. Support explicit loopback CDP attachment to an already-enabled user browser, retaining an honestly labeled isolated fallback. Do not copy or commandeer a daily profile. A companion extension remains a separate integration requiring user installation, not something this runtime can silently assume.
8. Add regression tests for real failure classes, build, run offline contracts and harmless local-browser integration, then use QA_Tester for real UCRT64 workflows. Append verified results and remaining limitations to logs.md.

Held/rejected: per-app keyword parsers, site/brand/region URL mappings, forcibly clicking overlays, indiscriminately clearing failures after unrelated success, accepting LLM done without step evidence, silently replacing corrupt user payload, unconfigured paid model fallback, and guessing ownership of an ordinary browser session.

Follow-up live failures: accept structured proof from verified transitive dependencies; evaluate conditional branches using typed backend facts and mark false branches not applicable. Regression coverage exercises both true and false branches and refuses execution of a false branch. These are workflow mechanisms, with no command-specific handlers.
