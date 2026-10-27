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

#pragma once

#include <nvrhi/nvrhi.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif

// The preview Agility SDK ships its headers flat, without the DirectX-Headers directx/ prefix.
#if NVRHI_D3D12_WITH_DXRCLUSTER
#include <d3d12.h>
// Exactly 1.722, not "1.722 or later": the DXR cluster structs and the experimental SM 6.10
// opcodes match this preview, and later previews are not guaranteed to work with
// NVIDIA driver 621.01.
#if !defined(D3D12_PREVIEW_SDK_VERSION) || D3D12_PREVIEW_SDK_VERSION != 722
#error "NVRHI_D3D12_WITH_DXRCLUSTER requires exactly the 1.722 preview D3D12 Agility SDK; later previews are not guaranteed to work with NVIDIA driver 621.01"
#endif
#else
#include <directx/d3d12.h>
#endif

namespace nvrhi
{
    namespace ObjectTypes
    {
        constexpr ObjectType Nvrhi_D3D12_Device         = 0x00020101;
        constexpr ObjectType Nvrhi_D3D12_CommandList    = 0x00020102;
    };
}

namespace nvrhi::d3d12
{
    class IRootSignature : public IResource
    {
    };

    typedef RefCountPtr<IRootSignature> RootSignatureHandle;

    class ICommandList : public nvrhi::ICommandList
    {
    public:
        virtual bool allocateUploadBuffer(size_t size, void** pCpuAddress, D3D12_GPU_VIRTUAL_ADDRESS* pGpuAddress) = 0;
        virtual bool commitDescriptorHeaps() = 0;
        virtual D3D12_GPU_VIRTUAL_ADDRESS getBufferGpuVA(IBuffer* buffer) = 0;

        virtual void updateGraphicsVolatileBuffers() = 0;
        virtual void updateComputeVolatileBuffers() = 0;
    };

    typedef RefCountPtr<ICommandList> CommandListHandle;

    typedef uint32_t DescriptorIndex;

    class IDescriptorHeap
    {
    protected:
        IDescriptorHeap() = default;
        virtual ~IDescriptorHeap() = default;
    public:
        virtual DescriptorIndex allocateDescriptors(uint32_t count) = 0;
        virtual DescriptorIndex allocateDescriptor() = 0;
        virtual void releaseDescriptors(DescriptorIndex baseIndex, uint32_t count) = 0;
        virtual void releaseDescriptor(DescriptorIndex index) = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandle(DescriptorIndex index) = 0;
        virtual D3D12_CPU_DESCRIPTOR_HANDLE getCpuHandleShaderVisible(DescriptorIndex index) = 0;
        virtual D3D12_GPU_DESCRIPTOR_HANDLE getGpuHandle(DescriptorIndex index) = 0;
        [[nodiscard]] virtual ID3D12DescriptorHeap* getHeap() const = 0;
        [[nodiscard]] virtual ID3D12DescriptorHeap* getShaderVisibleHeap() const = 0;

        IDescriptorHeap(const IDescriptorHeap&) = delete;
        IDescriptorHeap(const IDescriptorHeap&&) = delete;
        IDescriptorHeap& operator=(const IDescriptorHeap&) = delete;
        IDescriptorHeap& operator=(const IDescriptorHeap&&) = delete;
    };

    enum class DescriptorHeapType
    {
        RenderTargetView,
        DepthStencilView,
        ShaderResourceView,
        Sampler
    };

    class IDevice : public nvrhi::IDevice
    {
    public:
        // D3D12-specific methods
        virtual RootSignatureHandle buildRootSignature(const static_vector<BindingLayoutHandle, c_MaxBindingLayouts>& pipelineLayouts, bool allowInputLayout, bool isLocal, const D3D12_ROOT_PARAMETER1* pCustomParameters = nullptr, uint32_t numCustomParameters = 0) = 0;
        virtual GraphicsPipelineHandle createHandleForNativeGraphicsPipeline(IRootSignature* rootSignature, ID3D12PipelineState* pipelineState, const GraphicsPipelineDesc& desc, const FramebufferInfo& framebufferInfo) = 0;
        virtual MeshletPipelineHandle createHandleForNativeMeshletPipeline(IRootSignature* rootSignature, ID3D12PipelineState* pipelineState, const MeshletPipelineDesc& desc, const FramebufferInfo& framebufferInfo) = 0;
        [[nodiscard]] virtual IDescriptorHeap* getDescriptorHeap(DescriptorHeapType heapType) = 0;
    };

    typedef RefCountPtr<IDevice> DeviceHandle;

    struct DeviceDesc
    {
        IMessageCallback* errorCB = nullptr;
        ID3D12Device* pDevice = nullptr;
        ID3D12CommandQueue* pGraphicsCommandQueue = nullptr;
        ID3D12CommandQueue* pComputeCommandQueue = nullptr;
        ID3D12CommandQueue* pCopyCommandQueue = nullptr;

        uint32_t renderTargetViewHeapSize = 1024;
        uint32_t depthStencilViewHeapSize = 1024;
        uint32_t shaderResourceViewHeapSize = 16384;
        uint32_t samplerHeapSize = 1024;
        uint32_t maxTimerQueries = 256;

        // If enabled and the device has the capability,
        // create RootSignatures with D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED 
        // and D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED
        bool enableHeapDirectlyIndexed = false;

        bool aftermathEnabled = false;

        // Enable logging the buffer lifetime to IMessageCallback
        // Useful for debugging resource lifetimes
        bool logBufferLifetime = false;

        // Enable NVAPI ray tracing validation (NvAPI_D3D12_EnableRaytracingValidation).
        // Requires NVAPI. The nvrhi::Device constructor sets the NV_ALLOW_RAYTRACING_VALIDATION=1
        // environment variable automatically, so callers don't need to export it beforehand.
        // Validation must be enabled before any other ray tracing call (including capability
        // queries).
        bool enableRayTracingValidation = false;

        // Enable D3D12 Enhanced Barriers, if supported.
        // Use device->queryFeatureSupport(Feature::EnhancedBarriers) to query the actual support.
        bool enableEnhancedBarriers = true;

        // Cluster operations run through the DXR cluster API when it is enabled and supported, otherwise
        // through NVAPI. Query Feature::RayTracingClusters with RayTracingClustersFeatureInfo
        // for the API in use.
        bool enableDxrClusters = true;
        bool enableNvapiClusters = true;
    };

    NVRHI_API DeviceHandle createDevice(const DeviceDesc& desc);

    // Enables the experimental D3D12 features that DXR clusters and SM 6.10 require.
    // Must be called before the application creates its ID3D12Device: changing feature
    // enablement puts every live device in the process into DEVICE_REMOVED.
    // Returns false when NVRHI was built without NVRHI_D3D12_WITH_DXRCLUSTER, or when the
    // runtime refused (most often because Windows Developer Mode is off).
    NVRHI_API bool enableExperimentalFeatures(IMessageCallback* messageCallback = nullptr);

    NVRHI_API DXGI_FORMAT convertFormat(nvrhi::Format format);
}