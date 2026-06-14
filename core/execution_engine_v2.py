class ExecutionEngineV2:

    def execute(self, action):

        action = action.lower()

        if "open notepad" in action:

            return self._open_notepad()

        elif "open calculator" in action:

            return self._open_calculator()

        else:

            return {

                "success": False,

                "reason":
                    f"No handler for: {action}"

            }

    def _open_notepad(self):

        import subprocess

        subprocess.Popen(
            "notepad.exe"
        )

        return {

            "success": True,

            "action":
                "open notepad"

        }

    def _open_calculator(self):

        import subprocess

        subprocess.Popen(
            "calc.exe"
        )

        return {

            "success": True,

            "action":
                "open calculator"

        }