cmake_minimum_required(VERSION 3.10)
project(SurroundViewSimple CUDA CXX)

# Set CMake policies to suppress warnings
if(POLICY CMP0072)
    cmake_policy(SET CMP0072 NEW)  # Prefer GLVND over legacy OpenGL
endif()

if(POLICY CMP0104)
    cmake_policy(SET CMP0104 NEW)  # CUDA architectures required
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O3")
set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -O3")

# Set CUDA architecture for Jetson (adjust based on your device)
# Jetson Nano: 53, Jetson TX2: 62, Jetson Xavier: 72, Jetson Orin: 87
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
    set(CMAKE_CUDA_ARCHITECTURES 87)  # Change to 53 for Jetson Nano
endif()

set(PROJ_INCLUDE_DIRS)
set(PROJ_LIBRARIES)

# 3rd party base directory
set(3DP_INCLUDE_DIR "${PROJECT_SOURCE_DIR}/3dparty/")

# ============ Find Packages ============
# Prefer GLVND (modern OpenGL) over legacy
set(OpenGL_GL_PREFERENCE GLVND)

find_package(CUDA REQUIRED)
find_package(OpenCV REQUIRED)
find_package(OpenGL REQUIRED)
find_package(PkgConfig REQUIRED)

# ============ GLM (OpenGL Mathematics) ============
find_package(glm QUIET)
if(glm_FOUND)
    message(STATUS "✓ Using system GLM")
    list(APPEND PROJ_INCLUDE_DIRS "/usr/include/glm")
else()
    message(STATUS "⚠ System GLM not found, trying 3dparty")
    set(GLM_INCLUDE_DIR "${3DP_INCLUDE_DIR}/glm")
    if(EXISTS ${GLM_INCLUDE_DIR})
        message(STATUS "✓ Using 3dparty GLM")
        list(APPEND PROJ_INCLUDE_DIRS ${GLM_INCLUDE_DIR})
    else()
        message(FATAL_ERROR "GLM not found! Install: sudo apt install libglm-dev")
    endif()
endif()

# ============ GLFW (Window/Input Library) ============
find_package(glfw3 QUIET)
if(glfw3_FOUND)
    message(STATUS "✓ Using system GLFW")
    list(APPEND PROJ_LIBRARIES glfw)
else()
    message(STATUS "⚠ System GLFW not found, trying 3dparty")
    set(GLFW_LIB "${3DP_INCLUDE_DIR}/lib/libglfw.so.3.1")
    if(EXISTS ${GLFW_LIB})
        message(STATUS "✓ Using 3dparty GLFW: ${GLFW_LIB}")
        list(APPEND PROJ_LIBRARIES ${GLFW_LIB})
    else()
        message(FATAL_ERROR "GLFW not found! Install: sudo apt install libglfw3-dev")
    endif()
endif()

# ============ EGL/GLES (OpenGL ES) ============
find_library(EGL_LIB EGL)
find_library(GLES_LIB2 GLESv2)

if(EGL_LIB AND GLES_LIB2)
    message(STATUS "✓ Using system EGL/GLES")
    list(APPEND PROJ_LIBRARIES ${EGL_LIB} ${GLES_LIB2})
else()
    message(STATUS "⚠ System EGL/GLES not found, trying 3dparty")
    set(EGL_3DP "${3DP_INCLUDE_DIR}/lib/libEGL.so")
    set(GLES_3DP "${3DP_INCLUDE_DIR}/lib/libGLESv2.so")
    
    if(EXISTS ${EGL_3DP} AND EXISTS ${GLES_3DP})
        message(STATUS "✓ Using 3dparty EGL/GLES")
        list(APPEND PROJ_LIBRARIES ${EGL_3DP} ${GLES_3DP})
    else()
        message(FATAL_ERROR "EGL/GLES not found! Install: sudo apt install libgles2-mesa-dev libegl1-mesa-dev")
    endif()
endif()

# ============ GStreamer ============
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)

list(APPEND PROJ_INCLUDE_DIRS ${GSTREAMER_INCLUDE_DIRS})
list(APPEND PROJ_INCLUDE_DIRS ${GSTREAMER_APP_INCLUDE_DIRS})
list(APPEND PROJ_LIBRARIES ${GSTREAMER_LIBRARIES})
list(APPEND PROJ_LIBRARIES ${GSTREAMER_APP_LIBRARIES})

# ============ ASSIMP (3D Model Loader) ============
find_package(assimp QUIET)
if(NOT assimp_FOUND)
    find_package(ASSIMP QUIET)
endif()

if(assimp_FOUND OR ASSIMP_FOUND)
    message(STATUS "✓ ASSIMP found")
    if(assimp_FOUND)
        list(APPEND PROJ_LIBRARIES assimp::assimp)
    else()
        list(APPEND PROJ_LIBRARIES ${ASSIMP_LIBRARIES})
        list(APPEND PROJ_INCLUDE_DIRS ${ASSIMP_INCLUDE_DIRS})
    endif()
else()
    message(FATAL_ERROR "ASSIMP not found! Install: sudo apt install libassimp-dev")
endif()

message(STATUS "✓ OpenCV ${OpenCV_VERSION} found")
message(STATUS "✓ CUDA ${CUDA_VERSION} found")
message(STATUS "✓ OpenGL found")

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CUDA_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
    ${PROJ_INCLUDE_DIRS}
)

# Source files - SIMPLIFIED (removed unused files)
set(SOURCES
    src/main.cpp
    src/SVAppSimple.cpp
    src/SVStitcherSimple.cpp
    src/SVRenderSimple.cpp
    src/SVEthernetCamera.cpp
    src/SVBlender.cpp
    src/SVGainCompensator.cpp
    src/Bowl.cpp
    src/OGLShader.cpp
    src/Model.cpp
    src/Mesh.cpp
)

# CUDA kernel sources
set(CUDA_SOURCES
    cusrc/kernelblend.cu
    cusrc/kernelgain.cu
)

# Compile CUDA kernels
cuda_add_library(cuda_kernels ${CUDA_SOURCES})

# Main executable
add_executable(SurroundViewSimple ${SOURCES})

# Link libraries
target_link_libraries(SurroundViewSimple
    cuda_kernels
    ${OpenCV_LIBS}
    ${CUDA_LIBRARIES}
    ${CUDA_CUDA_LIBRARY}
    ${OPENGL_LIBRARIES}
    ${PROJ_LIBRARIES}
    pthread
    dl
)

# Installation
install(TARGETS SurroundViewSimple DESTINATION bin)
install(DIRECTORY shaders DESTINATION share/surroundview)
install(DIRECTORY models DESTINATION share/surroundview)

message(STATUS "===================================")
message(STATUS "Surround View Simple - Build Config")
message(STATUS "===================================")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "CUDA version: ${CUDA_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "===================================")
