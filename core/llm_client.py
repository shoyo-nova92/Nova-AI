import os
import time
from typing import Optional

from dotenv import load_dotenv
from openai import OpenAI

load_dotenv()


class LLMClient:
    """
    Nova's unified LLM interface.

    Primary:
        OpenRouter -> NVIDIA Nemotron 3 Ultra

    Fallback:
        Local Ollama -> Qwen3 14B

    The provider boundary is intentionally defensive:
        - validates responses
        - handles empty choices
        - handles empty content
        - retries transient primary failures
        - never crashes the caller because of malformed provider output
    """

    PRIMARY_MODEL = "nvidia/nemotron-3-ultra-550b-a55b:free"
    PRIMARY_BASE_URL = "https://openrouter.ai/api/v1"

    FALLBACK_MODEL = "qwen3:14b"
    FALLBACK_BASE_URL = "http://localhost:11434/v1"

    PRIMARY_RETRIES = 2
    PRIMARY_RETRY_DELAY = 1.5

    def __init__(self):
        self.primary_client = None
        self.fallback_client = None

        self._initialize_primary()
        self._initialize_fallback()

    # ==================================================
    # PRIMARY
    # ==================================================

    def _initialize_primary(self):
        api_key = os.getenv("OPENROUTER_API_KEY")

        if not api_key:
            print("[LLM] OpenRouter API key not found.")
            return

        try:
            self.primary_client = OpenAI(
                base_url=self.PRIMARY_BASE_URL,
                api_key=api_key,
                timeout=60.0,
                max_retries=0,
            )

            print(
                "[LLM] Primary provider ready: "
                "OpenRouter / Nemotron 3 Ultra"
            )

        except Exception as exc:
            print(
                f"[LLM] Primary initialization failed: {exc}"
            )

    # ==================================================
    # FALLBACK
    # ==================================================

    def _initialize_fallback(self):
        try:
            self.fallback_client = OpenAI(
                base_url=self.FALLBACK_BASE_URL,
                api_key="ollama",
                timeout=60.0,
                max_retries=0,
            )

            print(
                "[LLM] Fallback provider configured: "
                "Ollama / Qwen3 14B"
            )

        except Exception as exc:
            print(
                f"[LLM] Fallback initialization failed: {exc}"
            )

    # ==================================================
    # RESPONSE EXTRACTION
    # ==================================================

    @staticmethod
    def _extract_content(response) -> Optional[str]:
        """
        Safely extract textual content from an OpenAI-compatible
        chat completion response.

        Never assumes that choices/message/content are present.
        """

        if response is None:
            print("[LLM DEBUG] Response is None.")
            return None

        print(
            f"[LLM DEBUG] Response type: {type(response)}"
        )

        # ----------------------------------------------
        # choices
        # ----------------------------------------------

        choices = getattr(response, "choices", None)

        if not choices:
            print("[LLM DEBUG] Response contains no choices.")

            try:
                print(
                    "[LLM DEBUG] Raw response:",
                    response.model_dump()
                )
            except Exception:
                print(
                    "[LLM DEBUG] Raw response:",
                    repr(response)
                )

            return None

        # ----------------------------------------------
        # first choice
        # ----------------------------------------------

        choice = choices[0]

        if choice is None:
            print("[LLM DEBUG] First choice is None.")
            return None

        # ----------------------------------------------
        # message
        # ----------------------------------------------

        message = getattr(choice, "message", None)

        if message is None:
            print("[LLM DEBUG] Choice message is None.")

            try:
                print(
                    "[LLM DEBUG] Choice:",
                    choice.model_dump()
                )
            except Exception:
                print(
                    "[LLM DEBUG] Choice:",
                    repr(choice)
                )

            return None

        # ----------------------------------------------
        # content
        # ----------------------------------------------

        content = getattr(message, "content", None)

        if content is None:
            print(
                "[LLM DEBUG] Message content is None."
            )

            try:
                print(
                    "[LLM DEBUG] Message:",
                    message.model_dump()
                )
            except Exception:
                print(
                    "[LLM DEBUG] Message:",
                    repr(message)
                )

            return None

        # ----------------------------------------------
        # normal string response
        # ----------------------------------------------

        if isinstance(content, str):
            content = content.strip()

            if content:
                return content

            print("[LLM DEBUG] Message content is empty.")
            return None

        # ----------------------------------------------
        # Unexpected content type
        # ----------------------------------------------

        print(
            "[LLM DEBUG] Unexpected content type:",
            type(content)
        )

        try:
            return str(content).strip() or None
        except Exception:
            return None

    # ==================================================
    # PRIMARY REQUEST
    # ==================================================

    def _generate_primary(self, messages: list) -> Optional[str]:

        if self.primary_client is None:
            return None

        for attempt in range(1, self.PRIMARY_RETRIES + 1):

            try:

                print(
                    f"[LLM] Requesting Nemotron 3 Ultra "
                    f"(attempt {attempt}/{self.PRIMARY_RETRIES})..."
                )

                response = (
                    self.primary_client
                    .chat
                    .completions
                    .create(
                        model=self.PRIMARY_MODEL,
                        messages=messages,

                        # Nova generally does not need
                        # highly random responses.
                        temperature=0.3,

                        # Keep normal conversational/router
                        # responses bounded.
                        max_tokens=2048,

                        # Nemotron is a reasoning model.
                        # We don't need reasoning tokens
                        # returned to Nova's application layer.
                        extra_body={
                            "reasoning": {
                                "effort": "low",
                                "exclude": True,
                            }
                        },
                    )
                )

                content = self._extract_content(response)

                if content:
                    print(
                        "[LLM] Primary response received."
                    )
                    return content

                print(
                    "[LLM] Primary returned no usable content."
                )

            except Exception as exc:

                print(
                    f"[LLM] Primary attempt {attempt} failed: "
                    f"{type(exc).__name__}: {exc}"
                )

                if attempt < self.PRIMARY_RETRIES:
                    print(
                        f"[LLM] Retrying Nemotron in "
                        f"{self.PRIMARY_RETRY_DELAY}s..."
                    )

                    time.sleep(
                        self.PRIMARY_RETRY_DELAY
                    )

        return None

    # ==================================================
    # GENERATION
    # ==================================================

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

        # ==================================================
        # PRIMARY — OpenRouter / Nemotron
        # ==================================================

        if self.primary_client is not None:

            max_attempts = 2

            for attempt in range(1, max_attempts + 1):

                try:

                    print(
                        f"[LLM] Requesting Nemotron 3 Ultra "
                        f"(attempt {attempt}/{max_attempts})..."
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

                    print(
                        f"[LLM DEBUG] Response type: "
                        f"{type(response)}"
                    )

                    # ------------------------------------------
                    # Validate response structure
                    # ------------------------------------------

                    if response is None:
                        print(
                            "[LLM DEBUG] Response is None."
                        )

                    elif not getattr(response, "choices", None):
                        print(
                            "[LLM DEBUG] Response contains "
                            "no choices."
                        )

                        try:
                            print(
                                "[LLM DEBUG] Raw response:",
                                response.model_dump()
                            )
                        except Exception:
                            print(
                                "[LLM DEBUG] Raw response:",
                                response
                            )

                    else:

                        content = response.choices[0].message.content

                        if content:
                            print(
                                "[LLM] Primary response received."
                            )

                            return content.strip()

                        print(
                            "[LLM DEBUG] Choice contains "
                            "no usable content."
                        )

                except Exception as exc:

                    print(
                        f"[LLM] Primary attempt "
                        f"{attempt}/{max_attempts} failed: {exc}"
                    )

                # ------------------------------------------
                # Retry delay
                # ------------------------------------------

                if attempt < max_attempts:
                    print(
                        "[LLM] Retrying Nemotron..."
                    )

                    import time
                    time.sleep(1.0)

        # ==================================================
        # FALLBACK — Ollama / Qwen
        # ==================================================

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

                print(
                    f"[LLM DEBUG] Fallback response type: "
                    f"{type(response)}"
                )

                if not getattr(response, "choices", None):
                    print(
                        "[LLM DEBUG] Fallback response "
                        "contains no choices."
                    )

                else:

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

        # ==================================================
        # TOTAL FAILURE
        # ==================================================

        return (
            "I'm unable to reach my reasoning systems "
            "right now."
        )