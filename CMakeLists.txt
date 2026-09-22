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
    src/action_validator.cpp
    src/execution_plan_builder.cpp
    
)

target_link_libraries(Nova
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

add_executable(ActionParserTest
    src/action_parser_test.cpp
    src/action_parser.cpp
)

add_executable(RequestClassifierTest
    src/request_classifier_test.cpp
    src/request_classifier.cpp
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
    src/action_validator.cpp
    src/execution_plan_builder.cpp
)

add_executable(ParameterResolverTest
    src/parameter_resolver_test.cpp
    src/parameter_resolver.cpp
)

add_executable(ActionValidatorTest
    src/action_validator_test.cpp
    src/action_validator.cpp
)

add_executable(ExecutionPlanBuilderTest
    src/execution_plan_builder_test.cpp
    src/execution_plan_builder.cpp
)