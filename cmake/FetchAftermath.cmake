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

# ---------------------------------------------------------------------------
# Nsight Aftermath SDK version. To update: change the version(s) and MD5s below.
# Packages are listed at https://developer.nvidia.com/nsight-aftermath/getting-started
# Format is <major>.<minor>.<patch>.<build>; Windows and Linux are published separately.
# ---------------------------------------------------------------------------
set(AFTERMATH_SDK_VERSION_WINDOWS 2026.3.0.26197)
set(AFTERMATH_SDK_VERSION_LINUX   2026.3.1.26217)

set(AFTERMATH_SDK_MD5_windows_x64   0dfd9a5530b81ad8e065696f1d81bb10)
set(AFTERMATH_SDK_MD5_windows_arm64 5882ebff768fc4161da6ef694cbfb038)
set(AFTERMATH_SDK_MD5_linux_x64     44365a455fa7ced3430b3b5c1ccfe874)
set(AFTERMATH_SDK_MD5_linux_arm64   c88f8d604078155087aef6dc156b5938)

# Oldest SDK the source compiles against. GFSDK_Aftermath_DX12_UpdateResourceInfo
# was introduced and RegisterResource removed in 2026.3 (API version 0x21B).
set(AFTERMATH_MIN_API_VERSION 0x21B)
set(AFTERMATH_MIN_SDK_NAME "2026.3")
# ---------------------------------------------------------------------------

include("${CMAKE_CURRENT_LIST_DIR}/NvrhiTargetArch.cmake")
set(AFTERMATH_ARCH ${NVRHI_TARGET_ARCH})

if (NOT AFTERMATH_SEARCH_PATHS)
    set(AFTERMATH_FETCH_DIR "" CACHE STRING "Directory to fetch aftermath sdk to, empty string uses build directory default")

    include(FetchContent)
    if (NOT DEFINED AFTERMATH_FETCH_URL)
        if (WIN32)
            set(AFTERMATH_SDK_OS windows)
            set(AFTERMATH_SDK_EXT zip)
            set(AFTERMATH_SDK_FULL_VERSION ${AFTERMATH_SDK_VERSION_WINDOWS})
        else()
            set(AFTERMATH_SDK_OS linux)
            set(AFTERMATH_SDK_EXT tgz)
            set(AFTERMATH_SDK_FULL_VERSION ${AFTERMATH_SDK_VERSION_LINUX})
        endif()

        # Split <major>.<minor>.<patch>.<build> into the version and build parts the URL uses.
        if (NOT AFTERMATH_SDK_FULL_VERSION MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)\\.([0-9]+)$")
            message(FATAL_ERROR "AFTERMATH_SDK_VERSION_* must be <major>.<minor>.<patch>.<build>, got '${AFTERMATH_SDK_FULL_VERSION}'")
        endif()
        set(AFTERMATH_SDK_VERSION ${CMAKE_MATCH_1})
        set(AFTERMATH_SDK_BUILD ${CMAKE_MATCH_2})

        # <root>/<ver with underscores>/<os>_<arch>/nvidia_nsight_aftermath_sdk_<ver>.<build>-<os>_<arch>.<ext>
        # The server is case-insensitive on the file name.
        set(AFTERMATH_SDK_PACKAGE "${AFTERMATH_SDK_OS}_${AFTERMATH_ARCH}")
        string(REPLACE "." "_" AFTERMATH_SDK_VERSION_PATH "${AFTERMATH_SDK_VERSION}")
        set(AFTERMATH_FETCH_URL
            "https://developer.nvidia.com/downloads/assets/tools/secure/nsight-aftermath-sdk/${AFTERMATH_SDK_VERSION_PATH}/${AFTERMATH_SDK_PACKAGE}/nvidia_nsight_aftermath_sdk_${AFTERMATH_SDK_VERSION}.${AFTERMATH_SDK_BUILD}-${AFTERMATH_SDK_PACKAGE}.${AFTERMATH_SDK_EXT}")
        set(AFTERMATH_FETCH_MD5 "${AFTERMATH_SDK_MD5_${AFTERMATH_SDK_PACKAGE}}")
        if (NOT AFTERMATH_FETCH_MD5)
            message(FATAL_ERROR "No Aftermath SDK package is known for ${AFTERMATH_SDK_PACKAGE}")
        endif()
    endif()
    if (NOT DEFINED AFTERMATH_FETCH_MD5)
        message(FATAL_ERROR "AFTERMATH_FETCH_URL was overridden but AFTERMATH_FETCH_MD5 was not")
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
    REQUIRED
    PATH_SUFFIXES "include")

# Fail at configure time with a clear message instead of an undeclared-identifier
# compile error when a stale or user-supplied SDK predates the API we use.
file(STRINGS "${AFTERMATH_INCLUDE_DIR}/GFSDK_Aftermath_Defines.h" AFTERMATH_API_VERSION_LINE
    REGEX "GFSDK_Aftermath_Version_API[ \t]*=[ \t]*0x[0-9A-Fa-f]+")
string(REGEX MATCH "0x[0-9A-Fa-f]+" AFTERMATH_API_VERSION "${AFTERMATH_API_VERSION_LINE}")
if (NOT AFTERMATH_API_VERSION)
    message(FATAL_ERROR "Could not determine the Aftermath SDK API version from ${AFTERMATH_INCLUDE_DIR}/GFSDK_Aftermath_Defines.h")
endif()
if (AFTERMATH_API_VERSION LESS AFTERMATH_MIN_API_VERSION)
    message(FATAL_ERROR
        "Nsight Aftermath SDK at ${AFTERMATH_INCLUDE_DIR} has API version ${AFTERMATH_API_VERSION}; "
        "NVRHI requires ${AFTERMATH_MIN_SDK_NAME} or newer (API ${AFTERMATH_MIN_API_VERSION}). "
        "Update AFTERMATH_SEARCH_PATHS, or delete the AFTERMATH_* entries from CMakeCache.txt to re-fetch.")
endif()
message(STATUS "Aftermath SDK API version ${AFTERMATH_API_VERSION} (${AFTERMATH_ARCH})")

find_library(AFTERMATH_LIBRARY GFSDK_Aftermath_Lib.${AFTERMATH_ARCH}
    PATHS ${AFTERMATH_SEARCH_PATHS}
    REQUIRED
    PATH_SUFFIXES "lib/${AFTERMATH_ARCH}")

add_library(aftermath SHARED IMPORTED)
target_include_directories(aftermath INTERFACE ${AFTERMATH_INCLUDE_DIR})

if(WIN32)
    find_file(AFTERMATH_RUNTIME_LIBRARY GFSDK_Aftermath_Lib.${AFTERMATH_ARCH}.dll
        PATHS ${AFTERMATH_SEARCH_PATHS}
        PATH_SUFFIXES "lib/${AFTERMATH_ARCH}")
    set_property(TARGET aftermath PROPERTY IMPORTED_LOCATION ${AFTERMATH_RUNTIME_LIBRARY})
    set_property(TARGET aftermath PROPERTY IMPORTED_IMPLIB ${AFTERMATH_LIBRARY})
else()
    set_property(TARGET aftermath PROPERTY IMPORTED_LOCATION ${AFTERMATH_LIBRARY})
endif()
