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
    MeshletPipelineHandle Device::createMeshletPipeline(const MeshletPipelineDesc& desc, FramebufferInfo const& fbinfo)
    {
        if (!m_Context.extensions.EXT_mesh_shader)
        {
            utils::NotSupported();
            return nullptr;
        }

        vk::Result res;

        MeshletPipeline *pso = new MeshletPipeline(m_Context);
        pso->desc = desc;
        pso->framebufferInfo = fbinfo;

        Shader* AS = checked_cast<Shader*>(desc.AS.Get());
        Shader* MS = checked_cast<Shader*>(desc.MS.Get());
        Shader* PS = checked_cast<Shader*>(desc.PS.Get());

        size_t numShaders = 0;
        size_t numShadersWithSpecializations = 0;
        size_t numSpecializationConstants = 0;

        // Count the spec constants for all stages
        countSpecializationConstants(AS, numShaders, numShadersWithSpecializations, numSpecializationConstants);
        countSpecializationConstants(MS, numShaders, numShadersWithSpecializations, numSpecializationConstants);
        countSpecializationConstants(PS, numShaders, numShadersWithSpecializations, numSpecializationConstants);

        std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
        std::vector<vk::SpecializationInfo> specInfos;
        std::vector<vk::SpecializationMapEntry> specMapEntries;
        std::vector<uint32_t> specData;

        // Allocate buffers for specialization constants and related structures
        // so that shaderStageCreateInfo(...) can directly use pointers inside the vectors
        // because the vectors won't reallocate their buffers
        shaderStages.reserve(numShaders);
        specInfos.reserve(numShadersWithSpecializations);
        specMapEntries.reserve(numSpecializationConstants);
        specData.reserve(numSpecializationConstants);

        // Set up shader stages
        if (desc.AS)
        {
            shaderStages.push_back(makeShaderStageCreateInfo(AS, 
                specInfos, specMapEntries, specData));
            pso->shaderMask = pso->shaderMask | ShaderType::Vertex;
        }

        if (desc.MS)
        {
            shaderStages.push_back(makeShaderStageCreateInfo(MS, 
                specInfos, specMapEntries, specData));
            pso->shaderMask = pso->shaderMask | ShaderType::Hull;
        }
        
        if (desc.PS)
        {
            shaderStages.push_back(makeShaderStageCreateInfo(PS, 
                specInfos, specMapEntries, specData));
            pso->shaderMask = pso->shaderMask | ShaderType::Pixel;
        }

        auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo()
            .setTopology(convertPrimitiveTopology(desc.primType));
        
        // fixed function state
        const auto& rasterState = desc.renderState.rasterState;
        const auto& depthStencilState = desc.renderState.depthStencilState;
        const auto& blendState = desc.renderState.blendState;

        auto viewportState = vk::PipelineViewportStateCreateInfo()
            .setViewportCount(1)
            .setScissorCount(1);

        auto rasterizer = vk::PipelineRasterizationStateCreateInfo()
                            // .setDepthClampEnable(??)
                            // .setRasterizerDiscardEnable(??)
                            .setPolygonMode(convertFillMode(rasterState.fillMode))
                            .setCullMode(convertCullMode(rasterState.cullMode))
                            .setFrontFace(rasterState.frontCounterClockwise ?
                                            vk::FrontFace::eCounterClockwise : vk::FrontFace::eClockwise)
                            .setDepthBiasEnable(rasterState.depthBias ? true : false)
                            .setDepthBiasConstantFactor(float(rasterState.depthBias))
                            .setDepthBiasClamp(rasterState.depthBiasClamp)
                            .setDepthBiasSlopeFactor(rasterState.slopeScaledDepthBias)
                            .setLineWidth(1.0f);
        
        auto multisample = vk::PipelineMultisampleStateCreateInfo()
                            .setRasterizationSamples(vk::SampleCountFlagBits(fbinfo.sampleCount))
                            .setAlphaToCoverageEnable(blendState.alphaToCoverageEnable);

        auto depthStencil = vk::PipelineDepthStencilStateCreateInfo()
                                .setDepthTestEnable(depthStencilState.depthTestEnable)
                                .setDepthWriteEnable(depthStencilState.depthWriteEnable)
                                .setDepthCompareOp(convertCompareOp(depthStencilState.depthFunc))
                                .setStencilTestEnable(depthStencilState.stencilEnable)
                                .setFront(convertStencilState(depthStencilState, depthStencilState.frontFaceStencil))
                                .setBack(convertStencilState(depthStencilState, depthStencilState.backFaceStencil));

        res = createPipelineLayout(
            pso->pipelineLayout,
            pso->pipelineBindingLayouts,
            pso->pushConstantVisibility,
            pso->descriptorSetIdxToBindingIdx,
            m_Context,
            desc.bindingLayouts);
        CHECK_VK_FAIL(res)

        static_vector<vk::PipelineColorBlendAttachmentState, c_MaxRenderTargets> colorBlendAttachments(fbinfo.colorFormats.size());

        for(uint32_t i = 0; i < uint32_t(fbinfo.colorFormats.size()); i++)
        {
            colorBlendAttachments[i] = convertBlendState(blendState.targets[i]);
        }

        auto colorBlend = vk::PipelineColorBlendStateCreateInfo()
                            .setAttachmentCount(uint32_t(colorBlendAttachments.size()))
                            .setPAttachments(colorBlendAttachments.data());

        pso->usesBlendConstants = blendState.usesConstantColor(uint32_t(fbinfo.colorFormats.size()));
        
        static_vector<vk::DynamicState, 4> dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor
        };
        if (pso->usesBlendConstants)
            dynamicStates.push_back(vk::DynamicState::eBlendConstants);
        if (pso->desc.renderState.depthStencilState.dynamicStencilRef)
            dynamicStates.push_back(vk::DynamicState::eStencilReference);

        auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo()
            .setDynamicStateCount(uint32_t(dynamicStates.size()))
            .setPDynamicStates(dynamicStates.data());
            
        std::array<vk::Format, c_MaxRenderTargets> colorFormats;
        for (size_t i = 0; i < fbinfo.colorFormats.size(); i++)
            colorFormats[i] = vk::Format(convertFormat(fbinfo.colorFormats[i]));

        FormatInfo const& depthStencilFormatInfo = getFormatInfo(fbinfo.depthFormat);
        vk::Format depthStencilFormat = vk::Format(convertFormat(fbinfo.depthFormat));

        auto renderingInfo = vk::PipelineRenderingCreateInfo()
            .setColorAttachmentCount(uint32_t(fbinfo.colorFormats.size()))
            .setPColorAttachmentFormats(colorFormats.data())
            .setDepthAttachmentFormat(depthStencilFormatInfo.hasDepth ? depthStencilFormat : vk::Format::eUndefined)
            .setStencilAttachmentFormat(depthStencilFormatInfo.hasStencil ? depthStencilFormat : vk::Format::eUndefined);

        auto pipelineInfo = vk::GraphicsPipelineCreateInfo()
            .setPNext(&renderingInfo)
            .setStageCount(uint32_t(shaderStages.size()))
            .setPStages(shaderStages.data())
            .setPInputAssemblyState(&inputAssembly)
            .setPViewportState(&viewportState)
            .setPRasterizationState(&rasterizer)
            .setPMultisampleState(&multisample)
            .setPDepthStencilState(&depthStencil)
            .setPColorBlendState(&colorBlend)
            .setPDynamicState(&dynamicStateInfo)
            .setLayout(pso->pipelineLayout)
            .setBasePipelineHandle(nullptr)
            .setBasePipelineIndex(-1);

        res = m_Context.device.createGraphicsPipelines(m_Context.pipelineCache,
                                                     1, &pipelineInfo,
                                                     m_Context.allocationCallbacks,
                                                     &pso->pipeline);

        ASSERT_VK_OK(res); // for debugging
        CHECK_VK_FAIL(res)
        
        return MeshletPipelineHandle::Create(pso);
    }

    MeshletPipelineHandle Device::createMeshletPipeline(const MeshletPipelineDesc& desc, IFramebuffer* fb)
    {
        if (!fb)
            return nullptr;
            
        return createMeshletPipeline(desc, fb->getFramebufferInfo());
    }

    MeshletPipeline::~MeshletPipeline()
    {
        if (pipeline)
        {
            m_Context.device.destroyPipeline(pipeline, m_Context.allocationCallbacks);
            pipeline = nullptr;
        }

        if (pipelineLayout)
        {
            m_Context.device.destroyPipelineLayout(pipelineLayout, m_Context.allocationCallbacks);
            pipelineLayout = nullptr;
        }
    }

    Object MeshletPipeline::getNativeObject(ObjectType objectType)
    {
        switch (objectType)
        {
        case ObjectTypes::VK_PipelineLayout:
            return Object(pipelineLayout);
        case ObjectTypes::VK_Pipeline:
            return Object(pipeline);
        default:
            return nullptr;
        }
    }

    static vk::Viewport VKViewportWithDXCoords(const Viewport& v)
    {
        return vk::Viewport(v.minX, v.maxY, v.maxX - v.minX, -(v.maxY - v.minY), v.minZ, v.maxZ);
    }

    void CommandList::setMeshletState(const MeshletState& state)
    {
        assert(m_CurrentCmdBuf);

        MeshletPipeline* pso = checked_cast<MeshletPipeline*>(state.pipeline);
        Framebuffer* fb = checked_cast<Framebuffer*>(state.framebuffer);

        if (m_EnableAutomaticBarriers)
        {
            insertMeshletResourceBarriers(state);
        }

        bool anyBarriers = this->anyBarriers();
        bool updatePipeline = false;

        if (m_CurrentMeshletState.pipeline != state.pipeline)
        {
            m_CurrentCmdBuf->cmdBuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pso->pipeline);

            m_CurrentCmdBuf->referencedResources.push_back(state.pipeline);
            updatePipeline = true;
        }

        if (m_CurrentMeshletState.framebuffer != state.framebuffer || anyBarriers /* because barriers cannot be set inside a renderpass */)
        {
            endRenderPass();
        }

        commitBarriers();

        if(!m_CurrentMeshletState.framebuffer)
        {
            beginRenderPass(fb);
        }

        m_CurrentPipelineLayout = pso->pipelineLayout;
        m_CurrentPushConstantsVisibility = pso->pushConstantVisibility;

        if (arraysAreDifferent(m_CurrentComputeState.bindings, state.bindings) || m_AnyVolatileBufferWrites)
        {
            bindBindingSets(vk::PipelineBindPoint::eGraphics, pso->pipelineLayout, state.bindings, pso->descriptorSetIdxToBindingIdx);
        }

        if (!state.viewport.viewports.empty() && arraysAreDifferent(state.viewport.viewports, m_CurrentMeshletState.viewport.viewports))
        {
            nvrhi::static_vector<vk::Viewport, c_MaxViewports> viewports;
            for (const auto& vp : state.viewport.viewports)
            {
                viewports.push_back(VKViewportWithDXCoords(vp));
            }

            m_CurrentCmdBuf->cmdBuf.setViewport(0, uint32_t(viewports.size()), viewports.data());
        }

        if (!state.viewport.scissorRects.empty() && arraysAreDifferent(state.viewport.scissorRects, m_CurrentMeshletState.viewport.scissorRects))
        {
            nvrhi::static_vector<vk::Rect2D, c_MaxViewports> scissors;
            for (const auto& sc : state.viewport.scissorRects)
            {
                scissors.push_back(vk::Rect2D(vk::Offset2D(sc.minX, sc.minY),
                    vk::Extent2D(std::abs(sc.maxX - sc.minX), std::abs(sc.maxY - sc.minY))));
            }

            m_CurrentCmdBuf->cmdBuf.setScissor(0, uint32_t(scissors.size()), scissors.data());
        }
        
        if (pso->desc.renderState.depthStencilState.dynamicStencilRef && (updatePipeline || m_CurrentMeshletState.dynamicStencilRefValue != state.dynamicStencilRefValue))
        {
            m_CurrentCmdBuf->cmdBuf.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, state.dynamicStencilRefValue);
        }

        if (pso->usesBlendConstants && (updatePipeline || m_CurrentMeshletState.blendConstantColor != state.blendConstantColor))
        {
            m_CurrentCmdBuf->cmdBuf.setBlendConstants(&state.blendConstantColor.r);
        }

        if (state.indirectParams)
        {
            m_CurrentCmdBuf->referencedResources.push_back(state.indirectParams);
        }

        m_CurrentComputeState = ComputeState();
        m_CurrentGraphicsState = GraphicsState();
        m_CurrentMeshletState = state;
        m_CurrentRayTracingState = rt::State();
        m_AnyVolatileBufferWrites = false;
    }

    void CommandList::updateMeshletVolatileBuffers()
    {
        if (m_AnyVolatileBufferWrites && m_CurrentMeshletState.pipeline)
        {
            MeshletPipeline* pso = checked_cast<MeshletPipeline*>(m_CurrentMeshletState.pipeline);

            bindBindingSets(vk::PipelineBindPoint::eGraphics, pso->pipelineLayout, m_CurrentMeshletState.bindings, pso->descriptorSetIdxToBindingIdx);

            m_AnyVolatileBufferWrites = false;
        }
    }

    void CommandList::dispatchMesh(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        assert(m_CurrentCmdBuf);

        updateMeshletVolatileBuffers();

        m_CurrentCmdBuf->cmdBuf.drawMeshTasksEXT(groupsX, groupsY, groupsZ);
    }

} // namespace nvrhi::vulkan
