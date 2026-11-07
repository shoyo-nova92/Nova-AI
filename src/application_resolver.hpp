#pragma once

#include <string>
#include <vector>
#include <utility>

using namespace std;

struct ApplicationResolverResult
{
    bool success;

    string application;

    string appId;

    string displayName;

    string message;
    bool acceptsCommandLine = false;
    vector<pair<string,string>> candidates; // Observed display name and AppID.
};

class ApplicationResolver
{
public:
    ApplicationResolverResult resolve(
        const string& application
    );
};
