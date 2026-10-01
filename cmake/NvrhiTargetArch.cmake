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

# Sets NVRHI_TARGET_ARCH to one of: arm64, x64, x86, and NVRHI_HOST_ARCH likewise.
#

macro(_nvrhi_arch out id processor)
    if (NOT "${id}" STREQUAL "")
        set(_nvrhi_arch_value "${id}")
    else()
        set(_nvrhi_arch_value "${processor}")
    endif()

    if ("${_nvrhi_arch_value}" MATCHES "^(ARM64|arm64|aarch64)$")
        set(${out} arm64)
    elseif ("${_nvrhi_arch_value}" MATCHES "^(x64|X64|x86_64|AMD64|amd64)$")
        set(${out} x64)
    elseif ("${_nvrhi_arch_value}" MATCHES "^(X86|x86|i[3-6]86)$")
        set(${out} x86)
    else()
        message(FATAL_ERROR
            "NVRHI: cannot determine ${out} from compiler id '${id}' / processor '${processor}'. "
            "Set ${out} to arm64, x64, or x86.")
    endif()
    unset(_nvrhi_arch_value)
endmacro()

if (NOT NVRHI_TARGET_ARCH)
    _nvrhi_arch(NVRHI_TARGET_ARCH "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" "${CMAKE_SYSTEM_PROCESSOR}")
endif()
if (NOT NVRHI_HOST_ARCH)
    if (CMAKE_HOST_SYSTEM_PROCESSOR)
        _nvrhi_arch(NVRHI_HOST_ARCH "" "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    else()
        set(NVRHI_HOST_ARCH ${NVRHI_TARGET_ARCH})
    endif()
endif()
