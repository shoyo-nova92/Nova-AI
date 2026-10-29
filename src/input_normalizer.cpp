#include "input_normalizer.hpp"
#include "utf8_text.hpp"

NormalizedInput InputNormalizer::normalize(const string& input) {
    NormalizedInput result{};
    result.originalText = input;
    Utf8::require(input, "raw_user_input");
    const auto start = input.find_first_not_of(" \t\r\n");
    if (start == string::npos) { result.message = "Please enter a request."; return result; }
    result.normalizedText = input.substr(start, input.find_last_not_of(" \t\r\n") - start + 1);
    result.success = true;
    result.message = "Input ready; literal contents preserved.";
    return result;
}
