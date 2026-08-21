## Design Principles

Nova is being developed around several architectural principles.

1. Use deterministic automation whenever possible

If Nova can safely perform an operation through an API, filesystem operation, terminal command, or native Windows mechanism, it should prefer that over visual clicking.

2. Use the LLM for intelligence, not everything

The LLM should primarily provide:

Understanding
Classification
Planning
Reasoning
Conversational responses

It should not be the only execution mechanism.

3. Keep execution modular

Each capability should have a dedicated component.

Examples:

Git → GitHandler
Filesystem → FilesystemHandler
Terminal → TerminalHandler
Applications → ApplicationHandler
Browser → BrowserHandler 4. Verify execution

Nova should not assume:

"I called the function, therefore the task succeeded."

It should verify the result whenever possible.

5. Recover from failures

Failed steps should have an opportunity for retry, correction, or recovery.

6. Separate planning from execution

The planner creates the plan.

The execution system executes it.

This prevents the LLM from directly controlling every low-level operation.
