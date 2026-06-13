import time

from core.nova_runtime import (
    NovaRuntime
)


class RuntimeLoop:

    def __init__(self):

        self.runtime = (
            NovaRuntime()
        )

    def start(

        self,

        cycles=3

    ):

        print(
            "\n=== NOVA RUNTIME LOOP ===\n"
        )

        for cycle in range(cycles):

            print(
                f"\nCycle {cycle + 1}"
            )

            print(
                "------------------"
            )

            result = (
                self.runtime.run()
            )

            print(
                "\nStatus:"
            )

            print(
                result["status"]
            )

            time.sleep(2)

        print(
            "\n=== LOOP COMPLETE ==="
        )