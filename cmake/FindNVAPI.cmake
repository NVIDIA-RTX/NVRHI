#
# Copyright (c) 2021, NVIDIA CORPORATION. All rights reserved.
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


find_package(PackageHandleStandardArgs)

if (WIN32)

    if (NOT NVAPI_SEARCH_PATHS)
        set (NVAPI_SEARCH_PATHS
            "${CMAKE_SOURCE_DIR}/nvapi"
            "${CMAKE_PROJECT_DIR}/nvapi")
    endif()

    include("${CMAKE_CURRENT_LIST_DIR}/NvrhiTargetArch.cmake")

    # NVAPI SDK layout: <arch folder>/<library>.lib
    if (NVRHI_TARGET_ARCH STREQUAL "arm64")
        set(NVAPI_LIBRARY_NAME nvapia64)   # R615 and later
        set(NVAPI_LIBRARY_DIR aarch64)
    elseif (NVRHI_TARGET_ARCH STREQUAL "x64")
        set(NVAPI_LIBRARY_NAME nvapi64)
        set(NVAPI_LIBRARY_DIR amd64)
    else()
        set(NVAPI_LIBRARY_NAME nvapi)
        set(NVAPI_LIBRARY_DIR x86)
    endif()

    find_library(NVAPI_LIBRARY ${NVAPI_LIBRARY_NAME}
        PATHS ${NVAPI_SEARCH_PATHS}
        PATH_SUFFIXES ${NVAPI_LIBRARY_DIR})

    find_path(NVAPI_INCLUDE_DIR nvapi.h 
        PATHS ${NVAPI_SEARCH_PATHS})
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(NVAPI
    REQUIRED_VARS
        NVAPI_INCLUDE_DIR
        NVAPI_LIBRARY
)

