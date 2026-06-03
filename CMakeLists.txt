cmake_minimum_required(VERSION 3.24)
# Enable both CXX and CUDA languages
project(CustomOFXPlugin VERSION 1.0.0 LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CUDA_ARCHITECTURES native)

# Global MSVC Static Runtime Override
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# OpenFX Configuration
set_property(GLOBAL PROPERTY OFX_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/openfx")
set(OFX_HEADER_DIR "${PROJECT_SOURCE_DIR}/openfx/include")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/openfx/cmake")
include(OpenFX)

# Define Target with both CPP and CU sources
add_library(CustomPlugin SHARED 
    src/CustomPlugin.cpp 
    src/CustomPlugin.cu
    src/CustomPlugin.def
)

if(MSVC)
    set_property(TARGET CustomPlugin PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

add_subdirectory(openfx/Support)
target_link_libraries(CustomPlugin PRIVATE OfxSupport)
target_include_directories(CustomPlugin PRIVATE ${OFX_HEADER_DIR})

set_target_properties(CustomPlugin PROPERTIES 
    PREFIX ""
    SUFFIX ".ofx"
)