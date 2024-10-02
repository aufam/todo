# TODO
A simple, modern, and feature-rich TODO application built with C++. 
This application demonstrates various advanced C++ features and third-party libraries integration, 
including HTTP handling, JSON serialization, dependency injection, JWT authentication, SQLite database integration, and more.

## Prerequisites
To build and run the TODO app, ensure that the following are installed on your system:
* C++17 minimum
* CMake (version 3.14 or higher)
* OpenSSL (for password hashing and JWT)
* SQLite3 (for database management)

## Features
The TODO app includes several powerful features:
* Dependency management using [`cpm-cmake`](https://github.com/cpm-cmake/CPM.cmake)
* [`delameta`](https://github.com/aufam/delameta) framework:
    * FastAPI-like decorator pattern using `delameta::http` for easy and readable http route handling
    * Lightweight JSON Serialization/Deserialization using `delameta::json`
    * Type safety for cleaner and safer code
    * Dependency Injection for better testability and maintainability
    * Result-variant pattern using `delameta::Result` for error handling
    * Advanced getopt handler using `delameta::Opts` for command-line argument parsing
* String formatting using [`fmt`](https://github.com/fmtlib/fmt)
* Type-safe SQLite Integration using [`sqlpp11`](https://github.com/rbock/sqlpp11)
* JWT Authentication and password hashing:
    * Secure token-based authentication using JSON Web Tokens using [`jwt-cpp`](https://github.com/Thalhammer/jwt-cpp)
    * Secure user password storage using OpenSSL
* Advanced Macro Preprocessing using [`Boost::preprocessor`](https://github.com/boostorg/preprocessor) for generating database and JSON structs
* Unit Testing using [`Catch2`](https://github.com/catchorg/Catch2)

## Database Model
You can view the database schema for this project [here](https://dbdiagram.io/d/Todo-66fa2a8af9b1444815d8a247)

## Build and Run
To build the project, follow these steps:
```bash
mkdir build
cmake -B build -DCMAKE_CXX_STANDARD=23
cmake --build build
```
Once built, you can run the TODO app:
```bash
./build/todo
```
By default, the app will host the server on `localhost:5000`.
You can specify a custom host using the `--host` option :
```bash
./build/todo --host=$CUSTOM_HOST
```
To see more command-line options, use the --help flag:
```bash
./build/todo --help
```

## Build and Run with Docker
```bash
docker build -t todo:gcc .
docker run -d --name todo -p 5000:5000 \
  -v .assets/:/root/todo/assets \
  -v .static/:/root/todo/static \
  -t todo:gcc
```
