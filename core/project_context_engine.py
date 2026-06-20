import subprocess


class ProjectContextEngine:

    def get_context(self):

        branch = None

        try:

            branch = subprocess.check_output(

                "git branch --show-current",

                shell=True,

                text=True

            ).strip()

        except:

            pass

        return {

            "project":
                "Nova",

            "repository":
                "Nova",

            "branch":
                branch

        }