class Planner:

    def create_plan(self, command):
        command = command.lower()

        if "design" in command:
            return [
                "open photoshop",
                "open chrome"
            ]

        elif "nova" in command:
            return [
                "open vscode",
                "open chrome"
            ]

        elif "main study" in command:
            return [
                "open chrome",
                "open brain fm"
            ]

        else:
            return [command]