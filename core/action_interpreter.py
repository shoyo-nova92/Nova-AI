class ActionInterpreter:

    def interpret(
        self,
        step
    ):

        step = step.lower()

        vscode_open_phrases = [

            "open vscode",
            "open vs code",
            "open visual studio code",
            "launch vscode",
            "launch vs code",
            "launch visual studio code",
            "start vscode",
            "start vs code",
            "start visual studio code",
            "focus vscode",
            "focus vs code",
            "focus visual studio code"

        ]

        if any(
            phrase in step
            for phrase in vscode_open_phrases
        ):

            return {

                "action_type":
                    "open_app",

                "target":
                    "vscode"

            }

        if "terminal" in step:

            return {

                "action_type":
                    "open_terminal",

                "target":
                    None

            }

        if "git status" in step:

            return {

                "action_type":
                    "git_status",

                "target":
                    None

            }

        return {

            "action_type":
                None,

            "target":
                None

        }
