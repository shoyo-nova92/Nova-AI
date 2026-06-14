import psutil


class ExecutionVerifier:

    def verify(self, action):

        action = action.lower()

        verification_rules = {

            "notepad": "notepad",

            "vscode": "code",

            "calculator": "calculator",

            "chrome": "chrome",

            "edge": "msedge",

            "terminal": "cmd"

        }

        if "git_status" in action:

            return {

                "success": True,

                "reason":
                    "git status completed",

                "process_found": True

            }

        for key, process_name in (

            verification_rules.items()

        ):

            if key in action:

                for proc in psutil.process_iter(

                    ['name']

                ):

                    try:

                        if proc.info['name']:

                            if (

                                process_name

                                in

                                proc.info[
                                    'name'
                                ].lower()

                            ):

                                return {

                                    "success": True,

                                    "reason":

                                        f"{key} process detected.",

                                    "process_found": True

                                }

                    except:

                        pass

                return {

                    "success": False,

                    "reason":

                        f"{key} process not found.",

                    "process_found": False

                }

        return {

            "success": False,

            "reason":

                "No verification rule exists.",

            "process_found": False

        }