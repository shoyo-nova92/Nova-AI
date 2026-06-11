import psutil


class ExecutionVerifier:

    def verify(self, action):

        action = action.lower()

        if "notepad" in action:

            for proc in psutil.process_iter(['name']):

                try:

                    if proc.info['name']:

                        if "notepad" in proc.info['name'].lower():

                            return {
                                "success": True,
                                "reason": "Notepad process detected."
                            }

                except:
                    pass

            return {
                "success": False,
                "reason": "Notepad process not found."
            }

        return {
            "success": False,
            "reason": "No verification rule exists."
        }