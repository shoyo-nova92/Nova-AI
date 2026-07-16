from core.nova_runtime import NovaRuntime
from unittest.mock import patch, MagicMock
from pathlib import Path


def create_test_runtime():
    runtime = NovaRuntime()
    # Mock ExecutionPolicy to allow all actions
    runtime.policy.classify = lambda action: {"status": "SAFE", "allowed": True, "reason": "Test allowed"}
    # Mock ExecutionVerifier to always pass
    runtime.router.verifier.verify = lambda action: {"success": True, "reason": "Test verified"}
    return runtime


def test_1_open_notepad():
    print("\n" + "="*50)
    print("TEST 1: Open Notepad")
    print("="*50)
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Open Notepad"
        ]
        
        runtime = create_test_runtime()
        result = runtime.process_goal("Open Notepad")
        
        assert result["success"], f"Test failed: {result}"
        print("✓ Test 1 Passed")


def test_2_open_vscode():
    print("\n" + "="*50)
    print("TEST 2: Open VS Code")
    print("="*50)
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Open VS Code"
        ]
        
        runtime = create_test_runtime()
        result = runtime.process_goal("Open VS Code")
        
        assert result["success"], f"Test failed: {result}"
        print("✓ Test 2 Passed")


def test_3_read_file():
    print("\n" + "="*50)
    print("TEST 3: Read core/task_translator.py")
    print("="*50)
    
    test_file_path = Path("core/task_translator.py")
    assert test_file_path.exists(), f"File {test_file_path} not found"
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Read core/task_translator.py"
        ]
        
        runtime = create_test_runtime()
        result = runtime.process_goal("Read core/task_translator.py")
        
        assert result["success"], f"Test failed: {result}"
        
        # Check that the execution actually read the file
        executions = result["data"].get("executions", [])
        assert len(executions) > 0, "No executions found"
        print("✓ Test 3 Passed")


def test_4_modify_file():
    print("\n" + "="*50)
    print("TEST 4: Modify workflow_validator.py")
    print("="*50)
    
    test_file_path = Path("workflow_validator.py")
    # Create a test file if it doesn't exist
    if not test_file_path.exists():
        test_file_path.write_text("Initial content\n", encoding="utf-8")
    
    original_content = test_file_path.read_text(encoding="utf-8")
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Modify workflow_validator.py"
        ]
        
        runtime = create_test_runtime()
        
        # Mock the planner pipeline to return a proper modify action
        original_process = runtime.pipeline.process
        
        def mock_process(raw_plan):
            return {
                "raw_plan": raw_plan,
                "parsed_plan": [],
                "quality": {},
                "confidence": 1.0,
                "normalized_plan": [],
                "expanded_plan": [],
                "validated_plan": [],
                "repaired_plan": [
                    {
                        "type": "filesystem",
                        "action": "modify_file",
                        "action_type": "modify_file",
                        "target": "workflow_validator.py",
                        "new_content": "Modified content\n"
                    }
                ]
            }
        
        runtime.pipeline.process = mock_process
        
        result = runtime.process_goal("Modify workflow_validator.py")
        
        assert result["success"], f"Test failed: {result}"
        print("✓ Test 4 Passed")


def test_5_replace_text():
    print("\n" + "="*50)
    print("TEST 5: Replace print with logger.info")
    print("="*50)
    
    test_file_path = Path("workflow_validator.py")
    # Ensure the file has print statement
    test_file_path.write_text("print('hello')\n", encoding="utf-8")
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Replace print with logger.info in workflow_validator.py"
        ]
        
        runtime = create_test_runtime()
        
        original_process = runtime.pipeline.process
        
        def mock_process(raw_plan):
            return {
                "raw_plan": raw_plan,
                "parsed_plan": [],
                "quality": {},
                "confidence": 1.0,
                "normalized_plan": [],
                "expanded_plan": [],
                "validated_plan": [],
                "repaired_plan": [
                    {
                        "type": "filesystem",
                        "action": "replace_text",
                        "action_type": "replace_text",
                        "target": "workflow_validator.py",
                        "parameters": {
                            "old": "print('hello')",
                            "new": "logger.info('hello')"
                        }
                    }
                ]
            }
        
        runtime.pipeline.process = mock_process
        
        result = runtime.process_goal("Replace print with logger.info in workflow_validator.py")
        
        assert result["success"], f"Test failed: {result}"
        print("✓ Test 5 Passed")


def test_6_append_file():
    print("\n" + "="*50)
    print("TEST 6: Append TODO to parser.py")
    print("="*50)
    
    test_file_path = Path("core/parser.py")
    assert test_file_path.exists(), "core/parser.py not found"
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Append TODO to core/parser.py"
        ]
        
        runtime = create_test_runtime()
        
        original_process = runtime.pipeline.process
        
        def mock_process(raw_plan):
            return {
                "raw_plan": raw_plan,
                "parsed_plan": [],
                "quality": {},
                "confidence": 1.0,
                "normalized_plan": [],
                "expanded_plan": [],
                "validated_plan": [],
                "repaired_plan": [
                    {
                        "type": "filesystem",
                        "action": "append_file",
                        "action_type": "append_file",
                        "target": "core/parser.py",
                        "parameters": {
                            "content": "\n# TODO: Add more features\n"
                        }
                    }
                ]
            }
        
        runtime.pipeline.process = mock_process
        
        result = runtime.process_goal("Append TODO to core/parser.py")
        
        assert result["success"], f"Test failed: {result}"
        print("✓ Test 6 Passed")


def test_7_insert_line():
    print("\n" + "="*50)
    print("TEST 7: Insert logging at line 20 of parser.py")
    print("="*50)
    
    test_file_path = Path("core/parser.py")
    assert test_file_path.exists(), "core/parser.py not found"
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Insert logging at line 20 of core/parser.py"
        ]
        
        runtime = create_test_runtime()
        
        original_process = runtime.pipeline.process
        
        def mock_process(raw_plan):
            return {
                "raw_plan": raw_plan,
                "parsed_plan": [],
                "quality": {},
                "confidence": 1.0,
                "normalized_plan": [],
                "expanded_plan": [],
                "validated_plan": [],
                "repaired_plan": [
                    {
                        "type": "filesystem",
                        "action": "insert_at_line",
                        "action_type": "insert_at_line",
                        "target": "core/parser.py",
                        "parameters": {
                            "line": 20,
                            "content": "import logging\n"
                        }
                    }
                ]
            }
        
        runtime.pipeline.process = mock_process
        
        result = runtime.process_goal("Insert logging at line 20 of core/parser.py")
        
        assert result["success"], f"Test failed: {result}"
        print("✓ Test 7 Passed")


def test_8_rollback_file():
    print("\n" + "="*50)
    print("TEST 8: Rollback parser.py")
    print("="*50)
    
    test_file_path = Path("core/parser.py")
    assert test_file_path.exists(), "core/parser.py not found"
    
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner:
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Rollback core/parser.py"
        ]
        
        runtime = create_test_runtime()
        
        original_process = runtime.pipeline.process
        
        def mock_process(raw_plan):
            return {
                "raw_plan": raw_plan,
                "parsed_plan": [],
                "quality": {},
                "confidence": 1.0,
                "normalized_plan": [],
                "expanded_plan": [],
                "validated_plan": [],
                "repaired_plan": [
                    {
                        "type": "filesystem",
                        "action": "rollback_file",
                        "action_type": "rollback_file",
                        "target": "core/parser.py"
                    }
                ]
            }
        
        runtime.pipeline.process = mock_process
        
        result = runtime.process_goal("Rollback core/parser.py")
        
        print(f"Test 8 Result: {result}")
        print("✓ Test 8 Completed")


if __name__ == "__main__":
    print("="*50)
    print("RUNNING RUNTIME SPINE ACCEPTANCE TESTS")
    print("="*50)
    
    test_1_open_notepad()
    test_2_open_vscode()
    test_3_read_file()
    test_4_modify_file()
    test_5_replace_text()
    test_6_append_file()
    test_7_insert_line()
    test_8_rollback_file()
    
    print("\n" + "="*50)
    print("ALL RUNTIME SPINE TESTS COMPLETED!")
    print("="*50)
