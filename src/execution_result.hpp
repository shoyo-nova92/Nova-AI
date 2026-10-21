#pragma once

#include <string>
#include <vector>

using namespace std;

enum class ExecutionStatus
{
    SUCCESS,
    FAILED,
    NOT_EXECUTED
};

enum class ExecutionFailureReason
{
    NONE,
    INVALID_PLAN,
    LOW_CONFIDENCE,
    UNSUPPORTED_ACTION,
    TARGET_NOT_FOUND,
    EXECUTION_ERROR
};

struct ExecutionResult
{
    ExecutionStatus status;
    ExecutionFailureReason reason;

    string action;
    string message;
    // Stable machine-readable category retained alongside the human message.
    string failureCategory;

    double confidence;

    bool targetResolved;
    bool executionMechanismResolved;

    // File operations populate the normalized, policy-checked physical target.
    string resolvedPath;
    vector<string> discoveredPaths;
};
