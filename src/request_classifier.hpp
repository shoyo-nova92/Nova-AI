#pragma once

#include "request_classification.hpp"

#include <string>

using namespace std;

class RequestClassifier
{
public:
    ClassificationResult classify(
        const string& input
    );
};