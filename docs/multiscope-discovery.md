# Multi-scope filesystem discovery

Discovery postconditions support individually resolved root alternatives:

```json
{
  "source": "filesystem",
  "kind": "search_result_matches",
  "match": {
    "query": "resume",
    "entry_type": "file",
    "extension": ".pdf",
    "search_roots": ["documents", "downloads"],
    "root_semantics": "ANY",
    "scope_strength": "PREFERRED",
    "scope_provenance": "USER_HINT"
  }
}
```

`search_root` remains supported for one root. It cannot be combined with
`search_roots`. Root sets contain at most 16 entries and use `ANY` semantics.
`REQUIRED` is the default strength; `PREFERRED` explicitly permits compatible
broadening while retaining all other declared result constraints. Provenance
is `USER_CONSTRAINT`, `USER_HINT`, or `INFERRED`; language interpretation supplies
these fields, without runtime phrase matching.

Graph construction resolves each root with the existing path policy, authorizes
discovery, and deduplicates canonical equivalents. Malformed paths are rejected.
An unresolved required resource fails admission. An unresolved preference remains
a planning clue and cannot be sent directly to a filesystem primitive.

The runtime exposes the bound scope to the planner. After an empty search, it
can reuse the retained registered search shape at the next untried root within
the same READY step. Stated alternatives may exceed the normal autonomous
attempt allowance, but remain bounded by the existing total strategy budget.
A premature planner question can similarly become a registered search derived
from the declared query and constraints when multiple resolved roots remain.

Required scopes are enforced before search/directory enumeration and again
by the semantic verifier. A required-root search may inspect a descendant of an
allowed root. Candidate paths must remain beneath the actual canonical search
root, including for preferred scopes. Path comparisons preserve Windows case,
separator, and component-boundary semantics. Candidate records, query metadata,
and other declared constraints remain independently verified.

When scopes are exhausted, existing strategy recovery and clarification apply.
A location hint can extend a preferred scope's strategy context without changing
the declared postcondition, replacing the graph, or restarting decomposition.
An outside location hint cannot broaden a required scope. Verified discovery
binds `$resolved_target` / `$resolved_parent` for dependent actions; existing
candidate confirmation and replay protection remain in force.

Validation: `check-workflow` passes all 18 offline suites, including
`MultiScopeDiscoveryTest`. Its temporary fixture exercises Documents without a
match, Downloads with `Shourya_Resume_2026.pdf`, canonical target confirmation,
four-root recovery, admission failures, unresolved preferences, and user-guided
location recovery. Searches use only its owned fixture; application dispatch,
observations, and model responses are mocked. No live QA was performed.
