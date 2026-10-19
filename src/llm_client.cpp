#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "llm_client.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

using json = nlohmann::json;

namespace
{
    size_t writeCallback(
        void* contents,
        size_t size,
        size_t count,
        void* userPointer
    )
    {
        size_t totalSize =
            size * count;

        string* response =
            static_cast<string*>(userPointer);

        response->append(
            static_cast<char*>(contents),
            totalSize
        );

        return totalSize;
    }
}

LLMResponse LLMClient::generate(
    const string& prompt
)
{
    return generate(
        "You are Nova, a helpful AI assistant.",
        prompt
    );
}

LLMResponse LLMClient::generate(
    const string& systemPrompt,
    const string& userPrompt
)
{
    LLMResponse result;

    result.success = false;
    result.content = "";
    result.message = "";

    // --------------------------------------------------------
    // API KEY
    // --------------------------------------------------------

    const char* apiKey =
        getenv("OPENROUTER_API_KEY");

    if (
        apiKey == nullptr ||
        string(apiKey).empty()
    )
    {
        result.message =
            "OPENROUTER_API_KEY environment variable is not set.";

        return result;
    }

    // --------------------------------------------------------
    // CURL INITIALIZATION
    // --------------------------------------------------------

    CURL* curl =
        curl_easy_init();

    if (curl == nullptr)
    {
        result.message =
            "Failed to initialize libcurl.";

        return result;
    }

    string responseBody;

    // --------------------------------------------------------
    // REQUEST BODY
    // --------------------------------------------------------

    json requestBody =
    {
        {
            "model",
            "dots-studio/dots-3-note-preview:free"
        },
        {
            "messages",
            json::array(
            {
                {
                    {
                        "role",
                        "system"
                    },
                    {
                        "content",
                        systemPrompt
                    }
                },
                {
                    {
                        "role",
                        "user"
                    },
                    {
                        "content",
                        userPrompt
                    }
                }
            })
        }
    };

    string requestBodyString =
        requestBody.dump();

    // --------------------------------------------------------
    // HEADERS
    // --------------------------------------------------------

    struct curl_slist* headers =
        nullptr;

    string authorization =
        "Authorization: Bearer "
        + string(apiKey);

    headers =
        curl_slist_append(
            headers,
            "Content-Type: application/json"
        );

    headers =
        curl_slist_append(
            headers,
            authorization.c_str()
        );

    // --------------------------------------------------------
    // CURL CONFIGURATION
    // --------------------------------------------------------

    curl_easy_setopt(
        curl,
        CURLOPT_URL,
        "https://openrouter.ai/api/v1/chat/completions"
    );

    curl_easy_setopt(
        curl,
        CURLOPT_HTTPHEADER,
        headers
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POST,
        1L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_POSTFIELDS,
        requestBodyString.c_str()
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEFUNCTION,
        writeCallback
    );

    curl_easy_setopt(
        curl,
        CURLOPT_WRITEDATA,
        &responseBody
    );

    curl_easy_setopt(
        curl,
        CURLOPT_CONNECTTIMEOUT,
        10L
    );

    curl_easy_setopt(
        curl,
        CURLOPT_TIMEOUT,
        60L
    );

    // --------------------------------------------------------
    // EXECUTE REQUEST
    // --------------------------------------------------------

    CURLcode curlResult =
        curl_easy_perform(curl);

    if (
        curlResult != CURLE_OK
    )
    {
        result.message =
            "OpenRouter request failed: "
            + string(
                curl_easy_strerror(
                    curlResult
                )
            );

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return result;
    }

    // --------------------------------------------------------
    // HTTP STATUS
    // --------------------------------------------------------

    long httpStatus = 0;

    curl_easy_getinfo(
        curl,
        CURLINFO_RESPONSE_CODE,
        &httpStatus
    );

    if (
        httpStatus < 200 ||
        httpStatus >= 300
    )
    {
        result.message =
            "OpenRouter returned HTTP status "
            + to_string(httpStatus)
            + ".";

        cout
            << "[LLM] HTTP status: "
            << httpStatus
            << endl;

        cout
            << "[LLM] Response: "
            << responseBody
            << endl;

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        return result;
    }

    // --------------------------------------------------------
    // PARSE RESPONSE
    // --------------------------------------------------------

    
    try
    {
        json response = json::parse(responseBody);

        
        if (!response.is_object() ||
            !response.contains("choices") ||
            !response["choices"].is_array() ||
            response["choices"].empty())
        {
            result.message =
                "Invalid OpenRouter response: missing or empty choices array.";

            // Print the response for diagnosis, without exposing credentials.
            std::cerr << "[LLM] Unexpected OpenRouter response: "
                    << response.dump().substr(0, 2000)
                    << std::endl;
        }

        else
        {
            const auto& choice = response["choices"][0];

            if (!choice.is_object() ||
                !choice.contains("message") ||
                !choice["message"].is_object())
            {
                result.message =
                    "Invalid OpenRouter response: missing message object.";
            }
            else
            {
                const auto& message = choice["message"];

                if (!message.contains("content") ||
                    !message["content"].is_string())
                {
                    // Some providers return null content for refusal,
                    // tool-call, or otherwise non-text responses.
                    string detail =
                        "OpenRouter returned no usable text content.";

                    if (message.contains("refusal") &&
                        message["refusal"].is_string())
                    {
                        detail += " Refusal: " +
                            message["refusal"].get<string>();
                    }
                    else if (choice.contains("finish_reason") &&
                             choice["finish_reason"].is_string())
                    {
                        detail += " Finish reason: " +
                            choice["finish_reason"].get<string>();
                    }

                    result.message = detail;
                }
                else
                {
                    result.content = message["content"].get<string>();

                    if (result.content.empty())
                    {
                        result.message =
                            "OpenRouter returned an empty text response.";
                    }
                    else
                    {
                        result.success = true;
                        result.message = "LLM response received.";
                    }
                }
            }
        }
    }
    catch (const exception& exception)
    {
        result.message =
            "Failed to parse OpenRouter response: " +
            string(exception.what());
    }


    // --------------------------------------------------------
    // CLEANUP
    // --------------------------------------------------------

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    return result;
}