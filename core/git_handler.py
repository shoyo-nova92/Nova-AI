import os
import subprocess
import time
from pathlib import Path


class GitHandler:

    def git_add(self, target="."):
        try:
            repo_root = self._find_repo_root()
            if not repo_root:
                return {
                    "success": False,
                    "reason": "git repository not found",
                    "action": "git_add",
                    "target": str(target),
                }

            started = time.time()
            result = subprocess.run(
                ["git", "add", str(target)],
                capture_output=True,
                text=True,
                cwd=repo_root,
            )
            duration = round(time.time() - started, 2)

            return {
                "success": result.returncode == 0,
                "action": "git_add",
                "target": str(target),
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration,
            }
        except Exception as e:
            return {
                "success": False,
                "reason": str(e),
                "action": "git_add",
                "target": str(target),
            }

    def git_commit(self, message):
        try:
            repo_root = self._find_repo_root()
            if not repo_root:
                return {"success": False, "reason": "git repository not found", "action": "git_commit", "message": str(message)}
            if not message:
                return {"success": False, "reason": "message is required", "action": "git_commit"}

            started = time.time()
            result = subprocess.run(
                ["git", "commit", "-m", str(message)],
                capture_output=True,
                text=True,
                cwd=repo_root,
            )
            duration = round(time.time() - started, 2)
            return {
                "success": result.returncode == 0,
                "action": "git_commit",
                "message": str(message),
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration,
            }
        except Exception as e:
            return {"success": False, "reason": str(e), "action": "git_commit", "message": str(message)}

    def git_checkout(self, branch):
        try:
            repo_root = self._find_repo_root()
            if not repo_root:
                return {"success": False, "reason": "git repository not found", "action": "git_checkout", "branch": str(branch)}
            if not branch:
                return {"success": False, "reason": "branch is required", "action": "git_checkout"}

            started = time.time()
            result = subprocess.run(
                ["git", "checkout", str(branch)],
                capture_output=True,
                text=True,
                cwd=repo_root,
            )
            duration = round(time.time() - started, 2)
            return {
                "success": result.returncode == 0,
                "action": "git_checkout",
                "branch": str(branch),
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration,
            }
        except Exception as e:
            return {"success": False, "reason": str(e), "action": "git_checkout", "branch": str(branch)}

    def git_pull(self):
        try:
            repo_root = self._find_repo_root()
            if not repo_root:
                return {"success": False, "reason": "git repository not found", "action": "git_pull"}
            started = time.time()
            result = subprocess.run(["git", "pull"], capture_output=True, text=True, cwd=repo_root)
            duration = round(time.time() - started, 2)
            return {
                "success": result.returncode == 0,
                "action": "git_pull",
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration,
            }
        except Exception as e:
            return {"success": False, "reason": str(e), "action": "git_pull"}

    def git_push(self):
        try:
            repo_root = self._find_repo_root()
            if not repo_root:
                return {"success": False, "reason": "git repository not found", "action": "git_push"}
            started = time.time()
            result = subprocess.run(["git", "push"], capture_output=True, text=True, cwd=repo_root)
            duration = round(time.time() - started, 2)
            return {
                "success": result.returncode == 0,
                "action": "git_push",
                "stdout": result.stdout,
                "stderr": result.stderr,
                "exit_code": result.returncode,
                "duration": duration,
            }
        except Exception as e:
            return {"success": False, "reason": str(e), "action": "git_push"}

    def _find_repo_root(self):
        current = Path.cwd()
        for path in [current, *current.parents]:
            if (path / ".git").exists():
                return str(path)
        return None
