from core.runtime_metrics import (
    RuntimeMetrics
)

metrics = RuntimeMetrics()

metrics.record_metric(

    "planning_duration",

    205

)

metrics.record_metric(

    "planning_duration",

    180

)

metrics.record_metric(

    "planning_duration",

    220

)

average = metrics.get_average(
    "planning_duration"
)

print(
    "\n=== METRICS ===\n"
)

print(
    f"Average Planning Time: "
    f"{average}"
)