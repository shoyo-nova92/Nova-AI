import json
from pathlib import Path


class PersistentContext:

    def __init__(self):

        self.context_file = Path(
            "memory/current_context.json"
        )

        self.context_file.parent.mkdir(
            exist_ok=True
        )

        if not self.context_file.exists():

            with open(
                self.context_file,
                "w"
            ) as f:

                json.dump({}, f)

    def save_context(

        self,

        context

    ):

        with open(

            self.context_file,

            "w"

        ) as f:

            json.dump(

                context,

                f,

                indent=4

            )

    def load_context(self):

        with open(

            self.context_file,

            "r"

        ) as f:

            return json.load(f)