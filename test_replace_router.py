import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("print('Hello')\n", encoding="utf-8")

    result = router.route(
        {
            "type": "filesystem",
            "action": "replace_text",
            "action_type": "replace_text",
            "target": str(file_path),
            "parameters": {"old": "print(", "new": "logger.info("},
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("replace router ok")
