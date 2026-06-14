class SelfCorrectionEngine:

    def diagnose(

        self,

        action,

        failure_reason

    ):

        failure_reason = (
            failure_reason.lower()
        )

        if "not found" in failure_reason:

            return {

                "diagnosis":
                    "target_missing",

                "alternative":

                    f"search for {action}"

            }

        elif "timeout" in failure_reason:

            return {

                "diagnosis":
                    "slow_response",

                "alternative":

                    f"retry {action}"

            }

        elif "permission" in failure_reason:

            return {

                "diagnosis":
                    "permission_denied",

                "alternative":

                    "request approval"

            }

        return {

            "diagnosis":
                "unknown",

            "alternative":
                f"retry {action}"

        }