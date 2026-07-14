import tempfile
from pathlib import Path

from core.execution_verifier import ExecutionVerifier

verifier = ExecutionVerifier()

with tempfile.TemporaryDirectory() as tmp_dir:
    valid_path = Path(tmp_dir) / "config.json"
    valid_path.write_text('{"ok": true}', encoding="utf-8")

    invalid_path = Path(tmp_dir) / "broken.json"
    invalid_path.write_text('{bad json}', encoding="utf-8")

    assert verifier.verify_json(str(valid_path))["success"] is True
    assert verifier.verify_json(str(invalid_path))["success"] is False

print("json verification ok")
