import subprocess
import time
from pathlib import Path


class TerminalSession:
    def __init__(self, cwd=None):
        self.cwd = str(cwd or ".")
        self.process = None
        self.started = False

    def start(self):
        if self.started and self.process and self.process.poll() is None:
            return {"success": True, "action": "start_session"}

        try:
            self.process = subprocess.Popen(
                ["cmd.exe"],
                cwd=self.cwd,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
            )
            self.started = True
            return {"success": True, "action": "start_session"}
        except Exception as e:
            return {"success": False, "reason": str(e)}

    def execute(self, command):
        if not self.started or not self.process or self.process.poll() is not None:
            self.start()

        if not self.process or self.process.poll() is not None:
            return {"success": False, "reason": "session not available"}

        try:
            if self.process.stdin is None:
                return {"success": False, "reason": "stdin unavailable"}
            self.process.stdin.write(command + "\n")
            self.process.stdin.flush()
            time.sleep(0.2)
            return {"success": True, "action": "execute_command", "command": command}
        except Exception as e:
            return {"success": False, "reason": str(e)}

    def close(self):
        if self.process and self.process.poll() is None:
            try:
                if self.process.stdin is not None:
                    self.process.stdin.write("exit\n")
                    self.process.stdin.flush()
                self.process.terminate()
                self.process.wait(timeout=2)
            except Exception:
                self.process.kill()
        self.process = None
        self.started = False
        return {"success": True, "action": "close_session"}
