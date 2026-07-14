import tempfile
from pathlib import Path

from core.nova_runtime import NovaRuntime

runtime = NovaRuntime()

with tempfile.TemporaryDirectory() as tmp_dir:
    script_path = Path(tmp_dir) / "hello.py"
    script_path.write_text("print('hello')\n", encoding="utf-8")

    result = runtime.router.route(
        {
            "type": "terminal",
            "action": "run_python",
            "action_type": "run_python",
            "target": str(script_path),
        }
    )

    assert result["state"] == "complete", result
    assert result["execution"]["success"] is True, result
    assert result["verification"]["success"] is True, result

print("run runtime ok")