#include "action_parser.hpp"

#include <iostream>

using namespace std;

int main()
{
    ActionParser parser;

    cout << "============================================================"
         << endl;
    cout << "ACTION PARSER TEST"
         << endl;
    cout << "============================================================"
         << endl;

    // --------------------------------------------------------
    // TEST 1 — CREATE FOLDER
    // --------------------------------------------------------

    {
        string input =
            "create a folder called nova on my desktop";

        cout << endl;
        cout << "[TEST 1]"
             << endl;
        cout << "Input: "
             << input
             << endl;

        ActionParserResult result =
            parser.parse(input);

        cout << "Success: "
             << (result.success ? "true" : "false")
             << endl;

        cout << "Action: "
             << result.action.name
             << endl;

        cout << "Message: "
             << result.message
             << endl;
    }

    // --------------------------------------------------------
    // TEST 2 — OPEN APPLICATION
    // --------------------------------------------------------

    {
        string input =
            "open chrome";

        cout << endl;
        cout << "[TEST 2]"
             << endl;
        cout << "Input: "
             << input
             << endl;

        ActionParserResult result =
            parser.parse(input);

        cout << "Success: "
             << (result.success ? "true" : "false")
             << endl;

        cout << "Action: "
             << result.action.name
             << endl;

        cout << "Message: "
             << result.message
             << endl;
    }

    // --------------------------------------------------------
    // TEST 3 — CLOSE APPLICATION
    // --------------------------------------------------------

    {
        string input =
            "close spotify";

        cout << endl;
        cout << "[TEST 3]"
             << endl;
        cout << "Input: "
             << input
             << endl;

        ActionParserResult result =
            parser.parse(input);

        cout << "Success: "
             << (result.success ? "true" : "false")
             << endl;

        cout << "Action: "
             << result.action.name
             << endl;

        cout << "Message: "
             << result.message
             << endl;
    }

    // --------------------------------------------------------
    // TEST 4 — UNKNOWN ACTION
    // --------------------------------------------------------

    {
        string input =
            "do something completely unsupported";

        cout << endl;
        cout << "[TEST 4]"
             << endl;
        cout << "Input: "
             << input
             << endl;

        ActionParserResult result =
            parser.parse(input);

        cout << "Success: "
             << (result.success ? "true" : "false")
             << endl;

        cout << "Action: "
             << result.action.name
             << endl;

        cout << "Message: "
             << result.message
             << endl;
    }

    // --------------------------------------------------------
    // TEST 5 — EMPTY INPUT
    // --------------------------------------------------------

    {
        string input = "";

        cout << endl;
        cout << "[TEST 5]"
             << endl;
        cout << "Input: [empty]"
             << endl;

        ActionParserResult result =
            parser.parse(input);

        cout << "Success: "
             << (result.success ? "true" : "false")
             << endl;

        cout << "Action: "
             << result.action.name
             << endl;

        cout << "Message: "
             << result.message
             << endl;
    }

    return 0;
}