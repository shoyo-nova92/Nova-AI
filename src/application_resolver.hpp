#pragma once

#include <string>

using namespace std;

struct ApplicationResolverResult
{
    bool success;

    string application;

    string appId;

    string displayName;

    string message;
};

class ApplicationResolver
{
public:
    ApplicationResolverResult resolve(
        const string& application
    );
};