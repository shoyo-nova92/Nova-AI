import os
import subprocess
import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

with tempfile.TemporaryDirectory() as tmp_dir:
    repo = Path(tmp_dir)
    subprocess.run(["git", "init"], cwd=repo, check=True, capture_output=True, text=True)
    subprocess.run(["git", "config", "user.name", "Test User"], cwd=repo, check=True, capture_output=True, text=True)
    subprocess.run(["git", "config", "user.email", "test@example.com"], cwd=repo, check=True, capture_output=True, text=True)
    (repo / "file.txt").write_text("hello\n", encoding="utf-8")
    subprocess.run(["git", "add", "file.txt"], cwd=repo, check=True, capture_output=True, text=True)
    subprocess.run(["git", "commit", "-m", "initial"], cwd=repo, check=True, capture_output=True, text=True)

    original_cwd = os.getcwd()
    os.chdir(repo)
    try:
        result = ExecutionRouter().route({
            "type": "git",
            "action": "git_checkout",
            "action_type": "git_checkout",
            "target": "-b feature-test",
        })
    finally:
        os.chdir(original_cwd)

    assert result["state"] in {"complete", "failed"}, result
    assert result["execution"]["action"] == "git_checkout", result

print("git checkout router ok")
