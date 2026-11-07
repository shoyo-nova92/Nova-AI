#include "input_coordinator.hpp"
#include "keyboard_input.hpp"
#include "part_a_bridge.hpp"
#include <iostream>
#include <string>

InputCoordinator::InputCoordinator(VoiceInput voice,TextInput text)
    :voice_(voice?std::move(voice):VoiceInput([]{return PartABridge{}.waitForText();})),
     text_(text?std::move(text):TextInput([]{return KeyboardInput{}.waitForInput();})){}

InputEvent InputCoordinator::waitForInput(bool keyboardOnly) {
    if (keyboardOnly) return text_();
    std::cout << "\n[WAKEWORD] Waiting for Hey Nova. Press V for text input, or Q to quit.\n";
    if(beforeVoice_)beforeVoice_();
    const auto voice=voice_();
    if(voice.success && !voice.keyboardRequest)return {InputSource::SPEECH,voice.rawText};
    std::cout << (voice.keyboardRequest?"[INPUT] Switched to text input.\n":"Voice unavailable. Type your request.\n");
    return text_();
}
