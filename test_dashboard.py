from core.dashboard import (
    RuntimeDashboard
)

dashboard = (
    RuntimeDashboard()
)

context = {

    "project":
        "Nova v0.7.1",

    "activity":
        "coding"

}

plan = [

    "step1",

    "step2",

    "step3"

]

memories = [

    "memory1",

    "memory2"

]

verification = {

    "success":
        True

}

recovery = {

    "status":
        "completed"

}

dashboard.display(

    context,

    plan,

    memories,

    verification,

    recovery

)