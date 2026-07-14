import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "notes.txt"
    file_path.write_text("first\n", encoding="utf-8")

    result = router.route(
        {
            "type": "filesystem",
            "action": "append_file",
            "action_type": "append_file",
            "target": str(file_path),
            "parameters": {"content": "second"},
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("append router ok")
