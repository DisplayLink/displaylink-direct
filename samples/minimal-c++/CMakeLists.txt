cmake_minimum_required(VERSION 3.15)
project(dlsdk-minimal-cxx-sample CXX)

find_package(dlsdk 2.0.0 REQUIRED)

add_executable(dlsdk-minimal-cxx-sample minimal-example.cpp)
target_link_libraries(dlsdk-minimal-cxx-sample PRIVATE dlsdk::dlsdk)
