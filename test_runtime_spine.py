from pathlib import Path
from unittest.mock import patch, MagicMock
from core.nova_runtime import NovaRuntime
from core.execution_policy import ExecutionPolicy


# Test-only policy that allows all actions
class TestExecutionPolicy(ExecutionPolicy):
    def classify(self, action):
        result = super().classify(action)
        result["allowed"] = True
        return result


def reset_test_workspace():
    """Reset test workspace to clean state before each test"""
    workspace_path = Path("tests/runtime_workspace")
    workspace_path.mkdir(parents=True, exist_ok=True)

    # Remove .bak files from previous tests
    for bak_file in workspace_path.glob("*.bak"):
        try:
            bak_file.unlink()
        except Exception:
            pass

    # Re-create sample files
    sample_files = {
        "parser.py": """def parse_text(text):
    return text.strip()

if __name__ == "__main__":
    print(parse_text("  hello world  "))
""",
        "workflow_validator.py": """def validate():
    print("Validation passed")
    return True

if __name__ == "__main__":
    validate()
""",
        "sample.txt": "This is a sample text file\n",
        "sample.py": """def main():
    print("Sample Python program")
    return 0

if __name__ == "__main__":
    main()
""",
        "test_sample.py": """def test_sample():
    assert 1 + 1 == 2
"""
    }

    for filename, content in sample_files.items():
        file_path = workspace_path / filename
        file_path.write_text(content, encoding="utf-8")


def test_1_open_notepad():
    print("\n" + "=" * 50)
    print("TEST 1: Open Notepad (Fast Path - Skip Planner)")
    print("=" * 50)
    reset_test_workspace()

    with patch("core.nova_runtime.VisionEngine") as mock_vision:
        mock_vision.return_value.analyze_screen.return_value = {
            "active_window": {"title": "Desktop"},
            "running_apps": [],
            "visible_text": ""
        }

        runtime = NovaRuntime()
        runtime.policy = TestExecutionPolicy()

        # Mock app handler and verifier for unit test environment
        runtime.router.apps.open_app = MagicMock(return_value={"success": True, "app": "notepad"})
        runtime.router.verifier.verify = MagicMock(return_value={"success": True, "reason": "notepad process verified"})

        result = runtime.process_goal("Open Notepad")
        assert result["success"], f"Test 1 failed: {result}"
        assert result["metadata"].get("fast_path") is True, "Fast path was not triggered for Open Notepad"
        assert len(result["executions"]) > 0, "No executions recorded"
        assert result["executions"][0]["action"]["action"] == "open_app"
        print("PASSED: Test 1 Open Notepad (Fast Path -> Skip Planner -> Execute)")


def test_2_read_parser_py():
    print("\n" + "=" * 50)
    print("TEST 2: Read parser.py (Translator -> Filesystem -> Done)")
    print("=" * 50)
    reset_test_workspace()
    test_file = str(Path("tests/runtime_workspace/parser.py").resolve()).replace("\\", "/")

    with patch("core.nova_runtime.VisionEngine") as mock_vision:
        mock_vision.return_value.analyze_screen.return_value = {
            "active_window": {"title": "Terminal"},
            "running_apps": [],
            "visible_text": ""
        }

        runtime = NovaRuntime()
        runtime.policy = TestExecutionPolicy()

        result = runtime.process_goal(f"Read {test_file}")
        assert result["success"], f"Test 2 failed: {result}"
        assert result["metadata"].get("fast_path") is True, "Fast path was not triggered for Read file"
        assert len(result["executions"]) > 0, "No executions recorded"
        read_action = result["executions"][0]["action"]
        assert read_action["action"] == "read_file"
        print("PASSED: Test 2 Read parser.py (Translator -> Filesystem -> Done)")


def test_3_implement_parser(): #Planner smoke test
    print("\n" + "=" * 50)
    print("TEST 3: Implement parser (Planner -> Pipeline -> Repair -> Execute -> Verify)")
    print("=" * 50)
    reset_test_workspace()
    test_file = str(Path("tests/runtime_workspace/parser.py").resolve()).replace("\\", "/")

    with patch("core.nova_runtime.VisionEngine") as mock_vision, \
         patch("core.nova_runtime.LLMPlanner") as mock_planner:
        mock_vision.return_value.analyze_screen.return_value = {
            "active_window": {"title": "VS Code"},
            "running_apps": [],
            "visible_text": ""
        }
        mock_planner.return_value.create_plan.return_value = [f"Create {test_file}"]

        runtime = NovaRuntime()
        runtime.policy = TestExecutionPolicy()

        result = runtime.process_goal("Implement parser")
        assert result["success"], f"Test 3 failed: {result}"
        assert result["metadata"].get("fast_path") is False, "Complex goal should not use fast path"
        assert result["raw_plan"], "Raw plan must be preserved in context"
        assert result["repaired_plan"], "Repaired plan must be preserved in context"
        print("PASSED: Test 3 Implement parser (Planner -> Pipeline -> Repair -> Execute -> Verify)")


def test_4_modify_parser_syntax_error_recovery():
    print("\n" + "=" * 50)
    print("TEST 4: Modify parser -> Syntax Error (Verify -> Recover -> Rollback -> Verify)")
    print("=" * 50)
    reset_test_workspace()
    test_file = str(Path("tests/runtime_workspace/parser.py").resolve()).replace("\\", "/")

    with patch("core.nova_runtime.VisionEngine") as mock_vision:
        mock_vision.return_value.analyze_screen.return_value = {
            "active_window": {"title": "Terminal"},
            "running_apps": [],
            "visible_text": ""
        }

        runtime = NovaRuntime()
        runtime.policy = TestExecutionPolicy()

        # Step A: First modify parser.py normally so a valid .bak backup is created
        runtime.router.route({
            "type": "filesystem",
            "action": "modify_file",
            "action_type": "modify_file",
            "target": test_file,
            "new_content": "def parse_text(text):\n    return text.strip()\n"
        })

        # Mock TaskTranslator to inject broken syntax content into modify_file action
        original_translate = runtime.task_translator.translate
        def mock_translate(step):
            res = original_translate(step)
            if isinstance(res, dict) and res.get("action") == "modify_file":
                res["new_content"] = "def broken(:\n    pass\n"
            return res
        runtime.task_translator.translate = mock_translate

        # Mock verifier to report failure on broken file, then success after rollback
        def mock_verifier(target_str):
            content = Path(test_file).read_text(encoding="utf-8", errors="ignore")
            if "def broken(:" in content:
                return {"success": False, "reason": "SyntaxError: invalid syntax"}
            return {"success": True, "reason": "Verification passed"}
        runtime.router.verifier.verify = mock_verifier

        # Run goal to modify parser with broken content
        result = runtime.process_goal(f"Modify {test_file}")

        assert result["recovery"].get("attempted") is True, "Recovery routine was not attempted"
        assert result["recovery"].get("recovered") is True, "Self-healing recovery failed"
        assert result["verification"].get("success") is True, "Post-recovery verification failed"

        # Verify file content was rolled back to valid state
        content_after = Path(test_file).read_text(encoding="utf-8")
        assert "def broken(:" not in content_after, "Syntax error was not rolled back"
        print("PASSED: Test 4 Modify parser -> Syntax Error (Verify -> Recover -> Rollback -> Verify)")


def test_5_run_pytest():
    print("\n" + "=" * 50)
    print("TEST 5: Run pytest (Terminal -> Verify -> Done)")
    print("=" * 50)
    reset_test_workspace()

    with patch("core.nova_runtime.VisionEngine") as mock_vision:
        mock_vision.return_value.analyze_screen.return_value = {
            "active_window": {"title": "Terminal"},
            "running_apps": [],
            "visible_text": ""
        }

        runtime = NovaRuntime()
        runtime.policy = TestExecutionPolicy()

        # Mock terminal execution and verifier for test environment
        runtime.router.terminal.run_pytest = MagicMock(return_value={
            "success": True,
            "stdout": "1 passed in 0.01s",
            "stderr": "",
            "returncode": 0
        })
        runtime.router.verifier.verify = MagicMock(return_value={"success": True, "reason": "pytest execution verified"})

        result = runtime.process_goal("Run pytest")
        assert result["success"], f"Test 5 failed: {result}"
        assert result["metadata"].get("fast_path") is True, "Fast path was not triggered for Run pytest"
        assert len(result["executions"]) > 0, "No executions recorded"
        assert result["executions"][0]["action"]["action"] == "run_pytest"
        print("PASSED: Test 5 Run pytest (Terminal -> Verify -> Done)")


def test_6_complete_spine_workflow():
    print("\n" + "=" * 50)
    print("TEST 6: Complete Spine Workflow (Open Notepad -> Read -> Modify -> Run pytest)")
    print("=" * 50)
    reset_test_workspace()

    test_1_open_notepad()
    test_2_read_parser_py()
    test_3_implement_parser()
    test_4_modify_parser_syntax_error_recovery()
    test_5_run_pytest()

    print("\n" + "=" * 50)
    print("PASSED: Test 6 Complete Runtime Spine Workflow (All 5 sub-tests executed seamlessly)")
    print("=" * 50)


if __name__ == "__main__":
    print("=" * 50)
    print("RUNNING v0.9.5 RUNTIME SPINE ACCEPTANCE TESTS")
    print("=" * 50)
    print(f"Using workspace: {Path('tests/runtime_workspace').resolve()}")

    test_1_open_notepad()
    test_2_read_parser_py()
    test_3_implement_parser()
    test_4_modify_parser_syntax_error_recovery()
    test_5_run_pytest()
    test_6_complete_spine_workflow()

    print("\n" + "=" * 50)
    print("ALL 6 RUNTIME SPINE ACCEPTANCE TESTS PASSED SUCCESSFULLY!")
    print("=" * 50)
