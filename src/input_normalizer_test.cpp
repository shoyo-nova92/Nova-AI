#include "input_normalizer.hpp"

#include <iostream>
#include <vector>
#include <string>

using namespace std;

int main()
{
    InputNormalizer normalizer;

    vector<string> testInputs =
    {
        "uhh Nova, can you like create a folder called Nova on my desktop",
        "Nova, open Chrome",
        "please close Spotify",
        "create a folder called Test on my desktop",
        "What is inheritance in Java?"
    };

    cout << "========================================" << endl;
    cout << "LLM INPUT NORMALIZER TEST" << endl;
    cout << "========================================" << endl;

    for (const string& input : testInputs)
    {
        cout << endl;
        cout << "[RAW]" << endl;
        cout << input << endl;

        NormalizedInput result =
            normalizer.normalize(input);

        if (!result.success)
        {
            cout << "[FAILURE]" << endl;
            cout << result.message << endl;
            continue;
        }

        cout << "[NORMALIZED]" << endl;
        cout << result.normalizedText << endl;
    }

    cout << endl;
    cout << "========================================" << endl;

    return 0;
}