#pragma once
#include "branch3_runtime.hpp"
#include "narration.hpp"
#include <filesystem>

// Adaptive coordinator; the existing registry/mapper remains the execution boundary.
class AdaptiveRuntime {
public:
    using Executor=std::function<ExecutionResult(const PrimitiveOperation&)>;
    using TemporaryExecutor=std::function<nlohmann::json(const nlohmann::json&,const std::filesystem::path&)>;
    AdaptiveRuntime(GoalDecomposerLLM decomposer,NextActionLLM planner,Executor executor,
                    TemporaryExecutor temporaryExecutor={});
    ~AdaptiveRuntime();
    Branch3RuntimeResult start(const std::string& goal);
    Branch3RuntimeResult resume();
    Branch3RuntimeResult answer(const std::string& questionId,const std::string& text);
    Branch3RuntimeResult guidance();
    RuntimeControls controls() const;
    std::optional<PendingUserDecision> pending() const;
    bool active() const;
    void abandon();
    void setEventSink(std::function<void(const std::string&)> sink);
    void setNarrationSink(NarrationSink sink);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
