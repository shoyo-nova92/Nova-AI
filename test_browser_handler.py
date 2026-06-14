from core.browser_handler import (
    BrowserHandler
)

handler = (
    BrowserHandler()
)

result = handler.search_query(
    "best OCR models"
)

print(
    "\n=== BROWSER RESULT ===\n"
)

print(result)