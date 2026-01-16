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

#include "vulkan-backend.h"
#include <nvrhi/common/misc.h>
#include <sstream>

namespace nvrhi::vulkan
{

    BindingLayoutHandle Device::createBindingLayout(const BindingLayoutDesc& desc)
    {
        BindingLayout* ret = new BindingLayout(m_Context, desc);

        ret->bake();

        return BindingLayoutHandle::Create(ret);
    }

    BindingLayoutHandle Device::createBindlessLayout(const BindlessLayoutDesc& desc)
    {
        BindingLayout* ret = new BindingLayout(m_Context, desc);

        ret->bake();

        return BindingLayoutHandle::Create(ret);
    }

    static uint32_t getRegisterOffsetForResourceType(VulkanBindingOffsets const& bindingOffsets, ResourceType type)
    {
        switch (type)
        {
        case ResourceType::Texture_SRV:
        case ResourceType::TypedBuffer_SRV:
        case ResourceType::StructuredBuffer_SRV:
        case ResourceType::RawBuffer_SRV:
        case ResourceType::RayTracingAccelStruct:
            return bindingOffsets.shaderResource;

        case ResourceType::Texture_UAV:
        case ResourceType::TypedBuffer_UAV:
        case ResourceType::StructuredBuffer_UAV:
        case ResourceType::RawBuffer_UAV:
            return bindingOffsets.unorderedAccess;

        case ResourceType::ConstantBuffer:
        case ResourceType::VolatileConstantBuffer:
        case ResourceType::PushConstants:
            return bindingOffsets.constantBuffer;
            break;

        case ResourceType::Sampler:
            return bindingOffsets.sampler;

        default:
            utils::InvalidEnum();
            return 0;
        }        
    }

    BindingLayout::BindingLayout(const VulkanContext& context, const BindingLayoutDesc& _desc)
        : desc(_desc)
        , isBindless(false)
        , m_Context(context)
    {
        vk::ShaderStageFlagBits shaderStageFlags = convertShaderTypeToShaderStageFlagBits(desc.visibility);

        // iterate over all binding types and add to map
        for (const BindingLayoutItem& binding : desc.bindings)
        {
            if (binding.type == ResourceType::PushConstants)
            {
                // Don't need any descriptors for the push constants
                continue;
            }

            vk::DescriptorType const descriptorType = convertResourceType(binding.type);
            uint32_t const descriptorCount = binding.size;
            uint32_t const registerOffset = getRegisterOffsetForResourceType(_desc.bindingOffsets, binding.type);

            const uint32_t bindingLocation = registerOffset + binding.slot;

            vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(bindingLocation)
                .setDescriptorCount(descriptorCount)
                .setDescriptorType(descriptorType)
                .setStageFlags(shaderStageFlags);

            vulkanLayoutBindings.push_back(descriptorSetLayoutBinding);
        }
    }

    BindingLayout::BindingLayout(const VulkanContext& context, const BindlessLayoutDesc& _desc)
        : bindlessDesc(_desc)
        , isBindless(true)
        , m_Context(context)
    {
        desc.visibility = bindlessDesc.visibility;
        vk::ShaderStageFlagBits shaderStageFlags = convertShaderTypeToShaderStageFlagBits(bindlessDesc.visibility);
        uint32_t bindingPoint = 0;
        uint32_t arraySize = bindlessDesc.maxCapacity;
        
        if (bindlessDesc.layoutType != BindlessLayoutDesc::LayoutType::Immutable)
        {
            if (!m_Context.extensions.EXT_mutable_descriptor_type)
            {
                m_Context.error("Mutable descriptor types are not supported by this device. VK_EXT_mutable_descriptor_type extension is required for mutable bindless layouts.");
            }

            if (bindlessDesc.registerSpaces.size() > 0)
            {
                m_Context.error("Mutable descriptor sets cannot specify register spaces");
            }

            vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding()
                .setBinding(bindingPoint)
                .setDescriptorCount(arraySize)
                .setDescriptorType(vk::DescriptorType::eMutableEXT)
                .setStageFlags(shaderStageFlags);

            vulkanLayoutBindings.push_back(descriptorSetLayoutBinding);
        }
        else
        {
            // iterate over all binding types and add to map
            for (const BindingLayoutItem& space : bindlessDesc.registerSpaces)
            {
                vk::DescriptorType const descriptorType = convertResourceType(space.type);
                
                if (space.type == ResourceType::VolatileConstantBuffer)
                    m_Context.error("Volatile constant buffers are not supported in bindless layouts");

                vk::DescriptorSetLayoutBinding descriptorSetLayoutBinding = vk::DescriptorSetLayoutBinding()
                    .setBinding(bindingPoint)
                    .setDescriptorCount(arraySize)
                    .setDescriptorType(descriptorType)
                    .setStageFlags(shaderStageFlags);

                vulkanLayoutBindings.push_back(descriptorSetLayoutBinding);

                ++bindingPoint;
            }
        }
    }

    Object BindingLayout::getNativeObject(ObjectType objectType)
    {
        switch (objectType)
        {
        case ObjectTypes::VK_DescriptorSetLayout:
            return Object(descriptorSetLayout);
        default:
            return nullptr;
        }
    }

    vk::Result BindingLayout::bake()
    {
        // create the descriptor set layout object
        
        auto descriptorSetLayoutInfo = vk::DescriptorSetLayoutCreateInfo()
            .setBindingCount(uint32_t(vulkanLayoutBindings.size()))
            .setPBindings(vulkanLayoutBindings.data());

        std::vector<vk::DescriptorBindingFlags> bindFlag(vulkanLayoutBindings.size(), vk::DescriptorBindingFlagBits::ePartiallyBound);

        auto extendedInfo = vk::DescriptorSetLayoutBindingFlagsCreateInfo()
            .setBindingCount(uint32_t(vulkanLayoutBindings.size()))
            .setPBindingFlags(bindFlag.data());

        vk::DescriptorType cbvSrvUavTypes[] =
        {
            vk::DescriptorType::eSampledImage,
            vk::DescriptorType::eStorageImage,
            vk::DescriptorType::eUniformTexelBuffer,
            vk::DescriptorType::eStorageTexelBuffer,
            vk::DescriptorType::eUniformBuffer,
            vk::DescriptorType::eStorageBuffer
        };

        vk::DescriptorType counterTypes[] =
        {
            vk::DescriptorType::eStorageBuffer
        };

        vk::DescriptorType samplerTypes[] =
        {
            vk::DescriptorType::eSampler
        };

        auto cbvSrvUavTypesList = vk::MutableDescriptorTypeListEXT()
            .setDescriptorTypeCount(uint32_t(sizeof(cbvSrvUavTypes) / sizeof(cbvSrvUavTypes[0])))
            .setPDescriptorTypes(cbvSrvUavTypes);

        auto counterTypesList = vk::MutableDescriptorTypeListEXT()
            .setDescriptorTypeCount(uint32_t(sizeof(counterTypes) / sizeof(counterTypes[0])))
            .setPDescriptorTypes(counterTypes);

        auto samplerTypesList = vk::MutableDescriptorTypeListEXT()
            .setDescriptorTypeCount(uint32_t(sizeof(samplerTypes) / sizeof(samplerTypes[0])))
            .setPDescriptorTypes(samplerTypes);

        auto pMutableDescriptorTypeLists =
            bindlessDesc.layoutType == BindlessLayoutDesc::LayoutType::MutableCounters ? &counterTypesList :
            bindlessDesc.layoutType == BindlessLayoutDesc::LayoutType::MutableSampler ? &samplerTypesList :
            &cbvSrvUavTypesList;

        auto mutableDescriptorTypeCreateInfo = vk::MutableDescriptorTypeCreateInfoEXT()
            .setMutableDescriptorTypeListCount(1)
            .setPMutableDescriptorTypeLists(pMutableDescriptorTypeLists)
            .setPNext(&extendedInfo);

        if (isBindless)
        {
            if (bindlessDesc.layoutType != BindlessLayoutDesc::LayoutType::Immutable)
            {
                descriptorSetLayoutInfo.setPNext(&mutableDescriptorTypeCreateInfo);
            }
            else
            {
                descriptorSetLayoutInfo.setPNext(&extendedInfo);
            }
        }

        const vk::Result res = m_Context.device.createDescriptorSetLayout(&descriptorSetLayoutInfo,
                                                                        m_Context.allocationCallbacks,
                                                                        &descriptorSetLayout);
        CHECK_VK_RETURN(res)

        // count the number of descriptors required per type
        std::unordered_map<vk::DescriptorType, uint32_t> poolSizeMap;
        for (auto layoutBinding : vulkanLayoutBindings)
        {
            if (poolSizeMap.find(layoutBinding.descriptorType) == poolSizeMap.end())
            {
                poolSizeMap[layoutBinding.descriptorType] = 0;
            }

            poolSizeMap[layoutBinding.descriptorType] += layoutBinding.descriptorCount;
        }

        // compute descriptor pool size info
        for (auto poolSizeIter : poolSizeMap)
        {
            if (poolSizeIter.second > 0)
            {
                descriptorPoolSizeInfo.push_back(vk::DescriptorPoolSize()
                    .setType(poolSizeIter.first)
                    .setDescriptorCount(poolSizeIter.second));
            }
        }

        return vk::Result::eSuccess;
    }

    BindingLayout::~BindingLayout()
    {
        if (descriptorSetLayout)
        {
            m_Context.device.destroyDescriptorSetLayout(descriptorSetLayout, m_Context.allocationCallbacks);
            descriptorSetLayout = vk::DescriptorSetLayout();
        }
    }

    static Texture::TextureSubresourceViewType getTextureViewType(Format bindingFormat, Format textureFormat)
    {
        Format format = (bindingFormat == Format::UNKNOWN) ? textureFormat : bindingFormat;

        const FormatInfo& formatInfo = getFormatInfo(format);

        if (formatInfo.hasDepth)
            return Texture::TextureSubresourceViewType::DepthOnly;
        else if (formatInfo.hasStencil)
            return Texture::TextureSubresourceViewType::StencilOnly;
        else
            return Texture::TextureSubresourceViewType::AllAspects;
    }

    BindingSetHandle Device::createBindingSet(const BindingSetDesc& desc, IBindingLayout* _layout)
    {
        BindingLayout* layout = checked_cast<BindingLayout*>(_layout);

        BindingSet *ret = new BindingSet(m_Context);
        ret->desc = desc;
        ret->layout = layout;

        const auto& descriptorSetLayout = layout->descriptorSetLayout;
        const auto& poolSizes = layout->descriptorPoolSizeInfo;

        // create descriptor pool to allocate a descriptor from
        auto poolInfo = vk::DescriptorPoolCreateInfo()
            .setPoolSizeCount(uint32_t(poolSizes.size()))
            .setPPoolSizes(poolSizes.data())
            .setMaxSets(1);

        vk::Result res = m_Context.device.createDescriptorPool(&poolInfo,
                                                             m_Context.allocationCallbacks,
                                                             &ret->descriptorPool);
        CHECK_VK_FAIL(res)
        
        // create the descriptor set
        auto descriptorSetAllocInfo = vk::DescriptorSetAllocateInfo()
            .setDescriptorPool(ret->descriptorPool)
            .setDescriptorSetCount(1)
            .setPSetLayouts(&descriptorSetLayout);

        res = m_Context.device.allocateDescriptorSets(&descriptorSetAllocInfo,
            &ret->descriptorSet);
        CHECK_VK_FAIL(res)
        
        // collect all of the descriptor write data
        std::vector<vk::DescriptorImageInfo> descriptorImageInfo;
        std::vector<vk::DescriptorBufferInfo> descriptorBufferInfo;
        std::vector<vk::WriteDescriptorSet> descriptorWriteInfo;
        std::vector<vk::WriteDescriptorSetAccelerationStructureKHR> accelStructWriteInfo;
        descriptorImageInfo.reserve(desc.bindings.size());
        descriptorBufferInfo.reserve(desc.bindings.size());
        descriptorWriteInfo.reserve(desc.bindings.size());
        accelStructWriteInfo.reserve(desc.bindings.size());

        auto generateWriteDescriptorData =
            // generates a vk::WriteDescriptorSet struct in descriptorWriteInfo
            [&](uint32_t bindingLocation,
                uint32_t arrayElement,
                vk::DescriptorType descriptorType,
                vk::DescriptorImageInfo *imageInfo,
                vk::DescriptorBufferInfo *bufferInfo,
                vk::BufferView *bufferView,
                const void* pNext = nullptr)
        {
            descriptorWriteInfo.push_back(
                vk::WriteDescriptorSet()
                .setDstSet(ret->descriptorSet)
                .setDstBinding(bindingLocation)
                .setDstArrayElement(arrayElement)
                .setDescriptorCount(1)
                .setDescriptorType(descriptorType)
                .setPImageInfo(imageInfo)
                .setPBufferInfo(bufferInfo)
                .setPTexelBufferView(bufferView)
                .setPNext(pNext)
            );
        };

        for (size_t bindingIndex = 0; bindingIndex < desc.bindings.size(); bindingIndex++)
        {
            const BindingSetItem& binding = desc.bindings[bindingIndex];

            if (binding.resourceHandle == nullptr)
            {
                continue;
            }

            ret->resources.push_back(binding.resourceHandle); // keep a strong reference to the resource

            vk::DescriptorType const descriptorType = convertResourceType(binding.type);
            uint32_t const registerOffset = getRegisterOffsetForResourceType(layout->desc.bindingOffsets, binding.type);
            
            switch (binding.type)
            {
            case ResourceType::Texture_SRV:
            {
                const auto texture = checked_cast<Texture *>(binding.resourceHandle);

                const auto subresource = binding.subresources.resolve(texture->desc, false);
                const auto textureViewType = getTextureViewType(binding.format, texture->desc.format);
                auto& view = texture->getSubresourceView(subresource, binding.dimension, binding.format, vk::ImageUsageFlagBits::eSampled, textureViewType);

                auto& imageInfo = descriptorImageInfo.emplace_back();
                imageInfo = vk::DescriptorImageInfo()
                    .setImageView(view.view)
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

                generateWriteDescriptorData(
                    registerOffset + binding.slot,
                    binding.arrayElement,
                    descriptorType,
                    &imageInfo, nullptr, nullptr);

                if (!texture->permanentState)
                    ret->bindingsThatNeedTransitions.push_back(static_cast<uint16_t>(bindingIndex));
                else
                    verifyPermanentResourceState(texture->permanentState,
                        ResourceStates::ShaderResource,
                        true, texture->desc.debugName, m_Context.messageCallback);
            }

            break;

            case ResourceType::Texture_UAV:
            {
                const auto texture = checked_cast<Texture *>(binding.resourceHandle);

                const auto subresource = binding.subresources.resolve(texture->desc, true);
                const auto textureViewType = getTextureViewType(binding.format, texture->desc.format);
                auto& view = texture->getSubresourceView(subresource, binding.dimension, binding.format, vk::ImageUsageFlagBits::eStorage, textureViewType);

                auto& imageInfo = descriptorImageInfo.emplace_back();
                imageInfo = vk::DescriptorImageInfo()
                    .setImageView(view.view)
                    .setImageLayout(vk::ImageLayout::eGeneral);

                generateWriteDescriptorData(
                    registerOffset + binding.slot,
                    binding.arrayElement,
                    descriptorType,
                    &imageInfo, nullptr, nullptr);

                if (!texture->permanentState)
                    ret->bindingsThatNeedTransitions.push_back(static_cast<uint16_t>(bindingIndex));
                else
                    verifyPermanentResourceState(texture->permanentState,
                        ResourceStates::UnorderedAccess,
                        true, texture->desc.debugName, m_Context.messageCallback);
            }

            break;

            case ResourceType::TypedBuffer_SRV:
            case ResourceType::TypedBuffer_UAV:
            {
                const auto buffer = checked_cast<Buffer *>(binding.resourceHandle);

                assert(buffer->desc.canHaveTypedViews);
                const bool isUAV = (binding.type == ResourceType::TypedBuffer_UAV);
                if (isUAV)
                    assert(buffer->desc.canHaveUAVs);

                Format format = binding.format;

                if (format == Format::UNKNOWN)
                {
                    format = buffer->desc.format;
                }

                auto vkformat = nvrhi::vulkan::convertFormat(format);
                const auto range = binding.range.resolve(buffer->desc);

                size_t viewInfoHash = 0;
                nvrhi::hash_combine(viewInfoHash, range.byteOffset);
                nvrhi::hash_combine(viewInfoHash, range.byteSize);
                nvrhi::hash_combine(viewInfoHash, (uint64_t)vkformat);

                const auto& bufferViewFound = buffer->viewCache.find(viewInfoHash);
                auto& bufferViewRef = (bufferViewFound != buffer->viewCache.end()) ? bufferViewFound->second : buffer->viewCache[viewInfoHash];
                if (bufferViewFound == buffer->viewCache.end())
                {
                    assert(format != Format::UNKNOWN);

                    auto bufferViewInfo = vk::BufferViewCreateInfo()
                        .setBuffer(buffer->buffer)
                        .setOffset(range.byteOffset)
                        .setRange(range.byteSize)
                        .setFormat(vk::Format(vkformat));

                    res = m_Context.device.createBufferView(&bufferViewInfo, m_Context.allocationCallbacks, &bufferViewRef);
                    ASSERT_VK_OK(res);
                }

                generateWriteDescriptorData(
                    registerOffset + binding.slot,
                    binding.arrayElement,
                    descriptorType,
                    nullptr, nullptr, &bufferViewRef);

                if (!buffer->permanentState)
                    ret->bindingsThatNeedTransitions.push_back(static_cast<uint16_t>(bindingIndex));
                else
                    verifyPermanentResourceState(buffer->permanentState, 
                        isUAV ? ResourceStates::UnorderedAccess : ResourceStates::ShaderResource,
                        false, buffer->desc.debugName, m_Context.messageCallback);
            }
            break;

            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_SRV:
            case ResourceType::RawBuffer_UAV:
            case ResourceType::ConstantBuffer:
            case ResourceType::VolatileConstantBuffer:
            {
                const auto buffer = checked_cast<Buffer *>(binding.resourceHandle);

                if (binding.type == ResourceType::StructuredBuffer_UAV || binding.type == ResourceType::RawBuffer_UAV)
                    assert(buffer->desc.canHaveUAVs);
                if (binding.type == ResourceType::StructuredBuffer_UAV || binding.type == ResourceType::StructuredBuffer_SRV)
                    assert(buffer->desc.structStride != 0);
                if (binding.type == ResourceType::RawBuffer_SRV|| binding.type == ResourceType::RawBuffer_UAV)
                    assert(buffer->desc.canHaveRawViews);

                const auto range = binding.range.resolve(buffer->desc);

                auto& bufferInfo = descriptorBufferInfo.emplace_back();
                bufferInfo = vk::DescriptorBufferInfo()
                    .setBuffer(buffer->buffer)
                    .setOffset(range.byteOffset)
                    .setRange(range.byteSize);

                assert(buffer->buffer);
                generateWriteDescriptorData(
                    registerOffset + binding.slot,
                    binding.arrayElement,
                    descriptorType,
                    nullptr, &bufferInfo, nullptr);

                if (binding.type == ResourceType::VolatileConstantBuffer) 
                {
                    assert(buffer->desc.isVolatile);
                    ret->volatileConstantBuffers.push_back(buffer);
                }
                else
                {
                    if (!buffer->permanentState)
                        ret->bindingsThatNeedTransitions.push_back(static_cast<uint16_t>(bindingIndex));
                    else
                    {
                        ResourceStates requiredState;
                        if (binding.type == ResourceType::StructuredBuffer_UAV || binding.type == ResourceType::RawBuffer_UAV)
                            requiredState = ResourceStates::UnorderedAccess;
                        else if (binding.type == ResourceType::ConstantBuffer)
                            requiredState = ResourceStates::ConstantBuffer;
                        else
                            requiredState = ResourceStates::ShaderResource;

                        verifyPermanentResourceState(buffer->permanentState, requiredState,
                            false, buffer->desc.debugName, m_Context.messageCallback);
                    }
                }
            }

            break;

            case ResourceType::Sampler:
            {
                const auto sampler = checked_cast<Sampler *>(binding.resourceHandle);

                auto& imageInfo = descriptorImageInfo.emplace_back();
                imageInfo = vk::DescriptorImageInfo()
                    .setSampler(sampler->sampler);

                generateWriteDescriptorData(
                    registerOffset + binding.slot,
                    binding.arrayElement,
                    descriptorType,
                    &imageInfo, nullptr, nullptr);
            }

            break;

            case ResourceType::RayTracingAccelStruct:
            {
                const auto as = checked_cast<AccelStruct*>(binding.resourceHandle);

                auto& accelStructWrite = accelStructWriteInfo.emplace_back();
                accelStructWrite.accelerationStructureCount = 1;
                accelStructWrite.pAccelerationStructures = &as->accelStruct;

                generateWriteDescriptorData(
                    registerOffset + binding.slot,
                    binding.arrayElement,
                    descriptorType,
                    nullptr, nullptr, nullptr, &accelStructWrite);

                ret->bindingsThatNeedTransitions.push_back(static_cast<uint16_t>(bindingIndex));
            }

            break;

            case ResourceType::PushConstants:
                break;

            case ResourceType::None:
            case ResourceType::Count:
            default:
                utils::InvalidEnum();
                break;
            }

            // Update the hasUavBindings flag, it's cleaner to do it as a separate switch.
            switch (binding.type)
            {
            case ResourceType::Texture_UAV:
            case ResourceType::TypedBuffer_UAV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_UAV:
            case ResourceType::SamplerFeedbackTexture_UAV:
                ret->hasUavBindings = true;
                break;
            default:
                break;
            }

        }

        m_Context.device.updateDescriptorSets(uint32_t(descriptorWriteInfo.size()), descriptorWriteInfo.data(), 0, nullptr);

        return BindingSetHandle::Create(ret);
    }

    BindingSet::~BindingSet()
    {
        if (descriptorPool)
        {
            m_Context.device.destroyDescriptorPool(descriptorPool, m_Context.allocationCallbacks);
            descriptorPool = vk::DescriptorPool();
            descriptorSet = vk::DescriptorSet();
        }
    }

    Object BindingSet::getNativeObject(ObjectType objectType)
    {
        switch (objectType)
        {
        case ObjectTypes::VK_DescriptorPool:
            return Object(descriptorPool);
        case ObjectTypes::VK_DescriptorSet:
            return Object(descriptorSet);
        default:
            return nullptr;
        }
    }

    DescriptorTableHandle Device::createDescriptorTable(IBindingLayout* _layout)
    { 
        BindingLayout* layout = checked_cast<BindingLayout*>(_layout);

        DescriptorTable* ret = new DescriptorTable(m_Context);
        ret->layout = layout;
        ret->capacity = layout->vulkanLayoutBindings[0].descriptorCount;

        const auto& descriptorSetLayout = layout->descriptorSetLayout;
        const auto& poolSizes = layout->descriptorPoolSizeInfo;

        // create descriptor pool to allocate a descriptor from
        auto poolInfo = vk::DescriptorPoolCreateInfo()
            .setPoolSizeCount(uint32_t(poolSizes.size()))
            .setPPoolSizes(poolSizes.data())
            .setMaxSets(1);

        vk::Result res = m_Context.device.createDescriptorPool(&poolInfo,
                                                             m_Context.allocationCallbacks,
                                                             &ret->descriptorPool);
        CHECK_VK_FAIL(res)

        // create the descriptor set
        auto descriptorSetAllocInfo = vk::DescriptorSetAllocateInfo()
            .setDescriptorPool(ret->descriptorPool)
            .setDescriptorSetCount(1)
            .setPSetLayouts(&descriptorSetLayout);

        res = m_Context.device.allocateDescriptorSets(&descriptorSetAllocInfo,
            &ret->descriptorSet);
        CHECK_VK_FAIL(res)

        return DescriptorTableHandle::Create(ret);
    }

    DescriptorTable::~DescriptorTable()
    {
        if (descriptorPool)
        {
            m_Context.device.destroyDescriptorPool(descriptorPool, m_Context.allocationCallbacks);
            descriptorPool = vk::DescriptorPool();
            descriptorSet = vk::DescriptorSet();
        }
    }

    Object DescriptorTable::getNativeObject(ObjectType objectType)
    {
        switch (objectType)
        {
        case ObjectTypes::VK_DescriptorPool:
            return Object(descriptorPool);
        case ObjectTypes::VK_DescriptorSet:
            return Object(descriptorSet);
        default:
            return nullptr;
        }
    }

    void Device::resizeDescriptorTable(IDescriptorTable* _descriptorTable, uint32_t newSize, bool keepContents)
    {
        assert(newSize <= checked_cast<DescriptorTable*>(_descriptorTable)->layout->getBindlessDesc()->maxCapacity);
        (void)_descriptorTable;
        (void)newSize;
        (void)keepContents;
    }

    bool Device::writeDescriptorTable(IDescriptorTable* _descriptorTable, const BindingSetItem& binding)
    {
        DescriptorTable* descriptorTable = checked_cast<DescriptorTable*>(_descriptorTable);
        BindingLayout* layout = checked_cast<BindingLayout*>(descriptorTable->layout.Get());

        if (binding.slot >= descriptorTable->capacity)
            return false;

        if (binding.type == ResourceType::None)
        {
            // Vulkan doesn't support null descriptors, we use vk::DescriptorBindingFlagBits::ePartiallyBound
            return true;
        }

        // collect all of the descriptor write data
        static_vector<vk::DescriptorImageInfo, c_MaxBindlessRegisterSpaces> descriptorImageInfo;
        static_vector<vk::DescriptorBufferInfo, c_MaxBindlessRegisterSpaces> descriptorBufferInfo;
        static_vector<vk::WriteDescriptorSet, c_MaxBindlessRegisterSpaces> descriptorWriteInfo;

        auto generateWriteDescriptorData =
            // generates a vk::WriteDescriptorSet struct in descriptorWriteInfo
            [&](uint32_t bindingLocation,
                vk::DescriptorType descriptorType,
                vk::DescriptorImageInfo* imageInfo,
                vk::DescriptorBufferInfo* bufferInfo,
                vk::BufferView* bufferView)
        {
            descriptorWriteInfo.push_back(
                vk::WriteDescriptorSet()
                .setDstSet(descriptorTable->descriptorSet)
                .setDstBinding(bindingLocation)
                .setDstArrayElement(binding.slot)
                .setDescriptorCount(1)
                .setDescriptorType(descriptorType)
                .setPImageInfo(imageInfo)
                .setPBufferInfo(bufferInfo)
                .setPTexelBufferView(bufferView)
            );
        };

        auto writeDescriptorForBinding = [&](const vk::DescriptorSetLayoutBinding& layoutBinding) -> void
        {
            switch (binding.type)
            {
            case ResourceType::Texture_SRV:
            {
                const auto& texture = checked_cast<Texture*>(binding.resourceHandle);

                const auto subresource = binding.subresources.resolve(texture->desc, false);
                const auto textureViewType = getTextureViewType(binding.format, texture->desc.format);
                auto& view = texture->getSubresourceView(subresource, binding.dimension, binding.format, vk::ImageUsageFlagBits::eSampled, textureViewType);

                auto& imageInfo = descriptorImageInfo.emplace_back();
                imageInfo = vk::DescriptorImageInfo()
                    .setImageView(view.view)
                    .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

                generateWriteDescriptorData(layoutBinding.binding,
                    convertResourceType(binding.type),
                    &imageInfo, nullptr, nullptr);
            }
            break;

            case ResourceType::Texture_UAV:
            {
                const auto texture = checked_cast<Texture*>(binding.resourceHandle);

                const auto subresource = binding.subresources.resolve(texture->desc, true);
                const auto textureViewType = getTextureViewType(binding.format, texture->desc.format);
                auto& view = texture->getSubresourceView(subresource, binding.dimension, binding.format, vk::ImageUsageFlagBits::eStorage, textureViewType);

                auto& imageInfo = descriptorImageInfo.emplace_back();
                imageInfo = vk::DescriptorImageInfo()
                    .setImageView(view.view)
                    .setImageLayout(vk::ImageLayout::eGeneral);

                generateWriteDescriptorData(layoutBinding.binding,
                    convertResourceType(binding.type),
                    &imageInfo, nullptr, nullptr);
            }
            break;

            case ResourceType::TypedBuffer_SRV:
            case ResourceType::TypedBuffer_UAV:
            {
                const auto& buffer = checked_cast<Buffer*>(binding.resourceHandle);

                auto vkformat = nvrhi::vulkan::convertFormat(binding.format);

                const auto range = binding.range.resolve(buffer->desc);
                size_t viewInfoHash = 0;
                nvrhi::hash_combine(viewInfoHash, range.byteOffset);
                nvrhi::hash_combine(viewInfoHash, range.byteSize);
                nvrhi::hash_combine(viewInfoHash, (uint64_t)vkformat);

                const auto& bufferViewFound = buffer->viewCache.find(viewInfoHash);
                auto& bufferViewRef = (bufferViewFound != buffer->viewCache.end()) ? bufferViewFound->second : buffer->viewCache[viewInfoHash];
                if (bufferViewFound == buffer->viewCache.end())
                {
                    assert(binding.format != Format::UNKNOWN);

                    auto bufferViewInfo = vk::BufferViewCreateInfo()
                        .setBuffer(buffer->buffer)
                        .setOffset(range.byteOffset)
                        .setRange(range.byteSize)
                        .setFormat(vk::Format(vkformat));

                    vk::Result res = m_Context.device.createBufferView(&bufferViewInfo, m_Context.allocationCallbacks, &bufferViewRef);
                    ASSERT_VK_OK(res);
                }

                generateWriteDescriptorData(layoutBinding.binding,
                    convertResourceType(binding.type),
                    nullptr, nullptr, &bufferViewRef);
            }
            break;

            case ResourceType::StructuredBuffer_SRV:
            case ResourceType::StructuredBuffer_UAV:
            case ResourceType::RawBuffer_SRV:
            case ResourceType::RawBuffer_UAV:
            case ResourceType::ConstantBuffer:
            case ResourceType::VolatileConstantBuffer:
            {
                const auto buffer = checked_cast<Buffer*>(binding.resourceHandle);

                const auto range = binding.range.resolve(buffer->desc);

                auto& bufferInfo = descriptorBufferInfo.emplace_back();
                bufferInfo = vk::DescriptorBufferInfo()
                    .setBuffer(buffer->buffer)
                    .setOffset(range.byteOffset)
                    .setRange(range.byteSize);

                assert(buffer->buffer);
                generateWriteDescriptorData(layoutBinding.binding,
                    convertResourceType(binding.type),
                    nullptr, &bufferInfo, nullptr);
            }
            break;

            case ResourceType::Sampler:
            {
                const auto& sampler = checked_cast<Sampler*>(binding.resourceHandle);

                auto& imageInfo = descriptorImageInfo.emplace_back();
                imageInfo = vk::DescriptorImageInfo()
                    .setSampler(sampler->sampler);

                generateWriteDescriptorData(layoutBinding.binding,
                    convertResourceType(binding.type),
                    &imageInfo, nullptr, nullptr);
            }
            break;

            case ResourceType::RayTracingAccelStruct:
                utils::NotImplemented();
                break;

            case ResourceType::PushConstants:
                utils::NotSupported();
                break;

            case ResourceType::None:
            case ResourceType::Count:
            default:
                utils::InvalidEnum();
            }
        };

        if (layout->bindlessDesc.layoutType != BindlessLayoutDesc::LayoutType::Immutable)
        {
            // For mutable descriptor sets, there are no register spaces, so always use the first layout binding
            assert(layout->vulkanLayoutBindings.size() > 0);
            writeDescriptorForBinding(layout->vulkanLayoutBindings[0]);
        }
        else
        {
            // For regular bindless layouts, iterate through register spaces to find matching binding type
            for (uint32_t bindingLocation = 0; bindingLocation < uint32_t(layout->bindlessDesc.registerSpaces.size()); bindingLocation++)
            {
                if (layout->bindlessDesc.registerSpaces[bindingLocation].type == binding.type)
                {
                    const vk::DescriptorSetLayoutBinding& layoutBinding = layout->vulkanLayoutBindings[bindingLocation];
                    writeDescriptorForBinding(layoutBinding);
                }
            }
        }

        m_Context.device.updateDescriptorSets(uint32_t(descriptorWriteInfo.size()), descriptorWriteInfo.data(), 0, nullptr);

        return true;
    }

    void CommandList::bindBindingSets(vk::PipelineBindPoint bindPoint, vk::PipelineLayout pipelineLayout, const BindingSetVector& bindings, BindingVector<uint32_t> const& descriptorSetIdxToBindingIdx)
    {
        const uint32_t numBindings = (uint32_t)bindings.size();
        const uint32_t numDescriptorSets = descriptorSetIdxToBindingIdx.empty() ? numBindings : (uint32_t)descriptorSetIdxToBindingIdx.size();

        BindingVector<vk::DescriptorSet> descriptorSets;
        uint32_t nextDescriptorSetToBind = 0;
        static_vector<uint32_t, c_MaxVolatileConstantBuffers> dynamicOffsets;
        for (uint32_t i = 0; i < numDescriptorSets; ++i)
        {
            IBindingSet* bindingSetHandle = nullptr;
            if (descriptorSetIdxToBindingIdx.empty())
            {
                bindingSetHandle = bindings[i];
            }
            else if(descriptorSetIdxToBindingIdx[i] != 0xffffffff)
            {
                bindingSetHandle = bindings[descriptorSetIdxToBindingIdx[i]];
            }

            if (bindingSetHandle == nullptr)
            {
                // This is a hole in the descriptor sets, so bind the contiguous descriptor sets we've got so far
                if (!descriptorSets.empty())
                {
                    m_CurrentCmdBuf->cmdBuf.bindDescriptorSets(bindPoint, pipelineLayout,
                        /* firstSet = */ nextDescriptorSetToBind, uint32_t(descriptorSets.size()), descriptorSets.data(),
                        uint32_t(dynamicOffsets.size()), dynamicOffsets.data());

                    descriptorSets.resize(0);
                    dynamicOffsets.resize(0);
                }
                nextDescriptorSetToBind = i + 1;
            }
            else
            {
                const BindingSetDesc* desc = bindingSetHandle->getDesc();
                if (desc)
                {
                    BindingSet* bindingSet = checked_cast<BindingSet*>(bindingSetHandle);
                    descriptorSets.push_back(bindingSet->descriptorSet);

                    for (Buffer* constantBuffer : bindingSet->volatileConstantBuffers)
                    {
                        auto found = m_VolatileBufferStates.find(constantBuffer);
                        if (found == m_VolatileBufferStates.end())
                        {
                            std::stringstream ss;
                            ss << "Binding volatile constant buffer " << utils::DebugNameToString(constantBuffer->desc.debugName)
                                << " before writing into it is invalid.";
                            m_Context.error(ss.str());

                            dynamicOffsets.push_back(0); // use zero offset just to use something
                        }
                        else
                        {
                            uint32_t version = found->second.latestVersion;
                            uint64_t offset = version * constantBuffer->desc.byteSize;
                            assert(offset < std::numeric_limits<uint32_t>::max());
                            dynamicOffsets.push_back(uint32_t(offset));
                        }
                    }

                    if (desc->trackLiveness)
                        m_CurrentCmdBuf->referencedResources.push_back(bindingSetHandle);
                }
                else
                {
                    DescriptorTable* table = checked_cast<DescriptorTable*>(bindingSetHandle);
                    descriptorSets.push_back(table->descriptorSet);
                }
            }
        }
        if (!descriptorSets.empty())
        {
            // Bind the remaining sets
            m_CurrentCmdBuf->cmdBuf.bindDescriptorSets(bindPoint, pipelineLayout,
                /* firstSet = */ nextDescriptorSetToBind, uint32_t(descriptorSets.size()), descriptorSets.data(),
                uint32_t(dynamicOffsets.size()), dynamicOffsets.data());
        }
    }

    vk::Result createPipelineLayout(
        vk::PipelineLayout& outPipelineLayout,
        BindingVector<RefCountPtr<BindingLayout>>& outBindingLayouts,
        vk::ShaderStageFlags& outPushConstantVisibility,
        BindingVector<uint32_t>& outDescriptorSetIdxToBindingIdx,
        VulkanContext const& context,
        BindingLayoutVector const& inBindingLayouts)
    {
        // Establish if we're going to use outDescriptorSetIdxToBindingIdx
        // We do this if the layout descs specify registerSpaceIsDescriptorSet
        // (Validation ensures all the binding layouts have it set to the same value)
        bool createDescriptorSetIdxToBindingIdx = false;
        for (BindingLayoutHandle const& _layout : inBindingLayouts)
        {
            BindingLayout const* layout = checked_cast<BindingLayout const*>(_layout.Get());
            if (!layout->isBindless)
            {
                createDescriptorSetIdxToBindingIdx = layout->getDesc()->registerSpaceIsDescriptorSet;
                break;
            }
        }

        if (createDescriptorSetIdxToBindingIdx)
        {
            // Figure out how many descriptor sets we'll need in outBindingLayouts.
            // There's not necessarily a one-to-one relationship because there could potentially be
            // holes in binding layout.  E.g. if a binding layout uses register spaces 0 and 2
            // then we'll need to use 3 descriptor sets, with a hole at index 1 because Vulkan
            // descriptor set indices map to register spaces.
            // Bindless layouts are assumed to not need binding to specific descriptor set
            // indices, so we put those last
            uint32_t numRegularDescriptorSets = 0;
            for (BindingLayoutHandle const& _layout : inBindingLayouts)
            {
                BindingLayout const* layout = checked_cast<BindingLayout const*>(_layout.Get());
                if (!layout->isBindless)
                {
                    numRegularDescriptorSets = std::max(numRegularDescriptorSets, layout->getDesc()->registerSpace + 1);
                }
            }

            // Now create the layout
            outBindingLayouts.resize(numRegularDescriptorSets);
            outDescriptorSetIdxToBindingIdx.resize(numRegularDescriptorSets);
            for (uint32_t i = 0; i < numRegularDescriptorSets; ++i)
            {
                outDescriptorSetIdxToBindingIdx[i] = 0xffffffff;
            }
            for (uint32_t i = 0; i < (uint32_t)inBindingLayouts.size(); ++i)
            {
                BindingLayout* layout = checked_cast<BindingLayout*>(inBindingLayouts[i].Get());
                if (layout->isBindless)
                {
                    outBindingLayouts.push_back(layout);
                    // Let's always put the bindless ones at the end.
                    outDescriptorSetIdxToBindingIdx.push_back(i);
                }
                else
                {
                    uint32_t const descriptorSetIdx = layout->getDesc()->registerSpace;
                    // Can't have multiple binding sets with the same registerSpace
                    // Should not have passed validation in validatePipelineBindingLayouts
                    assert(outBindingLayouts[descriptorSetIdx] == nullptr);
                    outBindingLayouts[descriptorSetIdx] = layout;
                    outDescriptorSetIdxToBindingIdx[descriptorSetIdx] = i;
                }
            }
        }
        else
        {
            // Legacy behaviour mode, where we don't fill in outDescriptorSetIdxToBindingIdx
            // In this mode, there can be no holes in the binding layout
            for (const BindingLayoutHandle& _layout : inBindingLayouts)
            {
                BindingLayout* layout = checked_cast<BindingLayout*>(_layout.Get());
                outBindingLayouts.push_back(layout);
            }
        }

        BindingVector<vk::DescriptorSetLayout> descriptorSetLayouts;
        uint32_t pushConstantSize = 0;
        outPushConstantVisibility = vk::ShaderStageFlagBits();
        for (BindingLayout const* layout : outBindingLayouts)
        {
            if (layout)
            {
                descriptorSetLayouts.push_back(layout->descriptorSetLayout);

                if (!layout->isBindless)
                {
                    for (const BindingLayoutItem& item : layout->desc.bindings)
                    {
                        if (item.type == ResourceType::PushConstants)
                        {
                            pushConstantSize = item.size;
                            outPushConstantVisibility = convertShaderTypeToShaderStageFlagBits(layout->desc.visibility);
                            // assume there's only one push constant item in all layouts -- the validation layer makes sure of that
                            break;
                        }
                    }
                }
            }
            else
            {
                // Empty descriptor set
                descriptorSetLayouts.push_back(context.emptyDescriptorSetLayout);
            }
        }

        auto pushConstantRange = vk::PushConstantRange()
            .setOffset(0)
            .setSize(pushConstantSize)
            .setStageFlags(outPushConstantVisibility);

        auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo()
            .setSetLayoutCount(uint32_t(descriptorSetLayouts.size()))
            .setPSetLayouts(descriptorSetLayouts.data())
            .setPushConstantRangeCount(pushConstantSize ? 1 : 0)
            .setPPushConstantRanges(&pushConstantRange);

        vk::Result res = context.device.createPipelineLayout(&pipelineLayoutInfo,
            context.allocationCallbacks,
            &outPipelineLayout);

        return res;
    }

} // namespace nvrhi::vulkan
