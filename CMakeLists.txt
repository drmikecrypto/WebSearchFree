cmake_minimum_required(VERSION 3.20)
project(WebSearchFree VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(WSF_BUILD_CLI "Build wsf CLI" ON)
option(WSF_BUILD_SERVER "Build optional HTTP server into CLI" ON)
option(WSF_BUILD_TESTS "Build unit tests" ON)
option(WSF_BUILD_PYTHON "Build Python bindings" OFF)

include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.18.3
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(httplib)

add_library(wsf_core
  src/http_client.cpp
  src/html_utils.cpp
  src/extract.cpp
  src/rank.cpp
  src/search.cpp
  src/robots.cpp
  src/engines/ddg.cpp
  src/engines/brave.cpp
  src/engines/wikipedia.cpp
  src/engines/searx.cpp
  src/engines/factory.cpp
)

add_library(wsf::core ALIAS wsf_core)

target_include_directories(wsf_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(wsf_core
  PUBLIC nlohmann_json::nlohmann_json
)

if(WIN32)
  target_link_libraries(wsf_core PRIVATE winhttp)
  target_compile_definitions(wsf_core PRIVATE WSF_USE_WINHTTP=1)
else()
  find_package(CURL REQUIRED)
  target_link_libraries(wsf_core PRIVATE CURL::libcurl)
  target_compile_definitions(wsf_core PRIVATE WSF_USE_CURL=1)
endif()

if(MSVC)
  target_compile_options(wsf_core PRIVATE /W4 /permissive-)
else()
  target_compile_options(wsf_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WSF_BUILD_CLI)
  add_executable(wsf apps/wsf_cli/main.cpp)
  target_link_libraries(wsf PRIVATE wsf::core)
  if(WSF_BUILD_SERVER)
    target_compile_definitions(wsf PRIVATE WSF_HAS_SERVER=1)
    target_include_directories(wsf PRIVATE ${httplib_SOURCE_DIR})
  endif()
  if(WIN32)
    target_link_libraries(wsf PRIVATE ws2_32)
  endif()
endif()

if(WSF_BUILD_TESTS)
  enable_testing()
  add_executable(wsf_tests
    tests/test_ddg.cpp
    tests/test_html_utils.cpp
    tests/test_rank.cpp
    tests/test_extract.cpp
    tests/test_brave.cpp
    tests/test_searx.cpp
  )
  target_link_libraries(wsf_tests PRIVATE wsf::core)
  target_compile_definitions(wsf_tests PRIVATE
    WSF_FIXTURES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
  )
  add_test(NAME wsf_tests COMMAND wsf_tests)
endif()

if(WSF_BUILD_PYTHON)
  add_subdirectory(bindings/python)
endif()

install(TARGETS wsf_core
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(DIRECTORY include/wsf DESTINATION include)
if(WSF_BUILD_CLI)
  install(TARGETS wsf RUNTIME DESTINATION bin)
endif()
