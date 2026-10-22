cmake_minimum_required(VERSION 3.20)

project(Nova
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

add_executable(Nova
    src/main.cpp
    src/logger.cpp
    src/input_normalizer.cpp
    src/part_a_bridge.cpp
    src/keyboard_input.cpp
    src/input_coordinator.cpp
    src/request_classifier.cpp
    src/conversation_handler.cpp
    src/llm_client.cpp
    src/action_parser.cpp
    src/single_action_pipeline.cpp
    src/capability_resolver.cpp
    src/parameter_resolver.cpp
    src/primitive_registry.cpp
    src/action_validator.cpp
    src/execution_plan_builder.cpp
    src/execution_engine.cpp
    src/application_resolver.cpp
    src/action_primitive_resolver.cpp
    src/content_resolver.cpp
    src/multistep_pipeline.cpp
    src/goal_decomposer.cpp
    src/dependency_resolver.cpp
    src/context_resolver.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/plan_validator.cpp
    src/primitive_execution_engine.cpp
    src/multi_step_executor.cpp
    src/primitive_mapper.cpp
    src/branch3_runtime.cpp
    src/next_action_planner.cpp
    src/state_manager.cpp
)

target_link_libraries(Nova
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        shell32
        ole32
        psapi
        user32
        winhttp
)
add_executable(ExecutionEngineTest
    src/execution_engine_test.cpp
    src/execution_engine.cpp
    src/application_resolver.cpp
)

target_link_libraries(ExecutionEngineTest PRIVATE)

add_executable(ActionParserTest
    src/action_parser_test.cpp
    src/action_parser.cpp
)

add_executable(InputNormalizerTest
    src/input_normalizer_test.cpp
    src/input_normalizer.cpp
    src/llm_client.cpp
)

target_link_libraries(InputNormalizerTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(RequestClassifierTest

    src/request_classifier_test.cpp

    src/request_classifier.cpp

    src/llm_client.cpp

)

target_link_libraries(RequestClassifierTest

    PRIVATE

        CURL::libcurl

        nlohmann_json::nlohmann_json

)

add_executable(CapabilityResolverTest
    src/capability_resolver_test.cpp
    src/capability_resolver.cpp
)

add_executable(SingleActionPipelineTest
    src/single_action_pipeline_test.cpp
    src/single_action_pipeline.cpp
    src/action_parser.cpp
    src/capability_resolver.cpp
    src/parameter_resolver.cpp
    src/primitive_registry.cpp
    src/action_validator.cpp
    src/execution_plan_builder.cpp
    src/execution_engine.cpp
    src/application_resolver.cpp
    src/content_resolver.cpp
    src/llm_client.cpp
)
target_link_libraries(SingleActionPipelineTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)
add_executable(ApplicationResolverTest
    src/application_resolver_test.cpp
    src/application_resolver.cpp
)

add_executable(ParameterResolverTest
    src/parameter_resolver_test.cpp
    src/parameter_resolver.cpp
    src/primitive_registry.cpp
    src/content_resolver.cpp
    src/llm_client.cpp
)
target_link_libraries(ParameterResolverTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(ActionValidatorTest
    src/action_validator_test.cpp
    src/action_validator.cpp
)

add_executable(ExecutionPlanBuilderTest
    src/execution_plan_builder_test.cpp
    src/execution_plan_builder.cpp
)

add_executable(Branch2Benchmark
    src/branch2_benchmark.cpp
    src/single_action_pipeline.cpp
    src/action_parser.cpp
    src/capability_resolver.cpp
    src/parameter_resolver.cpp
    src/primitive_registry.cpp
    src/action_validator.cpp
    src/execution_plan_builder.cpp
    src/execution_engine.cpp
    src/application_resolver.cpp
    src/content_resolver.cpp
    src/llm_client.cpp
)
target_link_libraries(Branch2Benchmark
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(GoalDecomposerTest
    src/goal_decomposer_test.cpp
    src/goal_decomposer.cpp
    src/llm_client.cpp
    src/dependency_resolver.cpp
)

target_link_libraries(GoalDecomposerTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(DependencyResolverTest
    src/dependency_resolver_test.cpp
    src/dependency_resolver.cpp
)

add_executable(ActionPrimitiveResolverTest
    src/action_primitive_resolver_test.cpp
    src/action_primitive_resolver.cpp
    src/primitive_registry.cpp
    src/llm_client.cpp
)
target_link_libraries(ActionPrimitiveResolverTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(PrimitiveRegistryTest
    src/primitive_registry_test.cpp
    src/primitive_registry.cpp
)

add_executable(FilesystemPathPolicyTest
    src/filesystem_path_policy_test.cpp
    src/filesystem_path_policy.cpp
)
target_include_directories(FilesystemPathPolicyTest PRIVATE src)
target_link_libraries(FilesystemPathPolicyTest PRIVATE shell32 ole32)

add_executable(FilesystemLocationResolverTest
    src/filesystem_location_resolver_test.cpp
    src/filesystem_location_resolver.cpp
)
target_include_directories(FilesystemLocationResolverTest PRIVATE src)
target_link_libraries(FilesystemLocationResolverTest PRIVATE shell32 ole32)

add_executable(ContextResolverTest
    src/context_resolver_test.cpp
    src/context_resolver.cpp
    src/filesystem_path_policy.cpp
)
target_link_libraries(ContextResolverTest PRIVATE shell32 ole32)

add_executable(MultiStepPipelineTest
    src/multistep_pipeline_test.cpp
    src/multistep_pipeline.cpp
    src/goal_decomposer.cpp
    src/dependency_resolver.cpp
    src/action_primitive_resolver.cpp
    src/primitive_registry.cpp
    src/capability_resolver.cpp
    src/parameter_resolver.cpp
    src/context_resolver.cpp
    src/filesystem_path_policy.cpp
    src/llm_client.cpp
    src/content_resolver.cpp
    src/plan_validator.cpp
    src/execution_plan_builder.cpp
)

target_link_libraries(MultiStepPipelineTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        shell32 ole32
)

add_executable(
    PrimitiveExecutionEngineTest
    src/primitive_execution_engine_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)
target_link_libraries(PrimitiveExecutionEngineTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)

add_executable(
    MultiStepIntegrationTest
    src/multi_step_integration_test.cpp
    src/multi_step_executor.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp

    src/multistep_pipeline.cpp
    src/goal_decomposer.cpp
    src/dependency_resolver.cpp
    src/action_primitive_resolver.cpp
    src/primitive_registry.cpp
    src/capability_resolver.cpp
    src/parameter_resolver.cpp
    src/context_resolver.cpp
    src/plan_validator.cpp
    src/execution_plan_builder.cpp
    src/content_resolver.cpp
    src/llm_client.cpp
)
target_link_libraries(MultiStepIntegrationTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
        shell32 ole32 psapi user32 winhttp
)

add_executable(GoalDecomposerContractTest
    src/goal_decomposer_contract_test.cpp
    src/goal_decomposer.cpp
    src/llm_client.cpp
)

target_link_libraries(GoalDecomposerContractTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)
add_executable(PrimitiveMapperContractTest

    src/primitive_mapper_contract_test.cpp

    src/primitive_mapper.cpp

    src/primitive_registry.cpp

    src/llm_client.cpp

)

target_include_directories(PrimitiveMapperContractTest PRIVATE src)

target_link_libraries(PrimitiveMapperContractTest PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

add_executable(Branch3LivePlanningTest
    src/branch3_live_planning_test.cpp
    src/goal_decomposer.cpp
    src/primitive_mapper.cpp
    src/primitive_registry.cpp
    src/llm_client.cpp
)

target_include_directories(Branch3LivePlanningTest PRIVATE src)

target_link_libraries(Branch3LivePlanningTest
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(
    FilesystemCreationGroupTest
    src/filesystem_creation_group_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)

target_include_directories(FilesystemCreationGroupTest PRIVATE src)
target_link_libraries(FilesystemCreationGroupTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)

add_executable(
    FilesystemManipulationGroupTest
    src/filesystem_manipulation_group_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)
target_include_directories(FilesystemManipulationGroupTest PRIVATE src)
target_link_libraries(FilesystemManipulationGroupTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)

add_executable(
    FilesystemDiscoveryGroupTest
    src/filesystem_discovery_group_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)
target_include_directories(FilesystemDiscoveryGroupTest PRIVATE src)
target_link_libraries(FilesystemDiscoveryGroupTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)

add_executable(
    ApplicationLifecycleGroupTest
    src/application_lifecycle_group_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)
target_include_directories(ApplicationLifecycleGroupTest PRIVATE src)
target_link_libraries(ApplicationLifecycleGroupTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)

add_executable(
    TextKeyboardGroupTest
    src/text_keyboard_group_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)
target_include_directories(TextKeyboardGroupTest PRIVATE src)
target_link_libraries(TextKeyboardGroupTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)

add_executable(
    MouseGroupTest
    src/mouse_group_test.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
)
target_include_directories(MouseGroupTest PRIVATE src)
target_link_libraries(MouseGroupTest PRIVATE shell32 ole32 psapi user32 winhttp nlohmann_json::nlohmann_json)


add_executable(Branch3RuntimeIntegrationTest
    src/branch3_runtime_test.cpp
    src/branch3_runtime.cpp
    src/next_action_planner.cpp
    src/state_manager.cpp
    src/goal_decomposer.cpp
    src/primitive_mapper.cpp
    src/primitive_registry.cpp
    src/primitive_execution_engine.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/application_resolver.cpp
    src/llm_client.cpp
)
target_include_directories(Branch3RuntimeIntegrationTest PRIVATE src)
target_link_libraries(Branch3RuntimeIntegrationTest PRIVATE
    CURL::libcurl
    nlohmann_json::nlohmann_json
    shell32 ole32 psapi user32 winhttp
)
