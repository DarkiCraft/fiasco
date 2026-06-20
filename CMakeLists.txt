cmake_minimum_required(VERSION 3.28)
project(fiasco LANGUAGES CXX)

option(FIASCO_BUILD_MODULES "Build C++ module targets" OFF)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(FIASCO_BUILD_MODULES)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

# == DEPENDENCIES =============================================================

include(FetchContent)

# -- nlohmann/json ----------------------------------------
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)

# Catch2 v3
# FetchContent_Declare(
#         Catch2
#         GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#         GIT_TAG        v3.5.2
#         GIT_SHALLOW    TRUE
# )

# -- llhttp -----------------------------------------------
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

# -- asio -------------------------------------------------
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-38-0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE ${asio_SOURCE_DIR}/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE)
if (WIN32)
  target_link_libraries(asio INTERFACE ws2_32 mswsock)
  target_compile_definitions(asio INTERFACE _WIN32_WINNT=0x0A00)
endif()

# == FIASCO ===================================================================

# -- fiasco_types -----------------------------------------
add_library(fiasco_types STATIC
    src/fiasco/internal/types/json.cpp
)

target_include_directories(fiasco_types PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_types PRIVATE
    nlohmann_json::nlohmann_json
)

# -- fiasco_core ------------------------------------------
add_library(fiasco_core STATIC
    src/fiasco/internal/core/tcp_server.cpp
)

target_include_directories(fiasco_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_core PUBLIC
    asio
)

# -- fiasco_http ------------------------------------------
add_library(fiasco_http STATIC
    src/fiasco/internal/http/parser.cpp
    src/fiasco/internal/http/request.cpp
    src/fiasco/internal/http/response.cpp
)

target_include_directories(fiasco_http PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_http PUBLIC
    fiasco_types
    llhttp_static
)

# -- fiasco_routing ------------------------------------------
add_library(fiasco_routing STATIC
    src/fiasco/internal/routing/route_table.cpp
)

target_include_directories(fiasco_routing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco_routing PUBLIC
    fiasco_types
    fiasco_http
)

# -- fiasco -----------------------------------------------
add_library(fiasco INTERFACE)

target_include_directories(fiasco INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fiasco INTERFACE
    fiasco_types
    fiasco_core
    fiasco_http
    fiasco_routing
)

# == SUBDIRECTORUES ===========================================================
# include(CTest)
# enable_testing()
# add_subdirectory(tests)

add_subdirectory(examples)

if(FIASCO_BUILD_MODULES)
  add_subdirectory(modules)
endif()

# == INSTALLATION =============================================================
install(DIRECTORY include/ DESTINATION include)
