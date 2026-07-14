import tempfile
from pathlib import Path

from core.execution_verifier import ExecutionVerifier

verifier = ExecutionVerifier()

with tempfile.TemporaryDirectory() as tmp_dir:
    valid_path = Path(tmp_dir) / "valid.py"
    valid_path.write_text("def ok():\n    return 1\n", encoding="utf-8")

    invalid_path = Path(tmp_dir) / "invalid.py"
    invalid_path.write_text("def broken(:\n    pass\n", encoding="utf-8")

    assert verifier.verify_python(str(valid_path))["success"] is True
    assert verifier.verify_python(str(invalid_path))["success"] is False

print("python verification ok")
