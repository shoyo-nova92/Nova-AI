#pragma once

#include "input_event.hpp"
#include "part_a_bridge.hpp"
#include <functional>

class InputCoordinator
{
public:
    using VoiceInput=std::function<PartAResult()>;
    using TextInput=std::function<InputEvent()>;
    explicit InputCoordinator(VoiceInput voice={},TextInput text={});
    InputEvent waitForInput(bool keyboardOnly = false);
    void setBeforeVoiceInput(std::function<void()> callback){beforeVoice_=std::move(callback);}
private:
    std::function<void()> beforeVoice_;
    VoiceInput voice_;
    TextInput text_;
};
