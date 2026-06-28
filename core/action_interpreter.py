from core.task_translator import (
    TaskTranslator
)


class ActionInterpreter:

    def __init__(self):

        self.translator = (
            TaskTranslator()
        )

    def interpret(
        self,
        step
    ):

        return self.translator.translate(
            step
        )
