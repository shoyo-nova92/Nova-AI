import os
import subprocess
import tempfile
from pathlib import Path

from core.git_handler import GitHandler

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

    work2 = repo / "work2"
    subprocess.run(["git", "clone", str(repo / "remote.git"), str(work2)], check=True, capture_output=True, text=True)
    subprocess.run(["git", "config", "user.name", "Test User"], cwd=work2, check=True, capture_output=True, text=True)
    subprocess.run(["git", "config", "user.email", "test@example.com"], cwd=work2, check=True, capture_output=True, text=True)

    original_cwd = os.getcwd()
    os.chdir(work2)
    try:
        result = GitHandler().git_pull()
    finally:
        os.chdir(original_cwd)

    assert result["action"] == "git_pull", result
    assert "exit_code" in result, result

print("git pull ok")
