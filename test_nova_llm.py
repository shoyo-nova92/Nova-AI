from nova import NovaRuntimeSpine


def main():
    spine = NovaRuntimeSpine()

    response = spine.ask_llm(
        "In one short sentence, explain what Nova is."
    )

    print()
    print("NOVA LLM:")
    print(response)


if __name__ == "__main__":
    main()