/*
* Copyright (c) 2024, NVIDIA CORPORATION. All rights reserved.
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

#ifndef NVRHI_HLSL_H
#define NVRHI_HLSL_H

// Cross-language static-assert. C++ uses standard `static_assert`. HLSL
// support varies across DXC frontends: DXIL-DXC accepts `_Static_assert`,
// but the SPIRV-DXC codegen path returns "decl type StaticAssert
// unimplemented". Until both paths agree, gate the HLSL branch to a no-op
// — C++ asserts are load-bearing (on-wire layout follows the C++ struct).
#ifdef __cplusplus
#define NVRHI_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define NVRHI_STATIC_ASSERT(cond, msg)
#endif

// bit field defines
#if defined(__cplusplus) || __HLSL_VERSION >= 2021 || __SLANG__
namespace nvrhi
{
    typedef uint64_t GpuVirtualAddress;
    struct GpuVirtualAddressAndStride
    {
        GpuVirtualAddress startAddress;
        uint64_t strideInBytes;
    };

    namespace rt
    {
        //////////////////////////////////////////////////////////////////////////
        // Indirect Arg Structs that are shader friendly
        //////////////////////////////////////////////////////////////////////////
        
        // Shader friendly equivalent of nvrhi::rt::InstanceDesc
        struct IndirectInstanceDesc
        {
#ifdef __cplusplus
            float transform[12];
#else
            float4 transform[3];
#endif
            uint32_t instanceID : 24;
            uint32_t instanceMask : 8;
            uint32_t instanceContributionToHitGroupIndex : 24;
            uint32_t flags : 8;
            GpuVirtualAddress blasDeviceAddress;
        };

        namespace cluster
        {
            static const uint32_t kClasByteAlignment = 128;
            static const uint32_t kClasMaxTriangles = 256; // Defined by spec
            static const uint32_t kClasMaxVertices = 256; // Defined by spec
            static const uint32_t kMaxGeometryIndex = 16777215; // Defined by spec

            // Per-CLAS geometry flag bits.  Mirrors:
            //   - VK_CLUSTER_ACCELERATION_STRUCTURE_GEOMETRY_*_BIT_NV
            //   - NVAPI_D3D12_RAYTRACING_MULTI_INDIRECT_CLUSTER_OPERATION_GEOMETRY_FLAG_*
            // Both APIs encode the flag bits at uint32 bit positions 29-31; the
            // values below are the 3-bit-field encoding (i.e. shifted into the
            // low 3 bits) so they can be assigned to the GeometryFlags packed
            // sub-field of GeometryIndexAndFlags directly.
            enum class ClusterGeometryFlags : uint32_t
            {
                None                        = 0,
                CullDisable                 = 1,  // disables triangle culling — see D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE
                NoDuplicateAnyHitInvocation = 2,  // matches D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION
                Opaque                      = 4   // matches D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE — skips any-hit
            };

            // Per-CLAS flags for the clusterFlags argument field. Mirrors:
            //   - D3D12_RTAS_CLUSTER_OPERATION_CLAS_FLAGS
            //   - NVAPI_D3D12_RAYTRACING_MULTI_INDIRECT_CLUSTER_OPERATION_CLUSTER_FLAGS
            //   - VkClusterAccelerationStructureClusterFlagBitsNV
            enum class ClusterFlags : uint32_t
            {
                None               = 0,
                AllowDisableOMMs   = 1,  // instances may disable the OMM test with D3D12_RAYTRACING_INSTANCE_FLAG_DISABLE_OMMS
                DisallowDataAccess = 2   // DXR clusters only: opts this CLAS out of TriangleObjectPositions()
            };

            // Clone of VkClusterAccelerationStructureGeometryIndexAndGeometryFlagsNV.
            // NVAPI's equivalent is the raw packed uint32 with the flags at bits
            // 29-31; we name the struct after the Vulkan spelling but the bit
            // layout is identical across both APIs and the static_assert below
            // pins the packing.
            struct GeometryIndexAndFlags
            {
                uint32_t geometryIndex : 24;
                uint32_t reserved      : 5;
                uint32_t geometryFlags : 3;  // ClusterGeometryFlags
            };
            NVRHI_STATIC_ASSERT(sizeof(GeometryIndexAndFlags) == sizeof(uint32_t),
                                "GeometryIndexAndFlags must pack into a single uint32 — "
                                "the on-wire layout for NVAPI/Vulkan cluster builders, and "
                                "for shader-side per-triangle arrays, depends on it.");

            // These structs have 16-bit fields, so HLSL only declares them with -enable-16bit-types.
#if defined(__cplusplus) || defined(__HLSL_ENABLE_16_BIT) || defined(__SLANG__)
            // Callers pick the layout that matches the device's ClusterApi: nv for NVAPI and
            // Vulkan, which are identical, and dxr for DXR clusters.
            namespace nv
            {
                // Clone of NVAPI_D3D12_RAYTRACING_ACCELERATION_STRUCTURE_MULTI_INDIRECT_TRIANGLE_CLUSTER_ARGS
                // and VkClusterAccelerationStructureBuildTriangleClusterInfoNV
                struct IndirectTriangleClasArgs
                {
                    uint32_t          clusterId;                         // The user specified cluster Id to encode in the CLAS
                    uint32_t          clusterFlags;                      // ClusterFlags
                    uint32_t          triangleCount : 9;                 // The number of triangles used by the CLAS (max 256)
                    uint32_t          vertexCount : 9;                   // The number of vertices used by the CLAS (max 256)
                    uint32_t          positionTruncateBitCount : 6;      // The number of bits to truncate from the position values
                    uint32_t          indexFormat : 4;                   // The index format to use for the indexBuffer, see NVAPI_3D12_RAYTRACING_MULTI_INDIRECT_CLUSTER_OPERATION_INDEX_FORMAT for possible values
                    uint32_t          opacityMicromapIndexFormat : 4;    // The index format to use for the opacityMicromapIndexBuffer, see NVAPI_3D12_RAYTRACING_MULTI_INDIRECT_CLUSTER_OPERATION_INDEX_FORMAT for possible values
                    GeometryIndexAndFlags baseGeometryIndexAndFlags;     // base geometry index (low 24 bits) + base geometry flags (bits 29-31, see ClusterGeometryFlags) — see geometryIndexAndFlagsBuffer
                    uint16_t          indexBufferStride;                 // The stride of the elements of indexBuffer, in bytes
                    uint16_t          vertexBufferStride;                // The stride of the elements of vertexBuffer, in bytes
                    uint16_t          geometryIndexAndFlagsBufferStride; // The stride of the elements of geometryIndexBuffer, in bytes
                    uint16_t          opacityMicromapIndexBufferStride;  // The stride of the elements of opacityMicromapIndexBuffer, in bytes
                    GpuVirtualAddress indexBuffer;                       // The index buffer to construct the CLAS
                    GpuVirtualAddress vertexBuffer;                      // The vertex buffer to construct the CLAS
                    GpuVirtualAddress geometryIndexAndFlagsBuffer;       // (optional) Address of an array of GeometryIndexAndFlags (one 32-bit struct per triangle), size equal to the triangle count. Each element supplies the per-triangle geometry index (low 24 bits) and ClusterGeometryFlags (bits 29-31); the resulting CLAS triangle's geometry index is the element's geometryIndex + baseGeometryIndexAndFlags.geometryIndex, and its flags are the bitwise OR of the two flag fields.
                    GpuVirtualAddress opacityMicromapArray;              // (optional) Address of a valid OMM array, if used NVAPI_D3D12_RAYTRACING_MULTI_INDIRECT_CLUSTER_OPERATION_FLAG_ALLOW_OMM must be set on this and all other cluster operation calls interacting with the object(s) constructed
                    GpuVirtualAddress opacityMicromapIndexBuffer;        // (optional) Address of an array of indices into the OMM array
                };

                // Clone of NVAPI_D3D12_RAYTRACING_ACCELERATION_STRUCTURE_MULTI_INDIRECT_TRIANGLE_TEMPLATE_ARGS
                // and VkClusterAccelerationStructureBuildTriangleClusterTemplateInfoNV
                struct IndirectTriangleTemplateArgs
                {
                    uint32_t          clusterId;                         // The user specified cluster Id to encode in the cluster template
                    uint32_t          clusterFlags;                      // ClusterFlags
                    uint32_t          triangleCount : 9;                 // The number of triangles used by the cluster template (max 256)
                    uint32_t          vertexCount : 9;                   // The number of vertices used by the cluster template (max 256)
                    uint32_t          positionTruncateBitCount : 6;      // The number of bits to truncate from the position values
                    uint32_t          indexFormat : 4;                   // The index format to use for the indexBuffer, must be one of nvrhi::rt::ClusteOperationIndexFormat
                    uint32_t          opacityMicromapIndexFormat : 4;    // The index format to use for the opacityMicromapIndexBuffer, see nvrhi::rt::ClusteOperationIndexFormat for possible values
                    GeometryIndexAndFlags baseGeometryIndexAndFlags;     // base geometry index (low 24 bits) + base geometry flags (bits 29-31, see ClusterGeometryFlags) — see geometryIndexAndFlagsBuffer
                    uint16_t          indexBufferStride;                 // The stride of the elements of indexBuffer, in bytes
                    uint16_t          vertexBufferStride;                // The stride of the elements of vertexBuffer, in bytes
                    uint16_t          geometryIndexAndFlagsBufferStride; // The stride of the elements of geometryIndexBuffer, in bytes
                    uint16_t          opacityMicromapIndexBufferStride;  // The stride of the elements of opacityMicromapIndexBuffer, in bytes
                    GpuVirtualAddress indexBuffer;                       // The index buffer to construct the cluster template
                    GpuVirtualAddress vertexBuffer;                      // (optional) The vertex buffer to optimize the cluster template, the vertices will not be stored in the cluster template
                    GpuVirtualAddress geometryIndexAndFlagsBuffer;       // (optional) Address of an array of GeometryIndexAndFlags (one 32-bit struct per triangle), size equal to the triangle count. Each element supplies the per-triangle geometry index (low 24 bits) and ClusterGeometryFlags (bits 29-31). If non-zero, the resulting CLAS triangle's geometry index is the element's geometryIndex + baseGeometryIndexAndFlags.geometryIndex, and its flags are the bitwise OR of the two flag fields; otherwise all triangles use baseGeometryIndexAndFlags directly.
                    GpuVirtualAddress opacityMicromapArray;              // (optional) Address of a valid OMM array, if used NVAPI_D3D12_RAYTRACING_MULTI_INDIRECT_CLUSTER_OPERATION_FLAG_ALLOW_OMM must be set on this and all other cluster operation calls interacting with the object(s) constructed
                    GpuVirtualAddress opacityMicromapIndexBuffer;        // (optional) Address of an array of indices into the OMM array
                    GpuVirtualAddress instantiationBoundingBoxLimit;     // (optional) Pointer to 6 floats with alignment NVAPI_D3D12_RAYTRACING_CLUSTER_TEMPLATE_BOUNDS_BYTE_ALIGNMENT representing the limits of the positions of any vertices the template will ever be instantiated with
                };

                // Clone of NVAPI_D3D12_RAYTRACING_ACCELERATION_STRUCTURE_MULTI_INDIRECT_INSTANTIATE_TEMPLATE_ARGS
                // and VkClusterAccelerationStructureInstantiateClusterInfoNV
                struct IndirectInstantiateTemplateArgs
                {
                    uint32_t                   clusterIdOffset;      // The offset added to the clusterId stored in the Cluster template to calculate the final clusterId that will be written to the instantiated CLAS
                    uint32_t                   geometryIndexOffset;  // The offset added to the geometry index stored for each triangle in the Cluster template to calculate the final geometry index that will be written to the triangles of the instantiated CLAS, the resulting value may not exceed maxGeometryIndexValue both of this call, and the call used to construct the original cluster template referenced
                    GpuVirtualAddress          clusterTemplate;      // Address of a previously built cluster template to be instantiated
                    GpuVirtualAddressAndStride vertexBuffer;         // Vertex buffer with stride to use to fetch the vertex positions used for instantiation
                };

                // Element type of the operation size array (OperationDesc::outSizesBuffer).
                using OperationSize = uint32_t;
            } // namespace nv

            // DXR cluster layouts.
            namespace dxr
            {
                // Clone of D3D12_RTAS_OPERATION_BUILD_CLAS_FROM_TRIANGLES_ARGS. Field names match
                // the NVAPI version, but the order, packing and size (80 vs 64 bytes) do not,
                // and the index formats moved to OperationClasBuildParams.
                struct IndirectTriangleClasArgs
                {
                    uint32_t          clusterId;                         // The user specified cluster Id to encode in the CLAS
                    uint32_t          clusterFlags;                      // ClusterFlags
                    uint16_t          triangleCount;                     // The number of triangles used by the CLAS (max 256)
                    uint16_t          vertexCount;                       // The number of vertices used by the CLAS (max 256)
                    GeometryIndexAndFlags baseGeometryIndexAndFlags;     // base geometry index (low 24 bits) + base geometry flags (bits 29-31, see ClusterGeometryFlags) — see geometryIndexAndFlagsBuffer
                    uint32_t          opacityMicromapBaseLocation;       // (optional) Base location added to each opacity micromap index
                    uint16_t          vertexBufferStride;                // The stride of the elements of vertexBuffer, in bytes
                    uint16_t          indexBufferStride;                 // The stride of the elements of indexBuffer, in bytes
                    uint16_t          opacityMicromapIndexBufferStride;  // The stride of the elements of opacityMicromapIndexBuffer, in bytes
                    uint16_t          geometryIndexAndFlagsBufferStride; // The stride of the elements of geometryIndexAndFlagsBuffer, in bytes
                    uint16_t          positionTruncateBitCount;          // The number of bits to truncate from the position values
                    uint16_t          reservedPadding;                   // Reserved, must be 0
                    GpuVirtualAddress vertexBuffer;                      // The vertex buffer to construct the CLAS
                    GpuVirtualAddress indexBuffer;                       // The index buffer to construct the CLAS
                    GpuVirtualAddress geometryIndexAndFlagsBuffer;       // (optional) Address of an array of GeometryIndexAndFlags, one per triangle
                    GpuVirtualAddress geometryIndexAndFlagsIndexBuffer;  // (optional) Address of an array of indices into geometryIndexAndFlagsBuffer
                    GpuVirtualAddress opacityMicromapArray;              // (optional) Address of a valid OMM array
                    GpuVirtualAddress opacityMicromapIndexBuffer;        // (optional) Address of an array of indices into the OMM array
                };

                // Clone of D3D12_RTAS_OPERATION_BUILD_CLUSTER_TEMPLATES_FROM_TRIANGLES_ARGS, whose
                // leading members are a nested BUILD_CLAS_FROM_TRIANGLES_ARGS; they are inlined here
                // so the field names match IndirectTriangleClasArgs.
                struct IndirectTriangleTemplateArgs
                {
                    uint32_t          clusterId;                         // The user specified cluster Id to encode in the cluster template
                    uint32_t          clusterFlags;                      // ClusterFlags
                    uint16_t          triangleCount;                     // The number of triangles used by the cluster template (max 256)
                    uint16_t          vertexCount;                       // The number of vertices used by the cluster template (max 256)
                    GeometryIndexAndFlags baseGeometryIndexAndFlags;     // base geometry index (low 24 bits) + base geometry flags (bits 29-31, see ClusterGeometryFlags)
                    uint32_t          opacityMicromapBaseLocation;       // (optional) Base location added to each opacity micromap index
                    uint16_t          vertexBufferStride;                // The stride of the elements of vertexBuffer, in bytes
                    uint16_t          indexBufferStride;                 // The stride of the elements of indexBuffer, in bytes
                    uint16_t          opacityMicromapIndexBufferStride;  // The stride of the elements of opacityMicromapIndexBuffer, in bytes
                    uint16_t          geometryIndexAndFlagsBufferStride; // The stride of the elements of geometryIndexAndFlagsBuffer, in bytes
                    uint16_t          positionTruncateBitCount;          // The number of bits to truncate from the position values
                    uint16_t          reservedPadding;                   // Reserved, must be 0
                    GpuVirtualAddress vertexBuffer;                      // (optional) The vertex buffer to optimize the cluster template
                    GpuVirtualAddress indexBuffer;                       // The index buffer to construct the cluster template
                    GpuVirtualAddress geometryIndexAndFlagsBuffer;       // (optional) Address of an array of GeometryIndexAndFlags, one per triangle
                    GpuVirtualAddress geometryIndexAndFlagsIndexBuffer;  // (optional) Address of an array of indices into geometryIndexAndFlagsBuffer
                    GpuVirtualAddress opacityMicromapArray;              // (optional) Address of a valid OMM array
                    GpuVirtualAddress opacityMicromapIndexBuffer;        // (optional) Address of an array of indices into the OMM array
                    GpuVirtualAddress instantiationBoundingBoxLimit;     // (optional) Pointer to 6 floats bounding the positions the template will be instantiated with
                    uint32_t          compressed1Header;                 // Only used when the vertex format is COMPRESSED1
                    uint32_t          reservedPadding2;                  // Pads the format-specific union arm to 64 bits
                };

                // Clone of D3D12_RTAS_OPERATION_INSTANTIATE_CLUSTER_TEMPLATES_ARGS. Same size as the
                // NVAPI struct, but the first two members are in the opposite order.
                struct IndirectInstantiateTemplateArgs
                {
                    int32_t                    geometryIndexOffset;  // The offset added to the geometry index stored for each triangle in the Cluster template
                    uint32_t                   clusterIdOffset;      // The offset added to the clusterId stored in the Cluster template
                    GpuVirtualAddress          clusterTemplate;      // Address of a previously built cluster template to be instantiated
                    GpuVirtualAddressAndStride vertexBuffer;         // Vertex buffer with stride to use to fetch the vertex positions used for instantiation
                };

                // DXR clusters report result sizes as UINT64.
                using OperationSize = uint64_t;
            } // namespace dxr
#endif

            // Clone of D3D12_RTAS_OPERATION_BUILD_BLAS_FROM_CLAS_ARGS, which matches
            // VkClusterAccelerationStructureBuildClustersBottomLevelInfoNV. NVAPI still calls
            // the stride "reserved, must be 0"; the layout is the same either way.
            struct IndirectArgs
            {
                uint32_t                  clusterCount;         // The size of the array referenced by clusterAddresses
                uint32_t                  clusterAddressStride; // Stride of the elements of clusterAddresses, in bytes
                GpuVirtualAddress         clusterAddresses;     // Address of an array of D3D12_GPU_VIRTUAL_ADDRESS holding valid addresses of CLAS previously constructed
            };
        } // namespace cluster
    } // namespace rt
} // namespace nvrhi

#endif // __HLSL_VERSION 2021
#endif