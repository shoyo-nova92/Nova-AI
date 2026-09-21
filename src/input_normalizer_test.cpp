#include "input_normalizer.hpp"

#include <iostream>
#include <vector>

using namespace std;

int main()
{
    InputNormalizer normalizer;

    vector<string> testInputs =
    {
        "uhh Nova, can you like create a folder called Nova on my desktop",
        "   UMM   open   Chrome   ",
        "you know, launch VS Code",
        "hello Nova",
        "     "
    };

    for (const string& input : testInputs)
    {
        cout << "========================================" << endl;
        cout << "Original:    [" << input << "]" << endl;

        NormalizedInput result =
            normalizer.normalize(input);

        cout << "Success:     "
             << (result.success ? "true" : "false")
             << endl;

        cout << "Normalized:  ["
             << result.normalizedText
             << "]"
             << endl;

        cout << "Message:     "
             << result.message
             << endl;
    }

    return 0;
}