#include "input_normalizer.hpp"

#include <algorithm>
#include <cctype>

using namespace std;

NormalizedInput InputNormalizer::normalize(
    const string& input
)
{
    NormalizedInput result;

    result.originalText = input;

    if (input.empty())
    {
        result.success = false;
        result.normalizedText = "";
        result.message = "Input is empty.";

        return result;
    }

    string text = input;

    // --------------------------------------------------------
    // Convert to lowercase
    // --------------------------------------------------------

    transform(
        text.begin(),
        text.end(),
        text.begin(),
        [](unsigned char character)
        {
            return static_cast<char>(
                tolower(character)
            );
        }
    );

    // --------------------------------------------------------
    // Remove common conversational fillers
    // --------------------------------------------------------

    const string fillers[] =
    {
        "uhh ",
        "uh ",
        "umm ",
        "um ",
        "like "
    };

    for (const string& filler : fillers)
    {
        size_t position = 0;

        while (
            (position = text.find(filler, position))
            != string::npos
        )
        {
            text.erase(
                position,
                filler.length()
            );
        }
    }

    // --------------------------------------------------------
    // Collapse repeated whitespace
    // --------------------------------------------------------

    string cleaned;

    bool previousWasSpace = false;

    for (unsigned char character : text)
    {
        if (isspace(character))
        {
            if (!previousWasSpace)
            {
                cleaned += ' ';
                previousWasSpace = true;
            }
        }
        else
        {
            cleaned += static_cast<char>(
                character
            );

            previousWasSpace = false;
        }
    }

    // --------------------------------------------------------
    // Trim leading/trailing whitespace
    // --------------------------------------------------------

    if (!cleaned.empty() &&
        cleaned.front() == ' ')
    {
        cleaned.erase(
            cleaned.begin()
        );
    }

    if (!cleaned.empty() &&
        cleaned.back() == ' ')
    {
        cleaned.pop_back();
    }

    // --------------------------------------------------------
    // Remove conversational "you know" prefix
    // --------------------------------------------------------

    const string conversationalPrefixes[] =
    {
        "you know, ",
        "you know "
    };

    for (const string& prefix : conversationalPrefixes)
    {
        if (
            cleaned.rfind(prefix, 0) == 0
        )
        {
            cleaned.erase(
                0,
                prefix.length()
            );

            break;
        }
    }

    // --------------------------------------------------------
    // Remove assistant wake-name when used as an address
    // --------------------------------------------------------

    const string assistantPrefix = "nova";

    if (
        cleaned == assistantPrefix
        ||
        cleaned.rfind(
            assistantPrefix + ", ",
            0
        ) == 0
        ||
        cleaned.rfind(
            assistantPrefix + " ",
            0
        ) == 0
    )
    {
        if (cleaned == assistantPrefix)
        {
            cleaned = "";
        }
        else if (
            cleaned.rfind(
                assistantPrefix + ", ",
                0
            ) == 0
        )
        {
            cleaned.erase(
                0,
                assistantPrefix.length() + 2
            );
        }
        else
        {
            cleaned.erase(
                0,
                assistantPrefix.length() + 1
            );
        }
    }

    // --------------------------------------------------------
    // Trim again after prefix removal
    // --------------------------------------------------------

    if (!cleaned.empty() &&
        cleaned.front() == ' ')
    {
        cleaned.erase(
            cleaned.begin()
        );
    }

    if (!cleaned.empty() &&
        cleaned.back() == ' ')
    {
        cleaned.pop_back();
    }

    // --------------------------------------------------------
    // Validate normalized input
    // --------------------------------------------------------

    if (cleaned.empty())
    {
        result.success = false;
        result.normalizedText = "";
        result.message =
            "Normalized input is empty.";

        return result;
    }

    result.success = true;
    result.normalizedText = cleaned;
    result.message =
        "Input normalized successfully.";

    return result;
}