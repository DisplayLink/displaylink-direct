cmake_minimum_required(VERSION 3.15)
project(dlsdk-video-sample CXX)

find_package(dlsdk 2.0.0 REQUIRED)
find_package(Threads REQUIRED)

option(USE_GSTREAMER "Build video samples with GStreamer backend instead of VLC" OFF)

set(VIDEO_PLAYER_SOURCES common_vlc.cpp)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
if(USE_GSTREAMER)
	find_package(PkgConfig REQUIRED)
	pkg_check_modules(GSTREAMER REQUIRED IMPORTED_TARGET
		gstreamer-1.0
		gstreamer-video-1.0
		gstreamer-app-1.0
	)
	set(VIDEO_PLAYER_SOURCES common_gst.cpp)
else()
	find_package(LIBVLC 3.0.0 REQUIRED)
endif()

add_executable(video-wall video-wall.cpp ${VIDEO_PLAYER_SOURCES})
if(USE_GSTREAMER)
	target_compile_definitions(video-wall PRIVATE USE_GSTREAMER)
	target_link_libraries(video-wall PRIVATE PkgConfig::GSTREAMER Threads::Threads dlsdk::dlsdk)
else()
	target_include_directories(video-wall
	PUBLIC ${LIBVLC_INCLUDE_DIR}
	)
	target_link_libraries(video-wall PRIVATE ${LIBVLC_LIBRARY} dlsdk::dlsdk)
endif()

add_executable(video-clone video-clone.cpp ${VIDEO_PLAYER_SOURCES})
if(USE_GSTREAMER)
	target_compile_definitions(video-clone PRIVATE USE_GSTREAMER)
	target_link_libraries(video-clone PRIVATE PkgConfig::GSTREAMER Threads::Threads dlsdk::dlsdk)
else()
	target_include_directories(video-clone
	PUBLIC ${LIBVLC_INCLUDE_DIR}
	)
	target_link_libraries(video-clone PRIVATE ${LIBVLC_LIBRARY} dlsdk::dlsdk)
endif()
