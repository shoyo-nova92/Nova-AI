import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "parser.py"
    file_path.write_text("print('hello')\n", encoding="utf-8")

    result = router.route(
        {
            "type": "filesystem",
            "action": "insert_at_line",
            "action_type": "insert_at_line",
            "target": str(file_path),
            "parameters": {"line": 1, "content": "import logging\n"},
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("insert router ok")
