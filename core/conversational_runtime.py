"""
ConversationalRuntime — Nova's Branch 2 handler.

Lightweight runtime for informational and conversational responses.
No step generation, no plan parsing, no execution router.

Just: user prompt → Nemotron LLM → natural language response.

Personality: Playful, slightly cheeky, but always helpful.
Hardcoded system prompt locked for this branch's specific purpose.
"""

import logging

from core.llm_client import LLMClient

logger = logging.getLogger(__name__)


# ──────────────────────────────────────────────────────────
# Branch 2 System Prompt — hardcoded personality + task
# ──────────────────────────────────────────────────────────

CONVERSATIONAL_SYSTEM_PROMPT = """\
You are Nova — a desktop AI assistant with personality.

## Your Personality
- You're helpful, knowledgeable, and a little playful.
- You sprinkle in light humor when it fits, but never at the expense of accuracy.
- You're confident but not arrogant. Think "witty best friend who happens to know everything."
- You keep responses concise — no essays unless the user asks for depth.
- You use casual language but stay professional when the topic demands it.
- When you don't know something, you say so honestly — no making stuff up.

## Your Task
- You are answering questions, explaining concepts, having conversations, and providing information.
- You are NOT controlling the user's computer. You are NOT executing actions.
- You are NOT creating plans or steps. You are just talking.
- Give direct, clear answers. Don't hedge unnecessarily.
- For technical questions, be accurate and practical.
- For casual chat, be warm and engaging.

## Response Style
- Keep it SHORT unless depth is requested. 2-4 sentences for simple questions.
- Use bullet points for lists, but don't overdo formatting.
- No "As an AI..." disclaimers. Just answer naturally.
- If the user asks something you genuinely can't help with (like real-time data you don't have), say so briefly and suggest what they could do instead.
"""


class ConversationalRuntime:
    """Branch 2 runtime — handles informational and conversational queries."""

    def __init__(self, llm_client: LLMClient = None):
        self.llm_client = llm_client or LLMClient()

    def respond(self, command: str) -> dict:
        """Generate a conversational response for the user's query.

        Returns:
            {
                "success": bool,
                "response": str,
                "branch": "conversational",
                "command": str,
            }
        """
        if not command or not command.strip():
            return {
                "success": True,
                "response": "I'm here! What do you want to know?",
                "branch": "conversational",
                "command": command,
            }

        try:
            print(f"[CONVERSATIONAL] Processing: \"{command}\"")

            response = self.llm_client.generate(
                prompt=command,
                system_prompt=CONVERSATIONAL_SYSTEM_PROMPT,
            )

            print(f"[CONVERSATIONAL] Response received ({len(response)} chars)")

            return {
                "success": True,
                "response": response,
                "branch": "conversational",
                "command": command,
            }

        except Exception as exc:
            logger.error("[CONVERSATIONAL] LLM failed: %s", exc)
            print(f"[CONVERSATIONAL] Error: {exc}")

            return {
                "success": False,
                "response": "Hmm, my brain just hiccuped. Try asking again?",
                "branch": "conversational",
                "command": command,
                "error": str(exc),
            }
