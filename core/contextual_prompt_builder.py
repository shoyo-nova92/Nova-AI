class ContextualPromptBuilder:

    def build(

        self,

        context,

        retrieved_memories

    ):

        memory_section = ""

        for memory in retrieved_memories:

            memory_section += (

                f"- {memory['task']}\n"

            )

        prompt = f"""

You are Nova.

Current Context:
----------------
Project:
{context['project']}

Focus:
{context['focus']}

Workflow Stage:
{context['workflow_stage']}

Environment:
{context['environment_summary']}

Activity:
{context['activity']}


Relevant Memory:
----------------
{memory_section}


Instructions:
----------------
Generate a realistic next-step workflow
for the user.

Rules:
- Prefer practical architectural improvements
- Avoid unnecessary complexity
- Avoid enterprise-scale infrastructure
- Avoid reinforcement learning systems
- Avoid distributed systems
- Avoid premature optimization
- Focus on the NEXT logical engineering step
- Keep solutions lightweight and local-first

Current Development Philosophy:
- iterative development
- simplicity first
- reliability over scale
- local-first architecture
- practical execution

inside active project

Return ONLY executable workflow steps.

"""

        return prompt