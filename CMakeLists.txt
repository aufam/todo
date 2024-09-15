cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(todo C CXX ASM)

# executable
file(GLOB_RECURSE SOURCES src/*)
add_executable(todo ${SOURCES})

# external libraries
include(cmake/CPM.cmake)
CPMAddPackage("gh:aufam/delameta#main")
CPMAddPackage("gh:boostorg/preprocessor#boost-1.86.0")
CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage("gh:Thalhammer/jwt-cpp@0.7.0")

CPMAddPackage(
    GITHUB_REPOSITORY rbock/sqlpp11
    GIT_TAG           0.64 
    OPTIONS           "BUILD_SQLITE3_CONNECTOR ON"
)

CPMAddPackage("gh:catchorg/Catch2@3.7.0")

target_link_libraries(todo PRIVATE
    delameta
    Boost::preprocessor
    fmt
    jwt-cpp
    sqlpp11
    Catch2
)

# features
target_compile_features(todo PRIVATE 
    cxx_std_23
)

# warnings
target_compile_options(todo PRIVATE
    -Wall 
    -Wextra 
    -pedantic
)
