# Semantic discovery and retained user decisions

Implemented on 2026-10-29. Validation is offline; live acceptance remains with the user.

## Search and confidence

`filesystem.search` now returns `FILESYSTEM_SEARCH_RESULT` backend facts with `search_root`, `query`, `results`, `search_complete`, `results_truncated`, `scanned_entries`, and `clear_winner`. Each result includes its canonical path, name, parent, entry type, extension, candidate ID, numeric `match_score`, `match_reasons`, and modification metadata where available (`modified_time` in UTC, native ticks, and age in days).

Ranking combines normalized exact matches, substring matches, token coverage independent of order, separator-insensitive matching for joined names, and bounded edit similarity. Ties use canonical path ordering. Results are sorted before the requested result limit is applied. Traversal stops at 50,000 entries and reports incompleteness. Permission and canonicalization errors also prevent a clear-winner claim.

`entry_type`, `extension`, and `modified_within_days` are generic filters and produce corresponding match reasons. The planner extracts meaningful name words and explicit constraints from the human request; it can use `*` for a request constrained only by type or time. Modification time is explicitly not download time.

Policy constants live in `filesystem_ranking.hpp`: minimum candidate score 0.40, automatic discovery score 0.85, and required winner margin 0.15. These retain partial candidates while requiring strong coverage and separation for automatic resolution. Incomplete searches and a result limit that hides the runner-up cannot establish a clear winner. Confidence represents identity ranking, never execution safety.

## Decisions and terminal flow

`PendingUserDecision` stores a session-unique question ID, current step, typed reason, question intent, structured candidates, preferred candidate, source search evidence ID, allowed actions, affected operand fields, and a workflow snapshot. The actual kernel, graph, evidence, progress, and held operation remain in the same in-memory runtime session.

Close or weak candidates pause with `NEEDS_USER / TARGET_AMBIGUOUS`. A consequential operation using an inferred search identity pauses with `TARGET_CONFIRMATION` even when the winner is clear. Scoped confirmed targets and literal canonical paths already present in the original goal do not need an extra target confirmation. Read-only discovery does not require a decision for a clear winner.

The terminal lists numbered candidates and provides `[O] Show matches`, `[S] Search again`, `[A] Answer`, `[V] New request`, and `[Q] Quit`. Answers may also be entered directly. Retry and Continue cannot answer an active question.

Numbers, candidate IDs, unique names, common affirmative answers, and “the second one” use deterministic mapping. Ambiguous yes and duplicate names do not choose a candidate. Other language uses a constrained interpretation call with the active question, candidate metadata, allowed actions, and answer. The host rejects unknown IDs, stale questions, invalid actions, and invalid response shapes. Interpretation failures retain the question. Generic planner clarification also creates a retained `MISSING_INFORMATION` decision.

## Resume and evidence

Selection inserts host-authored `user.select_candidate` evidence containing `resolved_target`, canonical `path`, candidate ID, question ID, source search evidence ID, and `user_confirmed`. A held operation resumes directly through the existing mapper, validator, and executor, binding only the affected operand fields. Independent operands and literal payloads are preserved. The planner can consume `$resolved_target` and `$resolved_parent` in path parameters for dependent operations.

Selection does not route, decompose, or search again. It does not automatically verify an entire step: normal structured verification and graph-owned completion still apply. Search runs again only when the user requests another/refined search. Refinement reuses the original bounded search parameters in the currently paused step; model-provided refinement may change only query, extension, and modification-age constraints.

New filesystem semantic predicates are `path_exists`, `path_missing`, `search_has_results`, and `search_result_matches`. Result matching supports canonical path, exact name, name contains, extension, entry type, parent path, and search root; every constraint must match one result. Path predicates require an explicit inspect observation. Search invocation success is rejected as a search-result proof.

The only kernel change extends the existing evidence-currency mechanism: a new successful same-step search supersedes earlier same-step search results and their resolution evidence. Original facts remain retained. Scheduler ownership of READY, graph semantics, DAG validation, step isolation, and completion are unchanged.

## Application proof and continuation

Native observation retains bounded UI Automation window Name/HelpText and document/tab labels alongside Win32 window/process metadata. A consistent native document label can corroborate the existing trusted canonical launch/process/window context. Duplicate document labels, contradictory full paths, reused/delegated processes, missing process identity, and conflicting launch history cannot establish correlated proof. Full path exposure remains necessary only for exact proof.

`[APPLICATION_CORRELATION]` now reports requested/canonical target, dispatch and observed PIDs, process-instance and executable matches, observation timing, window presence/visibility/title, UIA metadata, document label, basename/full-path matches, correlation result, failure reason, and proof level. Normal diagnostics do not dump accessibility trees.

Continue retains host observation before planning, semantic comparison, bounded window-startup observation, and immediate `EVIDENCE_UNAVAILABLE` on unchanged refresh. Repeated unsupported semantic proof against unchanged facts now pauses with `EVIDENCE_UNAVAILABLE` or `UNSUPPORTED_VERIFICATION`, instead of exhausting planner validation retries. Successful mutation replay protection remains intact.

## Validation and limitations

`cmake --build build-runtime-supervisor --target check-workflow -j 4` passes all seven suites: runtime reliability, runtime integration, reliability regression, kernel, semantic evidence, workflow acceptance, and discovery decisions. The new suite covers candidate ranking, real search against inert temporary fixtures, ambiguity, target confirmation, deterministic and interpreted answers, rejection of invented IDs/stale questions, preserved decomposition/search counts, selected-path binding, refinement, evidence currency, terminal choices, explicit canonical targets, and harmless discovery.

The existing runtime executable was running and Windows prevented replacing it. The production executable is built separately at `build-discovery-decision/Nova.exe` without terminating that process. No Nova live acceptance, user applications, Downloads searches, installer execution, public browser sessions, or messaging accounts were used for validation.

Decisions are retained in memory, not restored across process restarts. Natural-language extraction and less common answer interpretation still depend on the configured provider. Search is deliberately bounded; modification metadata cannot establish when a file was downloaded. Token case normalization handles ASCII case and preserves UTF-8 text; it is not full Unicode case folding. Candidate resolution covers identities flowing through registered operation parameters; it does not infer arbitrary opaque UI payload semantics. Reused/delegated application processes may still return `EVIDENCE_UNAVAILABLE`. OCR is not included. Native correlation changes still require user live acceptance.

## Files changed by this implementation

- `CMakeLists.txt`
- `src/filesystem_ranking.hpp` (new)
- `src/pending_user_decision.hpp` (new)
- `src/discovery_decision_test.cpp` (new)
- `src/branch3_runtime.cpp`, `src/branch3_runtime.hpp`
- `src/primitive_execution_engine.cpp`, `src/primitive_parameters.hpp`
- `src/primitive_mapper.cpp`, `src/primitive_registry.cpp`
- `src/semantic_evidence.cpp`, `src/semantic_evidence_test.cpp`
- `src/runtime_outcome.hpp`
- `src/runtime_supervisor.cpp`, `src/runtime_supervisor.hpp`, `src/main.cpp`
- `src/next_action_planner.cpp`
- `src/application_observation.hpp`, `src/application_correlation.hpp`, `src/application_state.hpp`
- `src/workflow_kernel.cpp` (evidence currency only)
- `src/reliability_regression_test.cpp` (expected typed evidence pause)
- `docs/discovery-decisions.md` (this report)

The workspace already contained substantial changes before this implementation; this manifest identifies only files touched for this request.
