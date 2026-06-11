class AdaptiveRetryEngine:

    def retry(self, action, verification_result):

        if verification_result["success"]:

            return {
                "status": "completed",
                "action": action
            }

        return {
            "status": "retry_needed",
            "action": action,
            "reason": verification_result["reason"],
            "suggested_retry": f"Retry action: {action}"
        }