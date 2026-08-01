from core.nova_runtime import NovaRuntime
from unittest.mock import patch, MagicMock

def test_runtime_integration():
    # Mock components that require external dependencies
    with patch('core.nova_runtime.VisionEngine') as mock_vision, \
         patch('core.nova_runtime.LLMPlanner') as mock_planner, \
         patch('core.nova_runtime.ExecutionRouter') as mock_router:
        
        # Setup VisionEngine mock
        mock_vision_instance = mock_vision.return_value
        mock_vision_instance.analyze_screen.return_value = {
            "active_window": {"title": "Command Prompt"},
            "running_apps": ["python.exe"],
            "visible_text": ""
        }
        
        # Setup LLMPlanner mock - return simple plan
        mock_planner_instance = mock_planner.return_value
        mock_planner_instance.create_plan.return_value = [
            "Open Notepad"
        ]
        
        # Setup ExecutionRouter mock
        mock_router_instance = mock_router.return_value
        mock_router_instance.route.return_value = {"success": True}
        mock_router_instance.verifier = MagicMock()
        mock_router_instance.verifier.verify.return_value = {"success": True}
        
        runtime = NovaRuntime()
        
        # Test goals with mocked components
        test_goals = [
            "Open Notepad",
            "Test goal"
        ]
        
        for goal in test_goals:
            result = runtime.process_goal(goal)
            assert "success" in result, f"Missing 'success' in result: {result}"
            assert "data" in result, f"Missing 'data' in result: {result}"
            assert "message" in result, f"Missing 'message' in result: {result}"
            assert "confidence" in result, f"Missing 'confidence' in result: {result}"
            print(f"{goal} -> {result['message']}")
        
        print("runtime integration ok")

if __name__ == "__main__":
    test_runtime_integration()
