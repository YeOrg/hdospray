# Copyright 2019 Intel Corporation
# SPDX-License-Identifier: Apache-2.0

PROJECT(hdOSPRayPlugin)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (WIN32)
    cmake_minimum_required(VERSION 3.1.1)
else()
    cmake_minimum_required(VERSION 2.8.8)
endif()

if (NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    if (WIN32)
        message(FATAL_ERROR "Compiler does not support 64-bit builds. "
                "If you are using Visual Studio, make sure you are in the "
                "x64 command prompt and have specified the Win64 cmake "
                "generator (e.g., -G \"Visual Studio 14 2015 Win64\")")
    else()
        message(FATAL_ERROR "Compiler does not support 64-bit builds.")
    endif()
endif()

set(CMAKE_MODULE_PATH  ${CMAKE_SOURCE_DIR}/cmake/modules)

option(USE_HOUDINI_USD "use Houdini USD" OFF)
                      
if(USE_HOUDINI_USD)
	FIND_PACKAGE(Houdini CONFIG REQUIRED)
else()
	FIND_PACKAGE(pxr CONFIG REQUIRED)
endif()

add_subdirectory(hdOSPRay)

option(ENABLE_TESTING "build testing suite" OFF)

if (ENABLE_TESTING)
    add_subdirectory(testing)
endif()
