from core.semantic_memory_engine import (
    SemanticMemoryEngine
)

memory = SemanticMemoryEngine()

# SAVE MEMORY
memory.save_memory(

    project="Nova v0.6",

    task="Built context fusion engine",

    status="completed",

    tags=[

        "memory",
        "reasoning",
        "context"

    ]

)

memory.save_memory(

    project="Nova v0.6",

    task="Implemented adaptive executor",

    status="completed",

    tags=[

        "execution",
        "autonomy"

    ]

)

# SEARCH MEMORY
results = memory.search_memory(
    "context"
)

print(
    "\n=== MEMORY SEARCH RESULTS ===\n"
)

for result in results:

    print(result)