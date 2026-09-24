#include "request_classifier.hpp"

#include <iostream>
#include <string>
#include <vector>

using namespace std;

string requestTypeToString(RequestType type)
{
    switch (type)
    {
        case RequestType::CONVERSATION:
            return "CONVERSATION";

        case RequestType::SINGLE_ACTION:
            return "SINGLE_ACTION";

        case RequestType::MULTI_STEP_GOAL:
            return "MULTI_STEP_GOAL";

        case RequestType::EXISTING_WORKFLOW:
            return "EXISTING_WORKFLOW";

        case RequestType::CLARIFICATION_REQUIRED:
            return "CLARIFICATION_REQUIRED";

        case RequestType::UNSUPPORTED:
            return "UNSUPPORTED";
    }

    return "UNKNOWN";
}

int main()
{
    RequestClassifier classifier;

    vector<string> testInputs =
    {
        "What is inheritance in Java?",
        "Create a folder called Nova on my desktop.",
        "Open Chrome and search for Python tutorials.",
        "Start my morning routine.",
        "Open that file.",
        "Control an application Nova has no integration for."
    };

    cout << "========================================" << endl;
    cout << "LLM ROOT ROUTER TEST" << endl;
    cout << "========================================" << endl;

    for (const string& input : testInputs)
    {
        cout << endl;
        cout << "[INPUT]" << endl;
        cout << input << endl;

        ClassificationResult result =
            classifier.classify(input);

        if (!result.success)
        {
            cout << "[FAILURE]" << endl;
            cout << result.message << endl;
            continue;
        }

        cout << "[CLASSIFICATION]" << endl;
        cout << requestTypeToString(result.type) << endl;
    }

    cout << endl;
    cout << "========================================" << endl;

    return 0;
}