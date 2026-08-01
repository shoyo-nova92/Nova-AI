import tempfile
from pathlib import Path

from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

with tempfile.TemporaryDirectory() as tmp_dir:
    test_file = Path(tmp_dir) / "test_sample.py"
    test_file.write_text("def test_ok():\n    assert 1 == 1\n", encoding="utf-8")

    result = runtime.router.route(
        {
            "type": "terminal",
            "action": "run_pytest",
            "action_type": "run_pytest",
            "target": str(test_file),
        }
    )

    assert result["state"] == "completed", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("pytest runtime ok")
