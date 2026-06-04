from core.guidance_engine import GuidanceEngine

guide = GuidanceEngine()

app = input("Current App: ")
goal = input("What do you need help with?: ")

result = guide.guide(app, goal)

print("\nNova Guidance:")
print(result)