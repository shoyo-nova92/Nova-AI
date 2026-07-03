import re


class PlanParser:

    ACTION = "action"
    CODE = "code"
    CODE_START = "code_start"
    CODE_END = "code_end"
    TEXT = "text"

    def parse(
        self,
        raw_plan
    ):

        parsed = []

        in_code_block = False

        for line in raw_plan.splitlines():

            line = line.strip()

            if not line:
                continue

            # -------------------------
            # Markdown code fences
            # -------------------------

            if line.startswith("```"):

                if not in_code_block:

                    parsed.append({
                        "type": self.CODE_START
                    })

                    in_code_block = True

                else:

                    parsed.append({
                        "type": self.CODE_END
                    })

                    in_code_block = False

                continue

            # -------------------------
            # Code block
            # -------------------------

            if in_code_block:

                parsed.append({

                    "type": self.CODE,

                    "text": line

                })

                continue

            # -------------------------
            # Remove numbering
            # -------------------------

            cleaned = re.sub(
                r"^\d+\.\s*",
                "",
                line
            )

            # -------------------------
            # Detect executable actions
            # -------------------------

            if self._is_action(cleaned):

                parsed.append({

                    "type": self.ACTION,

                    "text": cleaned

                })

            else:

                parsed.append({

                    "type": self.TEXT,

                    "text": cleaned

                })

        return parsed

    def _is_action(
        self,
        text
    ):

        verbs = [

            "open",
            "create",
            "add",
            "run",
            "modify",
            "edit",
            "update",
            "delete",
            "move",
            "copy",
            "rename",
            "close",
            "search",
            "install",
            "launch",
            "focus"

        ]

        lower = text.lower()

        return any(
            lower.startswith(v)
            for v in verbs
        )