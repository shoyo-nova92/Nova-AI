from core.capability_router import CapabilityRouter

router = CapabilityRouter()

command = input("Command: ")

plugin = router.route(command)

if plugin:
    print(f"Plugin Found: {plugin.name}")
else:
    print("Use Core Nova")