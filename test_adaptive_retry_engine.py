from core.adaptive_retry_engine import AdaptiveRetryEngine

engine = AdaptiveRetryEngine()

failed_result = {
    "success": False,
    "reason": "Notepad process not found."
}

result = engine.retry(
    "open notepad",
    failed_result
)

print("\n=== RETRY RESULT ===\n")
print(result)