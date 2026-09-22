#include "request_classifier.hpp"

using namespace std;

ClassificationResult RequestClassifier::classify(
    const string& input
)
{
    ClassificationResult result;

    if (input.empty())
    {
        result.type =
            RequestType::CLARIFICATION_REQUIRED;

        result.success = false;

        result.message =
            "Cannot classify empty input.";

        return result;
    }

    // --------------------------------------------------------
    // BRANCH 2 — SINGLE ACTION
    // --------------------------------------------------------

    if (
        input.find("create a folder") != string::npos ||
        input.find("make a folder") != string::npos ||
        input == "open" ||
        input.find("open ") == 0 ||
        input == "launch" ||
        input.find("launch ") == 0 ||

        input == "close" ||
        input.find("close ") == 0
    )
    {
        result.type =
            RequestType::SINGLE_ACTION;

        result.success = true;

        result.message =
            "Request classified as single action.";

        return result;
    }

    // --------------------------------------------------------
    // BRANCH 1 — CONVERSATION
    // --------------------------------------------------------

    result.type =
        RequestType::CONVERSATION;

    result.success = true;

    result.message =
        "Request classified as conversation.";

    return result;
}