import time


class ExecutionQueue:

    def __init__(self, executor, parser):
        self.executor = executor
        self.parser = parser

    def run_plan(self, plan):
        results = []

        print("\nExecuting Plan...\n")

        for step in plan:
            print(f"Executing: {step}")

            try:    
                parsed_command = self.parser.parse(step)

                result = self.executor.execute(parsed_command)

                if result == "Command not understood":
                    print(f"Failed: {step}")
                    print("Skipping step...\n")
                    continue
                    

                results.append({
                    "step": step,
                    "result": result
                })

                time.sleep(2)

            except Exception as e:
                print(f"Error occurred while executing {step}: {e}")
                print("Skipping step...\n")
                continue

        print("\nWorkflow Complete.")

        return [
            "open chrome",
            "open somefakeapp",
            "open photoshop"
        ]