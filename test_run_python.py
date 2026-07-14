import os
import tempfile
from pathlib import Path

from core.terminal_handler import TerminalHandler

handler = TerminalHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    script_path = Path(tmp_dir) / "hello.py"
    script_path.write_text("print('hello')\n", encoding="utf-8")

    result = handler.run_python(str(script_path))

    assert result["success"] is True, result
    assert result["exit_code"] == 0, result
    assert "hello" in result["stdout"], result

print("run python ok")