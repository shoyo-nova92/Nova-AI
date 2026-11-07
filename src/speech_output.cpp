#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <sapi.h>
#include <sphelper.h>
#include <bcrypt.h>
#endif
#include "speech_output.hpp"
#include "utf8_text.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <thread>
using nlohmann::json;
namespace {
std::string setting(const char* key,std::string fallback={}){const auto* value=std::getenv(key);return value&&*value?value:std::move(fallback);}
std::string lower(std::string text){std::transform(text.begin(),text.end(),text.begin(),[](unsigned char c){return std::tolower(c);});return text;}
bool flag(const char* key,bool fallback){const auto value=lower(setting(key));return value.empty()?fallback:value!="0"&&value!="false"&&value!="off";}
unsigned number(const char* key,unsigned fallback,unsigned minimum,unsigned maximum){try{const auto text=setting(key);size_t consumed=0;auto value=std::stoul(text,&consumed);if(consumed==text.size() && value>=minimum && value<=maximum)return static_cast<unsigned>(value);}catch(...){}return fallback;}
struct Transfer {std::vector<char> bytes;std::string type,id,retryAfter;std::function<bool()> cancelled;};
size_t body(void* data,size_t size,size_t count,void* target){auto& transfer=*static_cast<Transfer*>(target);const auto bytes=size*count;if(transfer.bytes.size()+bytes>64*1024*1024)return 0;const auto* start=static_cast<char*>(data);transfer.bytes.insert(transfer.bytes.end(),start,start+bytes);return bytes;}
size_t header(char* data,size_t size,size_t count,void* target){auto& transfer=*static_cast<Transfer*>(target);std::string line(data,size*count);const auto colon=line.find(':');if(colon!=std::string::npos){auto name=lower(line.substr(0,colon));auto value=line.substr(colon+1);const auto start=value.find_first_not_of(" \t");value=start==std::string::npos?"":value.substr(start,value.find_last_not_of(" \r\n\t")-start+1);if(name=="content-type")transfer.type=lower(value);if(name=="x-generation-id")transfer.id=value;if(name=="retry-after")transfer.retryAfter=value;}return size*count;}
std::string digest(const std::string& text){
#ifdef _WIN32
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;unsigned char result[32]{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)throw std::runtime_error("Audio cache hash unavailable");
    auto status=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0);
    if(status>=0)status=BCryptHashData(hash,reinterpret_cast<PUCHAR>(const_cast<char*>(text.data())),static_cast<ULONG>(text.size()),0);
    if(status>=0)status=BCryptFinishHash(hash,result,sizeof(result),0);
    if(hash)BCryptDestroyHash(hash);BCryptCloseAlgorithmProvider(algorithm,0);if(status<0)throw std::runtime_error("Audio cache hash failed");
    std::ostringstream out;for(auto value:result)out<<std::hex<<std::setfill('0')<<std::setw(2)<<static_cast<unsigned>(value);return out.str();
#else
    return std::to_string(std::hash<std::string>{}(text));
#endif
}
void writeBytes(std::ofstream& output,unsigned value,unsigned bytes){for(unsigned i=0;i<bytes;++i)output.put(static_cast<char>((value>>(i*8))&255));}
bool play(const SpeechAudio& audio,const std::function<bool()>& cancelled){
#ifdef _WIN32
    WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_PCM;format.nChannels=static_cast<WORD>(audio.channels);format.nSamplesPerSec=audio.sampleRate;format.wBitsPerSample=static_cast<WORD>(audio.bits);format.nBlockAlign=format.nChannels*format.wBitsPerSample/8;format.nAvgBytesPerSec=format.nSamplesPerSec*format.nBlockAlign;
    HWAVEOUT device=nullptr;if(waveOutOpen(&device,WAVE_MAPPER,&format,0,0,CALLBACK_NULL)!=MMSYSERR_NOERROR)throw std::runtime_error("No usable Windows audio output device");
    WAVEHDR buffer{};buffer.lpData=const_cast<char*>(audio.pcm.data());buffer.dwBufferLength=static_cast<DWORD>(audio.pcm.size());
    if(waveOutPrepareHeader(device,&buffer,sizeof(buffer))!=MMSYSERR_NOERROR){waveOutClose(device);throw std::runtime_error("Cannot prepare PCM output");}
    if(waveOutWrite(device,&buffer,sizeof(buffer))!=MMSYSERR_NOERROR){waveOutUnprepareHeader(device,&buffer,sizeof(buffer));waveOutClose(device);throw std::runtime_error("Cannot play PCM audio");}
    while(!(buffer.dwFlags&WHDR_DONE)){if(cancelled()){waveOutReset(device);break;}std::this_thread::sleep_for(std::chrono::milliseconds(15));}
    waveOutUnprepareHeader(device,&buffer,sizeof(buffer));waveOutClose(device);return !cancelled();
#else
    throw std::runtime_error("Audio playback requires Nova's Windows host");
#endif
}
bool windowsVoice(const std::string& text,const std::function<bool()>& cancelled,std::string& name){
#ifdef _WIN32
    ISpVoice* voice=nullptr;if(FAILED(CoCreateInstance(CLSID_SpVoice,nullptr,CLSCTX_ALL,IID_ISpVoice,reinterpret_cast<void**>(&voice))))return false;
    IEnumSpObjectTokens* tokens=nullptr;
    if(SUCCEEDED(SpEnumTokens(SPCAT_VOICES,L"Gender=Female",nullptr,&tokens))){ISpObjectToken* token=nullptr;if(tokens->Next(1,&token,nullptr)==S_OK){voice->SetVoice(token);LPWSTR description=nullptr;if(SUCCEEDED(token->GetStringValue(nullptr,&description))){const int size=WideCharToMultiByte(CP_UTF8,0,description,-1,nullptr,0,nullptr,nullptr);if(size>1){std::string value(size,'\0');WideCharToMultiByte(CP_UTF8,0,description,-1,value.data(),size,nullptr,nullptr);name=value.substr(0,size-1);}CoTaskMemFree(description);}token->Release();}tokens->Release();}
    // Never silently substitute a male default when no female token is installed.
    if(name.empty()){voice->Release();return false;}
    const auto size=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),nullptr,0);std::wstring wide(size,L'\0');MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.data(),static_cast<int>(text.size()),wide.data(),size);
    const auto result=voice->Speak(wide.c_str(),SPF_ASYNC|SPF_IS_NOT_XML,nullptr);
    if(SUCCEEDED(result))while(voice->WaitUntilDone(25)==S_FALSE){if(cancelled()){voice->Speak(nullptr,SPF_ASYNC|SPF_PURGEBEFORESPEAK,nullptr);break;}}
    voice->Release();return SUCCEEDED(result);
#else
    return false;
#endif
}
}
SpeechOptions SpeechOptions::fromEnvironment(){SpeechOptions options;options.enabled=flag("NOVA_TTS_ENABLED",true);options.windowsFallback=lower(setting("NOVA_TTS_FALLBACK","windows"))=="windows";options.playback=flag("NOVA_TTS_PLAYBACK",true);options.cache=flag("NOVA_TTS_CACHE",true);options.narrateProgress=flag("NOVA_TTS_NARRATE_PROGRESS",true);options.endpoint=setting("NOVA_TTS_ENDPOINT",options.endpoint);options.model=setting("NOVA_TTS_MODEL",options.model);options.voice=setting("NOVA_TTS_VOICE",options.voice);options.apiKey=setting("OPENROUTER_API_KEY");options.sampleRate=number("NOVA_TTS_SAMPLE_RATE",44100,8000,192000);options.timeoutSeconds=number("NOVA_TTS_TIMEOUT_SECONDS",25,1,120);options.cacheDirectory=Utf8::toPath(setting("NOVA_TTS_CACHE_DIR","audio_output/tts"));return options;}
SpeechResult SpeechClient::synthesize(const std::string& text,const std::function<bool()>& cancelled)const{
    static std::once_flag curlInitialized;std::call_once(curlInitialized,[]{curl_global_init(CURL_GLOBAL_DEFAULT);});
    SpeechResult result;if(text.empty() || text.size()>5000){result.category="invalid_input";result.message="Speech chunk must contain 1 to 5000 UTF-8 bytes";return result;}
    try{Utf8::require(text,"tts.input");}catch(...){result.category="invalid_input";return result;}
    if(options_.apiKey.empty()){result.category="configuration";result.message="OPENROUTER_API_KEY is not configured";return result;}
    if(cancelled && cancelled()){result.cancelled=true;result.category="cancelled";return result;}
    auto* curl=curl_easy_init();if(!curl){result.category="transport";return result;}
    const auto request=json{{"model",options_.model},{"voice",options_.voice},{"input",text},{"response_format","pcm"}}.dump();Transfer transfer;transfer.cancelled=cancelled;
    curl_slist* headers=nullptr;headers=curl_slist_append(headers,("Authorization: Bearer "+options_.apiKey).c_str());headers=curl_slist_append(headers,"Content-Type: application/json");headers=curl_slist_append(headers,"Accept: audio/pcm");headers=curl_slist_append(headers,"X-Title: Nova Desktop TTS");
    curl_easy_setopt(curl,CURLOPT_URL,options_.endpoint.c_str());curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_POST,1L);curl_easy_setopt(curl,CURLOPT_POSTFIELDS,request.c_str());curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,static_cast<long>(request.size()));curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,body);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&transfer);curl_easy_setopt(curl,CURLOPT_HEADERFUNCTION,header);curl_easy_setopt(curl,CURLOPT_HEADERDATA,&transfer);curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,5L);curl_easy_setopt(curl,CURLOPT_TIMEOUT,static_cast<long>(options_.timeoutSeconds));curl_easy_setopt(curl,CURLOPT_NOPROGRESS,0L);curl_easy_setopt(curl,CURLOPT_XFERINFODATA,&transfer);
    curl_easy_setopt(curl,CURLOPT_XFERINFOFUNCTION,+[](void* data,curl_off_t,curl_off_t,curl_off_t,curl_off_t)->int{auto& request=*static_cast<Transfer*>(data);return request.cancelled && request.cancelled()?1:0;});
    const auto code=curl_easy_perform(curl);curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&result.httpStatus);curl_slist_free_all(headers);curl_easy_cleanup(curl);
    if(code==CURLE_ABORTED_BY_CALLBACK){result.cancelled=true;result.category="cancelled";return result;}
    if(code!=CURLE_OK){result.category=code==CURLE_OPERATION_TIMEDOUT?"timeout":"transport";result.message="Speech provider connection unavailable";return result;}
    if(result.httpStatus<200 || result.httpStatus>=300){
        result.category=result.httpStatus==429?"rate_limit":result.httpStatus==401?"authentication":result.httpStatus==402?"credits":result.httpStatus==404?"model_unavailable":result.httpStatus>=500?"provider":"invalid_request";result.message="Speech request returned HTTP "+std::to_string(result.httpStatus);
        if(result.httpStatus==429){result.retryAfterSeconds=60;try{auto retry=std::stoul(transfer.retryAfter);result.retryAfterSeconds=static_cast<unsigned>(std::min(retry,86400UL));}catch(...){}
            try{const auto error=json::parse(transfer.bytes.begin(),transfer.bytes.end());const auto metadata=error.at("error").at("metadata");if(metadata.value("limit_source",std::string{})=="openrouter_free_tier_daily"){const auto reset=std::stoll(metadata.at("headers").at("X-RateLimit-Reset").get<std::string>());const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();result.retryAfterSeconds=static_cast<unsigned>(std::clamp((reset-now)/1000,1LL,86400LL));result.category="daily_quota";}}catch(...){}
        }return result;
    }
    if(!transfer.type.starts_with("audio/pcm")){result.category="invalid_audio";result.message="Expected raw PCM; provider returned a different content type";return result;}
    result.audio.sampleRate=options_.sampleRate;result.audio.generationId=transfer.id;
    std::smatch match;if(std::regex_search(transfer.type,match,std::regex("rate=([0-9]+)")))result.audio.sampleRate=static_cast<unsigned>(std::stoul(match[1]));if(std::regex_search(transfer.type,match,std::regex("channels=([0-9]+)")))result.audio.channels=static_cast<unsigned>(std::stoul(match[1]));
    if(transfer.bytes.empty() || transfer.bytes.size()%2 || result.audio.sampleRate<8000 || result.audio.sampleRate>192000 || result.audio.channels!=1){result.category="invalid_audio";result.message="Speech response was empty or did not contain valid mono 16-bit PCM";return result;}
    result.audio.pcm=std::move(transfer.bytes);result.success=true;return result;
}
std::string SpeechOutput::readableText(const std::string& input){
    std::string text=input;
    for(const auto* marker:{"\nProposed operation:","\nProposed steps:","\nConfidence and uncertainty:","\nAdvisory concerns are recorded"}){const auto position=text.find(marker);if(position!=std::string::npos)text.erase(position);}
    text=std::regex_replace(text,std::regex("```[\\s\\S]*?```")," Code is available in the terminal. ");text=std::regex_replace(text,std::regex("\\[([^\\]]+)\\]\\([^\\)]+\\)"),"$1");
    text=std::regex_replace(text,std::regex("https?://[^\\s]+"),"the linked page");text=std::regex_replace(text,std::regex("[A-Za-z]:[\\\\/][^\\s]+\\.md"),"the proposal shown in the terminal");text=std::regex_replace(text,std::regex("[`*#]"),"");text=std::regex_replace(text,std::regex("[\\r\\n\\t ]+")," ");
    const auto start=text.find_first_not_of(' ');return start==std::string::npos?"":text.substr(start,text.find_last_not_of(' ')-start+1);
}
std::vector<std::string> SpeechOutput::splitText(const std::string& text,size_t maximum){
    std::vector<std::string> chunks;if(maximum<4)throw std::runtime_error("Speech chunk size is too small");size_t offset=0;
    while(offset<text.size()){size_t length=std::min(maximum,text.size()-offset);while(length && offset+length<text.size() && (static_cast<unsigned char>(text[offset+length])&0xc0)==0x80)--length;
        if(offset+length<text.size()){const auto boundary=text.find_last_of(".!?;\n ",offset+length-1);if(boundary!=std::string::npos && boundary>=offset+length/2)length=boundary-offset+1;}
        chunks.push_back(text.substr(offset,length));offset+=length;
    }return chunks;
}
void SpeechOutput::saveWave(const std::filesystem::path& path,const SpeechAudio& audio){
    std::ofstream out(path,std::ios::binary|std::ios::trunc);out.write("RIFF",4);writeBytes(out,static_cast<unsigned>(audio.pcm.size()+36),4);out.write("WAVEfmt ",8);writeBytes(out,16,4);writeBytes(out,1,2);writeBytes(out,audio.channels,2);writeBytes(out,audio.sampleRate,4);writeBytes(out,audio.sampleRate*audio.channels*audio.bits/8,4);writeBytes(out,audio.channels*audio.bits/8,2);writeBytes(out,audio.bits,2);out.write("data",4);writeBytes(out,static_cast<unsigned>(audio.pcm.size()),4);out.write(audio.pcm.data(),audio.pcm.size());if(!out)throw std::runtime_error("Cannot save speech audio");
}
struct SpeechOutput::Impl {
    struct Message{std::string text;NarrationKind kind;};SpeechOptions options;SpeechClient client;StatusSink status;
    std::mutex mutex;std::condition_variable changed;std::deque<Message> queue;std::thread worker;std::atomic<unsigned> generation{0};bool closing=false,busy=false;NarrationKind current=NarrationKind::PROGRESS;std::string lastText;std::chrono::steady_clock::time_point lastQueued{},cooldown{};bool fallbackReported=false;
    Impl(SpeechOptions value,StatusSink sink):options(std::move(value)),client(options),status(std::move(sink)){if(options.enabled){curl_global_init(CURL_GLOBAL_DEFAULT);worker=std::thread([this]{run();});}}
    void report(const std::string& message){if(status)try{status(message);}catch(...){/* Audio diagnostics must never stop the worker. */}}
    void run(){
#ifdef _WIN32
        const auto com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
#endif
        while(true){Message message;unsigned epoch;
            {std::unique_lock lock(mutex);changed.wait(lock,[&]{return closing||!queue.empty();});if(closing)break;message=std::move(queue.front());queue.pop_front();busy=true;current=message.kind;epoch=generation.load();}
            const auto cancelled=[&]{return generation.load()!=epoch;};
            try{for(const auto& chunk:SpeechOutput::splitText(message.text)){
                if(cancelled())break;SpeechResult result;std::filesystem::path cached;
                if(options.cache){std::filesystem::create_directories(options.cacheDirectory);cached=options.cacheDirectory/(digest(options.endpoint+options.model+options.voice+std::to_string(options.sampleRate)+chunk)+".wav");if(std::filesystem::exists(cached)){
                    std::ifstream file(cached,std::ios::binary);std::vector<char> data((std::istreambuf_iterator<char>(file)),{});
                    auto integer=[&](size_t position,unsigned count){unsigned value=0;for(unsigned i=0;i<count;++i)value|=static_cast<unsigned>(static_cast<unsigned char>(data[position+i]))<<(8*i);return value;};
                    if(data.size()>44 && std::string(data.data(),4)=="RIFF" && std::string(data.data()+36,4)=="data" && integer(4,4)+8==data.size() && integer(22,2)==1 && integer(34,2)==16){result.success=true;result.audio.sampleRate=integer(24,4);result.audio.pcm.assign(data.begin()+44,data.end());}
                }}
                if(!result.success && std::chrono::steady_clock::now()>=cooldown){result=client.synthesize(chunk,cancelled);if(result.retryAfterSeconds)cooldown=std::chrono::steady_clock::now()+std::chrono::seconds(result.retryAfterSeconds);if(result.success && options.cache)SpeechOutput::saveWave(cached,result.audio);}
                if(cancelled() || result.cancelled)break;
                if(result.success){fallbackReported=false;if(options.playback)play(result.audio,cancelled);}
                else if(options.windowsFallback && options.playback){std::string voice;if(!fallbackReported){report("HeyGen speech unavailable ("+(result.category.empty()?std::string("provider cooldown"):result.category)+"). Using the installed Windows female-voice fallback; text remains available.");fallbackReported=true;}if(!windowsVoice(chunk,cancelled,voice))report("Windows speech output unavailable; continuing with text.");}
                else if(!fallbackReported){report("Speech unavailable; continuing with text. "+result.message);fallbackReported=true;}
            }}catch(const std::exception& error){report(std::string("Audio output unavailable; continuing with text. ")+error.what());}
            {std::lock_guard lock(mutex);busy=false;changed.notify_all();}
        }
#ifdef _WIN32
        if(SUCCEEDED(com))CoUninitialize();
#endif
    }
};
SpeechOutput::SpeechOutput(SpeechOptions options,StatusSink status):impl_(std::make_unique<Impl>(std::move(options),std::move(status))){}
SpeechOutput::~SpeechOutput(){{std::lock_guard lock(impl_->mutex);impl_->closing=true;impl_->queue.clear();++impl_->generation;impl_->changed.notify_all();}if(impl_->worker.joinable())impl_->worker.join();}
void SpeechOutput::speak(const std::string& input,NarrationKind kind){auto& state=*impl_;if(!state.options.enabled || (kind==NarrationKind::PROGRESS && !state.options.narrateProgress))return;auto text=readableText(input);if(text.empty())return;std::lock_guard lock(state.mutex);const auto now=std::chrono::steady_clock::now();if(text==state.lastText && now-state.lastQueued<std::chrono::seconds(3))return;state.lastText=text;state.lastQueued=now;
    if(kind==NarrationKind::PROGRESS)state.queue.erase(std::remove_if(state.queue.begin(),state.queue.end(),[](const auto& message){return message.kind==NarrationKind::PROGRESS;}),state.queue.end());
    else {state.queue.erase(std::remove_if(state.queue.begin(),state.queue.end(),[&](const auto& message){return kind==NarrationKind::REPLY || message.kind==NarrationKind::PROGRESS;}),state.queue.end());if(state.busy && (state.current==NarrationKind::PROGRESS || kind==NarrationKind::REPLY))++state.generation;}
    state.queue.push_back({std::move(text),kind});state.changed.notify_one();
}
void SpeechOutput::interrupt(){auto& state=*impl_;std::lock_guard lock(state.mutex);++state.generation;state.queue.clear();state.changed.notify_all();}
bool SpeechOutput::waitUntilIdle(std::chrono::milliseconds timeout){std::unique_lock lock(impl_->mutex);return impl_->changed.wait_for(lock,timeout,[&]{return !impl_->busy && impl_->queue.empty();});}
