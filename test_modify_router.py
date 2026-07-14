import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    file_path = Path(tmp_dir) / "sample.txt"
    file_path.write_text("old content\n", encoding="utf-8")

    result = router.execute("modify_file", str(file_path))

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("modify router ok")
