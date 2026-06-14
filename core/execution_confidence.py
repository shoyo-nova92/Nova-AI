from core.execution_memory import (
    ExecutionMemory
)


class ExecutionConfidence:

    def __init__(self):

        self.memory = (
            ExecutionMemory()
        )

    def estimate(

        self,

        action

    ):

        stats = (

            self.memory.get_stats(
                action
            )

        )

        confidence = (
            stats["success_rate"]
        )

        if confidence == 0:

            confidence = 50

        return {

            "action":
                action,

            "confidence":
                confidence

        }