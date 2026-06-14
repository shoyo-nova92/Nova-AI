import subprocess


class TerminalHandler:

    def open_terminal(self):

        try:

            subprocess.Popen(
                "cmd.exe"
            )

            return {

                "success": True,

                "action":
                    "open terminal"

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def run_command(

        self,

        command

    ):

        try:

            result = subprocess.run(

                command,

                shell=True,

                capture_output=True,

                text=True

            )

            return {

                "success":
                    result.returncode == 0,

                "command":
                    command,

                "stdout":
                    result.stdout,

                "stderr":
                    result.stderr

            }

        except Exception as e:

            return {

                "success": False,

                "reason": str(e)

            }

    def git_status(self):

        return self.run_command(
            "git status"
        )

    def activate_venv(self):

        return self.run_command(
            r".\.venv\Scripts\activate"
        )