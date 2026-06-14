import json
import os


class SkillSystem:

    FILE_PATH = (
        "memory/skills.json"
    )

    def __init__(self):

        os.makedirs(
            "memory",
            exist_ok=True
        )

        if not os.path.exists(
            self.FILE_PATH
        ):

            with open(
                self.FILE_PATH,
                "w"
            ) as f:

                json.dump(
                    {},
                    f,
                    indent=4
                )

    def update_skill(

        self,

        skill_name,

        success

    ):

        with open(
            self.FILE_PATH,
            "r"
        ) as f:

            data = json.load(f)

        if skill_name not in data:

            data[skill_name] = {

                "usage_count": 0,

                "success_count": 0

            }

        data[skill_name][
            "usage_count"
        ] += 1

        if success:

            data[skill_name][
                "success_count"
            ] += 1

        with open(
            self.FILE_PATH,
            "w"
        ) as f:

            json.dump(
                data,
                f,
                indent=4
            )

    def get_skill_stats(

        self,

        skill_name

    ):

        with open(
            self.FILE_PATH,
            "r"
        ) as f:

            data = json.load(f)

        if skill_name not in data:

            return {

                "skill":
                    skill_name,

                "usage_count":
                    0,

                "success_rate":
                    0

            }

        usage = data[
            skill_name
        ][
            "usage_count"
        ]

        successes = data[
            skill_name
        ][
            "success_count"
        ]

        return {

            "skill":
                skill_name,

            "usage_count":
                usage,

            "success_rate":
                round(
                    successes
                    / usage
                    * 100,
                    2
                )

        }