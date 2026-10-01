cmake_minimum_required(VERSION 3.15)
project(dlsdk-minimal-c-sample C)

find_package(dlsdk 2.0.0 REQUIRED)

add_executable(dlsdk-minimal-c-sample minimal-example.c)
target_link_libraries(dlsdk-minimal-c-sample PRIVATE dlsdk::dlsdk)
