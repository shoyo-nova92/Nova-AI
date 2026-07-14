import tempfile
from pathlib import Path

from core.execution_verifier import ExecutionVerifier

verifier = ExecutionVerifier()

with tempfile.TemporaryDirectory() as tmp_dir:
    valid_path = Path(tmp_dir) / "config.yaml"
    valid_path.write_text("name: demo\nitems:\n  - one\n", encoding="utf-8")

    assert verifier.verify_yaml(str(valid_path))["success"] is True

print("yaml verification ok")
