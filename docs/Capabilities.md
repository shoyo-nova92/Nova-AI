1. Input Layer

Nova can currently be activated and controlled through multiple interfaces.

Voice

Nova uses:

faster-whisper
Whisper large-v3
CUDA/FP16 when available
CPU/int8 fallback

Voice input is recorded through sounddevice and passed through Whisper for transcription.

Wake Word

Nova uses openWakeWord.

Current wake phrase:

hey jarvis

The wake system continuously monitors the microphone for the wake phrase.

Keyboard Activation

Nova also supports direct activation through:

V

This allows the user to activate command listening without speaking the wake word.

Desktop UI

Nova uses a PyQt6 floating Orb interface.

The Orb provides:

Visual assistant state
Text input
Command submission
Status feedback
Listening state
Response display 2. EntryGate

RuntimeEntrygate is Nova's first command intelligence layer.

Its purpose is to handle obvious commands without unnecessarily invoking the full reasoning pipeline.

Examples:

open chrome
close spotify
exit
bye
thank you
hello

These can be handled directly.

For commands that are not obvious deterministic commands, EntryGate passes the request toward the runtime routing system.

3. InputRouter

InputRouter is the current LLM-based classification layer for non-trivial requests.

It classifies a command into one of three branches:

fast_action
conversational
complex_task
Fast Action

Examples:

open chrome
close spotify
launch calculator
create a folder

These are actions that can potentially be handled without complex planning.

Conversational

Examples:

What is RAM?
Explain how Whisper works.
What is the capital of India?
How do I use git rebase?

These require an answer rather than computer control.

They are sent to the conversational runtime.

Complex Task

Examples:

Create a Python file called test.py and write hello world into it.

Open VS Code, create a Python file, write some code,
save it and run it.

Debug this project and fix the failing tests.

These are sent into NovaRuntime.

4. Conversational Runtime

Nova has a dedicated conversational runtime.

It is responsible for requests where the user wants information, explanation, discussion, or normal conversation.

The LLM is used to generate the response.

Example:

User:
Tell me the capital of USA.

Nova:
Washington, D.C.

This branch does not need to create an execution plan.

5. NovaRuntime

NovaRuntime is the orchestration layer for complex tasks.

Its current lifecycle is approximately:

OBSERVE
↓
BUILD CONTEXT
↓
RETRIEVE MEMORY
↓
CREATE PLAN
↓
EXECUTE
↓
VERIFY
↓
RECOVER
↓
LEARN
↓
FINALIZE

NovaRuntime currently coordinates:

Runtime state
Context
Vision
Memory
LLM planning
Plan processing
Execution
Verification
Recovery
Runtime tracing 6. Observation & Context

Nova has an observation/context layer that can gather information about the current desktop environment.

Current infrastructure includes:

Screenshot capture
Active-window information
Running application information
OCR
Context fusion
Reasoning about the current activity

This gives NovaRuntime additional context before planning complex tasks.

7. Memory

Nova contains runtime and semantic memory infrastructure.

Memory can be used to maintain information relevant to:

Previous execution
Runtime history
Project context
Session context
Skills/workflows
Execution traces

Runtime traces and execution information are also used for debugging and verification.

Local runtime state is intentionally not part of the public repository's intended source-controlled state.

8. LLM Layer

Nova currently uses a unified LLM abstraction:

LLMClient
Primary

OpenRouter:

NVIDIA Nemotron 3 Ultra
Fallback

Local Ollama:

Qwen3 14B

The unified client allows Nova's higher-level systems to request model capabilities without directly depending on a particular provider.

The OpenRouter API key is loaded through .env.

Example:

OPENROUTER_API_KEY=your_key_here

The .env file should never be committed to Git.

9. Planning Pipeline

Complex tasks are converted into executable plans.

The current planning pipeline is:

LLM
↓
PlanParser
↓
PlanQualityAnalyzer
↓
PlannerConfidence
↓
PlanNormalizer
↓
IntentExpander
↓
PlanValidator
↓
PlanRepairEngine
↓
Repaired Plan

The purpose is to turn an LLM-generated plan into a structured representation that Nova's execution system can process.

10. TaskTranslator

TaskTranslator converts natural-language execution steps into structured actions.

For example:

Create a folder named test

can become conceptually:

{
"type": "filesystem",
"action": "create_folder",
"target": "test"
}

Other supported action categories include:

Application operations
Filesystem operations
Terminal operations
Python execution
Pytest
Git
Browser operations
File modifications
Folder operations

TaskTranslator is not the executor.

Its responsibility is to understand/translate a deterministic step into an action structure.

11. Execution Policy

Before execution, Nova can apply execution policies.

The policy layer determines whether an action is:

SAFE
CONFIRMATION_REQUIRED
BLOCKED

This provides a safety boundary between understanding a command and actually executing it.

12. ExecutionRouter

ExecutionRouter is the deterministic execution dispatcher.

It receives structured actions and routes them to the appropriate handler.

Conceptually:

Action
│
▼
ExecutionRouter
│
├── ApplicationHandler
├── FilesystemHandler
├── TerminalHandler
├── GitHandler
└── BrowserHandler 13. Existing Deterministic Execution

Nova already has a substantial deterministic execution backend.

Application Handler

Handles application operations such as:

Open application
Close application
Focus application
Other supported application lifecycle operations

Application discovery is handled through the application search infrastructure rather than relying exclusively on hardcoded executable paths.

Filesystem Handler

Supports operations including:

Create file
Create folder
Read file
Modify file
Replace text
Append text
Insert at line
Rollback/backup operations
Terminal Handler

Supports operations including:

Python execution
Pytest
Pip installation
Build operations
Shell commands
Terminal sessions
Git Handler

Nova has deterministic support for Git operations including:

Git status
Git add
Git commit
Git checkout
Git pull
Git push
Browser Handler

Browser operations are also available through the existing execution infrastructure.

14. Verification

Execution does not simply end after calling a handler.

Nova contains execution verification infrastructure to determine whether an action actually succeeded.

Depending on the action, verification can inspect things such as:

Filesystem state
Running processes
Git state
Execution results
Other runtime conditions 15. Recovery

Nova also contains recovery infrastructure.

When an execution fails, the runtime can attempt recovery/retry behavior rather than immediately treating the entire task as failed.

Supporting infrastructure includes:

RecoveryEngine
AdaptiveRetryEngine
SelfCorrectionEngine
Execution confidence
Execution memory 16. What Is NOT Implemented Yet

This section is intentionally explicit so the repository does not misrepresent its current capabilities.

Step Router — NOT IMPLEMENTED

The current runtime does not yet have the final architectural decision layer:

planned step
↓
StepRouter
├── Fast deterministic execution
└── TARS / UI execution

At present, complex plan steps are primarily processed through the existing deterministic execution pipeline.

The Step Router is the next major architectural component.

17. TARS / Visual Execution — NOT CONNECTED

Nova contains experimental/prototype components related to visual computer interaction.

These include infrastructure for:

UI execution
UI semantics
Desktop understanding
Environment awareness
Action selection
Desktop capture
OmniParser experimentation

However:

These components are not currently integrated into NovaRuntime's main plan execution loop.

Therefore Nova should not currently be described as a fully autonomous visual computer-use agent.

The planned architecture is:

Complex Task
↓
NovaRuntime
↓
Plan
↓
StepRouter
↓
Does deterministic backend support this?
│
├── YES → TaskTranslator
│ ↓
│ ExecutionRouter
│ ↓
│ Handler
│
└── NO → TARS / Visual Executor
↓
Screen/UI
↓
Action 18. Why the Step Router Matters

Consider:

Create a Python file,
write code into it,
open VS Code,
select the Python interpreter,
run it,
and save the result.

This is not one type of automation.

Some steps can be executed directly:

Create file
Write file
Run Python

Other steps may require visual interaction:

Select Python interpreter
Click a particular UI button
Navigate a GUI dialog

The future Step Router will evaluate each step independently.

For example:

Step 1 → FAST
Step 2 → FAST
Step 3 → TARS
Step 4 → TARS
Step 5 → FAST

This allows Nova to combine deterministic automation and visual automation inside the same task.

19. Intended Final Execution Model

The goal is not:

LLM → do everything

The goal is:

LLM
↓
Understand / Plan
↓
Structured Steps
↓
Step Router
├── deterministic automation whenever possible
└── visual automation when necessary
↓
Execute
↓
Verify
↓
Recover if needed
↓
Continue
↓
Final response

This separation is important.

The LLM should not be responsible for blindly clicking buttons or performing every filesystem operation.

Nova should use the most reliable execution mechanism available for each step.

20. Example: Deterministic Task

User:

Create a folder on my desktop called NovaTest.

Current architecture can handle this through:

User
↓
EntryGate / InputRouter
↓
NovaRuntime / appropriate action path
↓
TaskTranslator
↓
ExecutionPolicy
↓
ExecutionRouter
↓
FilesystemHandler
↓
Folder created
↓
Verification

No visual agent is necessary.

21. Example: Conversational Task

User:

Explain how Whisper works.

Pipeline:

User
↓
EntryGate
↓
InputRouter
↓
Conversational
↓
ConversationalRuntime
↓
LLM
↓
Answer

No execution is required.

22. Example: Complex Task

User:

Create a Python file called test.py and write hello world into it.

The intended architecture is:

User
↓
EntryGate
↓
InputRouter
↓
complex_task
↓
NovaRuntime
↓
LLM Planner
↓
Plan
↓
StepRouter
↓
Fast execution
↓
TaskTranslator
↓
ExecutionRouter
↓
FilesystemHandler
↓
Verification
↓
Result

This is already close to the current implementation.

23. Example: Future Hybrid Task

User:

Open VS Code, open my project,
create a Python file,
write this code,
select the Python interpreter,
run it,
and save the file.

Future architecture:

                NovaRuntime
                     │
                     ▼
                   PLAN
                     │
                     ▼
                STEP ROUTER
                     │
       ┌─────────────┴─────────────┐
       │                           │
       ▼                           ▼
     FAST                         TARS
       │                           │
       ▼                           ▼

create_file open project
write_file select interpreter
run_python GUI interaction
│ │
└─────────────┬─────────────┘
▼
VERIFY
│
▼
NEXT STEP

This hybrid execution model is the primary direction of Nova's next development phase.

24. Current Development Phase
    Phase: Hybrid Execution Architecture
    Completed
    ✓ Voice input
    ✓ Wake word
    ✓ Keyboard activation
    ✓ Desktop UI
    ✓ EntryGate
    ✓ Input routing
    ✓ Conversation
    ✓ LLM integration
    ✓ Runtime orchestration
    ✓ Context awareness
    ✓ Memory infrastructure
    ✓ LLM planning
    ✓ Plan parsing
    ✓ Plan normalization
    ✓ Plan validation
    ✓ Plan repair
    ✓ Deterministic execution
    ✓ Application control
    ✓ Filesystem automation
    ✓ Terminal automation
    ✓ Git automation
    ✓ Browser automation
    ✓ Verification
    ✓ Recovery
    Current
    → Step Router
    Next
    → TARS / UI execution integration
    → Hybrid step-by-step execution
    → Visual verification
    → Improved recovery across execution backends
    → End-to-end hybrid task testing

25. Roadmap

### Phase 1 — Step Router

Build the missing routing layer between:

Plan
↓
StepRouter

The router will determine whether each step is:

FAST
TARS
CONVERSATION / STATUS

### Phase 2 — TARS Integration

Connect the existing visual components to the Step Router.

Target:

StepRouter
↓
TARS
↓
Desktop observation
↓
UI understanding
↓
Action selection
↓
Mouse / keyboard action
↓
Verification

#### Phase 3 — Hybrid Execution

Allow one plan to alternate between execution backends.

Example:

FAST
↓
TARS
↓
FAST
↓
TARS
↓
FAST

### Phase 4 — Visual Verification

TARS should verify whether the intended UI state was actually achieved before Nova continues.

### Phase 5 — End-to-End Agentic Workflows

Test real workflows involving:

VS Code
Photoshop
Browser workflows
File management
Development environments
Git workflows
Multi-application tasks 30. Project Vision

The long-term vision for Nova is a computer assistant that does not require the user to manually translate every goal into:

click here
type this
open that
press this
save there

Instead:

User:
"Prepare this project for submission."

Nova should eventually be able to:

Understand the goal
↓
Inspect the environment
↓
Understand the project
↓
Create a plan
↓
Route every step appropriately
↓
Use deterministic automation where possible
↓
Use visual interaction where necessary
↓
Verify every important operation
↓
Recover from failures
↓
Report the final result

That is the direction of the project.
