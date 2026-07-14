from core.terminal_session import TerminalSession

session = TerminalSession(cwd=".")
session.start()
assert session.execute("echo first")["success"] is True
assert session.execute("echo second")["success"] is True
assert session.close()["success"] is True

print("terminal queue ok")
