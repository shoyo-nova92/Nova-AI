from core.session_continuity import (
    SessionContinuity
)

session = (
    SessionContinuity()
)

session.save_session({

    "project": "Nova",

    "milestone": "v0.9.1",

    "task": "Build Work Context Layer",

    "objective": "Create Session Continuity"

})

print(

    session.load_session()

)