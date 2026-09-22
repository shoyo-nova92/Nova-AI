#include "request_classifier.hpp"

#include <iostream>

using namespace std;

void runTest(
    RequestClassifier& classifier,
    const string& input
)
{
    ClassificationResult result =
        classifier.classify(input);

    cout << endl;
    cout << "Input: "
         << input
         << endl;

    cout << "Success: "
         << (result.success ? "true" : "false")
         << endl;

    cout << "Type: ";

    switch (result.type)
    {
        case RequestType::CONVERSATION:
            cout << "CONVERSATION";
            break;

        case RequestType::SINGLE_ACTION:
            cout << "SINGLE_ACTION";
            break;

        case RequestType::MULTI_STEP_GOAL:
            cout << "MULTI_STEP_GOAL";
            break;

        case RequestType::EXISTING_WORKFLOW:
            cout << "EXISTING_WORKFLOW";
            break;

        case RequestType::CLARIFICATION_REQUIRED:
            cout << "CLARIFICATION_REQUIRED";
            break;

        case RequestType::UNSUPPORTED:
            cout << "UNSUPPORTED";
            break;
    }

    cout << endl;

    cout << "Message: "
         << result.message
         << endl;
}

int main()
{
    RequestClassifier classifier;

    cout << "============================================================"
         << endl;

    cout << "ROOT ROUTER TEST"
         << endl;

    cout << "============================================================"
         << endl;

    // --------------------------------------------------------
    // CONVERSATION
    // --------------------------------------------------------

    runTest(
        classifier,
        "what is inheritance in java?"
    );

    runTest(
        classifier,
        "explain dynamic method dispatch"
    );

    // --------------------------------------------------------
    // SINGLE ACTION
    // --------------------------------------------------------

    runTest(
        classifier,
        "open chrome"
    );

    runTest(
        classifier,
        "create a folder called nova on my desktop"
    );

    runTest(
        classifier,
        "close spotify"
    );

    // --------------------------------------------------------
    // EMPTY INPUT
    // --------------------------------------------------------

    runTest(
        classifier,
        ""
    );

    return 0;
}