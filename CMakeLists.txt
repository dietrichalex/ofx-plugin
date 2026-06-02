cmake_minimum_required(VERSION 3.16)
project(CustomPlugin VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Configure OpenFX properties
set_property(GLOBAL PROPERTY OFX_PROJECT_SOURCE_DIR "${PROJECT_SOURCE_DIR}/openfx")
set(OFX_HEADER_DIR "${PROJECT_SOURCE_DIR}/openfx/include")
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/openfx/cmake")

# Include ASWF macros
include(OpenFX)

# Define the plugin target
add_library(CustomPlugin SHARED src/CustomPlugin.cpp)

# Link the OpenFX C++ Support library
add_subdirectory(openfx/Support)
target_link_libraries(CustomPlugin PRIVATE OfxSupport)
target_include_directories(CustomPlugin PRIVATE ${OFX_HEADER_DIR})

# Rename output to .ofx (or .ofx.bundle on Mac)
set_target_properties(CustomPlugin PROPERTIES 
    PREFIX ""
    SUFFIX ".ofx"
)

if(APPLE)
    set_target_properties(CustomPlugin PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION "ofx.bundle")
endif()