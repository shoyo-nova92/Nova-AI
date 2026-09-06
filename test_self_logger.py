"""Comprehensive test suite for Nova's Unified Self-Logging & Self-Building Memory System."""

import json
from pathlib import Path
from core.self_logger import SelfLogger
from nova import NovaRuntimeSpine


def run_tests():
    print("=" * 60)
    print("NOVA SELF-LOGGING & CONTINUOUS LEARNING TEST SUITE")
    print("=" * 60)

    logger = SelfLogger()
    spine = NovaRuntimeSpine()

    initial_tasks_count = len(logger.get_recent_logs(limit=1000))
    print(f"Initial tasks logged in memory/task_logs.json: {initial_tasks_count}")

    # -------------------------------------------------------------
    # Test 1: Standard App Open ("open chrome")
    # -------------------------------------------------------------
    print("\n" + "-" * 50)
    print("TEST 1: Standard App Open ('open chrome')")
    print("-" * 50)

    res1 = spine.handle_command("open chrome")
    assert res1.get("status") == "handled", f"Test 1 failed to handle: {res1}"
    assert "self_log" in res1, "Test 1 response missing self_log entry"
    log1 = res1["self_log"]

    assert log1["status"] == "SUCCESS", f"Expected SUCCESS, got {log1['status']}"
    assert log1["intent_category"] == "open"
    assert "Google Chrome" in log1["crux"] or "chrome" in log1["crux"]
    assert len(log1["flowchart_steps"]) >= 4, "Flowchart should have at least 4 steps"
    assert len(log1["what_went_right"]) > 0, "what_went_right should not be empty"
    assert log1["flowchart_ascii"], "flowchart_ascii must be populated"

    # Date, Time, Day assertions
    assert "day" in log1 and log1["day"], "Log entry must contain 'day'"
    assert "date" in log1 and len(log1["date"]) == 10, "Log entry must contain 'date' in YYYY-MM-DD format"
    assert "time" in log1 and len(log1["time"]) == 8, "Log entry must contain 'time' in HH:MM:SS format"
    assert "datetime_formatted" in log1, "Log entry must contain 'datetime_formatted'"
    assert log1["day"] in log1["datetime_formatted"], "datetime_formatted must contain day name"
    assert "[TIMESTAMP]" in log1["flowchart_ascii"], "flowchart_ascii must include [TIMESTAMP] header"
    print(f"TEST 1 PASSED: 'open chrome' logged with date={log1['date']}, time={log1['time']}, day={log1['day']}.")

    # -------------------------------------------------------------
    # Test 2: Typo Auto-Correction ("open crhome")
    # -------------------------------------------------------------
    print("\n" + "-" * 50)
    print("TEST 2: Typo Auto-Correction ('open crhome')")
    print("-" * 50)

    res2 = spine.handle_command("open crhome")
    assert res2.get("status") == "handled", f"Test 2 failed to handle: {res2}"
    assert "self_log" in res2, "Test 2 response missing self_log entry"
    log2 = res2["self_log"]

    assert log2["status"] == "SUCCESS", f"Expected SUCCESS, got {log2['status']}"
    assert "crhome" in log2["self_building_data"].get("learned_aliases", {}), (
        "Learned alias 'crhome' should be recorded in self_building_data"
    )
    assert len(log2["what_went_wrong"]) > 0, "Typo warning should be listed in what_went_wrong"
    assert "auto-correction" in log2["crux"] or "typo" in log2["crux"], (
        f"Crux should mention auto-correction: {log2['crux']}"
    )
    print("TEST 2 PASSED: 'open crhome' logged typo resolution, flowchart, and crux.")

    # -------------------------------------------------------------
    # Test 3: Failed Command ("open nonexistentsomethingxyz")
    # -------------------------------------------------------------
    print("\n" + "-" * 50)
    print("TEST 3: Non-existent App Failure ('open nonexistentsomethingxyz')")
    print("-" * 50)

    res3 = spine.handle_command("open nonexistentsomethingxyz")
    assert res3.get("status") == "handled", f"Test 3 failed to handle: {res3}"
    assert "self_log" in res3, "Test 3 response missing self_log entry"
    log3 = res3["self_log"]

    assert log3["status"] == "FAILED", f"Expected FAILED, got {log3['status']}"
    assert log3["self_building_data"].get("error_category") == "APP_NOT_FOUND"
    assert len(log3["what_went_wrong"]) > 0, "what_went_wrong should list the failure cause"
    assert "not installed" in log3["crux"] or "not found" in log3["crux"], (
        f"Crux should explain missing app: {log3['crux']}"
    )
    print("TEST 3 PASSED: Non-existent app failure recorded with root cause and error category.")

    # -------------------------------------------------------------
    # Test 4: Conversational Command ("hello")
    # -------------------------------------------------------------
    print("\n" + "-" * 50)
    print("TEST 4: Conversational Intake ('hello')")
    print("-" * 50)

    res4 = spine.handle_command("hello")
    assert res4.get("status") == "handled", f"Test 4 failed to handle: {res4}"
    assert "self_log" in res4, "Test 4 response missing self_log entry"
    log4 = res4["self_log"]
    assert log4["status"] == "SUCCESS"
    assert log4["intent_category"] == "conversation"
    print("TEST 4 PASSED: Conversational command logged cleanly.")

    # -------------------------------------------------------------
    # Test 5: Verify Persistence & Self-Learning Insights
    # -------------------------------------------------------------
    print("\n" + "-" * 50)
    print("TEST 5: Verify Memory Files & Self-Learning Insights")
    print("-" * 50)

    insights = logger.get_insights()
    stats = logger.get_summary_stats()

    print(f"Total tasks logged: {stats['total_tasks']}")
    print(f"Success rate: {stats['success_rate_percent']}%")
    print(f"Learned aliases count: {stats['learned_aliases_count']}")
    print(f"Learned aliases: {stats['learned_aliases']}")
    print(f"Recommendations: {stats['recommendations']}")

    assert stats["total_tasks"] >= 4, "Should have logged at least 4 tasks"
    assert "crhome" in stats["learned_aliases"], "Alias 'crhome' must be in learned_aliases"
    assert stats["learned_aliases"]["crhome"]["resolved_target"] == "chrome"

    # Verify memory files on disk
    task_logs_path = Path("memory/task_logs.json")
    insights_path = Path("memory/self_learning_insights.json")
    exec_hist_path = Path("memory/execution_history.json")
    runtime_hist_path = Path("memory/runtime_history.json")

    assert task_logs_path.exists(), "task_logs.json missing"
    assert insights_path.exists(), "self_learning_insights.json missing"
    assert exec_hist_path.exists(), "execution_history.json missing"
    assert runtime_hist_path.exists(), "runtime_history.json missing"

    # Check 4-space indentation
    with open(task_logs_path, "r", encoding="utf-8") as f:
        content = f.read()
        assert "    {\n" in content or "    \"task_id\"" in content, "task_logs.json must use 4-space indentation"

    with open(task_logs_path, "r", encoding="utf-8") as f:
        tasks = json.load(f)
        assert len(tasks) > initial_tasks_count, "New tasks should be persisted in task_logs.json"
        for t in tasks:
            assert "day" in t and "date" in t and "time" in t, "All task entries must include day, date, and time"

    with open(exec_hist_path, "r", encoding="utf-8") as f:
        execs = json.load(f)
        for e in execs:
            assert "day" in e and "date" in e and "time" in e, "All exec entries must include day, date, and time"

    with open(runtime_hist_path, "r", encoding="utf-8") as f:
        runtimes = json.load(f)
        for r in runtimes:
            assert "day" in r and "date" in r and "time" in r, "All runtime entries must include day, date, and time"

    print("TEST 5 PASSED: Memory files, 4-space indentation & date/time/day verified successfully.")

    # -------------------------------------------------------------
    # Test 6: Verify Fresh Start Reset (reset_all_logs)
    # -------------------------------------------------------------
    print("\n" + "-" * 50)
    print("TEST 6: Fresh Start Reset")
    print("-" * 50)

    logger.reset_all_logs()
    fresh_tasks = logger.get_recent_logs(limit=100)
    assert len(fresh_tasks) == 0, "Fresh start must wipe all task logs"
    fresh_insights = logger.get_insights()
    assert fresh_insights.get("total_tasks_logged") == 0, "Fresh start must reset total_tasks_logged to 0"
    assert "created_day" in fresh_insights, "Fresh insights must have created_day"
    assert "created_date" in fresh_insights, "Fresh insights must have created_date"
    assert "created_time" in fresh_insights, "Fresh insights must have created_time"

    print("TEST 6 PASSED: Fresh start successfully reset all memory logs with date and time metadata.")

    print("\n" + "=" * 60)
    print("ALL 6 SELF-LOGGING, DATE/TIME/DAY & FRESH START TESTS PASSED!")
    print("=" * 60)


if __name__ == "__main__":
    run_tests()
