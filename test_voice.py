from core.voice import VoiceEngine


def main():
    print("=" * 60)
    print("Nova VoiceEngine Test")
    print("=" * 60)

    voice = VoiceEngine()

    print()
    print("Whisper model loaded.")
    print("Speak something after the recording starts.")
    print()

    text = voice.listen(duration=5)

    print()
    print("=" * 60)
    print("TRANSCRIPTION:")
    print(repr(text))
    print("=" * 60)


if __name__ == "__main__":
    main()