from enum import Enum


class RuntimeState(Enum):
    IDLE = "idle"
    OBSERVING = "observing"
    CONTEXT_BUILDING = "context_building"
    MEMORY_RETRIEVAL = "memory_retrieval"
    PLANNING = "planning"
    VALIDATING_PLAN = "validating_plan"
    EXECUTING = "executing"
    VERIFYING = "verifying"
    RECOVERING = "recovering"
    LEARNING = "learning"
    COMPLETED = "completed"
    FAILED = "failed"