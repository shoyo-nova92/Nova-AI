from core.memory_v2 import MemoryV2

memory = MemoryV2()

memory.save_workflow(
    "study workflow",
    [
        "open chrome",
        "open notion",
        "open spotify"
    ]
)

workflow = memory.load_workflow("study workflow")

print(workflow)