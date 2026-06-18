from core.browser_handler import (
    BrowserHandler
)

handler = BrowserHandler()

print("\n=== BROWSER SUITE ===\n")

import time

time.sleep(3)

print(
    handler.open_new_tab()
)

time.sleep(3)

print(
    handler.search_query(
        "Nova AI"
    )
)

time.sleep(3)

print(
    handler.next_tab()
)

time.sleep(3)

print(
    handler.previous_tab()
)

time.sleep(3)

print(
    handler.close_tab()
)