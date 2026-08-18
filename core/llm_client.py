import os
from typing import Optional

from dotenv import load_dotenv
from openai import OpenAI

load_dotenv()

OPENROUTER_API_KEY = os.getenv("OPENROUTER_API_KEY")

class LLMClient:
    """
    Nova's unified LLM interface.

    Primary:
        OpenRouter -> NVIDIA Nemotron 3 Ultra

    Fallback:
        Local Ollama -> Qwen3 14B
    """

    PRIMARY_MODEL = "nvidia/nemotron-3-ultra-550b-a55b:free"
    PRIMARY_BASE_URL = "https://openrouter.ai/api/v1"

    FALLBACK_MODEL = "qwen3:14b"
    FALLBACK_BASE_URL = "http://localhost:11434/v1"

    def __init__(self):
        self.primary_client = None
        self.fallback_client = None

        self._initialize_primary()
        self._initialize_fallback()

    # --------------------------------------------------
    # PRIMARY
    # --------------------------------------------------

    def _initialize_primary(self):
        api_key = os.getenv("OPENROUTER_API_KEY")

        if not api_key:
            print("[LLM] OpenRouter API key not found.")
            return

        try:
            self.primary_client = OpenAI(
                base_url=self.PRIMARY_BASE_URL,
                api_key=api_key,
            )

            print(
                "[LLM] Primary provider ready: "
                "OpenRouter / Nemotron 3 Ultra"
            )

        except Exception as exc:
            print(
                f"[LLM] Primary initialization failed: {exc}"
            )

    # --------------------------------------------------
    # FALLBACK
    # --------------------------------------------------

    def _initialize_fallback(self):
        try:
            self.fallback_client = OpenAI(
                base_url=self.FALLBACK_BASE_URL,
                api_key="ollama",
            )

            print(
                "[LLM] Fallback provider configured: "
                "Ollama / Qwen3 14B"
            )

        except Exception as exc:
            print(
                f"[LLM] Fallback initialization failed: {exc}"
            )

    # --------------------------------------------------
    # GENERATION
    # --------------------------------------------------

    def generate(
        self,
        prompt: str,
        system_prompt: Optional[str] = None,
    ) -> str:

        messages = []

        if system_prompt:
            messages.append(
                {
                    "role": "system",
                    "content": system_prompt,
                }
            )

        messages.append(
            {
                "role": "user",
                "content": prompt,
            }
        )

        # ==============================================
        # PRIMARY
        # ==============================================

        if self.primary_client is not None:

            try:

                print(
                    "[LLM] Requesting "
                    "Nemotron 3 Ultra..."
                )

                response = (
                    self.primary_client
                    .chat
                    .completions
                    .create(
                        model=self.PRIMARY_MODEL,
                        messages=messages,
                        temperature=0.3,
                    )
                )

                content = response.choices[0].message.content

                if content:
                    print(
                        "[LLM] Primary response received."
                    )

                    return content.strip()

            except Exception as exc:

                print(
                    "[LLM] Primary failed:"
                    f" {exc}"
                )

        # ==============================================
        # FALLBACK
        # ==============================================

        if self.fallback_client is not None:

            try:

                print(
                    "[LLM] Falling back to "
                    "Ollama / Qwen3 14B..."
                )

                response = (
                    self.fallback_client
                    .chat
                    .completions
                    .create(
                        model=self.FALLBACK_MODEL,
                        messages=messages,
                        temperature=0.3,
                    )
                )

                content = response.choices[0].message.content

                if content:
                    print(
                        "[LLM] Fallback response received."
                    )

                    return content.strip()

            except Exception as exc:

                print(
                    "[LLM] Fallback failed:"
                    f" {exc}"
                )

        return (
            "I'm unable to reach my reasoning systems "
            "right now."
        )