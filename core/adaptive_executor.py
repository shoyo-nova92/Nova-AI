import time

class AdaptiveExecutor:

    def __init__(self):

        self.max_retries = 3

    def execute_action(
        self,
        action,
        validator,
        context
    ):

        # STEP 1 — VALIDATE
        validation = validator.validate(
            action,
            context
        )

        if not validation["valid"]:

            return {

                "status": "blocked",

                "reason":
                    validation["reason"]

            }

        # STEP 2 — ATTEMPT EXECUTION
        for attempt in range(
            self.max_retries
        ):

            try:

                print(
                    f"\nAttempt "
                    f"{attempt + 1}: "
                    f"{action}"
                )

                # Simulated execution
                time.sleep(1)

                # Simulated success
                success = True

                if success:

                    return {

                        "status": "success",

                        "action": action,

                        "attempts":
                            attempt + 1

                    }

            except Exception as e:

                print(
                    "Execution failed:",
                    e
                )

                time.sleep(1)

        # STEP 3 — FAILURE
        return {

            "status": "failed",

            "action": action,

            "reason":
                "Maximum retries exceeded."

        }