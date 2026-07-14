from core.terminal_session import TerminalSession

session = TerminalSession(cwd=".")
start_result = session.start()
assert start_result["success"] is True, start_result
execute_result = session.execute("echo hello")
assert execute_result["success"] is True, execute_result
close_result = session.close()
assert close_result["success"] is True, close_result

print("terminal session ok")
