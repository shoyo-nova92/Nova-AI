from core.action_abstraction import (
    ActionAbstraction
)

from core.execution_engine_v2 import (
    ExecutionEngineV2
)

from core.execution_memory import (
    ExecutionMemory
)

from core.execution_confidence import (
    ExecutionConfidence
)

from core.self_correction_engine import (
    SelfCorrectionEngine
)


class GoalRuntime:

    def __init__(self):

        self.abstraction = (
            ActionAbstraction()
        )

        self.executor = (
            ExecutionEngineV2()
        )

        self.memory = (
            ExecutionMemory()
        )

        self.confidence = (
            ExecutionConfidence()
        )

        self.corrector = (
            SelfCorrectionEngine()
        )

    def run_goal(

        self,

        goal

    ):

        actions = (
            self.abstraction.expand(
                goal
            )
        )

        results = []

        for action in actions:

            confidence = (

                self.confidence.estimate(
                    action
                )

            )

            execution = (

                self.executor.execute(
                    action
                )

            )

            success = (
                execution.get(
                    "success",
                    False
                )
            )

            self.memory.record(

                action=action,

                success=success,

                duration=0

            )

            result = {

                "action":
                    action,

                "confidence":
                    confidence[
                        "confidence"
                    ],

                "success":
                    success

            }

            if not success:

                correction = (

                    self.corrector.diagnose(

                        action,

                        execution.get(
                            "reason",
                            "unknown"
                        )

                    )

                )

                result[
                    "correction"
                ] = correction

            results.append(
                result
            )

        return results