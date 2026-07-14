import tempfile
from pathlib import Path

from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("original\n", encoding="utf-8")
    runtime.router.filesystem.modify_file(str(file_path), "changed\n")

    result = runtime.router.route(
        {
            "type": "filesystem",
            "action": "rollback_file",
            "action_type": "rollback_file",
            "target": str(file_path),
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("rollback runtime ok")
