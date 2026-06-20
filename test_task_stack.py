from core.task_stack import (
    TaskStack
)

stack = TaskStack()

stack.update(

    project="Nova",

    milestone="v0.9.1",

    task="Build Work Context Layer",

    subtask="Implement Task Stack"

)

print(
    stack.get_stack()
)