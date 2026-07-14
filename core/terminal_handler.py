import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

from core.terminal_session import TerminalSession


class TerminalHandler:

    def open_terminal(self):

        try:

            subprocess.Popen(
                "cmd.exe"
            )

            return {

                "success": True,

                "action":
                    "open terminal"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def run_command(

        self,

        command

    ):

        try:

            result = subprocess.run(

                command,

                shell=True,

                capture_output=True,

                text=True

            )

            return {

                "success":
                    result.returncode == 0,

                "command":
                    command,

                "stdout":
                    result.stdout,

                "stderr":
                    result.stderr

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def run_python(self, path):

        try:

            if not path:

                return {

                    "success": False,

                    "reason": "path is required"
                }

            file_path = Path(path)

            if not file_path.exists():

                return {

                    "success": False,

                    "reason": "file does not exist"
                }

            if not file_path.is_file():

                return {

                    "success": False,

                    "reason": "target is not a file"
                }

            started = time.time()
            result = subprocess.run(
                ["python", str(file_path)],
                capture_output=True,
                text=True,
                cwd=str(file_path.parent)
            )
            duration = round(time.time() - started, 2)

            return {

                "success": result.returncode == 0,

                "action": "run_python",

                "path": str(file_path),
                "stdout": result.stdout,

                "stderr": result.stderr,

                "exit_code": result.returncode,

                "duration": duration

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def run_pytest(self, target=None):

        try:

            started = time.time()
            command = self._build_pytest_command(target)

            cwd = os.getcwd()
            if target:

                target_path = Path(str(target))
                if target_path.exists() and target_path.is_file():
                    cwd = str(target_path.parent)
                elif target_path.parent != Path('.'):
                    cwd = str(target_path.parent)

            print("\nCOMMAND:")
            print(command)

            print("\nCWD:")
            print(cwd)

            result = subprocess.run(
                command,
                capture_output=True,
                text=True,
                cwd=cwd
            )

            print("\nSTDOUT:")
            print(result.stdout)

            print("\nSTDERR:")
            print(result.stderr)

            print("\nEXIT:")
            print(result.returncode)

            duration = round(time.time() - started, 2)

            summary = self._parse_pytest_summary(result.stdout, result.stderr)

            return {

                "success": result.returncode == 0,

                "action": "run_pytest",

                "target": str(target) if target else None,
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration,
                "passed": summary.get("passed", 0),
                "failed": summary.get("failed", 0),
                "errors": summary.get("errors", 0)
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def _build_pytest_command(self, target=None):

        command = [
            sys.executable,
            "-m",
            "pytest"
        ]

        if target:
            command.append(str(target))

        return command

    def _parse_pytest_summary(self, stdout, stderr):

        text = f"{stdout}\n{stderr}"
        passed = 0
        failed = 0
        errors = 0

        passed_match = re.search(r"(\d+) passed", text, re.IGNORECASE)
        failed_match = re.search(r"(\d+) failed", text, re.IGNORECASE)
        errors_match = re.search(r"(\d+) errors?", text, re.IGNORECASE)

        if passed_match:
            passed = int(passed_match.group(1))

        if failed_match:
            failed = int(failed_match.group(1))

        if errors_match:
            errors = int(errors_match.group(1))

        return {
            "passed": passed,
            "failed": failed,
            "errors": errors
        }

    def pip_install(self, package):

        try:

            if not package:

                return {

                    "success": False,

                    "reason": "package is required"
                }

            started = time.time()
            result = subprocess.run(
                [sys.executable, "-m", "pip", "install", str(package)],
                capture_output=True,
                text=True,
                cwd=os.getcwd()
            )
            duration = round(time.time() - started, 2)

            return {

                "success": result.returncode == 0,

                "action": "pip_install",

                "package": str(package),
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def build_project(self, command=None, cwd=None):

        try:

            if not command:
                command = self._infer_build_command(cwd)

            if not command:
                return {
                    "success": False,
                    "reason": "build command is required"
                }

            started = time.time()
            result = subprocess.run(
                command,
                shell=True,
                capture_output=True,
                text=True,
                cwd=(cwd or os.getcwd())
            )
            duration = round(time.time() - started, 2)

            return {

                "success": result.returncode == 0,

                "action": "build_project",
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration
            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)
            }

    def _infer_build_command(self, cwd=None):

        if cwd is None:
            cwd = os.getcwd()

        if os.path.exists(os.path.join(cwd, "package.json")):
            return "npm run build"

        if os.path.exists(os.path.join(cwd, "setup.py")) or os.path.exists(os.path.join(cwd, "pyproject.toml")):
            return "python -m compileall ."

        return None

    def git_status(self):

        return self.run_command(
            "git status"
        )

    def activate_venv(self):

        return self.run_command(
            r".\.venv\Scripts\activate"
        )
    def get_session(self, cwd=None):
        return TerminalSession(cwd=cwd)
