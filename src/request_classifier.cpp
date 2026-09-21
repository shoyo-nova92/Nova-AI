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
    // BRANCH 1 — CONVERSATION
    // --------------------------------------------------------

    result.type =
        RequestType::CONVERSATION;

    result.success = true;

    result.message =
        "Request classified as conversation.";

    return result;
}