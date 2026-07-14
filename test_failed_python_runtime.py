import tempfile
from pathlib import Path

from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("print('ok')\n", encoding="utf-8")

    result = runtime.router.route(
        {
            "type": "filesystem",
            "action": "modify_file",
            "action_type": "modify_file",
            "target": str(file_path),
            "new_content": "def broken(:\n    pass\n",
        }
    )

    assert result["state"] == "failed", result
    assert result["verification"]["success"] is False, result

print("failed python runtime ok")
