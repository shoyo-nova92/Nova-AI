"""
InputRouter — Nova's central command intelligence layer.

Sits between RuntimeEntrygate and execution backends.
When EntryGate returns action="runtime" (not a simple open/close/greet/exit),
InputRouter uses a single Nemotron LLM call to classify the command into
one of three branches:

    Branch 1: fast_action    — simple app/system commands that need no thinking
    Branch 2: conversational — questions, info requests, explanations, chat
    Branch 3: complex_task   — multi-step agentic goals needing planning
"""

import json
import logging

from core.llm_client import LLMClient

logger = logging.getLogger(__name__)


# ──────────────────────────────────────────────────────────
# Classification prompt — hardcoded and locked
# ──────────────────────────────────────────────────────────

ROUTER_SYSTEM_PROMPT = """\
You are Nova's InputRouter — a command classification engine.

Your ONLY job is to read the user's command and decide which branch it belongs to.
You must respond with ONLY a valid JSON object. No markdown, no explanation, no extra text.

## Branches

### fast_action
Simple, direct commands that need NO thinking or planning.
These are clear-cut actions like opening/closing apps, launching websites,
toggling settings, or basic system operations.

Examples:
- "open chrome and go to youtube"
- "close all browsers"
- "launch spotify"
- "minimize everything"
- "open file explorer"
- "open youtube music"
- "restart explorer"

### conversational
Questions, information requests, explanations, opinions, chat, or anything
where the user wants a TEXT ANSWER — not an action performed on their computer.
Also includes commands that are unclear and need clarification.

Examples:
- "what is quantum computing?"
- "explain python decorators"
- "who won the 2022 world cup?"
- "what's the weather like?"
- "how do I use git rebase?"
- "tell me a joke"
- "what time is it in Tokyo?"
- "compare React and Vue"
- "what does this error mean: ModuleNotFoundError"
- "summarize what machine learning is"

### complex_task
Multi-step tasks that require planning, execution of multiple actions,
file manipulation, coding, project setup, or any goal that needs
sequential steps to accomplish.

Examples:
- "create a new python project with tests and push to github"
- "refactor the code in main.py to use classes"
- "set up a virtual environment and install requirements"
- "find all TODO comments in my project and list them"
- "build a flask API with three endpoints"
- "debug why my test is failing and fix it"
- "organize my desktop files into folders by type"

## Rules
1. If the command is CLEARLY just asking for information → conversational
2. If the command is a simple direct action (open/close/launch) → fast_action
3. If the command requires MULTIPLE steps or file/code work → complex_task
4. If you're unsure → conversational (safest default)
5. A command that says "open X" but X is not an app (e.g., "open a new project") → complex_task
6. If the user asks "how to do X" → conversational (they want an explanation, not execution)
7. If the user says "do X" where X is multi-step → complex_task

## Response Format
Respond with ONLY this JSON (no markdown fences, no extra text):
{"branch": "<fast_action|conversational|complex_task>", "confidence": <0.0-1.0>, "reasoning": "<one short sentence>"}
"""


class InputRouter:
    """Classifies non-trivial commands into one of three execution branches."""

    BRANCH_FAST_ACTION = "fast_action"
    BRANCH_CONVERSATIONAL = "conversational"
    BRANCH_COMPLEX_TASK = "complex_task"

    VALID_BRANCHES = {BRANCH_FAST_ACTION, BRANCH_CONVERSATIONAL, BRANCH_COMPLEX_TASK}

    def __init__(self, llm_client: LLMClient = None):
        self.llm_client = llm_client or LLMClient()

    def classify(self, command: str) -> dict:
        """Classify a command into a branch using Nemotron.

        Returns:
            {
                "branch": "fast_action" | "conversational" | "complex_task",
                "confidence": float,
                "reasoning": str,
                "raw_response": str,
            }
        """
        if not command or not command.strip():
            return self._default_result("empty command")

        normalized = command.strip().lower()

        # ── Hardcoded fast-exits (no LLM needed) ──────────

        # Pure open/close/launch that somehow got past entrygate
        simple_prefixes = ("open ", "close ", "launch ", "start ", "kill ")
        if any(normalized.startswith(p) for p in simple_prefixes):
            # But check if it's actually complex ("open a new project")
            complex_indicators = (
                "and ", "then ", "with ", "project", "file", "folder",
                "create", "setup", "set up", "build", "new ",
            )
            if not any(ind in normalized for ind in complex_indicators):
                logger.info("[ROUTER] Hardcoded fast_action for: %s", command)
                return {
                    "branch": self.BRANCH_FAST_ACTION,
                    "confidence": 1.0,
                    "reasoning": "simple open/close command (hardcoded match)",
                    "raw_response": None,
                }

        # ── LLM classification ────────────────────────────

        try:
            print(f"[ROUTER] Classifying command: \"{command}\"")

            raw_response = self.llm_client.generate(
                prompt=f"Classify this command:\n{command}",
                system_prompt=ROUTER_SYSTEM_PROMPT,
            )

            print(f"[ROUTER] Raw LLM response: {raw_response}")

            result = self._parse_response(raw_response)
            result["raw_response"] = raw_response
            return result

        except Exception as exc:
            logger.error("[ROUTER] LLM classification failed: %s", exc)
            print(f"[ROUTER] Classification failed: {exc}. Defaulting to conversational.")
            return self._default_result(f"LLM error: {exc}")

    # ──────────────────────────────────────────────────────
    # Response parsing
    # ──────────────────────────────────────────────────────

    def _parse_response(self, raw: str) -> dict:
        """Extract branch/confidence/reasoning from the LLM JSON response."""
        if not raw:
            return self._default_result("empty LLM response")

        # Strip markdown fences if model wraps in ```json ... ```
        cleaned = raw.strip()
        if cleaned.startswith("```"):
            lines = cleaned.split("\n")
            # Remove first and last lines (fences)
            lines = [l for l in lines if not l.strip().startswith("```")]
            cleaned = "\n".join(lines).strip()

        try:
            data = json.loads(cleaned)
        except json.JSONDecodeError:
            # Try to find JSON object in the response
            start = cleaned.find("{")
            end = cleaned.rfind("}") + 1
            if start != -1 and end > start:
                try:
                    data = json.loads(cleaned[start:end])
                except json.JSONDecodeError:
                    return self._default_result("could not parse LLM response as JSON")
            else:
                return self._default_result("no JSON found in LLM response")

        branch = data.get("branch", "").strip().lower()
        confidence = data.get("confidence", 0.5)
        reasoning = data.get("reasoning", "no reasoning provided")

        if branch not in self.VALID_BRANCHES:
            print(f"[ROUTER] Unknown branch '{branch}', defaulting to conversational")
            branch = self.BRANCH_CONVERSATIONAL

        # Clamp confidence
        try:
            confidence = max(0.0, min(1.0, float(confidence)))
        except (ValueError, TypeError):
            confidence = 0.5

        return {
            "branch": branch,
            "confidence": confidence,
            "reasoning": reasoning,
        }

    def _default_result(self, reason: str) -> dict:
        """Safest default: conversational (won't break anything)."""
        return {
            "branch": self.BRANCH_CONVERSATIONAL,
            "confidence": 0.0,
            "reasoning": f"default fallback — {reason}",
            "raw_response": None,
        }
