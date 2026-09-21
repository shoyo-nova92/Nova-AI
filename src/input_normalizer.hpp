#pragma once

#include <string>

using namespace std;

struct NormalizedInput
{
    bool success;
    string originalText;
    string normalizedText;
    string message;
};

class InputNormalizer
{
public:
    NormalizedInput normalize(
        const string& input
    );
};