import tempfile
from pathlib import Path

from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("TODO\n", encoding="utf-8")

    result = runtime.router.route(
        {
            "type": "filesystem",
            "action": "replace_text",
            "action_type": "replace_text",
            "target": str(file_path),
            "parameters": {"old": "TODO", "new": "DONE"},
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("replace runtime ok")
