cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project(todo
    VERSION "0.1.0"
    DESCRIPTION "Simple todo web app in c++"
)

# executable
file(GLOB_RECURSE SOURCES src/*)
add_executable(todo ${SOURCES})

# sqlite3
find_package(SQLite3 REQUIRED)
target_link_libraries(todo PRIVATE sqlite3)

# external libraries
include(cmake/CPM.cmake)

CPMAddPackage("gh:aufam/delameta#main")
CPMAddPackage("gh:boostorg/preprocessor#boost-1.86.0")
CPMAddPackage("gh:fmtlib/fmt#11.0.2")
CPMAddPackage("gh:Thalhammer/jwt-cpp@0.6.0")
CPMAddPackage("gh:HowardHinnant/date@3.0.1")
CPMAddPackage("gh:rbock/sqlpp11#0.64")
CPMAddPackage("gh:catchorg/Catch2@3.7.0")

target_link_libraries(todo PRIVATE
    delameta
    Boost::preprocessor
    fmt
    jwt-cpp
    sqlpp11
    Catch2
)

# link options
target_link_options(todo PRIVATE
    -static-libstdc++
)

# warnings
target_compile_options(todo PRIVATE
    -Wall 
    -Wextra 
    -pedantic
    -Wno-attributes
)

# home dir
set(HOME_DIR "${CMAKE_HOME_DIRECTORY}" CACHE STRING "Must contain `/assets` and `/static` directories")
target_compile_definitions(todo PRIVATE 
    HOME_DIR="${HOME_DIR}"
)

# install
install(
    TARGETS todo
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(
    DIRECTORY assets/
    DESTINATION /usr/share/todo/assets
)

install(
    DIRECTORY static/
    DESTINATION /usr/share/todo/static
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE
    DESTINATION /usr/share/todo
)

install(
    FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md
    DESTINATION /usr/share/todo
)

# cpack
set(CPACK_PACKAGE_NAME todo)
set(CPACK_PACKAGE_VERSION ${todo_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${todo_DESCRIPTION})

set(CPACK_PACKAGE_VENDOR "Delameta Bilano")
set(CPACK_PACKAGE_CONTACT "aufam@github.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Aufa")

set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
include(CPack)

