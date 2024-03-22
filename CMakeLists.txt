cmake_minimum_required(VERSION 3.8)

project(
    adk
    VERSION 0.1
    LANGUAGES CXX
)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(adk INTERFACE)

target_include_directories(adk INTERFACE include)
target_compile_features(adk INTERFACE cxx_std_20)
set_target_properties(adk PROPERTIES CXX_EXTENSIONS OFF)
