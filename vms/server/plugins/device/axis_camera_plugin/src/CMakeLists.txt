## Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

cmake_minimum_required(VERSION 3.14)

project(axis_camera_plugin)

set(CMAKE_AUTOMOC ON)

if(DEFINED ENV{QT_DIR})
    list(APPEND CMAKE_PREFIX_PATH $ENV{QT_DIR})
endif()

if(NOT QT_HOST_PATH AND DEFINED ENV{QT_HOST_PATH})
    set(QT_HOST_PATH $ENV{QT_HOST_PATH})
endif()

file(GLOB_RECURSE source_files CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*)
add_library(axis_camera_plugin SHARED ${source_files})

if(WIN32)
    set(API_IMPORT_MACRO "__declspec(dllimport)")
    set(API_EXPORT_MACRO "__declspec(dllexport)")
else()
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(API_IMPORT_MACRO "")
    set(API_EXPORT_MACRO "__attribute__((visibility(\"default\")))")
endif()

target_compile_definitions(axis_camera_plugin
    PRIVATE NX_PLUGIN_API=${API_EXPORT_MACRO}
    INTERFACE NX_PLUGIN_API=${API_IMPORT_MACRO}
)

find_package(Qt6Core)
find_package(Qt6Network)

if (NOT Qt6Core_FOUND OR NOT Qt6Network_FOUND)
    message(FATAL_ERROR
        "Some of the Qt packages are not found. If you are building this plugin as a sample from "
        "the SDK package, make sure that you set the environment variables QT_DIR and "
        "QT_HOST_PATH (see readme.md for more information)."
    )
endif()

target_link_libraries(axis_camera_plugin PRIVATE
    Qt6::Core
    Qt6::Network
)

target_include_directories(axis_camera_plugin PRIVATE ../../../../src) #< SDK headers.

# Use all CPU cores by MSVC.
if(MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /MP")
    # Do not create separate .pdb files for object files (workaround for mspdbsrv.exe bug, needed
    # for using "/MP" flag).
    target_compile_options(axis_camera_plugin BEFORE PRIVATE /Z7)
endif()

if(WIN32)
    target_compile_definitions(axis_camera_plugin PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

set_target_properties(axis_camera_plugin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
