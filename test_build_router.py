import os
import tempfile

from core.execution_router import ExecutionRouter

router = ExecutionRouter()

with tempfile.TemporaryDirectory() as tmp_dir:
    original_cwd = os.getcwd()
    os.chdir(tmp_dir)
    try:
        result = router.route({
            "type": "terminal",
            "action": "build_project",
            "action_type": "build_project",
            "target": "python -m compileall .",
        })
    finally:
        os.chdir(original_cwd)

    assert result["state"] == "complete" or result["state"] == "failed", result
    assert result["execution"]["action"] == "build_project", result

print("build router ok")