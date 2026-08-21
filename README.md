# NOVA

### Personal AI Assistant for Voice, Automation, and System Control

> **Current project status:**
>
> Nova currently has a working voice/text assistant pipeline, deterministic system automation, LLM-powered conversational intelligence, complex-task planning, execution verification, recovery infrastructure, and runtime context/memory systems.
>
> **Visual/TARS execution is not yet integrated into the main execution loop.**
>
> The next major development phase is the **Step Router + TARS/UI execution layer**.

---

# Overview

**Nova** is a Windows-based personal AI assistant designed to move beyond the traditional:

> User gives command → assistant gives answer

model.

The long-term goal is:

> User gives Nova a goal → Nova understands the goal → plans the required work → chooses the appropriate execution mechanism → performs the task → verifies the result → reports what happened.

Nova is being built as a modular local-first assistant with:

- Voice interaction
- Wake-word activation
- Keyboard activation
- Floating desktop UI
- Natural-language command understanding
- Deterministic Windows automation
- LLM-powered reasoning
- Complex task planning
- File and terminal automation
- Git automation
- Application control
- Context awareness
- Memory
- Execution verification
- Recovery mechanisms
- Future visual GUI automation through a TARS-style execution layer

---

# Current Status

### Core assistant foundation

| Component                         | Status             |
| --------------------------------- | ------------------ |
| Windows desktop assistant         | ✅ Working         |
| Voice input                       | ✅ Working         |
| Faster-Whisper speech recognition | ✅ Working         |
| Text input through Orb            | ✅ Working         |
| Wake-word detection               | ✅ Working         |
| Keyboard `V` activation           | ✅ Working         |
| Text-to-speech                    | ✅ Working         |
| Floating PyQt6 Orb                | ✅ Working         |
| EntryGate                         | ✅ Working         |
| Direct open/close handling        | ✅ Working         |
| Conversation handling             | ✅ Working         |
| LLM conversational responses      | ✅ Working         |
| InputRouter                       | ✅ Working         |
| Complex-task classification       | ✅ Working         |
| NovaRuntime                       | ✅ Working         |
| LLM planning                      | ✅ Working         |
| Plan parsing                      | ✅ Working         |
| Plan normalization                | ✅ Working         |
| Plan expansion                    | ✅ Working         |
| Plan validation                   | ✅ Working         |
| Plan repair                       | ✅ Working         |
| Deterministic execution           | ✅ Working         |
| Application execution             | ✅ Working         |
| Filesystem execution              | ✅ Working         |
| Terminal execution                | ✅ Working         |
| Git execution                     | ✅ Working         |
| Browser execution                 | ✅ Working         |
| Execution verification            | ✅ Working         |
| Recovery infrastructure           | ✅ Working         |
| Runtime context                   | ✅ Working         |
| Vision/context observation        | ✅ Working         |
| Memory/retrieval infrastructure   | ✅ Working         |
| Step Router                       | ❌ Not implemented |
| TARS/UI execution integration     | ❌ Not implemented |
| Full visual GUI agent             | ❌ Not implemented |

---

# Architecture

The current architecture consists of two major execution worlds.

The first is already implemented:

```text
                         USER
                           │
                           ▼
                    ┌─────────────┐
                    │  ENTRYGATE  │
                    └──────┬──────┘
                           │
             ┌─────────────┼──────────────┐
             │             │              │
             ▼             ▼              ▼
         OPEN/CLOSE   CONVERSATION   COMPLEX TASK
             │             │              │
             ▼             ▼              ▼
          DIRECT/LLM    DIRECT/LLM   NOVA RUNTIME
                                           │
                                           ▼
                                       PLANNER
                                           │
                                           ▼
                                       PLAN STEPS
                                           │
                                           ▼
                                   EXISTING EXECUTION
                                           │
                                           ▼
                                   TaskTranslator
                                           │
                                           ▼
                                   ExecutionPolicy
                                           │
                                           ▼
                                   ExecutionRouter
                                           │
                                           ▼
                                     HANDLERS

#Future (next)
                         USER
                           │
                           ▼
                    ┌─────────────┐
                    │  ENTRYGATE  │
                    └──────┬──────┘
                           │
             ┌─────────────┼──────────────┐
             │             │              │
             ▼             ▼              ▼
         OPEN/CLOSE   CONVERSATION   COMPLEX TASK
             │             │              │
             ▼             ▼              ▼
          DIRECT/LLM    DIRECT/LLM   NOVA RUNTIME
                                           │
                                           ▼
                                      PLAN STEPS
                                           │
                                           ▼
                                      STEP ROUTER
                                           │
                              ┌────────────┴────────────┐
                              │                         │
                              ▼                         ▼
                      FAST EXECUTION              TARS / UI
                              │                         │
                              ▼                         ▼
                       TaskTranslator           Visual Executor
                              │                         │
                              ▼                         │
                       ExecutionRouter                  │
                              │                         │
                              └────────────┬────────────┘
                                           ▼
                                      STEP RESULT
                                           │
                                           ▼
                                       NEXT STEP
                                           │
                                           ▼
                                     FINAL RESULT
                                           │
                                           ▼
                                     USER RESPONSE

## Status

### Nova — 21 August 2026

Cognitive pipeline + deterministic execution foundation: substantially implemented and operational.
Hybrid visual execution: next major development phase.
The project is currently transitioning from a planner + deterministic automation system into a hybrid computer-use architecture.

## Author

### Shourya Bhardwaj

Project:

## _NOVA — Personal AI Assistant_

Built with Python, PyQt6, Whisper, openWakeWord, OpenRouter, Ollama, and a modular execution architecture.

```
