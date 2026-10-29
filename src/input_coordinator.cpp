#include "input_coordinator.hpp"
#include "keyboard_input.hpp"
#include "part_a_bridge.hpp"
#include <iostream>
#include <string>

InputEvent InputCoordinator::waitForInput(bool keyboardOnly) {
    if (keyboardOnly) return KeyboardInput{}.waitForInput();
    std::cout << "\nNOVA\n[V] Type a request  [S] Voice input  [Q] Quit\n> ";
    std::string choice;
    while (std::getline(std::cin, choice)) {
        if (choice == "q" || choice == "Q") return {InputSource::KEYBOARD, "quit"};
        if (choice == "v" || choice == "V") return KeyboardInput{}.waitForInput();
        if (choice == "s" || choice == "S") {
            const auto voice = PartABridge{}.waitForText();
            if (voice.success && !voice.keyboardRequest) return {InputSource::SPEECH, voice.rawText};
            std::cout << "Voice unavailable or keyboard requested. Type your request.\n";
            return KeyboardInput{}.waitForInput();
        }
        if (choice.find_first_not_of(" \t\r\n") != std::string::npos)
            return {InputSource::KEYBOARD, choice};
        std::cout << "Choose V, S or Q, or type your request.\n> ";
    }
    return {InputSource::KEYBOARD, "quit"};
}
