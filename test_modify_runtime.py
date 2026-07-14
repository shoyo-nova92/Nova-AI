import tempfile
from pathlib import Path

from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "workflow_validator.py"
    file_path.write_text("old\n", encoding="utf-8")

    result = runtime.router.route(
        {
            "type": "filesystem",
            "action": "modify_file",
            "action_type": "modify_file",
            "target": str(file_path),
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("modify runtime ok")
