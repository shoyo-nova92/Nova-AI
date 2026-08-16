from core.wake_local import LocalWake


def main():

    wake = LocalWake(
        wakeword_models=["hey_jarvis"],
        inference_framework="onnx",
        threshold=0.35,
    )

    print()
    print("=" * 60)
    print("CONTINUOUS NOVA WAKE TEST")
    print("=" * 60)
    print()
    print("Say: HEY JARVIS")
    print("Press CTRL+C to stop.")
    print()

    try:

        wake.start()

        while True:

            detected = wake.listen_for_nova()

            if detected:
                print()
                print(
                    f"SUCCESS: Wake word detected -> {detected}"
                )
                print()
                print("Say it again...")

    except KeyboardInterrupt:

        print()
        print("Stopping...")

    finally:

        wake.stop()


if __name__ == "__main__":
    main()