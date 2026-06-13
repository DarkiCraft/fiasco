cmake_minimum_required(VERSION 3.28)
project(fiasco LANGUAGES CXX)

# -- C++23 --------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_SCAN_FOR_MODULES ON)

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
# FetchContent_Declare(
#         Catch2
#         GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#         GIT_TAG        v3.5.2
#         GIT_SHALLOW    TRUE
# )

# llhttp (build static only)
FetchContent_Declare(
        llhttp
        URL https://github.com/nodejs/llhttp/archive/refs/tags/release/v9.2.1.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

# Force llhttp to build a static library
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)

# FetchContent_MakeAvailable(json Catch2 llhttp)
FetchContent_MakeAvailable(json llhttp)

# Restore default after llhttp
unset(BUILD_SHARED_LIBS CACHE)
unset(BUILD_STATIC_LIBS CACHE)

# asio library
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)

# -- fiasco (header-only INTERFACE library) -----------------------------------
add_library(fiasco INTERFACE)

target_include_directories(fiasco INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco INTERFACE
        nlohmann_json::nlohmann_json
        llhttp_static
        asio
)

# -- Subdirectories -----------------------------------------------------------
# include(CTest)
# enable_testing()
# add_subdirectory(tests)

add_subdirectory(examples)
add_subdirectory(modules)

# -- Installation -------------------------------------------------------------
install(DIRECTORY include/ DESTINATION include)
