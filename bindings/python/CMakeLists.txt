cmake_minimum_required(VERSION 3.20)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

FetchContent_Declare(
  pybind11
  GIT_REPOSITORY https://github.com/pybind/pybind11.git
  GIT_TAG        v2.13.6
  GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(pybind11)

pybind11_add_module(wsf_native module.cpp)
target_link_libraries(wsf_native PRIVATE wsf::core)
target_compile_definitions(wsf_native PRIVATE VERSION_INFO="${PROJECT_VERSION}")
if(MINGW)
  target_link_options(wsf_native PRIVATE -static-libgcc -static-libstdc++ -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic)
endif()

install(TARGETS wsf_native DESTINATION .)
