import json
import tempfile
from pathlib import Path

from core.learned_rules import LearnedRules
from core.promotion_engine import PromotionEngine


class MockLLMClient:
    def __init__(self, return_text):
        self.return_text = return_text

    def generate(self, prompt="", system_prompt=None):
        return self.return_text


def test_promotion_candidates_discovery():
    with tempfile.TemporaryDirectory() as tmp_dir:
        rules_file = Path(tmp_dir) / "learned_rules.json"
        rules_file.write_text(json.dumps({"rules": []}), encoding="utf-8")
        rules = LearnedRules()
        rules.rules_file = rules_file

        engine = PromotionEngine(min_occurrences=3, rules_engine=rules)
        engine.task_logs_file = Path(tmp_dir) / "task_logs.json"
        engine.candidates_file = Path(tmp_dir) / "promotion_candidates.json"

        # 3 successes for "set up logging in parser.py"
        fake_logs = [
            {
                "task_id": f"task_{i}",
                "intent_category": "complex_task",
                "raw_input": "Set up logging in parser.py",
                "success": True,
                "metadata": {
                    "runtime_result": {
                        "executions": [
                            {
                                "action": {
                                    "type": "filesystem",
                                    "action": "modify_file",
                                    "target": "parser.py",
                                }
                            }
                        ]
                    }
                },
            }
            for i in range(3)
        ]

        # 2 successes for "open notepad" (less than min_occurrences=3)
        fake_logs.extend([
            {
                "task_id": f"short_{i}",
                "intent_category": "complex_task",
                "raw_input": "short task",
                "success": True,
            }
            for i in range(2)
        ])

        # 3 runs with 1 failure (not 100% success)
        fake_logs.extend([
            {
                "task_id": f"flaky_{i}",
                "intent_category": "complex_task",
                "raw_input": "flaky task",
                "success": (i != 0),
            }
            for i in range(3)
        ])

        engine.task_logs_file.write_text(json.dumps(fake_logs), encoding="utf-8")

        candidates = engine.find_candidates()
        assert len(candidates) == 1, f"Expected 1 candidate, got {len(candidates)}"
        assert candidates[0]["normalized_goal"] == "set up logging in parser.py"
        assert candidates[0]["occurrences"] == 3


def test_promotion_approval_and_matching():
    with tempfile.TemporaryDirectory() as tmp_dir:
        rules_file = Path(tmp_dir) / "learned_rules.json"
        rules_file.write_text(json.dumps({"rules": []}), encoding="utf-8")
        rules = LearnedRules()
        rules.rules_file = rules_file

        mock_rule = {
            "pattern": r"^set up logging in (.+)$",
            "action": {
                "type": "filesystem",
                "action": "modify_file",
                "action_type": "modify_file",
                "target": "{group_1}",
            },
        }
        mock_llm = MockLLMClient(json.dumps(mock_rule))

        engine = PromotionEngine(
            min_occurrences=3,
            llm_client=mock_llm,
            rules_engine=rules,
            approval_callback=lambda cand, rule: True,  # Auto-approve
        )
        engine.task_logs_file = Path(tmp_dir) / "task_logs.json"
        engine.candidates_file = Path(tmp_dir) / "promotion_candidates.json"

        candidate = {
            "normalized_goal": "set up logging in parser.py",
            "occurrences": 3,
            "success_count": 3,
            "sample_task_ids": ["task_0", "task_1", "task_2"],
        }
        fake_logs = [
            {
                "task_id": f"task_{i}",
                "metadata": {
                    "runtime_result": {
                        "executions": [
                            {
                                "action": {
                                    "type": "filesystem",
                                    "action": "modify_file",
                                    "target": "parser.py",
                                }
                            }
                        ]
                    }
                },
            }
            for i in range(3)
        ]
        engine.task_logs_file.write_text(json.dumps(fake_logs), encoding="utf-8")

        result = engine.promote(candidate)
        assert result["promoted"] is True, result

        # Verify rule is saved in rules_engine and matches with {group_1} replacement!
        matched = rules.match("set up logging in parser.py")
        assert matched is not None
        assert matched["type"] == "filesystem"
        assert matched["action"] == "modify_file"
        assert matched["target"] == "parser.py"

        # Verify candidate is recorded as approved
        history = engine._load_candidate_history()
        assert len(history) == 1
        assert history[0]["status"] == "approved"


def test_promotion_rejection():
    with tempfile.TemporaryDirectory() as tmp_dir:
        rules_file = Path(tmp_dir) / "learned_rules.json"
        rules_file.write_text(json.dumps({"rules": []}), encoding="utf-8")
        rules = LearnedRules()
        rules.rules_file = rules_file

        mock_rule = {
            "pattern": r"^dangerous action$",
            "action": {
                "type": "terminal",
                "action": "run_python",
                "action_type": "run_python",
                "target": "danger.py",
            },
        }
        mock_llm = MockLLMClient(json.dumps(mock_rule))

        engine = PromotionEngine(
            min_occurrences=3,
            llm_client=mock_llm,
            rules_engine=rules,
            approval_callback=lambda cand, rule: False,  # User rejects
        )
        engine.task_logs_file = Path(tmp_dir) / "task_logs.json"
        engine.candidates_file = Path(tmp_dir) / "promotion_candidates.json"

        candidate = {
            "normalized_goal": "dangerous action",
            "occurrences": 3,
            "sample_task_ids": ["t1"],
        }
        engine.task_logs_file.write_text(
            json.dumps([{
                "task_id": "t1",
                "executions": [{"action": {"type": "terminal", "action": "run_python", "target": "danger.py"}}],
            }]),
            encoding="utf-8",
        )

        result = engine.promote(candidate)
        assert result["promoted"] is False
        assert result["reason"] == "rule rejected"

        # Verify rule was NOT added
        assert rules.match("dangerous action") is None

        # Verify recorded as rejected
        history = engine._load_candidate_history()
        assert history[0]["status"] == "rejected"

        # And find_candidates() will ignore it in future scans!
        assert len(engine.find_candidates()) == 0


if __name__ == "__main__":
    test_promotion_candidates_discovery()
    test_promotion_approval_and_matching()
    test_promotion_rejection()
    print("ALL PROMOTION ENGINE TESTS PASSED")
