import os
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

        if action.startswith("create_file "):

            path = action.replace(
                "create_file ",
                "",
                1
            ).strip()

            return {

                "success":
                    os.path.isfile(path),

                "reason":
                    "file exists"
                    if os.path.isfile(path)
                    else
                    "file not found",

                "process_found":
                    os.path.isfile(path)

            }

        if action.startswith("create_folder "):

            path = action.replace(
                "create_folder ",
                "",
                1
            ).strip()

            return {

                "success":
                    os.path.isdir(path),

                "reason":
                    "folder exists"
                    if os.path.isdir(path)
                    else
                    "folder not found",

                "process_found":
                    os.path.isdir(path)

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
