from core.system_state import SystemState

state = SystemState()

print("\nActive Window:")
print(state.get_active_window_title())

print("\nRunning Apps:")
print(state.get_running_apps())