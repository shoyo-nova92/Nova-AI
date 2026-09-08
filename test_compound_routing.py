from core.runtime_entrygate import RuntimeEntrygate
e = RuntimeEntrygate()
cases = [
    ("open chrome", "open"),
    ("open chrome and search for best headphones under 5k", "runtime"),
    ("open notepad then close it", "runtime"),
    ("close chrome", "close"),
    ("open chrome & go to youtube", "runtime"),
    ("open youtube music", "open"),
]
ok = True
for cmd, expected in cases:
    result = e.classify(cmd)
    status = "OK  " if result["action"] == expected else "FAIL"
    if result["action"] != expected:
        ok = False
    print("[" + status + "] " + repr(cmd) + " -> " + result["action"] + " (expected: " + expected + ")")

print()
if ok:
    print("ALL COMPOUND ROUTING TESTS PASSED")
else:
    print("SOME TESTS FAILED")
