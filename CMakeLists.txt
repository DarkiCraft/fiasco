cmake_minimum_required(VERSION 3.16)
project(fiasco VERSION 0.1.0 LANGUAGES CXX)

# -- C++17 --------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -- FetchContent Dependencies ------------------------------------------------
include(FetchContent)

# nlohmann/json
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
)

# Catch2 v3
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.5.2
        GIT_SHALLOW    TRUE
)

# llhttp (build static only)
FetchContent_Declare(
        llhttp
        URL https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.2.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Force llhttp to build a static library
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(json Catch2 llhttp)

# Restore default after llhttp
unset(BUILD_SHARED_LIBS CACHE)
unset(BUILD_STATIC_LIBS CACHE)

# -- fiasco (header-only INTERFACE library) -----------------------------------
add_library(fiasco INTERFACE)

target_include_directories(fiasco INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco INTERFACE
        nlohmann_json::nlohmann_json
        llhttp_static
)

# -- Subdirectories -----------------------------------------------------------
include(CTest)
enable_testing()
add_subdirectory(tests)

add_subdirectory(examples)

# -- Installation -------------------------------------------------------------
install(DIRECTORY include/ DESTINATION include)