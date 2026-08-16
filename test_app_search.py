from core.app_search_engine import AppSearchEngine


engine = AppSearchEngine(auto_index=True)

print("\n=== CHROME ===")
matches = engine.rank_matches("chrome")

for app, score in matches[:10]:
    print(f"{score:6.2f} -> {app['name']}")

print("\n=== VSCODE ===")
matches = engine.rank_matches("vscode")

for app, score in matches[:10]:
    print(f"{score:6.2f} -> {app['name']}")

print("\n=== CHROME RESOLUTION ===")
result = engine.find_best_app_with_candidates("chrome")
print(result)

print("\n=== VSCODE RESOLUTION ===")
result = engine.find_best_app_with_candidates("vscode")
print(result)