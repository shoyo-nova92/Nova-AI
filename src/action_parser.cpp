#include "action_parser.hpp"

using namespace std;

ActionParserResult ActionParser::parse(
    const string& input
)
{
    ActionParserResult result;

    if (input.empty())
    {
        result.success = false;
        result.action.name = "";
        result.message =
            "Cannot parse an empty action request.";

        return result;
    }

    // --------------------------------------------------------
    // ACTION — CREATE FOLDER
    // --------------------------------------------------------

    if (
        input.find("create a folder") != string::npos ||
        input.find("make a folder") != string::npos
    )
    {
        result.success = true;

        result.action.name =
            "create_folder";

        result.message =
            "Action parsed as create_folder.";

        return result;
    }

    // --------------------------------------------------------
    // ACTION — OPEN APPLICATION
    // --------------------------------------------------------

    if (
        input.find("open ") == 0 ||
        input.find("launch ") == 0
    )
    {
        result.success = true;

        result.action.name =
            "open_app";

        result.message =
            "Action parsed as open_app.";

        return result;
    }

    // --------------------------------------------------------
    // ACTION — CLOSE APPLICATION
    // --------------------------------------------------------

    if (
        input.find("close ") == 0
    )
    {
        result.success = true;

        result.action.name =
            "close_app";

        result.message =
            "Action parsed as close_app.";

        return result;
    }

    // --------------------------------------------------------
    // UNKNOWN ACTION
    // --------------------------------------------------------

    result.success = false;

    result.action.name = "";

    result.message =
        "Unable to determine requested action.";

    return result;
}