import tempfile
from pathlib import Path

from core.terminal_handler import TerminalHandler

handler = TerminalHandler()

with tempfile.TemporaryDirectory() as tmp_dir:
    test_file = Path(tmp_dir) / "test_sample.py"
    test_file.write_text("def test_ok():\n    assert 1 == 1\n", encoding="utf-8")

    result = handler.run_pytest(str(test_file))

    assert result["success"] is True, result
    assert result["action"] == "run_pytest", result
    assert result["exit_code"] == 0, result
    assert result["passed"] >= 1, result

print("run pytest ok")
