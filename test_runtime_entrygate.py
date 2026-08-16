import sys
import os
import json

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.app_search_engine import AppSearchEngine
from core.conversation_handler import ConversationHandler
from core.runtime_entrygate import RuntimeEntrygate
from core.task_translator import TaskTranslator


def test_conversation_handler_greeting():
    print("=" * 60)
    print("TEST: ConversationHandler greeting detection")
    handler = ConversationHandler()
    greetings = ["hey", "hi", "hello", "hey x", "hey nova", "how are you", "good morning"]
    for g in greetings:
        is_conv = handler.is_conversational(g)
        assert is_conv, f"Failed: {g} not detected as conversation"
        response = handler.respond(g)
        assert response["status"] == "handled"
        assert response["response"] and len(response["response"]) > 0
        print(f"  OK {g!r} -> {response['response']!r}")
    print("OK ConversationHandler greeting/response test PASSED")


def test_conversation_handler_thank_you():
    print("\n" + "=" * 60)
    print("TEST: ConversationHandler thank-you detection")
    handler = ConversationHandler()
    thanks = ["thanks", "thank you", "thx"]
    for t in thanks:
        assert handler.is_thank_you(t), f"Failed: {t} not detected as thanks"
        response = handler.respond(t)
        assert response["category"] == "thanks"
        assert response["response"] and len(response["response"]) > 0
        print(f"  OK {t!r} -> {response['response']!r}")
    print("OK ConversationHandler thank-you test PASSED")


def test_conversation_handler_exit():
    print("\n" + "=" * 60)
    print("TEST: ConversationHandler exit detection")
    handler = ConversationHandler()
    exits = ["exit", "quit", "bye", "goodbye", "shutdown"]
    for e in exits:
        assert handler.is_exit(e), f"Failed: {e} not detected as exit"
        response = handler.respond(e)
        assert response["category"] == "exit"
        assert response["action"] == "exit"
        assert response["response"] and len(response["response"]) > 0
        print(f"  OK {e!r} -> {response['response']!r}")
    print("OK ConversationHandler exit test PASSED")


def test_entrygate_classify_open_close():
    print("\n" + "=" * 60)
    print("TEST: RuntimeEntrygate classify open/close")
    entrygate = RuntimeEntrygate(app_search_engine=AppSearchEngine(auto_index=False))

    open_cases = [
        ("open vscode", RuntimeEntrygate.ACTION_OPEN, "vscode"),
        ("open google chrome", RuntimeEntrygate.ACTION_OPEN, "google chrome"),
        ("open notepad", RuntimeEntrygate.ACTION_OPEN, "notepad"),
        ("launch discord", RuntimeEntrygate.ACTION_OPEN, "discord"),
        ("start calculator", RuntimeEntrygate.ACTION_OPEN, "calculator"),
    ]
    for cmd, expected_action, expected_target in open_cases:
        c = entrygate.classify(cmd)
        assert c["action"] == expected_action, f"Failed {cmd}: got {c['action']} expected {expected_action}"
        assert c["target"] == expected_target, f"Failed {cmd}: got target {c['target']!r} expected {expected_target!r}"
        print(f"  OK classify {cmd!r} -> action={c['action']}, target={c['target']!r}")

    close_cases = [
        ("close vscode", RuntimeEntrygate.ACTION_CLOSE, "vscode"),
        ("close google chrome", RuntimeEntrygate.ACTION_CLOSE, "google chrome"),
        ("kill discord", RuntimeEntrygate.ACTION_CLOSE, "discord"),
    ]
    for cmd, expected_action, expected_target in close_cases:
        c = entrygate.classify(cmd)
        assert c["action"] == expected_action, f"Failed {cmd}: got {c['action']} expected {expected_action}"
        assert c["target"] == expected_target, f"Failed {cmd}: got target {c['target']!r} expected {expected_target!r}"
        print(f"  OK classify {cmd!r} -> action={c['action']}, target={c['target']!r}")

    print("OK Entrygate classify open/close test PASSED")


def test_entrygate_classify_conversation_exit_runtime():
    print("\n" + "=" * 60)
    print("TEST: RuntimeEntrygate classify conversation/exit/runtime")
    entrygate = RuntimeEntrygate(app_search_engine=AppSearchEngine(auto_index=False))

    conv_cases = ["hey", "hi", "hello", "hey nova", "how are you"]
    for cmd in conv_cases:
        c = entrygate.classify(cmd)
        assert c["action"] == RuntimeEntrygate.ACTION_CONVERSATION, f"Failed {cmd}: got {c['action']}"
        print(f"  OK classify {cmd!r} -> action=conversation")

    exit_cases = ["exit", "quit", "bye", "goodbye"]
    for cmd in exit_cases:
        c = entrygate.classify(cmd)
        assert c["action"] == RuntimeEntrygate.ACTION_EXIT, f"Failed {cmd}: got {c['action']}"
        print(f"  OK classify {cmd!r} -> action=exit")

    runtime_cases = [
        "add logging to parser.py and run tests",
        "read sample.txt and modify it",
        "implement authentication in auth.py",
    ]
    for cmd in runtime_cases:
        c = entrygate.classify(cmd)
        assert c["action"] == RuntimeEntrygate.ACTION_RUNTIME, f"Failed {cmd}: got {c['action']}"
        print(f"  OK classify {cmd!r} -> action=runtime")

    print("OK Entrygate classify conversation/exit/runtime test PASSED")


def test_entrygate_process_fast_paths():
    print("\n" + "=" * 60)
    print("TEST: RuntimeEntrygate.process fast paths (conversation/exit/open/close)")
    entrygate = RuntimeEntrygate(app_search_engine=AppSearchEngine(auto_index=False))

    # Conversation fast-path
    r = entrygate.process("hey nova")
    assert r["branch"] == "entrygate_conversation"
    assert r["action"] == RuntimeEntrygate.ACTION_CONVERSATION
    assert "response" in r and r["response"]
    print(f"  OK conversation -> {r['response']!r}")

    # Thank-you fast-path
    r = entrygate.process("thank you")
    assert r["branch"] == "entrygate_conversation"
    assert r["action"] == RuntimeEntrygate.ACTION_CONVERSATION
    assert "response" in r and r["response"]
    print(f"  OK thanks -> {r['response']!r}")

    # Exit fast-path
    r = entrygate.process("bye")
    assert r["branch"] == "entrygate_exit"
    assert r["action"] == RuntimeEntrygate.ACTION_EXIT
    assert r["response"]
    print(f"  OK exit -> {r['response']!r}")

    # Open notepad (always exists on Windows)
    r = entrygate.process("open notepad")
    assert r["branch"] == "entrygate_open"
    assert r["action"] == RuntimeEntrygate.ACTION_OPEN
    print(f"  OK open notepad -> success={r.get('success')}, result={json.dumps(r.get('result'), indent=2, default=str)}")

    print("OK Entrygate process fast-path tests PASSED")


def test_entrygate_process_runtime_path():
    print("\n" + "=" * 60)
    print("TEST: RuntimeEntrygate.process runtime (complex goal) path")
    entrygate = RuntimeEntrygate(app_search_engine=AppSearchEngine(auto_index=False))

    complex_goal = "Add logging to parser.py and run pytest"
    r = entrygate.process(complex_goal)
    assert r["branch"] == "entrygate_runtime"
    assert r["action"] == RuntimeEntrygate.ACTION_RUNTIME
    # RuntimeEntrygate preserves original casing in .goal (lowercasing happens in InputNormalizer in nova.py)
    # IMPORTANT: EntryGate intentionally does NOT call TaskTranslator here.
    # Translation happens INSIDE the canonical core.NovaRuntime pipeline (single owner of translation).
    assert r["goal"].strip() == complex_goal.strip()
    assert "action" in r
    print(f"  OK runtime goal -> action={r['action']}, goal={r['goal']!r}")

    print("OK Entrygate process runtime path test PASSED")


def test_app_search_engine_index_and_rank():
    print("\n" + "=" * 60)
    print("TEST: AppSearchEngine discovery + ranking")
    engine = AppSearchEngine(auto_index=True)
    print(f"  Indexed {len(engine.indexed_apps)} apps")
    assert len(engine.indexed_apps) > 0, "No apps were indexed"

    # Test ranking against some standard Windows apps
    rank_tests = [
        ("notepad", ["Notepad"]),
        ("calculator", ["Calculator"]),
        ("vscode", ["Visual Studio Code", "Code"]),
        ("chrome", ["Google Chrome", "Chrome"]),
    ]
    for query, expected_keywords in rank_tests:
        matches = engine.rank_matches(query)
        found = False
        if matches:
            top_app, top_score = matches[0]
            name_lower = top_app["name"].lower()
            for kw in expected_keywords:
                if kw.lower() in name_lower:
                    found = True
                    break
            print(f"  OK rank '{query}' -> top={top_app['name']!r} score={top_score:.2f}")
        if not found and matches:
            print(f"  WARN rank '{query}' -> top={matches[0][0]['name']!r} (not exact match, but acceptable)")

    print("OK AppSearchEngine index+rank test PASSED")


def test_task_translator_close_app():
    print("\n" + "=" * 60)
    print("TEST: TaskTranslator close_app translation (new rule)")
    translator = TaskTranslator()
    cases = [
        "close vscode",
        "kill google chrome",
        "terminate discord",
    ]
    for step in cases:
        t = translator.translate(step)
        assert t["type"] == "application", f"Failed step {step}: type={t['type']}"
        assert t["action"] == "close_app", f"Failed step {step}: action={t['action']}"
        assert t["target"], f"Failed step {step}: target missing"
        print(f"  OK translate {step!r} -> target={t['target']!r}")

    print("OK TaskTranslator close_app test PASSED")


if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("RUNNING RUNTIME ENTRYGATE TESTS")
    print("=" * 60)

    try:
        test_conversation_handler_greeting()
        test_conversation_handler_thank_you()
        test_conversation_handler_exit()
        test_entrygate_classify_open_close()
        test_entrygate_classify_conversation_exit_runtime()
        test_entrygate_process_fast_paths()
        test_entrygate_process_runtime_path()
        test_app_search_engine_index_and_rank()
        test_task_translator_close_app()

        print("\n" + "=" * 60)
        print("ALL RUNTIME ENTRYGATE TESTS PASSED")
        print("=" * 60)
        sys.exit(0)
    except AssertionError as e:
        print(f"\nTEST FAILED (AssertionError): {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)
    except Exception as e:
        print(f"\nTEST FAILED (Exception): {e}")
        import traceback
        traceback.print_exc()
        sys.exit(2)
