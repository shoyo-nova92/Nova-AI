from core.persistent_context import (
    PersistentContext
)

context_engine = (
    PersistentContext()
)

context_engine.save_context(

    {

        "project":
            "Nova v0.7.1",

        "current_goal":
            "Build Persistent Context Engine",

        "current_step":
            "Testing save/load"

    }

)

context = (
    context_engine.load_context()
)

print(
    "\n=== PERSISTENT CONTEXT ===\n"
)

print(context)