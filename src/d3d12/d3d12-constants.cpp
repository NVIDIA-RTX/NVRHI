/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#include "d3d12-backend.h"

namespace nvrhi::d3d12
{
    DXGI_FORMAT convertFormat(nvrhi::Format format)
    {
        return getDxgiFormatMapping(format).srvFormat;
    }
    
    D3D12_SHADER_VISIBILITY convertShaderStage(ShaderType s)
    {
        switch (s)  // NOLINT(clang-diagnostic-switch-enum)
        {
        case ShaderType::Vertex:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case ShaderType::Hull:
            return D3D12_SHADER_VISIBILITY_HULL;
        case ShaderType::Domain:
            return D3D12_SHADER_VISIBILITY_DOMAIN;
        case ShaderType::Geometry:
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case ShaderType::Pixel:
            return D3D12_SHADER_VISIBILITY_PIXEL;
        case ShaderType::Amplification:
            return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
        case ShaderType::Mesh:
            return D3D12_SHADER_VISIBILITY_MESH;

        default:
            // catch-all case - actually some of the bitfield combinations are unrepresentable in DX12
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    D3D12_BLEND convertBlendValue(BlendFactor value)
    {
        switch (value)
        {
        case BlendFactor::Zero:
            return D3D12_BLEND_ZERO;
        case BlendFactor::One:
            return D3D12_BLEND_ONE;
        case BlendFactor::SrcColor:
            return D3D12_BLEND_SRC_COLOR;
        case BlendFactor::InvSrcColor:
            return D3D12_BLEND_INV_SRC_COLOR;
        case BlendFactor::SrcAlpha:
            return D3D12_BLEND_SRC_ALPHA;
        case BlendFactor::InvSrcAlpha:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:
            return D3D12_BLEND_DEST_ALPHA;
        case BlendFactor::InvDstAlpha:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendFactor::DstColor:
            return D3D12_BLEND_DEST_COLOR;
        case BlendFactor::InvDstColor:
            return D3D12_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlphaSaturate:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendFactor::ConstantColor:
            return D3D12_BLEND_BLEND_FACTOR;
        case BlendFactor::InvConstantColor:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case BlendFactor::Src1Color:
            return D3D12_BLEND_SRC1_COLOR;
        case BlendFactor::InvSrc1Color:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case BlendFactor::Src1Alpha:
            return D3D12_BLEND_SRC1_ALPHA;
        case BlendFactor::InvSrc1Alpha:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            utils::InvalidEnum();
            return D3D12_BLEND_ZERO;
        }
    }

    D3D12_BLEND_OP convertBlendOp(BlendOp value)
    {
        switch (value)
        {
        case BlendOp::Add:
            return D3D12_BLEND_OP_ADD;
        case BlendOp::Subtract:
            return D3D12_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:
            return D3D12_BLEND_OP_MIN;
        case BlendOp::Max:
            return D3D12_BLEND_OP_MAX;
        default:
            utils::InvalidEnum();
            return D3D12_BLEND_OP_ADD;
        }
    }

    D3D12_STENCIL_OP convertStencilOp(StencilOp value)
    {
        switch (value)
        {
        case StencilOp::Keep:
            return D3D12_STENCIL_OP_KEEP;
        case StencilOp::Zero:
            return D3D12_STENCIL_OP_ZERO;
        case StencilOp::Replace:
            return D3D12_STENCIL_OP_REPLACE;
        case StencilOp::IncrementAndClamp:
            return D3D12_STENCIL_OP_INCR_SAT;
        case StencilOp::DecrementAndClamp:
            return D3D12_STENCIL_OP_DECR_SAT;
        case StencilOp::Invert:
            return D3D12_STENCIL_OP_INVERT;
        case StencilOp::IncrementAndWrap:
            return D3D12_STENCIL_OP_INCR;
        case StencilOp::DecrementAndWrap:
            return D3D12_STENCIL_OP_DECR;
        default:
            utils::InvalidEnum();
            return D3D12_STENCIL_OP_KEEP;
        }
    }

    D3D12_COMPARISON_FUNC convertComparisonFunc(ComparisonFunc value)
    {
        switch (value)
        {
        case ComparisonFunc::Never:
            return D3D12_COMPARISON_FUNC_NEVER;
        case ComparisonFunc::Less:
            return D3D12_COMPARISON_FUNC_LESS;
        case ComparisonFunc::Equal:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case ComparisonFunc::LessOrEqual:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case ComparisonFunc::Greater:
            return D3D12_COMPARISON_FUNC_GREATER;
        case ComparisonFunc::NotEqual:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case ComparisonFunc::GreaterOrEqual:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case ComparisonFunc::Always:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            utils::InvalidEnum();
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }
    D3D_PRIMITIVE_TOPOLOGY convertPrimitiveType(PrimitiveType pt, uint32_t controlPoints)
    {
        switch (pt)
        {
        case PrimitiveType::PointList:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
        case PrimitiveType::LineList:
            return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::LineStrip:
            return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveType::TriangleList:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        case PrimitiveType::TriangleStrip:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::TriangleFan:
            utils::NotSupported();
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        case PrimitiveType::TriangleListWithAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ;
        case PrimitiveType::TriangleStripWithAdjacency:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ;
        case PrimitiveType::PatchList:
            if (controlPoints == 0 || controlPoints > 32)
            {
                utils::InvalidEnum();
                return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            }
            return D3D_PRIMITIVE_TOPOLOGY(D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST + (controlPoints - 1));
        default:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        }
    }

    D3D12_TEXTURE_ADDRESS_MODE convertSamplerAddressMode(SamplerAddressMode mode)
    {
        switch (mode)
        {
        case SamplerAddressMode::Clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case SamplerAddressMode::Wrap:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case SamplerAddressMode::Border:
            return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        case SamplerAddressMode::Mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case SamplerAddressMode::MirrorOnce:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR_ONCE;
        default:
            utils::InvalidEnum();
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        }
    }
    
    UINT convertSamplerReductionType(SamplerReductionType reductionType)
    {
        switch (reductionType)
        {
        case SamplerReductionType::Standard:
            return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
        case SamplerReductionType::Comparison:
            return D3D12_FILTER_REDUCTION_TYPE_COMPARISON;
        case SamplerReductionType::Minimum:
            return D3D12_FILTER_REDUCTION_TYPE_MINIMUM;
        case SamplerReductionType::Maximum:
            return D3D12_FILTER_REDUCTION_TYPE_MAXIMUM;
        default:
            utils::InvalidEnum();
            return D3D12_FILTER_REDUCTION_TYPE_STANDARD;
        }
    }

    D3D12_RESOURCE_STATES convertResourceStates(ResourceStates stateBits)
    {
        if (stateBits == ResourceStates::Common)
            return D3D12_RESOURCE_STATE_COMMON;

        D3D12_RESOURCE_STATES result = D3D12_RESOURCE_STATE_COMMON; // also 0

        if ((stateBits & ResourceStates::ConstantBuffer) != 0) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if ((stateBits & ResourceStates::VertexBuffer) != 0) result |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
        if ((stateBits & ResourceStates::IndexBuffer) != 0) result |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
        if ((stateBits & ResourceStates::IndirectArgument) != 0) result |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
        if ((stateBits & ResourceStates::PixelShaderResource) != 0) result |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        if ((stateBits & ResourceStates::NonPixelShaderResource) != 0) result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        if ((stateBits & ResourceStates::UnorderedAccess) != 0) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        if ((stateBits & ResourceStates::RenderTarget) != 0) result |= D3D12_RESOURCE_STATE_RENDER_TARGET;
        if ((stateBits & ResourceStates::DepthWrite) != 0) result |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
        if ((stateBits & ResourceStates::DepthRead) != 0) result |= D3D12_RESOURCE_STATE_DEPTH_READ;
        if ((stateBits & ResourceStates::StreamOut) != 0) result |= D3D12_RESOURCE_STATE_STREAM_OUT;
        if ((stateBits & ResourceStates::CopyDest) != 0) result |= D3D12_RESOURCE_STATE_COPY_DEST;
        if ((stateBits & ResourceStates::CopySource) != 0) result |= D3D12_RESOURCE_STATE_COPY_SOURCE;
        if ((stateBits & ResourceStates::ResolveDest) != 0) result |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
        if ((stateBits & ResourceStates::ResolveSource) != 0) result |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
        if ((stateBits & ResourceStates::Present) != 0) result |= D3D12_RESOURCE_STATE_PRESENT;
        if ((stateBits & ResourceStates::AccelStructRead) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        if ((stateBits & ResourceStates::AccelStructWrite) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        if ((stateBits & ResourceStates::AccelStructBuildInput) != 0) result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        if ((stateBits & ResourceStates::AccelStructBuildBlas) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        if ((stateBits & ResourceStates::ShadingRateSurface) != 0) result |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
        if ((stateBits & ResourceStates::OpacityMicromapBuildInput) != 0) result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        if ((stateBits & ResourceStates::OpacityMicromapWrite) != 0) result |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        if ((stateBits & ResourceStates::ConvertCoopVecMatrixInput) != 0) result |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        if ((stateBits & ResourceStates::ConvertCoopVecMatrixOutput) != 0) result |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

        return result;
    }

    static const EnhancedResourceStateMapping g_ResourceStateMap[] =
    {
        { ResourceStates::Common,
            D3D12_BARRIER_SYNC_ALL,
            D3D12_BARRIER_ACCESS_COMMON,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::ConstantBuffer,
            D3D12_BARRIER_SYNC_ALL_SHADING,
            D3D12_BARRIER_ACCESS_CONSTANT_BUFFER,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::VertexBuffer,
            D3D12_BARRIER_SYNC_ALL_SHADING,
            D3D12_BARRIER_ACCESS_VERTEX_BUFFER,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::IndexBuffer,
            D3D12_BARRIER_SYNC_INDEX_INPUT,
            D3D12_BARRIER_ACCESS_INDEX_BUFFER,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::IndirectArgument,
            D3D12_BARRIER_SYNC_EXECUTE_INDIRECT,
            D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::PixelShaderResource,
            D3D12_BARRIER_SYNC_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE },
        { ResourceStates::NonPixelShaderResource,
            D3D12_BARRIER_SYNC_NON_PIXEL_SHADING,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE },
        { ResourceStates::UnorderedAccess,
            D3D12_BARRIER_SYNC_ALL_SHADING,
            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
            D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS },
        { ResourceStates::RenderTarget,
            D3D12_BARRIER_SYNC_RENDER_TARGET,
            D3D12_BARRIER_ACCESS_RENDER_TARGET,
            D3D12_BARRIER_LAYOUT_RENDER_TARGET },
        { ResourceStates::DepthWrite,
            D3D12_BARRIER_SYNC_DEPTH_STENCIL,
            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE,
            D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE },
        { ResourceStates::DepthRead,
            D3D12_BARRIER_SYNC_DEPTH_STENCIL,
            D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ,
            D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ },
        { ResourceStates::StreamOut,
            D3D12_BARRIER_SYNC_VERTEX_SHADING,
            D3D12_BARRIER_ACCESS_STREAM_OUTPUT,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::CopyDest,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_COPY_DEST,
            D3D12_BARRIER_LAYOUT_COPY_DEST },
        { ResourceStates::CopySource,
            D3D12_BARRIER_SYNC_COPY,
            D3D12_BARRIER_ACCESS_COPY_SOURCE,
            D3D12_BARRIER_LAYOUT_COPY_SOURCE },
        { ResourceStates::ResolveDest,
            D3D12_BARRIER_SYNC_RESOLVE,
            D3D12_BARRIER_ACCESS_RESOLVE_DEST,
            D3D12_BARRIER_LAYOUT_RESOLVE_DEST },
        { ResourceStates::ResolveSource,
            D3D12_BARRIER_SYNC_RESOLVE,
            D3D12_BARRIER_ACCESS_RESOLVE_SOURCE,
            D3D12_BARRIER_LAYOUT_RESOLVE_SOURCE },
        { ResourceStates::Present,
            D3D12_BARRIER_SYNC_ALL,
            D3D12_BARRIER_ACCESS_COPY_SOURCE,
            D3D12_BARRIER_LAYOUT_PRESENT },
        { ResourceStates::AccelStructRead,
            D3D12_BARRIER_SYNC_ALL_SHADING, // Could be an RT pipeline or other shaders
            D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::AccelStructWrite,
            D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ | D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::AccelStructBuildInput,
            D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::AccelStructBuildBlas,
            D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::ShadingRateSurface,
            D3D12_BARRIER_SYNC_ALL_SHADING,
            D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE,
            D3D12_BARRIER_LAYOUT_SHADING_RATE_SOURCE },
        { ResourceStates::OpacityMicromapWrite,
            D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE,
            D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE,
            D3D12_BARRIER_LAYOUT_COMMON },
        { ResourceStates::OpacityMicromapBuildInput,
            D3D12_BARRIER_SYNC_ALL_SHADING,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE },
        { ResourceStates::ConvertCoopVecMatrixInput,
            D3D12_BARRIER_SYNC_CONVERT_LINEAR_ALGEBRA_MATRIX,
            D3D12_BARRIER_ACCESS_SHADER_RESOURCE,
            D3D12_BARRIER_LAYOUT_SHADER_RESOURCE },
        { ResourceStates::ConvertCoopVecMatrixOutput,
            D3D12_BARRIER_SYNC_CONVERT_LINEAR_ALGEBRA_MATRIX,
            D3D12_BARRIER_ACCESS_UNORDERED_ACCESS,
            D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS },
    };

    // Per the D3D12 Enhanced Barriers spec, each access bit is only legal under a subset of sync scopes -
    // ACCESS_INDEX_BUFFER for instance requires SYNC_ALL, SYNC_INDEX_INPUT or SYNC_DRAW. Returns the sync
    // bits D3D12 accepts alongside a single access bit.
    //
    // SYNC_CONVERT_LINEAR_ALGEBRA_MATRIX postdates that spec table and appears nowhere in it. It is listed
    // below for the access bits the ConvertCoopVecMatrix* entries in g_ResourceStateMap already pair it
    // with, which the debug layer accepts today.
    static D3D12_BARRIER_SYNC getCompatibleSyncBits(D3D12_BARRIER_ACCESS accessBit)
    {
        const D3D12_BARRIER_SYNC shading = D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_PIXEL_SHADING |
            D3D12_BARRIER_SYNC_NON_PIXEL_SHADING | D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_ALL_SHADING |
            D3D12_BARRIER_SYNC_DRAW;

        switch (accessBit)
        {
        case D3D12_BARRIER_ACCESS_VERTEX_BUFFER:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_ALL_SHADING;
        case D3D12_BARRIER_ACCESS_CONSTANT_BUFFER:
            return D3D12_BARRIER_SYNC_ALL | shading;
        case D3D12_BARRIER_ACCESS_INDEX_BUFFER:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_INDEX_INPUT | D3D12_BARRIER_SYNC_DRAW;
        case D3D12_BARRIER_ACCESS_RENDER_TARGET:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_RENDER_TARGET;
        case D3D12_BARRIER_ACCESS_UNORDERED_ACCESS:
            return D3D12_BARRIER_SYNC_ALL | shading | D3D12_BARRIER_SYNC_CLEAR_UNORDERED_ACCESS_VIEW |
                D3D12_BARRIER_SYNC_RAYTRACING | D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO |
                D3D12_BARRIER_SYNC_CONVERT_LINEAR_ALGEBRA_MATRIX;
        case D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE:
        case D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_DEPTH_STENCIL;
        case D3D12_BARRIER_ACCESS_SHADER_RESOURCE:
            // The spec's SHADER_RESOURCE row does not list SYNC_RAYTRACING; raytracing reads go through
            // SYNC_ALL_SHADING or SYNC_ALL. Kept narrow deliberately(see the note above areSyncAndAccessCompatible).
            return D3D12_BARRIER_SYNC_ALL | shading | D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE |
                D3D12_BARRIER_SYNC_CONVERT_LINEAR_ALGEBRA_MATRIX;
        case D3D12_BARRIER_ACCESS_STREAM_OUTPUT:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_VERTEX_SHADING | D3D12_BARRIER_SYNC_DRAW | D3D12_BARRIER_SYNC_ALL_SHADING;
        case D3D12_BARRIER_ACCESS_INDIRECT_ARGUMENT:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_EXECUTE_INDIRECT;
        case D3D12_BARRIER_ACCESS_COPY_DEST:
        case D3D12_BARRIER_ACCESS_COPY_SOURCE:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_COPY;
        case D3D12_BARRIER_ACCESS_RESOLVE_DEST:
        case D3D12_BARRIER_ACCESS_RESOLVE_SOURCE:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_RESOLVE;
        case D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_ALL_SHADING |
                D3D12_BARRIER_SYNC_RAYTRACING | D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE |
                D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE |
                D3D12_BARRIER_SYNC_EMIT_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO;
        case D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_COMPUTE_SHADING | D3D12_BARRIER_SYNC_ALL_SHADING |
                D3D12_BARRIER_SYNC_RAYTRACING | D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE |
                D3D12_BARRIER_SYNC_COPY_RAYTRACING_ACCELERATION_STRUCTURE;
        case D3D12_BARRIER_ACCESS_SHADING_RATE_SOURCE:
            return D3D12_BARRIER_SYNC_ALL | D3D12_BARRIER_SYNC_PIXEL_SHADING | D3D12_BARRIER_SYNC_ALL_SHADING;
        default:
            // Unreachable for the access bits g_ResourceStateMap produces - each one has a case above. Note
            // this switch is keyed on access bits, not sync bits, so a new sync scope never lands here: it
            // reaches this function only through the access bit it is paired with, and has to be added to
            // that bit's row. Permissive so that adding a new access bit cannot silently widen unrelated
            // barriers to SYNC_ALL; validateResourceStateMap below catches the omission instead.
            return D3D12_BARRIER_SYNC(~0u);
        }
    }

    // Erring narrow here: an omitted sync bit costs at most a spurious widening to
    // SYNC_ALL, whereas listing one D3D12 rejects lets an illegal barrier through unnoticed.
    static bool areSyncAndAccessCompatible(D3D12_BARRIER_SYNC sync, D3D12_BARRIER_ACCESS access)
    {
        // ACCESS_COMMON is zero and places no constraint on the sync scope.
        uint32_t accessBits = uint32_t(access);

        while (accessBits != 0)
        {
            uint32_t const accessBit = accessBits & (0u - accessBits);
            accessBits &= ~accessBit;

            if ((uint32_t(sync) & ~uint32_t(getCompatibleSyncBits(D3D12_BARRIER_ACCESS(accessBit)))) != 0)
                return false;
        }

        return true;
    }

#ifndef NDEBUG
    // Verifies that every g_ResourceStateMap entry pairs its sync scope with an access type D3D12 accepts,
    // i.e. that the table and getCompatibleSyncBits agree. Catches a transcription error in either one at
    // startup, rather than as an INCOMPATIBLE_BARRIER_VALUES failure from the debug layer on whichever
    // barrier happens to use the state first.
    static bool validateResourceStateMap()
    {
        bool valid = true;

        for (const EnhancedResourceStateMapping& mapping : g_ResourceStateMap)
        {
            if (!areSyncAndAccessCompatible(mapping.sync, mapping.access))
            {
                assert(!"g_ResourceStateMap entry pairs a sync scope with an incompatible access type");
                valid = false;
            }
        }

        return valid;
    }

    [[maybe_unused]] static const bool g_ResourceStateMapValidated = validateResourceStateMap();
#endif

    EnhancedResourceStateMapping convertResourceStatesForEnhancedBarriers(ResourceStates state, bool isTexture)
    {
        EnhancedResourceStateMapping result = {};

        constexpr uint32_t numStateBits = sizeof(g_ResourceStateMap) / sizeof(g_ResourceStateMap[0]);

        uint32_t stateTmp = uint32_t(state);
        uint32_t bitIndex = 0;
        uint32_t numStatesSet = 0;

        while (stateTmp != 0 && bitIndex < numStateBits)
        {
            uint32_t bit = (1 << bitIndex);

            if (stateTmp & bit)
            {
                const EnhancedResourceStateMapping& mapping = g_ResourceStateMap[bitIndex];

                assert(uint32_t(mapping.nvrhiState) == bit);

                result.nvrhiState = ResourceStates(result.nvrhiState | mapping.nvrhiState);
                result.access |= mapping.access;
                result.sync |= mapping.sync;
                ++numStatesSet;
                if (isTexture)
                {
                    if (result.layout == D3D12_BARRIER_LAYOUT_COMMON)
                    {
                        result.layout = mapping.layout;
                    }
                    else
                    {
                        assert(result.layout == mapping.layout);
                    }
                }

                stateTmp &= ~bit;
            }

            bitIndex++;
        }

        assert(result.nvrhiState == state);

        // Combining states ORs the sync and access bits of several entries independently, which can produce a
        // pair that D3D12 rejects. Donut's Scene::CreateMeshBuffers for example asks for
        // IndexBuffer|ShaderResource|AccelStructBuildInput on one buffer, yielding ACCESS_INDEX_BUFFER under
        // SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE (INCOMPATIBLE_BARRIER_VALUES). SYNC_ALL is legal
        // alongside every access bit, so widen to it rather than emitting an illegal barrier. Single states are
        // left alone: the mappings above are authored to be valid on their own, including for sync scopes the
        // compatibility table does not cover.
        if (numStatesSet > 1 && !areSyncAndAccessCompatible(result.sync, result.access))
            result.sync = D3D12_BARRIER_SYNC_ALL;

        return result;
    }

    D3D12_SHADING_RATE convertPixelShadingRate(VariableShadingRate shadingRate)
    {
        switch (shadingRate)
        {
        case VariableShadingRate::e1x2:
            return D3D12_SHADING_RATE_1X2;
        case VariableShadingRate::e2x1:
            return D3D12_SHADING_RATE_2X1;
        case VariableShadingRate::e2x2:
            return D3D12_SHADING_RATE_2X2;
        case VariableShadingRate::e2x4:
            return D3D12_SHADING_RATE_2X4;
        case VariableShadingRate::e4x2:
            return D3D12_SHADING_RATE_4X2;
        case VariableShadingRate::e4x4:
            return D3D12_SHADING_RATE_4X4;
        case VariableShadingRate::e1x1:
        default:
            return D3D12_SHADING_RATE_1X1;
        }
    }

    D3D12_SHADING_RATE_COMBINER convertShadingRateCombiner(ShadingRateCombiner combiner)
    {
        switch (combiner)
        {
        case ShadingRateCombiner::Override:
            return D3D12_SHADING_RATE_COMBINER_OVERRIDE;
        case ShadingRateCombiner::Min:
            return D3D12_SHADING_RATE_COMBINER_MIN;
        case ShadingRateCombiner::Max:
            return D3D12_SHADING_RATE_COMBINER_MAX;
        case ShadingRateCombiner::ApplyRelative:
            return D3D12_SHADING_RATE_COMBINER_SUM;
        case ShadingRateCombiner::Passthrough:
        default:
            return D3D12_SHADING_RATE_COMBINER_PASSTHROUGH;
        }
    }

#if NVRHI_D3D12_WITH_COOP_VECTOR_COMMON
    D3D12_LINEAR_ALGEBRA_DATATYPE convertCoopVecDataType(coopvec::DataType type)
    {
        switch (type)
        {
        case coopvec::DataType::UInt8:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8;
        case coopvec::DataType::SInt8:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8;
        case coopvec::DataType::UInt8Packed:
#if NVRHI_D3D12_WITH_LINALG
            // Not support in 720
            utils::InvalidEnum();
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
#else
            return D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED;
#endif
        case coopvec::DataType::SInt8Packed:
#if NVRHI_D3D12_WITH_LINALG
            // Not support in 720
            utils::InvalidEnum();
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
#else
            return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED;
#endif
        case coopvec::DataType::UInt16:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16;
        case coopvec::DataType::SInt16:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16;
        case coopvec::DataType::UInt32:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32;
        case coopvec::DataType::SInt32:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32;
        case coopvec::DataType::FloatE4M3:
#if NVRHI_D3D12_WITH_LINALG
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E4M3FN;
#else
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3;
#endif
        case coopvec::DataType::FloatE5M2:
#if NVRHI_D3D12_WITH_LINALG
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E5M2;
#else
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2;
#endif
        case coopvec::DataType::Float16:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16;
        case coopvec::DataType::Float32:
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
        default:
            utils::InvalidEnum();
            return D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32;
        }
    }

    coopvec::DataType convertCoopVecDataType(D3D12_LINEAR_ALGEBRA_DATATYPE type)
    {
        switch (type)
        {
        case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8:
            return coopvec::DataType::UInt8;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8:
            return coopvec::DataType::SInt8;
#if !NVRHI_D3D12_WITH_LINALG
        case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT8_T4_PACKED:
            return coopvec::DataType::UInt8Packed;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT8_T4_PACKED:
            return coopvec::DataType::SInt8Packed;
#endif
        case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT16:
            return coopvec::DataType::UInt16;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT16:
            return coopvec::DataType::SInt16;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_UINT32:
            return coopvec::DataType::UInt32;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_SINT32:
            return coopvec::DataType::SInt32;
#if NVRHI_D3D12_WITH_LINALG
        case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E4M3FN:
            return coopvec::DataType::FloatE4M3;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT8_E5M2:
            return coopvec::DataType::FloatE5M2;
#else
        case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E4M3:
            return coopvec::DataType::FloatE4M3;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT_E5M2:
            return coopvec::DataType::FloatE5M2;
#endif
        case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT16:
            return coopvec::DataType::Float16;
        case D3D12_LINEAR_ALGEBRA_DATATYPE_FLOAT32:
            return coopvec::DataType::Float32;
        default:
            utils::InvalidEnum();
            return coopvec::DataType::Float32;
        }
    }
    
    D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT convertCoopVecMatrixLayout(coopvec::MatrixLayout layout)
    {
        switch (layout)
        {
        case coopvec::MatrixLayout::RowMajor:
            return D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR;
        case coopvec::MatrixLayout::ColumnMajor:
            return D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_COLUMN_MAJOR;
        case coopvec::MatrixLayout::InferencingOptimal:
            return D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_MUL_OPTIMAL;
        case coopvec::MatrixLayout::TrainingOptimal:
            return D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_OUTER_PRODUCT_OPTIMAL;
        default:
            utils::InvalidEnum();
            return D3D12_LINEAR_ALGEBRA_MATRIX_LAYOUT_ROW_MAJOR;
        }
    }
#endif

} // namespace nvrhi::d3d12
