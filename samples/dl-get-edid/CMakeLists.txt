cmake_minimum_required(VERSION 3.15)
project(dl-get-edid CXX)

find_package(dlsdk 2.0.0 REQUIRED)

add_executable(dl-get-edid dl-get-edid.cpp)
target_link_libraries(dl-get-edid PRIVATE dlsdk::dlsdk)
