import msvcrt

print("Nova V-key bypass test")
print("Press V to trigger wake.")
print("Press Q to quit.")
print()

while True:

    key = msvcrt.getwch().lower()

    if key == "v":

        print("[WAKE DETECTED] V-key bypass")

    elif key == "q":

        print("[EXIT] V-key test stopped.")
        break