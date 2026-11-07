#pragma once
#include <functional>
#include <string>
enum class NarrationKind { PROGRESS, QUESTION, REPLY };
using NarrationSink=std::function<void(const std::string&,NarrationKind)>;
