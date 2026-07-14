import os
import tempfile
from pathlib import Path

from core.terminal_handler import TerminalHandler

handler = TerminalHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    temp_path = Path(tmp_dir)
    (temp_path / "demo.py").write_text("print('hello')\n", encoding="utf-8")

    result = handler.build_project("python -m compileall .", cwd=str(temp_path))

    assert result["success"] is True, result
    assert result["action"] == "build_project", result
    assert result["exit_code"] == 0, result

print("build project ok")
