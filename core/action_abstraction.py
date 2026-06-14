class ActionAbstraction:

    def expand(self, goal):

        goal = goal.lower()

        if "prepare coding environment" in goal:

            return [

                "open vscode",

                "open project",

                "open terminal",

                "run development server"

            ]

        elif "upload thumbnail" in goal:

            return [

                "open browser",

                "navigate to upload page",

                "click upload button",

                "select thumbnail file",

                "confirm upload",

                "verify upload"

            ]

        elif "open notepad" in goal:

            return [

                "open notepad"

            ]

        return [

            goal

        ]