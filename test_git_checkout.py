import os
import subprocess
import tempfile
from pathlib import Path

from core.git_handler import GitHandler

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
        result = GitHandler().git_checkout("-b feature-test")
    finally:
        os.chdir(original_cwd)

    assert result["action"] == "git_checkout", result
    assert "exit_code" in result, result

print("git checkout ok")
