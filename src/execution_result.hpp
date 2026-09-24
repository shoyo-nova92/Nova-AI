#pragma once

#include <string>

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

    double confidence;

    bool targetResolved;
    bool executionMechanismResolved;
};