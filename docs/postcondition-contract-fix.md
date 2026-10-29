# Discovery postcondition contract fix

## Origin of the reported failure

The rejected `execution.filesystem.search` came from the **planner's satisfaction proof**, not a declared decomposer postcondition.

Before this fix, `Branch3Runtime::interpretation` constructed that exact error string only when the current planner decision contained `verification.source = "execution"` and `verification.operation = "filesystem.search"`. The diagnostic combined those two fields into an action-shaped name. It was not a stored predicate default or a compatibility translation.

The old decomposer accepted only artifact postconditions (`directory_exists` and `file_content`, with name/location). Any other declared kind failed its schema check before a plan could run. Steps without an artifact declaration received the kernel's `INTERPRETED` placeholder, which asked the planner for a structured proof after observing the operation. The broad execution-proof examples in the planner prompt permitted the wrong response shape; the runtime then correctly refused to equate search invocation with finding the requested file.

The supplied log does not contain the planner's full raw JSON. The source/operation pair above is established by the exact runtime branch that emitted the reported message; other fields of that response are unknown.

## Contract changes

`src/predicate_registry.hpp` is the canonical vocabulary and schema authority for filesystem, browser, application, and artifact postconditions. It supplies both LLM-facing schema descriptions and host validation. Semantic compilation, decomposition validation, and workflow graph admission use that same authority.

Decomposition now accepts canonical semantic declarations in addition to the existing artifact format. `MultiStepAction::semanticPostconditionJson` preserves the declaration. For example, a discovery step can declare:

```json
{
  "source": "filesystem",
  "kind": "search_result_matches",
  "match": {
    "query": "requested filename words",
    "search_root": "user's root clue",
    "entry_type": "file",
    "extension": ".ext",
    "modified_within_days": 7
  }
}
```

Only applicable requested constraints belong in the declaration. The host resolves root clues using the existing path/location resolver. Predicate validation does not infer intent from filenames, applications, or step wording.

The existing one-repair decomposition bound now applies to invalid predicate names and schemas as well. An unrepaired declaration anywhere in the plan prevents all planner/executor operations. `WorkflowGraph` also validates semantic and artifact declarations on admission, before scheduling begins. Scheduling, FactStore currency/immutability, provider configuration, native application correlation, ranking, PendingUserDecision, and mutation replay protection are unchanged by this patch.

Action-shaped declarations such as `execution.filesystem.search`, `execution.application.launch`, `execution.browser.navigate`, and `execution.filesystem.delete` are rejected. No action-name-to-success translation was added. The old artifact spelling remains supported, and qualified names such as `filesystem.search_result_matches` are accepted only as spelling aliases for registered source/kind pairs.

## Verification and retained choice

For a declared contract, the graph's validated predicates are authoritative. After an operation inserts backend evidence, the runtime evaluates that contract directly against the scoped FactStore. It also evaluates declared contracts after host observation and before planning, allowing verified dependency evidence to satisfy a step without another operation. No planner satisfaction or expanded proof is required. A failed relevant observation returns `EVIDENCE_UNAVAILABLE` immediately, with a field-specific search diagnostic, rather than spending planner calls on equivalent claims. A step that still needs a genuine operation/observation can obtain it through the existing planner and host interfaces. Artifact verification retains its existing recovery behavior.

For older plans with no declared contract, a legacy unsupported planner proof receives one bounded proof-schema repair using retained evidence. That repair does not route, decompose, or execute a second search. Persistent invalid proofs still pause honestly.

Search verification separates search-level query/root/completeness constraints from candidate-level constraints in `results`. Every candidate constraint must match the same result. Search verification supports exact canonical path, name, name contains, type, extension, parent/root, query association, optional explicit completeness, and modification age. Query verification binds the predicate to the backend's actual search query and scored result, preserving token-order-independent discovery. Recency requires actual backend `modified_age_days` metadata on the same result. Extension identity accepts `exe`, `.exe`, and their case variants. Windows path comparison is lexical and ordinal case-insensitive, including Unicode, with separator, trailing-separator, and dot-segment normalization; it performs no new filesystem search. Root membership checks component boundaries and accepts recursive descendants, while parent constraints remain exact. Names and arbitrary text remain strict. Missing or mismatching metadata fails verification; invocation success and search-root existence cannot substitute for a result.

The full ranked evidence remains intact. Candidate views passed to the existing decision policy are filtered by declared discovery constraints. A candidate that violates the verified discovery contract cannot be used as an inferred action target. The existing confirmation gate and selected canonical-path resume mechanism remain in place.

Runtime-owned verification logs use `[VERIFICATION] origin=runtime contract=declared`. Diagnostics also distinguish planner proof input from the declared contract through `[VERIFICATION_INPUT] origin=planner ... contract=declared|observed_interpretation`, record bounded planner proof repair, and include postconditions in the workflow ledger.

## Offline verification

`PostconditionContractTest` reproduces the structured search shape and the `exe`/`.exe` plus root-separator mismatch with mocked search evidence and execution. It asserts exactly one planner call for the search step, deterministic verification, and immediate typed pauses for unmet contracts. It verifies discovery, opening the containing directory, pausing before execution, and natural-answer resume with one decomposition and one search. It also checks all-plan rejection before operations, one decomposition repair, proof-only repair for older guides, canonical-path/query/type/root/extension/recency constraints, missing metadata, empty results, graph admission, and refusal to weaken declared requirements to execution success.

Additional coverage checks dot segments, drive and UNC paths, Unicode Windows case comparison, recursive root membership, preservation of strict name matching, completeness metadata, candidate constraints within one row, and immutable source facts. `DiscoveryDecisionTest` also verifies the same predicate against actual backend evidence produced from inert temporary fixtures. Declared inspect and browser-observation contracts test deterministic evaluation beyond search. The workflow check target includes the decomposer contract test, for nine offline suites in total. Native application/browser activity and installer execution remain mocked; filesystem tests use test-owned temporary fixtures. No live QA was run. Model compliance and application behavior still require the user's acceptance retest.

## Files changed for this fix

- `src/predicate_registry.hpp` — new shared predicate vocabulary, schemas, and validation.
- `src/goal_decomposer.cpp` — registry context, semantic declarations, existing bounded repair.
- `src/multistep_plan.hpp` — retained semantic declaration.
- `src/next_action_planner.cpp` — canonical contract context and search-proof examples.
- `src/semantic_evidence.cpp` — shared validation and query/recency result constraints.
- `src/workflow_kernel.cpp` — declaration validation on graph admission only.
- `src/branch3_runtime.cpp` — declared-contract wiring, filtered candidate views, proof diagnostics, bounded compatibility repair.
- `src/postcondition_contract_test.cpp` — mocked regression suite and normalization/no-planner verification coverage.
- `src/discovery_decision_test.cpp` — actual backend evidence-shape regression against temporary fixtures.
- `CMakeLists.txt` — offline test registration and check-target dependencies.
- `docs/postcondition-contract-fix.md` — this report.
