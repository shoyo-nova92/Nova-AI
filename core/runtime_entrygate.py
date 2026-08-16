from typing import Dict, Optional

from core.app_search_engine import AppSearchEngine
from core.conversation_handler import ConversationHandler


class RuntimeEntrygate:
    """Central command gateway through which ALL user commands flow.

    Branches:
        1. open <app> / close <app>   -> deterministic, AppSearchEngine (no AI)
        2. greetings / salutations    -> ConversationHandler (offline quick replies)
        3. exit / quit / shutdown     -> runtime exit command
        4. everything else            -> complex goal (TaskTranslator / NovaRuntime)
    """

    ACTION_OPEN = "open"
    ACTION_CLOSE = "close"
    ACTION_CONVERSATION = "conversation"
    ACTION_EXIT = "exit"
    ACTION_RUNTIME = "runtime"
    ACTION_UNKNOWN = "unknown"

    def __init__(
        self,
        app_search_engine: Optional[AppSearchEngine] = None,
        conversation_handler: Optional[ConversationHandler] = None,
    ):
        self.app_search_engine = app_search_engine or AppSearchEngine(auto_index=True)
        self.conversation_handler = conversation_handler or ConversationHandler()

    def classify(self, raw_command: str) -> Dict:
        """Classify a raw user command and extract its deterministic action/target."""
        command = (raw_command or "").strip()
        normalized = command.lower()

        if not normalized:
            return {
                "action": self.ACTION_UNKNOWN,
                "target": None,
                "command": command,
                "normalized": normalized,
            }

        if normalized.startswith("open ") or normalized.startswith("launch ") or normalized.startswith("start "):
            if normalized.startswith("open "):
                target = command[5:].strip()
            elif normalized.startswith("launch "):
                target = command[7:].strip()
            else:
                target = command[6:].strip()

            return {
                "action": self.ACTION_OPEN,
                "target": target,
                "command": command,
                "normalized": normalized,
            }

        if normalized.startswith("close ") or normalized.startswith("kill ") or normalized.startswith("exit "):
            if normalized.startswith("close "):
                target = command[6:].strip()
            elif normalized.startswith("kill "):
                target = command[5:].strip()
            else:
                target = command[5:].strip()

            if target and not self.conversation_handler.is_exit(target):
                return {
                    "action": self.ACTION_CLOSE,
                    "target": target,
                    "command": command,
                    "normalized": normalized,
                }

        if self.conversation_handler.is_exit(normalized):
            return {
                "action": self.ACTION_EXIT,
                "target": None,
                "command": command,
                "normalized": normalized,
            }

        if self.conversation_handler.is_conversational(normalized) or self.conversation_handler.is_thank_you(normalized):
            return {
                "action": self.ACTION_CONVERSATION,
                "target": None,
                "command": command,
                "normalized": normalized,
            }

        return {
            "action": self.ACTION_RUNTIME,
            "target": None,
            "command": command,
            "normalized": normalized,
        }

    def process(self, raw_command: str) -> Dict:
        """Central entry point: classify and execute the command via the fast path or return a runtime goal."""
        classification = self.classify(raw_command)
        action = classification["action"]
        target = classification["target"]
        command = classification["command"]

        if action == self.ACTION_OPEN:
            launch_result = self.app_search_engine.launch_app(target)
            return {
                "success": launch_result.get("success", False),
                "action": self.ACTION_OPEN,
                "target": target,
                "branch": "entrygate_open",
                "result": launch_result,
                "command": command,
                "raw_command": raw_command,
            }

        if action == self.ACTION_CLOSE:
            close_result = self.app_search_engine.close_app(target)
            return {
                "success": close_result.get("success", False),
                "action": self.ACTION_CLOSE,
                "target": target,
                "branch": "entrygate_close",
                "result": close_result,
                "command": command,
                "raw_command": raw_command,
            }

        if action == self.ACTION_CONVERSATION:
            reply = self.conversation_handler.respond(command)
            return {
                "success": True,
                "action": self.ACTION_CONVERSATION,
                "target": None,
                "branch": "entrygate_conversation",
                "result": reply,
                "response": reply.get("response"),
                "command": command,
                "raw_command": raw_command,
            }

        if action == self.ACTION_EXIT:
            reply = self.conversation_handler.respond(command)
            return {
                "success": True,
                "action": self.ACTION_EXIT,
                "target": None,
                "branch": "entrygate_exit",
                "result": reply,
                "response": reply.get("response"),
                "command": command,
                "raw_command": raw_command,
            }

        return {
            "success": True,
            "action": self.ACTION_RUNTIME,
            "target": None,
            "branch": "entrygate_runtime",
            "goal": command or raw_command,
            "command": command,
            "raw_command": raw_command,
        }
