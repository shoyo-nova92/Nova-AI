cmake_minimum_required(VERSION 3.20)

project(Nova
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT WIN32)
    message(FATAL_ERROR "NOVA currently supports Windows only.")
endif()

find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)
add_library(NovaSpeech STATIC src/speech_output.cpp)
target_include_directories(NovaSpeech PUBLIC src)
target_link_libraries(NovaSpeech PUBLIC CURL::libcurl nlohmann_json::nlohmann_json winmm sapi ole32 uuid bcrypt)

add_executable(Nova
    src/main.cpp
    src/env_loader.cpp
    src/runtime_supervisor.cpp
    src/logger.cpp
    src/input_normalizer.cpp
    src/part_a_bridge.cpp
    src/keyboard_input.cpp
    src/input_coordinator.cpp
    src/request_classifier.cpp
    src/conversation_handler.cpp
    src/llm_client.cpp
    src/primitive_registry.cpp
    src/application_resolver.cpp
    src/goal_decomposer.cpp
    src/filesystem_path_policy.cpp
    src/filesystem_location_resolver.cpp
    src/primitive_execution_engine.cpp
    src/primitive_mapper.cpp
    src/branch3_runtime.cpp
    src/next_action_planner.cpp
    src/state_manager.cpp
)

target_link_libraries(Nova
    PRIVATE
        NovaSpeech
        CURL::libcurl
        nlohmann_json::nlohmann_json
        shell32
        ole32
        psapi
        user32
        winhttp
)

add_library(WorkflowKernel STATIC src/workflow_kernel.cpp src/semantic_evidence.cpp)
target_include_directories(WorkflowKernel PUBLIC src)
target_link_libraries(WorkflowKernel PUBLIC nlohmann_json::nlohmann_json)

add_library(AdaptiveCoordinator STATIC src/adaptive_runtime.cpp)
target_include_directories(AdaptiveCoordinator PUBLIC src)
target_link_libraries(AdaptiveCoordinator PUBLIC WorkflowKernel CURL::libcurl nlohmann_json::nlohmann_json)

target_link_libraries(Nova PRIVATE
    AdaptiveCoordinator
    WorkflowKernel
    uuid
    oleaut32
    shlwapi
)
