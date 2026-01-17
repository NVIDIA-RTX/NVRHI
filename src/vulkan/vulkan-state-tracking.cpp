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

namespace nvrhi::vulkan
{
    
    void CommandList::setResourceStatesForBindingSet(IBindingSet* _bindingSet)
    {
        if (_bindingSet == nullptr)
            return;
        if (_bindingSet->getDesc() == nullptr)
            return; // is bindless

        BindingSet* bindingSet = checked_cast<BindingSet*>(_bindingSet);

        for (auto bindingIndex : bindingSet->bindingsThatNeedTransitions)
        {
            const BindingSetItem& binding = bindingSet->desc.bindings[bindingIndex];

            switch(binding.type)  // NOLINT(clang-diagnostic-switch-enum)
            {
                case ResourceType::Texture_SRV:
                    requireTextureState(checked_cast<ITexture*>(binding.resourceHandle), binding.subresources, ResourceStates::ShaderResource);
                    break;

                case ResourceType::Texture_UAV:
                    requireTextureState(checked_cast<ITexture*>(binding.resourceHandle), binding.subresources, ResourceStates::UnorderedAccess);
                    break;

                case ResourceType::TypedBuffer_SRV:
                case ResourceType::StructuredBuffer_SRV:
                case ResourceType::RawBuffer_SRV:
                    requireBufferState(checked_cast<IBuffer*>(binding.resourceHandle), ResourceStates::ShaderResource);
                    break;

                case ResourceType::TypedBuffer_UAV:
                case ResourceType::StructuredBuffer_UAV:
                case ResourceType::RawBuffer_UAV:
                    requireBufferState(checked_cast<IBuffer*>(binding.resourceHandle), ResourceStates::UnorderedAccess);
                    break;

                case ResourceType::ConstantBuffer:
                    requireBufferState(checked_cast<IBuffer*>(binding.resourceHandle), ResourceStates::ConstantBuffer);
                    break;

                case ResourceType::RayTracingAccelStruct:
                    requireBufferState(checked_cast<AccelStruct*>(binding.resourceHandle)->dataBuffer, ResourceStates::AccelStructRead);

                default:
                    // do nothing
                    break;
            }
        }
    }

    void CommandList::insertResourceBarriersForBindingSets(const BindingSetVector& newBindings, const BindingSetVector& oldBindings)
    {
        uint32_t bindingUpdateMask = 0;

        if (m_BindingStatesDirty)
            bindingUpdateMask = ~0u;

        if (bindingUpdateMask == 0)
            bindingUpdateMask = arrayDifferenceMask(newBindings, oldBindings);

        if (bindingUpdateMask != 0)
        {
            for (size_t i = 0; i < newBindings.size(); i++)
            {
                if (newBindings[i]->getDesc() == nullptr) // Ignore bindless sets
                    continue;

                BindingSet const* bindingSet = checked_cast<BindingSet const*>(newBindings[i]);

                bool const updateThisSet = (bindingUpdateMask & (1u << i)) != 0;
                if (updateThisSet || bindingSet->hasUavBindings) // UAV bindings may place UAV barriers on the same binding set
                    setResourceStatesForBindingSet(newBindings[i]);
            }
        }
    }

    void CommandList::insertGraphicsResourceBarriers(const GraphicsState& state)
    {
        insertResourceBarriersForBindingSets(state.bindings, m_CurrentGraphicsState.bindings);

        if (state.indexBuffer.buffer && (m_BindingStatesDirty || state.indexBuffer.buffer != m_CurrentGraphicsState.indexBuffer.buffer))
        {
            requireBufferState(state.indexBuffer.buffer, ResourceStates::IndexBuffer);
        }

        if (m_BindingStatesDirty || arraysAreDifferent(state.vertexBuffers, m_CurrentGraphicsState.vertexBuffers))
        {
            for (const auto& vb : state.vertexBuffers)
            {
                requireBufferState(vb.buffer, ResourceStates::VertexBuffer);
            }
        }

        if (m_BindingStatesDirty || m_CurrentGraphicsState.framebuffer != state.framebuffer)
        {
            setResourceStatesForFramebuffer(state.framebuffer);
        }

        if (state.indirectParams && (m_BindingStatesDirty || state.indirectParams != m_CurrentGraphicsState.indirectParams))
        {
            requireBufferState(state.indirectParams, ResourceStates::IndirectArgument);
        }

        m_BindingStatesDirty = false;
    }

    void CommandList::insertComputeResourceBarriers(const ComputeState& state)
    {
        insertResourceBarriersForBindingSets(state.bindings, m_CurrentComputeState.bindings);

        if (state.indirectParams && (m_BindingStatesDirty || state.indirectParams != m_CurrentComputeState.indirectParams))
        {
            Buffer* indirectParams = checked_cast<Buffer*>(state.indirectParams);

            requireBufferState(indirectParams, ResourceStates::IndirectArgument);
        }

        m_BindingStatesDirty = false;
    }

    void CommandList::insertMeshletResourceBarriers(const MeshletState& state)
    {
        insertResourceBarriersForBindingSets(state.bindings, m_CurrentMeshletState.bindings);

        if (m_BindingStatesDirty || m_CurrentMeshletState.framebuffer != state.framebuffer)
        {
            setResourceStatesForFramebuffer(state.framebuffer);
        }

        if (state.indirectParams && (m_BindingStatesDirty || state.indirectParams != m_CurrentMeshletState.indirectParams))
        {
            requireBufferState(state.indirectParams, ResourceStates::IndirectArgument);
        }

        m_BindingStatesDirty = false;
    }

    void CommandList::insertRayTracingResourceBarriers(const rt::State& state)
    {
        insertResourceBarriersForBindingSets(state.bindings, m_CurrentRayTracingState.bindings);

        m_BindingStatesDirty = false;
    }

    void CommandList::requireTextureState(ITexture* _texture, TextureSubresourceSet subresources, ResourceStates state)
    {
        Texture* texture = checked_cast<Texture*>(_texture);

        m_StateTracker.requireTextureState(texture, subresources, state);
    }

    void CommandList::requireBufferState(IBuffer* _buffer, ResourceStates state)
    {
        Buffer* buffer = checked_cast<Buffer*>(_buffer);

        m_StateTracker.requireBufferState(buffer, state);
    }

    bool CommandList::anyBarriers() const
    {
        return !m_StateTracker.getBufferBarriers().empty() || !m_StateTracker.getTextureBarriers().empty();
    }

    void CommandList::commitBarriersInternal()
    {
        std::vector<vk::ImageMemoryBarrier2> imageBarriers;
        std::vector<vk::BufferMemoryBarrier2> bufferBarriers;

        for (const TextureBarrier& barrier : m_StateTracker.getTextureBarriers())
        {
            ResourceStateMapping before = convertResourceState(barrier.stateBefore, true);
            ResourceStateMapping after = convertResourceState(barrier.stateAfter, true);

            assert(after.imageLayout != vk::ImageLayout::eUndefined);

            Texture* texture = static_cast<Texture*>(barrier.texture);

            const FormatInfo& formatInfo = getFormatInfo(texture->desc.format);

            vk::ImageAspectFlags aspectMask = (vk::ImageAspectFlagBits)0;
            if (formatInfo.hasDepth) aspectMask |= vk::ImageAspectFlagBits::eDepth;
            if (formatInfo.hasStencil) aspectMask |= vk::ImageAspectFlagBits::eStencil;
            if (!aspectMask) aspectMask = vk::ImageAspectFlagBits::eColor;

            vk::ImageSubresourceRange subresourceRange = vk::ImageSubresourceRange()
                .setBaseArrayLayer(barrier.entireTexture ? 0 : barrier.arraySlice)
                .setLayerCount(barrier.entireTexture ? texture->desc.arraySize : 1)
                .setBaseMipLevel(barrier.entireTexture ? 0 : barrier.mipLevel)
                .setLevelCount(barrier.entireTexture ? texture->desc.mipLevels : 1)
                .setAspectMask(aspectMask);

            imageBarriers.push_back(vk::ImageMemoryBarrier2()
                .setSrcAccessMask(before.accessMask)
                .setDstAccessMask(after.accessMask)
                .setSrcStageMask(before.stageFlags)
                .setDstStageMask(after.stageFlags)
                .setOldLayout(before.imageLayout)
                .setNewLayout(after.imageLayout)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setImage(texture->image)
                .setSubresourceRange(subresourceRange));
        }

        if (!imageBarriers.empty())
        {
            vk::DependencyInfo dep_info;
            dep_info.setImageMemoryBarriers(imageBarriers);

            m_CurrentCmdBuf->cmdBuf.pipelineBarrier2(dep_info);
        }

        imageBarriers.clear();

        for (const BufferBarrier& barrier : m_StateTracker.getBufferBarriers())
        {
            ResourceStateMapping before = convertResourceState(barrier.stateBefore, false);
            ResourceStateMapping after = convertResourceState(barrier.stateAfter, false);

            Buffer* buffer = static_cast<Buffer*>(barrier.buffer);

            bufferBarriers.push_back(vk::BufferMemoryBarrier2()
                .setSrcAccessMask(before.accessMask)
                .setDstAccessMask(after.accessMask)
                .setSrcStageMask(before.stageFlags)
                .setDstStageMask(after.stageFlags)
                .setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED)
                .setBuffer(buffer->buffer)
                .setOffset(0)
                .setSize(buffer->desc.byteSize));
        }

        if (!bufferBarriers.empty())
        {
            vk::DependencyInfo dep_info;
            dep_info.setBufferMemoryBarriers(bufferBarriers);

            m_CurrentCmdBuf->cmdBuf.pipelineBarrier2(dep_info);
        }
        bufferBarriers.clear();

        m_StateTracker.clearBarriers();
    }

    void CommandList::commitBarriers()
    {
        if (m_StateTracker.getBufferBarriers().empty() && m_StateTracker.getTextureBarriers().empty())
            return;

        endRenderPass();

        commitBarriersInternal();
    }

    void CommandList::beginTrackingTextureState(ITexture* _texture, TextureSubresourceSet subresources, ResourceStates stateBits)
    {
        Texture* texture = checked_cast<Texture*>(_texture);

        m_StateTracker.beginTrackingTextureState(texture, subresources, stateBits);
    }

    void CommandList::beginTrackingBufferState(IBuffer* _buffer, ResourceStates stateBits)
    {
        Buffer* buffer = checked_cast<Buffer*>(_buffer);

        m_StateTracker.beginTrackingBufferState(buffer, stateBits);
    }

    void CommandList::setTextureState(ITexture* _texture, TextureSubresourceSet subresources, ResourceStates stateBits)
    {
        Texture* texture = checked_cast<Texture*>(_texture);

        m_StateTracker.requireTextureState(texture, subresources, stateBits);

        if (m_CurrentCmdBuf)
            m_CurrentCmdBuf->referencedResources.push_back(texture);
    }

    void CommandList::setBufferState(IBuffer* _buffer, ResourceStates stateBits)
    {
        Buffer* buffer = checked_cast<Buffer*>(_buffer);

        m_StateTracker.requireBufferState(buffer, stateBits);
        
        if (m_CurrentCmdBuf)
            m_CurrentCmdBuf->referencedResources.push_back(buffer);
    }
    
    void CommandList::setAccelStructState(rt::IAccelStruct* _as, ResourceStates stateBits)
    {
        AccelStruct* as = checked_cast<AccelStruct*>(_as);

        if (as->dataBuffer)
        {
            Buffer* buffer = checked_cast<Buffer*>(as->dataBuffer.Get());
            m_StateTracker.requireBufferState(buffer, stateBits);

            if (m_CurrentCmdBuf)
                m_CurrentCmdBuf->referencedResources.push_back(as);
        }
    }

    void CommandList::setPermanentTextureState(ITexture* _texture, ResourceStates stateBits)
    {
        Texture* texture = checked_cast<Texture*>(_texture);

        m_StateTracker.setPermanentTextureState(texture, AllSubresources, stateBits);

        if (m_CurrentCmdBuf)
            m_CurrentCmdBuf->referencedResources.push_back(texture);
    }

    void CommandList::setPermanentBufferState(IBuffer* _buffer, ResourceStates stateBits)
    {
        Buffer* buffer = checked_cast<Buffer*>(_buffer);

        m_StateTracker.setPermanentBufferState(buffer, stateBits);
        
        if (m_CurrentCmdBuf)
            m_CurrentCmdBuf->referencedResources.push_back(buffer);
    }

    ResourceStates CommandList::getTextureSubresourceState(ITexture* _texture, ArraySlice arraySlice, MipLevel mipLevel)
    {
        Texture* texture = checked_cast<Texture*>(_texture);

        return m_StateTracker.getTextureSubresourceState(texture, arraySlice, mipLevel);
    }

    ResourceStates CommandList::getBufferState(IBuffer* _buffer)
    {
        Buffer* buffer = checked_cast<Buffer*>(_buffer);

        return m_StateTracker.getBufferState(buffer);
    }

    void CommandList::setEnableAutomaticBarriers(bool enable)
    {
        m_EnableAutomaticBarriers = enable;
    }

    void CommandList::setEnableUavBarriersForTexture(ITexture* _texture, bool enableBarriers)
    {
        Texture* texture = checked_cast<Texture*>(_texture);

        m_StateTracker.setEnableUavBarriersForTexture(texture, enableBarriers);
    }

    void CommandList::setEnableUavBarriersForBuffer(IBuffer* _buffer, bool enableBarriers)
    {
        Buffer* buffer = checked_cast<Buffer*>(_buffer);

        m_StateTracker.setEnableUavBarriersForBuffer(buffer, enableBarriers);
    }

} // namespace nvrhi::vulkan
