from core.llm_client import LLMClient


def main():

    print("=" * 60)
    print("NOVA LLM TEST")
    print("=" * 60)

    client = LLMClient()

    response = client.generate(
        "Say hello to Nova in one short sentence."
    )

    print()
    print("RESPONSE:")
    print(response)
    print()


if __name__ == "__main__":
    main()