from PyQt6.QtCore import QTimer
from PyQt6.QtWidgets import QApplication

import json
import os
import queue
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone



class InputNormalizer:
    """Normalize incoming user commands for consistent intake."""

    @staticmethod
    def normalize(command: str) -> str:
        if not command:
            return ""

        cleaned = " ".join(str(command).strip().split())
        return cleaned.lower()


class SessionManager:
    """Persistent per-session command intake recorder."""

    def __init__(self, session_file="memory/input_layer_session.json"):
        self.session_file = session_file
        os.makedirs(os.path.dirname(session_file), exist_ok=True)
        self.session = self._load_session()

    def _load_session(self):
        if not os.path.exists(self.session_file):
            return {
                "session_id": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
                "turns": [],
                "created_at": datetime.now(timezone.utc).isoformat(),
            }

        try:
            with open(self.session_file, "r", encoding="utf-8") as file:
                data = json.load(file)
            data.setdefault("turns", [])
            return data
        except Exception:
            return {
                "session_id": datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ"),
                "turns": [],
                "created_at": datetime.now(timezone.utc).isoformat(),
            }

    def record_turn(self, source: str, command: str):
        turn = {
            "source": source,
            "command": command,
            "received_at": datetime.now(timezone.utc).isoformat(),
        }
        self.session.setdefault("turns", []).append(turn)
        self._save_session()
        return turn

    def _save_session(self):
        with open(self.session_file, "w", encoding="utf-8") as file:
            json.dump(self.session, file, indent=2)


class NovaInputLayer:
    """Branch 1: receive commands only. No execution yet."""

    def __init__(self):
        self.normalizer = InputNormalizer()
        self.session_manager = SessionManager()

        self.voice_engine = None
        self.wake_engine = None
        self._voice_class = None
        self._wake_class = None

        try:
            from core.voice import VoiceEngine
            self._voice_class = VoiceEngine
        except Exception:
            self._voice_class = None

        try:
            from core.wake_local import LocalWake
            self._wake_class = LocalWake
        except Exception:
            self._wake_class = None

    def receive_text(self, raw_text: str):
        command = self.normalizer.normalize(raw_text)

        if not command:
            return {
                "status": "empty",
                "source": "text",
                "command": "",
            }

        turn = self.session_manager.record_turn("text", command)
        return {
            "status": "received",
            "source": "text",
            "command": command,
            "turn": turn,
        }

    def receive_voice(self):
        if self._voice_class is None:
            return {
                "status": "unavailable",
                "source": "voice",
                "command": "",
                "reason": "voice model is not available in this session",
            }

        try:
            if self.voice_engine is None:
                self.voice_engine = self._voice_class()

            command = self.voice_engine.listen()
            return self.receive_text(command)
        except Exception as exc:
            return {
                "status": "error",
                "source": "voice",
                "command": "",
                "reason": str(exc),
            }

    def listen_for_wake_word(self):
        if self._wake_class is None:
            return {
                "status": "unavailable",
                "source": "wake_word",
                "command": "",
                "reason": "wake-word model is not available in this session",
            }

        try:
            if self.wake_engine is None:
                self.wake_engine = self._wake_class()

            detected = self.wake_engine.listen_for_nova()
            if not detected:
                return {
                    "status": "idle",
                    "source": "wake_word",
                    "command": "",
                }
            return {
                "status": "wake_detected",
                "source": "wake_word",
                "command": self.normalizer.normalize(detected),
            }
        except Exception as exc:
            return {
                "status": "error",
                "source": "wake_word",
                "command": "",
                "reason": str(exc),
            }

    def get_session_preview(self):
        turns = self.session_manager.session.get("turns", [])
        return turns[-5:] if turns else []


class CommandClassifier:
    """Lightweight categorizer for branch-1 intake."""

    EXIT_KEYWORDS = ("exit", "quit", "bye", "shutdown", "terminate")
    CONVERSATION_KEYWORDS = ("hi", "hello", "hey", "thanks", "thank you")
    UNDERSTANDING_KEYWORDS = (
        "what am i working on",
        "what am i doing",
        "what are you working on",
        "status",
        "what is the current context",
        "what is my current context",
        "what is happening"
    )

    @staticmethod
    def classify(command: str):
        normalized = InputNormalizer.normalize(command)
        if not normalized:
            return "unknown"

        if any(keyword in normalized for keyword in CommandClassifier.EXIT_KEYWORDS):
            return "exit"

        if any(keyword in normalized for keyword in CommandClassifier.CONVERSATION_KEYWORDS):
            return "conversation"

        if any(keyword in normalized for keyword in CommandClassifier.UNDERSTANDING_KEYWORDS):
            return "understanding"

        if normalized.startswith("open ") or normalized.startswith("launch "):
            return "open"

        return "unknown"


class FilesystemObserver:
    """Collect a small workspace snapshot for the observation layer."""

    def snapshot(self, root_path=".", max_depth=2, max_items=20):
        try:
            root = os.path.abspath(root_path)
            folders = []
            files = []

            for current_root, dirnames, filenames in os.walk(root):
                depth = current_root.count(os.sep) - root.count(os.sep)
                if depth > max_depth:
                    dirnames[:] = []
                    continue

                dirnames[:] = sorted(dirnames)[:10]
                filenames = sorted(filenames)[:10]

                for name in dirnames:
                    folders.append(os.path.relpath(os.path.join(current_root, name), root))

                for name in filenames:
                    files.append(os.path.relpath(os.path.join(current_root, name), root))

                if len(folders) + len(files) >= max_items:
                    break

            return {
                "status": "success",
                "root": root,
                "folders": folders[:max_items],
                "files": files[:max_items],
            }
        except Exception as exc:
            return {
                "status": "error",
                "reason": str(exc),
            }


class ClipboardObserver:
    """Optional clipboard observer for the desktop snapshot."""

    def read(self):
        try:
            import pyperclip
            text = pyperclip.paste()
            return {
                "status": "success",
                "text": text[:500] if text else "",
            }
        except Exception:
            return {
                "status": "unavailable",
                "reason": "clipboard support not available in this environment",
            }


class ObservationLayer:
    """Branch 2: observe active desktop context and project state."""

    def __init__(self):
        self.vision = None
        self.desktop = None
        self.project_context = None
        self.filesystem = FilesystemObserver()
        self.clipboard = ClipboardObserver()

        try:
            from core.vision_engine import VisionEngine
            self.vision = VisionEngine()
        except Exception:
            self.vision = None

        try:
            from core.desktop_observer import DesktopObserver
            self.desktop = DesktopObserver()
        except Exception:
            self.desktop = None

        try:
            from core.project_context_engine import ProjectContextEngine
            self.project_context = ProjectContextEngine()
        except Exception:
            self.project_context = None

    def observe(self):
        snapshot = {
            "active_window": None,
            "running_processes": [],
            "vision": None,
            "project_context": None,
            "filesystem": self.filesystem.snapshot(),
            "clipboard": self.clipboard.read(),
        }

        if self.desktop is not None:
            try:
                snapshot["active_window"] = self.desktop.get_active_window()
                snapshot["running_processes"] = self.desktop.get_running_apps()
            except Exception:
                snapshot["active_window"] = {"success": False, "reason": "desktop observer unavailable"}
                snapshot["running_processes"] = {"success": False, "reason": "desktop observer unavailable"}

        if self.vision is not None:
            try:
                snapshot["vision"] = self.vision.analyze_screen()
            except Exception as exc:
                snapshot["vision"] = {"status": "error", "reason": str(exc)}

        if self.project_context is not None:
            try:
                snapshot["project_context"] = self.project_context.get_context()
            except Exception as exc:
                snapshot["project_context"] = {"status": "error", "reason": str(exc)}

        return snapshot



class UnderstandingHandler:
    """Turns observation into a short understanding answer for status-style queries."""

    @staticmethod
    def respond(command: str, observation_snapshot: dict):
        project_context = observation_snapshot.get("project_context") or {}
        active_window = observation_snapshot.get("active_window") or {}
        vision = observation_snapshot.get("vision") or {}
        active_title = "unknown"
        if isinstance(active_window, dict):
            active_title = active_window.get("window_title") or active_window.get("title") or active_title
        elif active_window:
            active_title = str(active_window)

        visible_text = vision.get("visible_text") or {}
        visible_lines = []
        if isinstance(visible_text, dict) and "text" in visible_text:
            visible_lines = visible_text.get("text", [])

        project_name = project_context.get("project") or "unknown"
        repository = project_context.get("repository") or "unknown"
        branch = project_context.get("branch") or "unknown"

        answer = (
            f"You are working in the {project_name} project, repository {repository}, branch {branch}. "
            f"The current active window appears to be {active_title}."
        )

        if visible_lines:
            answer += f" I also detected visible desktop text including: {', '.join(str(item) for item in visible_lines[:4])}."

        return {
            "branch": "understanding",
            "status": "handled",
            "category": "understanding",
            "response": answer,
            "observation": observation_snapshot,
        }


class RuntimeCommandHandler:
    """Handles exit/quit/bye runtime commands."""

    @staticmethod
    def respond(command: str):
        return {
            "branch": "runtime_command",
            "status": "handled",
            "command": command,
            "action": "exit",
        }


class ApplicationCommandHandler:
    """Minimal app launcher for the first working open-command branch."""

    KNOWN_APPS = {
        "notepad": "notepad.exe",
        "calculator": "calc.exe",
        "vscode": "Code.exe",
        "code": "Code.exe",
        "chrome": "chrome.exe",
        "edge": "msedge.exe",
    }

    def __init__(self):
        try:
            from core.application_handler import ApplicationHandler
            self.application_handler = ApplicationHandler()
        except Exception:
            self.application_handler = None

    def open(self, target: str):
        normalized_target = InputNormalizer.normalize(target)
        if not normalized_target:
            return {"success": False, "reason": "target is required"}

        if self.application_handler is not None:
            if normalized_target in {"vscode", "code"}:
                return self.application_handler.open_app("vscode")
            if normalized_target in {"notepad", "calculator"}:
                return self.application_handler.open_app(normalized_target)

        executable = self.KNOWN_APPS.get(normalized_target)
        if executable:
            resolved = shutil.which(executable)
            if resolved:
                subprocess.Popen(resolved)
                return {"success": True, "action": f"open {normalized_target}"}

            try:
                subprocess.Popen(executable)
                return {"success": True, "action": f"open {normalized_target}"}
            except Exception as exc:
                return {"success": False, "reason": str(exc)}

        resolved = shutil.which(normalized_target)
        if resolved:
            subprocess.Popen(resolved)
            return {"success": True, "action": f"open {normalized_target}"}

        return {"success": False, "reason": f"open target '{normalized_target}' is not supported yet"}


class BranchRouter:
    """Routes the request into the appropriate branch handler."""

    def __init__(self):
        self.application_handler = ApplicationCommandHandler()
        self.conversation_handler = ConversationHandler()
        self.understanding_handler = UnderstandingHandler()
        self.runtime_command_handler = RuntimeCommandHandler()

    def route(self, command: str, observation_snapshot: dict):
        category = CommandClassifier.classify(command)

        if category == "exit":
            return self.runtime_command_handler.respond(command)

        if category == "conversation":
            return self.conversation_handler.respond(command)

        if category == "understanding":
            return self.understanding_handler.respond(command, observation_snapshot)

        if category == "open":
            target = command.split("open", 1)[1].strip() if command.startswith("open") else command.strip()
            result = self.application_handler.open(target)
            return {
                "branch": "application_handler",
                "status": "handled",
                "category": "open",
                "result": result,
                "observation": observation_snapshot,
            }

        return {
            "branch": "unknown",
            "status": "received",
            "category": "unknown",
            "observation": observation_snapshot,
        }


class NovaRuntimeSpine:
    """Frontend integration layer for Nova.
    Responsibilities:
    1. Receive user input
    2. Send through ONE RuntimeEntrygate for classification
    3. Execute fast branches directly (open/close/conversation/exit)
    4. Send complex goals to ONE canonical core.NovaRuntime.process_goal()
    5. Expose results to UI handle_runtime_result()
    """

    def __init__(self):
        from core.runtime_entrygate import RuntimeEntrygate
        from core.app_search_engine import AppSearchEngine
        from core.conversation_handler import ConversationHandler
        from core.input_router import InputRouter
        from core.conversational_runtime import ConversationalRuntime
        from core.nova_runtime import NovaRuntime
        from core.llm_client import LLMClient

        self.input_layer = NovaInputLayer()

        # --------------------------------------------------
        # ONE SHARED LLM CLIENT
        # --------------------------------------------------

        self.llm_client = LLMClient()

        # --------------------------------------------------
        # ENTRYGATE
        # --------------------------------------------------

        self.entrygate = RuntimeEntrygate(
            app_search_engine=AppSearchEngine(auto_index=True),
            conversation_handler=ConversationHandler(),
        )

        # --------------------------------------------------
        # ROUTING / CONVERSATION / RUNTIME
        # ALL SHARE THE SAME LLM CLIENT
        # --------------------------------------------------

        self.input_router = InputRouter(
            llm_client=self.llm_client
        )

        self.conversational_runtime = ConversationalRuntime(
            llm_client=self.llm_client
        )

        self.nova_runtime = NovaRuntime(
            llm_client=self.llm_client
        )

        self.voice_engine = None
        self.state = "PENDING"

    def get_voice_engine(self):
        if self.voice_engine is None:
            try:
                from core.voice import VoiceEngine
                self.voice_engine = VoiceEngine()
            except Exception:
                self.voice_engine = None
        return self.voice_engine

    def handle_command(self, raw_text: str):
        if not raw_text:
            return {"status": "empty", "state": "PENDING"}

        self.state = "PENDING"
        received = self.input_layer.receive_text(raw_text)
        if received.get("status") != "received":
            return {"status": received.get("status"), "state": self.state}

        # Step 0: ONE RuntimeEntrygate — single top-level classification
        self.state = "ENTRYGATE"
        command = received.get("command", "")
        entrygate_result = self.entrygate.process(command)
        entrygate_action = entrygate_result.get("action", "runtime")

        # Fast path: skip planner entirely for open / close / conversation / exit
        if entrygate_action in {"open", "close", "conversation", "exit"}:
            self.state = "EXECUTING"
            execution = {
                "state": "completed",
                "execution": {
                    "success": entrygate_result.get("success", False),
                    "action": entrygate_action,
                    "result": entrygate_result.get("result"),
                },
                "verification": {
                    "success": entrygate_result.get("success", True),
                    "reason": f"entrygate fast-path: {entrygate_action}",
                },
            }
            self.state = "VERIFYING"
            verification = execution.get("verification")
            self.state = "LEARNING"
            return {
                "status": "handled",
                "state": "COMPLETED",
                "received": received,
                "entrygate": entrygate_result,
                "category": entrygate_action,
                "plan": [command],
                "translated": None,
                "execution": execution,
                "verification": verification,
                "response": entrygate_result.get("response"),
            }

        # ── InputRouter: classify into Branch 1/2/3 ──────────────
        goal = entrygate_result.get("goal") or command or raw_text
        self.state = "ROUTING"
        router_result = self.input_router.classify(goal)
        branch = router_result.get("branch", "conversational")
        confidence = router_result.get("confidence", 0.0)
        reasoning_text = router_result.get("reasoning", "")

        print(f"[ROUTER] Branch: {branch} | Confidence: {confidence} | Reasoning: {reasoning_text}")

        # ── BRANCH 1: fast_action ────────────────────────────────
        if branch == "fast_action":
            self.state = "FAST_ACTION"
            print(f"[BRANCH 1] Fast action for: {goal}")

            # Re-run through entrygate to attempt the action
            fast_result = self.entrygate.process(goal)
            fast_success = fast_result.get("success", False)

            if fast_success:
                self.state = "COMPLETED"
                execution = {
                    "state": "completed",
                    "execution": {
                        "success": True,
                        "action": fast_result.get("action", "fast_action"),
                        "result": fast_result.get("result"),
                    },
                    "verification": {
                        "success": True,
                        "reason": "fast_action branch succeeded",
                    },
                }
                return {
                    "status": "handled",
                    "state": "COMPLETED",
                    "received": received,
                    "entrygate": fast_result,
                    "category": "fast_action",
                    "plan": [goal],
                    "translated": None,
                    "execution": execution,
                    "verification": execution.get("verification"),
                    "response": fast_result.get("response"),
                    "router": router_result,
                }
            else:
                # Escalate to Branch 3
                print("[BRANCH 1] Simple action failed, proceeding with complex planning.")
                branch = "complex_task"

        # ── BRANCH 2: conversational ─────────────────────────────
        if branch == "conversational":
            self.state = "CONVERSATIONAL"
            print(f"[BRANCH 2] Conversational response for: {goal}")

            convo_result = self.conversational_runtime.respond(goal)
            response_text = convo_result.get("response", "")

            print(f"[BRANCH 2] Nova says: {response_text[:200]}..." if len(response_text) > 200 else f"[BRANCH 2] Nova says: {response_text}")

            self.state = "COMPLETED"
            return {
                "status": "handled",
                "state": "COMPLETED",
                "received": received,
                "entrygate": entrygate_result,
                "category": "conversational",
                "plan": None,
                "translated": None,
                "execution": None,
                "verification": {"success": True, "reason": "conversational branch"},
                "response": response_text,
                "router": router_result,
                "snapshot": None,
                "understanding": None,
            }

        # ── BRANCH 3: complex_task ───────────────────────────────
        self.state = "COMPLEX_TASK"
        print(f"[BRANCH 3] Complex task planning for: {goal}")

        runtime_result = self.nova_runtime.process_goal(goal)
        runtime_status = runtime_result.get("status")
        self.state = str(runtime_status) if runtime_status else "COMPLETED"

        # Bridge canonical NovaRuntime schema to the UI-layer schema handle_runtime_result expects
        executions_list = runtime_result.get("executions") or []
        execution_last = executions_list[-1] if isinstance(executions_list, list) and executions_list else None
        execution = execution_last if isinstance(execution_last, dict) else {
            "execution": {"success": runtime_result.get("success", True), "action": "runtime"},
            "verification": runtime_result.get("verification") or {"success": runtime_result.get("success", True)},
        }
        verification = execution.get("verification") if isinstance(execution, dict) else (runtime_result.get("verification") or {"success": True})
        snapshot = runtime_result.get("vision")
        context = runtime_result.get("context")
        reasoning = runtime_result.get("reasoning")
        plan = runtime_result.get("raw_plan") or [goal]
        translated = runtime_result.get("validated_plan") or runtime_result.get("repaired_plan") or runtime_result.get("parsed_plan")
        understanding = {
            "activity": reasoning.get("current_activity") if isinstance(reasoning, dict) else None,
            "suggested_actions": reasoning.get("suggested_actions") if isinstance(reasoning, dict) else [],
            "project": context.get("project") if isinstance(context, dict) else None,
            "focus": context.get("focus") if isinstance(context, dict) else None,
            "workflow_stage": context.get("workflow_stage") if isinstance(context, dict) else None,
            "environment_summary": context.get("environment_summary") if isinstance(context, dict) else None,
        }

        print(f"[BRANCH 3] Plan steps: {plan}")

        return {
            "status": "handled",
            "state": runtime_result.get("status", "COMPLETED"),
            "received": received,
            "snapshot": snapshot,
            "reasoning": reasoning,
            "context": context,
            "understanding": understanding,
            "category": "complex_task",
            "entrygate": entrygate_result,
            "plan": plan,
            "translated": translated,
            "execution": execution,
            "verification": verification,
            "runtime_result": runtime_result,
            "router": router_result,
        }
        
    def ask_llm(self, prompt: str) -> str:
        """
        Send a reasoning request through Nova's LLM provider chain.

        Primary:
            OpenRouter / Nemotron 3 Ultra

        Fallback:
            Ollama / Qwen3
        """

        if not prompt:
            return ""

        return self.llm_client.generate(
            prompt=prompt,
            system_prompt=(
                "You are Nova, a desktop AI assistant. "
                "Be concise, direct, and useful. "
                "Do not invent actions you did not perform. "
                "When answering simple questions, keep the response short."
            ),
        )
def handle_runtime_result(orb, spine, result):
    status = result.get("status", "unknown")
    category = result.get("category", "unknown")
    entrygate = result.get("entrygate") or {}
    router = result.get("router") or {}
    branch = entrygate.get("branch", "branch_unknown")

    if category in {"open", "close"}:
        branch = entrygate.get("branch", "application_handler")
    elif category == "conversation":
        branch = entrygate.get("branch", "conversation")
    elif category == "conversational":
        branch = "input_router_conversational"
    elif category == "fast_action":
        branch = "input_router_fast_action"
    elif category == "complex_task":
        branch = "input_router_complex_task"
    elif category == "understanding":
        branch = "understanding"
    elif category == "exit":
        branch = entrygate.get("branch", "runtime_command")

    if status == "handled":
        orb.set_state("Observing", (255, 170, 0))

        print("INPUT ACCEPTED:", result["received"]["command"])
        print("INTENT CATEGORY:", category)
        print("DISPATCHED BRANCH:", branch)

        if router:
            print("ROUTER:", router)

        if entrygate:
            print("ENTRYGATE:", entrygate)

        print("OBSERVATION SNAPSHOT:", result.get("snapshot"))
        print("UNDERSTANDING:", result.get("understanding"))
        print("STATE:", result["state"])

        if category == "conversation":
            orb.set_state("Conversation", (180, 0, 255))

            voice_engine = spine.get_voice_engine()
            reply = (
                result.get("response")
                or "Hello! How can I help?"
            )

            print("NOVA REPLY:", reply)

            if voice_engine is not None:
                voice_engine.speak(reply)

        elif category == "conversational":
            # Branch 2: Informational / conversational via InputRouter
            orb.set_state("Thinking", (0, 190, 255))

            voice_engine = spine.get_voice_engine()
            reply = result.get("response") or "Hmm, I'm not sure about that."

            print("NOVA REPLY (BRANCH 2):", reply)

            if voice_engine is not None:
                voice_engine.speak(reply)

        elif category == "fast_action":
            # Branch 1: No-BS fast action via InputRouter
            orb.set_state("Executing", (0, 220, 120))
            print("FAST ACTION COMPLETED")

        elif category == "complex_task":
            # Branch 3: Complex step-wise task via InputRouter -> NovaRuntime
            orb.set_state("Planning", (255, 140, 0))
            plan = result.get("plan")
            print("COMPLEX TASK PLAN:", plan)

        elif category == "understanding":
            orb.set_state("Understanding", (0, 190, 255))

            voice_engine = spine.get_voice_engine()

            if voice_engine is not None:
                voice_engine.speak(
                    result.get(
                        "response",
                        "I am checking the current situation."
                    )
                )

        elif category == "exit":
            orb.set_state("Bye", (255, 50, 50))

            voice_engine = spine.get_voice_engine()
            reply = result.get("response") or "Shutting down"

            if voice_engine is not None:
                voice_engine.speak(reply)

            QTimer.singleShot(
                400,
                lambda: QApplication.instance().quit()
            )

            return

        else:
            orb.set_state("Received", (0, 220, 120))

        QTimer.singleShot(
            900,
            lambda: orb.set_state(
                "Listening",
                (0, 220, 120)
            )
        )

    elif status == "empty":
        orb.set_state("Idle", (0, 120, 255))

    else:
        orb.set_state("Error", (255, 50, 50))

        QTimer.singleShot(
            900,
            lambda: orb.set_state(
                "Listening",
                (0, 220, 120)
            )
        )


def handle_submit(orb, spine):
    raw_text = orb.get_text_command()

    if not raw_text:
        return

    result = spine.handle_command(raw_text)

    handle_runtime_result(
        orb,
        spine,
        result
    )


class UIEventQueue:
    """Thread-safe queue for UI actions (show, hide, set_state)."""

    def __init__(self):
        self.q = queue.Queue()

    def show_ui(self):
        self.q.put(("show", None))

    def hide_ui(self):
        self.q.put(("hide", None))

    def set_state(self, text, color):
        self.q.put(("set_state", (text, color)))


def poll_ui_queue(orb, ui_queue):
    while True:
        try:
            action, data = ui_queue.q.get_nowait()
            if action == "show":
                orb.show()
                orb.raise_()
                orb.activateWindow()
            elif action == "hide":
                orb.hide()
            elif action == "set_state":
                text, color = data
                orb.set_state(text, color)
        except Exception:
            break

    QTimer.singleShot(100, lambda: poll_ui_queue(orb, ui_queue))


def voice_wake_worker(spine, ui_queue, command_queue):
    wake_cls = spine.input_layer._wake_class
    voice_engine = spine.get_voice_engine()

    wake_engine = None
    key_monitor = None

    try:
        from core.wake_local import LocalWake, WakeKeyMonitor
        if wake_cls is not None:
            wake_engine = LocalWake(
            wakeword_models=["hey_jarvis"],
            inference_framework="onnx",
            threshold=0.35,
        )
        else:
            wake_engine = None
        key_monitor = WakeKeyMonitor(key_name="v")
    except Exception as exc:
        print(f"Wake listener setup warning: {exc}")

    print("[NOVA IDLE] Waiting for wake word ('hey jarvis') or activation key ('V'). UI is currently hidden.")

    while True:
        try:
            # Branch 2: Check V key press
            v_key_pressed = False

            if key_monitor is not None and key_monitor.was_pressed():
                v_key_pressed = True
                print("[V KEY PRESSED] Activating listening state directly.")

            # Branch 1: Check Wake Word detection
            wake_label = None
            if not v_key_pressed and wake_engine is not None:
                detected = wake_engine.listen_for_nova(timeout=0.1)

                if detected:
                    wake_label = detected

            if v_key_pressed or wake_label:
                # Wakeword TTS reply (Branch 1)
                if wake_label:
                    print(f"Wake word detected: {wake_label}")
                    if voice_engine is not None:
                        voice_engine.speak("hey there, how do i help?")
                    else:
                        print("Nova (TTS): hey there, how do i help?")

                # Pop up UI & set state to Listening!
                ui_queue.show_ui()
                ui_queue.set_state("Listening", (0, 220, 120))

                # Now in Listening state, listen for actual input command!
                spoken_command = ""
                if voice_engine is not None:
                    spoken_command = voice_engine.listen()

                cleaned = InputNormalizer.normalize(spoken_command)
                if cleaned:
                    command_queue.put(cleaned)

                time.sleep(0.1)
            else:
                time.sleep(0.05)

        except Exception as exc:
            print(f"Voice wake worker loop error: {exc}")
            time.sleep(0.1)



def poll_voice_queue(orb, spine, command_queue):
    while True:
        try:
            command = command_queue.get_nowait()

            # Show exactly what Whisper recognized in the Orb textbox.
            if hasattr(orb, "set_text_command"):
                orb.set_text_command(command)
            elif hasattr(orb, "command_input"):
                orb.command_input.setText(command)

            print(f"[VOICE -> UI] {command}")

            # Now send the same recognized command into Nova.
            result = spine.handle_command(command)

            handle_runtime_result(
                orb,
                spine,
                result
            )

        except queue.Empty:
            break

        except Exception as exc:
            print(
                f"[VOICE QUEUE ERROR] {exc}"
            )
            break

    QTimer.singleShot(
        250,
        lambda: poll_voice_queue(
            orb,
            spine,
            command_queue
        )
    )


def main():
    app = QApplication(sys.argv)
    from core.context_fusion_engine import ContextFusionEngine
    from core.execution_router import ExecutionRouter
    from core.reasoning_engine import ReasoningEngine
    from core.task_translator import TaskTranslator
    from core.runtime_entrygate import RuntimeEntrygate
    from core.app_search_engine import AppSearchEngine
    from core.conversation_handler import ConversationHandler
    from core.nova_runtime import NovaRuntime
    from ui.orb import NovaOrb

    orb = NovaOrb()

    # Initial state: IDLE, UI is NOT VISIBLE
    orb.hide()
    orb.set_state("Idle", (0, 120, 255))

    spine = NovaRuntimeSpine()
    ui_queue = UIEventQueue()
    command_queue = queue.Queue()

    worker = threading.Thread(
        target=voice_wake_worker,
        args=(spine, ui_queue, command_queue),
        daemon=True
    )
    worker.start()

    orb.send_button.clicked.connect(lambda: handle_submit(orb, spine))

    QTimer.singleShot(100, lambda: poll_ui_queue(orb, ui_queue))
    QTimer.singleShot(250, lambda: poll_voice_queue(orb, spine, command_queue))

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

