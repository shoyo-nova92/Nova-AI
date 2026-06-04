from core.memory_retriever import (
    MemoryRetriever
)

retriever = MemoryRetriever()

context = {

    "project": "Nova v0.6",

    "activity": "coding"

}

results = (
    retriever.retrieve_relevant_context(
        context
    )
)

print(
    "\n=== RELEVANT MEMORIES ===\n"
)

for result in results:

    print(result)