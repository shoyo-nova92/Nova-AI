import time
from core.wake_local import WakeKeyMonitor


monitor = WakeKeyMonitor("v")

print()
print("=" * 50)
print("V KEY TEST")
print("=" * 50)
print("Press V once.")
print("Press CTRL+C to stop.")
print()

try:
    while True:

        if monitor.was_pressed():
            print("[TEST] V PRESS CONSUMED")

        time.sleep(0.01)

except KeyboardInterrupt:
    print("\nStopping.")