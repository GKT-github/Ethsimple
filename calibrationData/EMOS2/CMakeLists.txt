cmake_minimum_required(VERSION 3.10)
project(CameraCapture)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find GStreamer
find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GLIB REQUIRED glib-2.0)

# Add executable
add_executable(camera_capture camera_capture.cpp)

# Include directories
target_include_directories(camera_capture PRIVATE 
    ${GSTREAMER_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(camera_capture 
    ${GSTREAMER_LIBRARIES}
    ${GLIB_LIBRARIES}
    pthread
)

# Add compiler flags
target_compile_options(camera_capture PRIVATE 
    ${GSTREAMER_CFLAGS_OTHER}
    ${GLIB_CFLAGS_OTHER}
)
