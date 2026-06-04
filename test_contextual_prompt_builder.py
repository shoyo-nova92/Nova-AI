from core.contextual_prompt_builder import (
    ContextualPromptBuilder
)

builder = ContextualPromptBuilder()

context = {

    "project": "Nova v0.6",

    "focus":
        "AI operating system development",

    "workflow_stage":
        "cognitive architecture development",

    "environment_summary":
        "User is actively developing Nova cognitive systems inside VSCode.",

    "activity": "coding"

}

retrieved_memories = [

    {

        "task":
            "Built context fusion engine"

    },

    {

        "task":
            "Implemented adaptive executor"

    }

]

prompt = builder.build(

    context,

    retrieved_memories

)

print(

    "\n=== GENERATED PROMPT ===\n"

)

print(prompt)