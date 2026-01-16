#
# Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

if( TARGET aftermath )
    return()
endif()

if (NOT AFTERMATH_SEARCH_PATHS)
    set(AFTERMATH_FETCH_DIR "" CACHE STRING "Directory to fetch aftermath sdk to, empty string uses build directory default")

    include(FetchContent)
    if(WIN32)
        set(AFTERMATH_FETCH_URL https://developer.nvidia.com/downloads/assets/tools/secure/nsight-aftermath-sdk/2025_1_0/windows/NVIDIA_Nsight_Aftermath_SDK_2025.1.0.25009.zip
            CACHE STRING "Url to Aftermath SDK archive (.zip)")
        set(AFTERMATH_FETCH_MD5 84101cad47eeb792c3b100e38d7ab453 CACHE STRING "MD5 Hash of Aftermath SDK archive")
    else()
        set(AFTERMATH_FETCH_URL https://developer.nvidia.com/downloads/assets/tools/secure/nsight-aftermath-sdk/2025_1_0/linux/NVIDIA_Nsight_Aftermath_SDK_2025.1.0.25009.tgz
            CACHE STRING "Url to Aftermath SDK archive (.tgz)")
        set(AFTERMATH_FETCH_MD5 ec6253c807da34e55052574cb5db8726 CACHE STRING "MD5 Hash of Aftermath SDK archive")
    endif()

    FetchContent_Declare(
        aftermath
        URL ${AFTERMATH_FETCH_URL}
        URL_HASH MD5=${AFTERMATH_FETCH_MD5}
        SOURCE_DIR ${AFTERMATH_FETCH_DIR}
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
   FetchContent_MakeAvailable(aftermath)
   
   message(STATUS "Updating aftermath from ${AFTERMATH_FETCH_URL} , md5 ${AFTERMATH_FETCH_MD5}, into folder ${aftermath_SOURCE_DIR}")
   set(AFTERMATH_SEARCH_PATHS "${aftermath_SOURCE_DIR}")
endif()

find_path(AFTERMATH_INCLUDE_DIR GFSDK_Aftermath.h
    PATHS ${AFTERMATH_SEARCH_PATHS}
    PATH_SUFFIXES "include")

find_library(AFTERMATH_LIBRARY GFSDK_Aftermath_Lib.x64
    PATHS ${AFTERMATH_SEARCH_PATHS}
    REQUIRED
    PATH_SUFFIXES "lib/x64")

add_library(aftermath SHARED IMPORTED)
target_include_directories(aftermath INTERFACE ${AFTERMATH_INCLUDE_DIR})

if(WIN32)
    find_file(AFTERMATH_RUNTIME_LIBRARY GFSDK_Aftermath_Lib.x64.dll
        PATHS ${AFTERMATH_SEARCH_PATHS}
        PATH_SUFFIXES "lib/x64")
    set_property(TARGET aftermath PROPERTY IMPORTED_LOCATION ${AFTERMATH_RUNTIME_LIBRARY})
    set_property(TARGET aftermath PROPERTY IMPORTED_IMPLIB ${AFTERMATH_LIBRARY})
else()
    set_property(TARGET aftermath PROPERTY IMPORTED_LOCATION ${AFTERMATH_LIBRARY})
endif()
