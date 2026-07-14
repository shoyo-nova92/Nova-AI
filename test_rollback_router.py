import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("original\n", encoding="utf-8")
    router.filesystem.modify_file(str(file_path), "changed\n")

    result = router.route(
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

print("rollback router ok")
