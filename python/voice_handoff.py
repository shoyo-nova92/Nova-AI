"""Explicit playback acknowledgement between the native parent and voice child."""
import ctypes
from ctypes import wintypes


class GreetingHandoff:
    def __init__(self, ready_name, cancel_name):
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.kernel.OpenEventW.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.LPCWSTR]
        self.kernel.OpenEventW.restype = wintypes.HANDLE
        self.kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
        self.kernel.SetEvent.argtypes = [wintypes.HANDLE]
        self.ready = self.kernel.OpenEventW(0x00100000, False, ready_name)
        self.cancel = self.kernel.OpenEventW(0x0002, False, cancel_name)
        if not self.ready or not self.cancel:
            raise OSError(ctypes.get_last_error(), 'Voice handoff event unavailable')

    def wait_for_greeting(self):
        if self.kernel.WaitForSingleObject(self.ready, 0xffffffff) != 0:
            raise OSError(ctypes.get_last_error(), 'Greeting acknowledgement failed')

    def cancel_greeting(self):
        self.kernel.SetEvent(self.cancel)


def greet_before_listening(stream, wait_for_greeting, announce):
    # Stop capture before playback, rather than retaining greeting audio in the
    # input buffer. Restart immediately after the actual acknowledgement.
    stream.stop()
    announce()
    wait_for_greeting()
    stream.start()
