#include "conversation_handler.hpp"

using namespace std;

LLMResponse ConversationHandler::handle(
    const string& input
)
{
    LLMResponse result;

    if (input.empty())
    {
        result.success = false;
        result.content = "";
        result.message =
            "Conversation input is empty.";

        return result;
    }

    LLMClient llmClient;

    return llmClient.generate("You are Nova, a helpful AI assistant.",input,"conversation");
}
