#include <curl/curl.h>
#include "llm_client.hpp"
#include "utf8_text.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iostream>
#include <vector>
using nlohmann::json;
namespace {
size_t writeCallback(void* data, size_t size, size_t count, void* target) {
    const size_t bytes = size * count;
    auto& out = *static_cast<string*>(target);
    if (out.size() + bytes > 8 * 1024 * 1024) return 0;
    out.append(static_cast<char*>(data), bytes);
    return bytes;
}
string setting(const string& name, const string& fallback = "") {
    const auto* value = getenv(name.c_str());
    return value && *value ? value : fallback;
}
long boundedSetting(const string& name, long fallback, long minimum, long maximum) {
    try { const auto text=setting(name); size_t used=0; const long n=stol(text,&used); return used==text.size() && n>=minimum && n<=maximum ? n : fallback; }
    catch (...) { return fallback; }
}
bool fallbackEligible(LLMFailureCategory category) {
    return category==LLMFailureCategory::TIMEOUT || category==LLMFailureCategory::CONNECTION_ERROR ||
        category==LLMFailureCategory::PROVIDER_ERROR || category==LLMFailureCategory::RATE_LIMIT;
}
}
LLMFailureCategory classifyLLMFailure(int curlCode, long status) {
    if (curlCode == CURLE_OPERATION_TIMEDOUT) return LLMFailureCategory::TIMEOUT;
    if (curlCode != CURLE_OK) return LLMFailureCategory::CONNECTION_ERROR;
    if (status == 401 || status == 403) return LLMFailureCategory::AUTH_ERROR;
    if (status == 429) return LLMFailureCategory::RATE_LIMIT;
    if (status < 200 || status >= 300) return LLMFailureCategory::PROVIDER_ERROR;
    return LLMFailureCategory::NONE;
}
LLMResponse LLMClient::generate(const string& prompt) {
    return generate("You are Nova, a helpful AI assistant.", prompt);
}
LLMResponse LLMClient::generate(const string& systemPrompt, const string& userPrompt, const string& role) {
    LLMResponse result{};
    try {
        Utf8::require(systemPrompt, "llm."+role+".system_prompt");
        Utf8::require(userPrompt, "llm."+role+".user_prompt");
    } catch (const Utf8::Error& error) {
        result.failureCategory=LLMFailureCategory::INVALID_UTF8; result.message=error.what(); return result;
    }
    const string key=setting("OPENROUTER_API_KEY");
    if (key.empty()) { result.failureCategory=LLMFailureCategory::CONFIGURATION_ERROR; result.message="OPENROUTER_API_KEY is not configured."; return result; }
    string roleKey=role;
    transform(roleKey.begin(),roleKey.end(),roleKey.begin(),[](unsigned char c){return static_cast<char>(toupper(c));});
    const string model=setting("NOVA_"+roleKey+"_MODEL",setting("NOVA_MODEL","nvidia/nemotron-3-ultra-550b-a55b:free"));
    vector<string> models{model};
    const string fallback=setting("NOVA_"+roleKey+"_FALLBACK_MODEL",setting("NOVA_FALLBACK_MODEL"));
    if (!fallback.empty() && fallback!=model) models.push_back(fallback);
    for (const auto& selected : models) {
        CURL* curl=curl_easy_init();
        if (!curl) { result.failureCategory=LLMFailureCategory::CONFIGURATION_ERROR; result.message="Could not initialize HTTP transport."; return result; }
        json body={{"model",selected},{"messages",json::array({{{"role","system"},{"content",systemPrompt}},{{"role","user"},{"content",userPrompt}}})},
            {"temperature",0},{"max_tokens",boundedSetting("NOVA_MAX_OUTPUT_TOKENS",8192,256,65536)}};
        string request;
        try { request=Utf8::dump(body,"llm_request"); }
        catch (const Utf8::Error& error) { curl_easy_cleanup(curl); result.failureCategory=LLMFailureCategory::INVALID_UTF8; result.message=error.what(); return result; }
        string response;
        curl_slist* headers=nullptr;
        headers=curl_slist_append(headers,"Content-Type: application/json; charset=utf-8");
        headers=curl_slist_append(headers,("Authorization: Bearer "+key).c_str());
        curl_easy_setopt(curl,CURLOPT_URL,"https://openrouter.ai/api/v1/chat/completions");
        curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
        curl_easy_setopt(curl,CURLOPT_POST,1L);
        curl_easy_setopt(curl,CURLOPT_POSTFIELDS,request.c_str());
        curl_easy_setopt(curl,CURLOPT_POSTFIELDSIZE,static_cast<long>(request.size()));
        curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,writeCallback);
        curl_easy_setopt(curl,CURLOPT_WRITEDATA,&response);
        curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,boundedSetting("NOVA_LLM_CONNECT_TIMEOUT_SECONDS",10,1,120));
        curl_easy_setopt(curl,CURLOPT_TIMEOUT,boundedSetting("NOVA_LLM_TIMEOUT_SECONDS",90,5,300));
        const auto began=chrono::steady_clock::now();
        const auto code=curl_easy_perform(curl);
        long http=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http);
        const auto elapsed=chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now()-began).count();
        curl_slist_free_all(headers); curl_easy_cleanup(curl);
        result={}; result.httpStatus=http; result.failureCategory=classifyLLMFailure(code,http);
        if (code!=CURLE_OK) result.message="Provider request failed: "+string(curl_easy_strerror(code));
        else if (http<200 || http>=300) result.message="Provider returned HTTP "+to_string(http)+". Check quota/provider availability; retry later.";
        else {
            try {
                const auto parsed=json::parse(response);
                const auto& content=parsed.at("choices").at(0).at("message").at("content");
                if (!content.is_string() || content.get_ref<const string&>().empty()) throw runtime_error("Empty response content.");
                result.content=content.get<string>();
                Utf8::require(result.content,"llm_response."+role);
                result.success=true; result.message="LLM response received."; result.failureCategory=LLMFailureCategory::NONE;
            } catch (const Utf8::Error& error) { result.failureCategory=LLMFailureCategory::INVALID_UTF8; result.message=error.what(); }
            catch (const exception&) { result.failureCategory=LLMFailureCategory::INVALID_RESPONSE; result.message="Provider returned an incomplete or malformed text response."; }
        }
        // Metadata only: never emit credentials, prompts, page text or provider identifiers.
        cout << "[LLM] role=" << role << " model=" << selected << " request_bytes=" << request.size()
             << " response_bytes=" << response.size() << " latency_ms=" << elapsed << " http=" << http
             << " category=" << llmFailureName(result.failureCategory) << '\n';
        if (result.success || !fallbackEligible(result.failureCategory)) return result;
    }
    return result;
}
