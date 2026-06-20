from core.runtime_trace import RuntimeTrace

trace = RuntimeTrace()

trace.start_trace(
    "prepare coding environment"
)

trace.log_event(

    "observe_complete",

    {

        "app": "VS Code"

    }

)

print(

    trace.save_trace()

)