#
# Copyright (c) 2026, NVIDIA CORPORATION. All rights reserved.
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

# Sets NVRHI_TARGET_ARCH to one of: arm64, x64, x86.
#
# CMAKE_CXX_COMPILER_ARCHITECTURE_ID is only populated for MSVC-style
# compilers on Windows (cl, clang-cl). GCC and Clang on Linux leave it empty,
# so CMAKE_SYSTEM_PROCESSOR (uname -m, or the toolchain file) is consulted too.

if (NVRHI_TARGET_ARCH)
    return()
endif()

if (CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "ARM64"
    OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|arm64|aarch64)$")
    set(NVRHI_TARGET_ARCH arm64)
elseif (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(NVRHI_TARGET_ARCH x64)
else()
    set(NVRHI_TARGET_ARCH x86)
endif()
