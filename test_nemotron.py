from core.llm_client import LLMClient


client = LLMClient()

print("\n" + "=" * 60)
print("NEMOTRON DOUBLE REQUEST TEST")
print("=" * 60)

print("\n--- REQUEST 1 ---")

result_1 = client.generate(
    prompt="Classify this command: I have VS Code open and want to save this file.",
    system_prompt="""
You are a command classifier.
Return only valid JSON.
""",
)

print("\nRESULT 1:")
print(result_1)


print("\n--- REQUEST 2 ---")

result_2 = client.generate(
    prompt="I have VS Code open. How exactly do I save this file?",
    system_prompt="""
You are Nova, a helpful desktop AI assistant.
Answer concisely and directly.
""",
)

print("\nRESULT 2:")
print(result_2)