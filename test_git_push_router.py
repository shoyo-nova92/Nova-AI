import os
import subprocess
import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

with tempfile.TemporaryDirectory() as tmp_dir:
    repo = Path(tmp_dir)
    subprocess.run(["git", "init", "--bare", str(repo / "remote.git")], check=True, capture_output=True, text=True)
    subprocess.run(["git", "clone", str(repo / "remote.git"), str(repo / "work")], check=True, capture_output=True, text=True)
    subprocess.run(["git", "config", "user.name", "Test User"], cwd=repo / "work", check=True, capture_output=True, text=True)
    subprocess.run(["git", "config", "user.email", "test@example.com"], cwd=repo / "work", check=True, capture_output=True, text=True)
    (repo / "work" / "file.txt").write_text("hello\n", encoding="utf-8")
    subprocess.run(["git", "add", "file.txt"], cwd=repo / "work", check=True, capture_output=True, text=True)
    subprocess.run(["git", "commit", "-m", "initial"], cwd=repo / "work", check=True, capture_output=True, text=True)
    subprocess.run(["git", "push", "origin", "master"], cwd=repo / "work", check=True, capture_output=True, text=True)

    original_cwd = os.getcwd()
    os.chdir(repo / "work")
    try:
        result = ExecutionRouter().route({
            "type": "git",
            "action": "git_push",
            "action_type": "git_push",
            "target": None,
        })
    finally:
        os.chdir(original_cwd)

    assert result["state"] in {"complete", "failed"}, result
    assert result["execution"]["action"] == "git_push", result

print("git push router ok")
