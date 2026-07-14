import tempfile
from pathlib import Path

from core.execution_verifier import ExecutionVerifier

verifier = ExecutionVerifier()

with tempfile.TemporaryDirectory() as tmp_dir:
    py_path = Path(tmp_dir) / "sample.py"
    py_path.write_text("print('ok')\n", encoding="utf-8")
    assert verifier.verify_python(str(py_path))["success"] is True

    json_path = Path(tmp_dir) / "sample.json"
    json_path.write_text('{"ok": true}', encoding="utf-8")
    assert verifier.verify_json(str(json_path))["success"] is True

    text_path = Path(tmp_dir) / "notes.txt"
    text_path.write_text("hello\n", encoding="utf-8")
    assert verifier.verify_text(str(text_path))["success"] is True

print("execution verifier suite ok")
