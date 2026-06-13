import requests
import time

class LLMPlanner:

    def create_plan(self, user_goal):

        try:

            system_prompt = """

You are Nova.

An advanced operating intelligence designed for:
- contextual reasoning
- workflow continuation
- software engineering assistance
- productivity optimization
- autonomous planning

Rules:
- avoid generic tutorials
- avoid beginner advice
- avoid unnecessary downloads
- focus on continuing active work
- generate context-aware workflows
- prioritize engineering productivity
- assume user is already inside active project

Return ONLY concise executable workflow steps.

"""

            print("\n==============================")
            print("Nova Reasoning Engine Activated")
            print("==============================")
            print("Model: Qwen 3 14B")
            print("Generating intelligent plan...")
            print(
                "This may take time depending "
                "on task complexity.\n"
            )

            start_time = time.time()

            response = requests.post(
                "http://127.0.0.1:11434/api/chat",
                json={
                    "model": "qwen3:14b",
                    "messages": [
                        {
                            "role": "system",
                            "content": system_prompt
                        },
                        {
                            "role": "user",
                            "content": user_goal
                        }
                    ],
                    "stream": False
                }
            )

            response = response.json()

            end_time = time.time()

            print(

                f"\nPlan generated in "
                f"{round(end_time - start_time, 2)} seconds."

            )

            raw_output = (
                response["message"]["content"]
            )

            print("\nRaw Model Output:")
            print("------------------")
            print(raw_output)

            steps = []

            for line in raw_output.split("\n"):

                cleaned = (

                    line.replace("-", "")
                    .replace("*", "")
                    .replace("1.", "")
                    .replace("2.", "")
                    .replace("3.", "")
                    .replace("4.", "")
                    .replace("5.", "")
                    .replace("6.", "")
                    .strip()

                )

                if cleaned:

                    steps.append(cleaned)

            if not steps:

                print(

                    "\nNo structured plan detected."
                    " Falling back to original goal."

                )

                return [user_goal]

            return steps

        except Exception as e:

            print(f"\nLLM Planner Error: {e}")

            print(
                "Falling back to basic execution.\n"
            )

            return [user_goal]