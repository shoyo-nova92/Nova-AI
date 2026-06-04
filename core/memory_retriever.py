from core.semantic_memory_engine import (
    SemanticMemoryEngine
)

class MemoryRetriever:

    def __init__(self):

        self.memory = (
            SemanticMemoryEngine()
        )

    def retrieve_relevant_context(

        self,
        current_context,
        limit=5

    ):

        project = current_context.get(
            "project",
            ""
        )

        activity = current_context.get(
            "activity",
            ""
        )

        memories = (
            self.memory.load_memories()
        )

        scored = []

        for memory in memories:

            score = 0

            # PROJECT MATCH
            if (
                project
                and project.lower()
                in str(memory).lower()
            ):

                score += 5

            # ACTIVITY MATCH
            if (
                activity
                and activity.lower()
                in str(memory).lower()
            ):

                score += 3

            # RECENCY BOOST
            score += 1

            scored.append(

                (score, memory)

            )

        # SORT BY SCORE
        scored.sort(
            key=lambda x: x[0],
            reverse=True
        )

        # RETURN TOP RESULTS
        return [

            item[1]

            for item in scored[:limit]

        ]