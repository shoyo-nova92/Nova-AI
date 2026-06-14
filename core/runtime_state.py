from enum import Enum


class RuntimeState(Enum):

    PENDING = "pending"

    RUNNING = "running"

    VERIFYING = "verifying"

    RECOVERING = "recovering"

    LEARNING = "learning"

    COMPLETE = "complete"

    FAILED = "failed"