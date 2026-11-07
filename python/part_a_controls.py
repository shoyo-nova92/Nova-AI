"""Console controls stay responsive while audio models load or transcribe."""
import os
import sys
import threading
import time
import msvcrt


def start_console_controls(on_exit=None):
    def watch():
        while True:
            if msvcrt.kbhit():
                key = msvcrt.getwch().lower()
                if key in ('v', 'q'):
                    if on_exit is not None:
                        on_exit()
                    marker = '[KEYBOARD_REQUEST]' if key == 'v' else '[QUIT_REQUEST]'
                    # This dedicated, one-shot child owns model loading/audio.
                    # Exit it as well as releasing its mic/GPU handles, even if
                    # the main thread is blocked inside a native model call.
                    print('\n' + marker, flush=True)
                    sys.stdout.flush()
                    os._exit(0)
            time.sleep(.025)
    threading.Thread(target=watch, daemon=True, name='nova-input-controls').start()
