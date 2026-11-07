#pragma once
#include "narration.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
#include <vector>

struct SpeechOptions {
    bool enabled=true,windowsFallback=true,playback=true,cache=true,narrateProgress=true;
    std::string endpoint="https://openrouter.ai/api/v1/audio/speech";
    std::string model="heygen/heygen-voice-1-turbo:free",voice="nadine",apiKey;
    unsigned sampleRate=44100,timeoutSeconds=25;
    std::filesystem::path cacheDirectory;
    static SpeechOptions fromEnvironment();
};
struct SpeechAudio {std::vector<char> pcm;unsigned sampleRate=44100,channels=1,bits=16;std::string generationId;};
struct SpeechResult {
    bool success=false,cancelled=false;long httpStatus=0;unsigned retryAfterSeconds=0;
    SpeechAudio audio;std::string category,message;
};
class SpeechClient {
    SpeechOptions options_;
public:
    explicit SpeechClient(SpeechOptions options):options_(std::move(options)){}
    SpeechResult synthesize(const std::string& text,const std::function<bool()>& cancelled={})const;
};
class SpeechOutput {
public:
    using StatusSink=std::function<void(const std::string&)>;
    explicit SpeechOutput(SpeechOptions options=SpeechOptions::fromEnvironment(),StatusSink status={});
    ~SpeechOutput();
    void speak(const std::string& text,NarrationKind kind=NarrationKind::REPLY);
    void interrupt();
    bool waitUntilIdle(std::chrono::milliseconds timeout);
    static std::string readableText(const std::string& text);
    static std::vector<std::string> splitText(const std::string& text,size_t maximumBytes=3900);
    static void saveWave(const std::filesystem::path& path,const SpeechAudio& audio);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
