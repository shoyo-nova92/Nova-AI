import tempfile
from pathlib import Path

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    test_file = Path(tmp_dir) / "test_sample.py"
    test_file.write_text("def test_ok():\n    assert 1 == 1\n", encoding="utf-8")

    result = router.route(
        {
            "type": "terminal",
            "action": "run_pytest",
            "action_type": "run_pytest",
            "target": str(test_file),
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("pytest router ok")
