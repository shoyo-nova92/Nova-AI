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
)

target_link_libraries(Nova
    PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)