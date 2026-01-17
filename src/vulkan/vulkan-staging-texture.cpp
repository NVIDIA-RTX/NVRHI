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

    extern vk::ImageAspectFlags guessImageAspectFlags(vk::Format format);

    static size_t alignBufferOffset(size_t off)
    {
        static constexpr size_t bufferAlignmentBytes = 4;
        return ((off + (bufferAlignmentBytes - 1)) / bufferAlignmentBytes) * bufferAlignmentBytes;
    }

    static size_t computePlacedBufferOffset(
        const PlacedSubresourceFootprint& footprint, uint32_t x, uint32_t y, uint32_t z) {
        auto formatInfo = getFormatInfo(footprint.format);

        uint32_t blockX = x / formatInfo.blockSize;
        uint32_t blockY = y / formatInfo.blockSize;
        uint32_t blockZ = z;

        return footprint.offset +
               (blockX + (blockY + blockZ * footprint.numRows) * footprint.rowPitch);
    }

    size_t StagingTexture::computeCopyableFootprints() {
        uint32_t width = desc.width;
        uint32_t height = desc.height;
        uint32_t depth = desc.depth;
        uint32_t numMips = desc.mipLevels;
        uint32_t arraySize = desc.arraySize;

        if (desc.dimension == TextureDimension::Texture3D) {
            assert(desc.arraySize == 1);
            arraySize = 1;
        } else {
            assert(desc.depth == 1);
            depth = 1;
        }

        const FormatInfo& formatInfo = getFormatInfo(desc.format);

        size_t offset = 0;

        for (uint32_t mipSlice = 0; mipSlice < numMips; ++mipSlice) {
            uint32_t wInBlocks =
                std::max((width + formatInfo.blockSize - 1) / formatInfo.blockSize, 1u);
            uint32_t hInBlocks =
                std::max((height + formatInfo.blockSize - 1) / formatInfo.blockSize, 1u);

            PlacedSubresourceFootprint layout;
            layout.rowSizeInBytes = wInBlocks * formatInfo.bytesPerBlock;
            layout.numRows = hInBlocks;
            layout.totalBytes = size_t(depth) * hInBlocks * layout.rowSizeInBytes;
            layout.format = desc.format;
            layout.width = width;
            layout.height = height;
            layout.depth = depth;
            layout.rowPitch = layout.rowSizeInBytes;

            for (uint32_t arraySlice = 0; arraySlice < arraySize; ++arraySlice) {
                layout.offset = alignBufferOffset(offset);
                offset += layout.totalBytes;
                placedFootprints.push_back(layout);
            }

            width = std::max(width >> 1u, 1u);
            height = std::max(height >> 1u, 1u);
            depth = std::max(depth >> 1u, 1u);
        }

        return offset; // total size in bytes of upload buffer
    }

    const PlacedSubresourceFootprint& StagingTexture::getCopyableFootprint(
        uint32_t mipLevel, uint32_t arraySlice) {
        uint32_t arraySize = desc.arraySize;

        uint32_t subresourceIndex = mipLevel * arraySize + arraySlice;
        return placedFootprints[subresourceIndex];
    }

    StagingTextureHandle Device::createStagingTexture(const TextureDesc& desc, CpuAccessMode cpuAccess)
    {
        assert(cpuAccess != CpuAccessMode::None);

        StagingTexture *tex = new StagingTexture();
        tex->desc = desc;

        size_t totalSizeInBytes = tex->computeCopyableFootprints();

        BufferDesc bufDesc;
        bufDesc.byteSize = totalSizeInBytes;
        assert(bufDesc.byteSize > 0);
        bufDesc.debugName = desc.debugName;
        bufDesc.cpuAccess = cpuAccess;

        BufferHandle internalBuffer = createBuffer(bufDesc);
        tex->buffer = checked_cast<Buffer*>(internalBuffer.Get());

        if (!tex->buffer)
        {
            delete tex;
            return nullptr;
        }

        return StagingTextureHandle::Create(tex);
    }

    void *Device::mapStagingTexture(IStagingTexture* _tex, const TextureSlice& slice, CpuAccessMode cpuAccess, size_t *outRowPitch)
    {
        assert(slice.x == 0);
        assert(slice.y == 0);
        assert(cpuAccess != CpuAccessMode::None);

        StagingTexture* tex = checked_cast<StagingTexture*>(_tex);

        auto resolvedSlice = slice.resolve(tex->desc);

        auto layout =
            tex->getCopyableFootprint(resolvedSlice.mipLevel, resolvedSlice.arraySlice);
        assert((layout.offset & 0x3) == 0); // per vulkan spec
        assert(layout.totalBytes > 0);

        *outRowPitch = layout.rowPitch;

        return mapBuffer(tex->buffer, cpuAccess, layout.offset, layout.totalBytes);
    }

    void Device::unmapStagingTexture(IStagingTexture* _tex)
    {
        StagingTexture* tex = checked_cast<StagingTexture*>(_tex);

        unmapBuffer(tex->buffer);
    }

    void CommandList::copyTexture(IStagingTexture* _dst, const TextureSlice& dstSlice, ITexture* _src, const TextureSlice& srcSlice)
    {
        Texture* src = checked_cast<Texture*>(_src);
        StagingTexture* dst = checked_cast<StagingTexture*>(_dst);

        auto &resolvedDstSlice = dstSlice;
        auto resolvedSrcSlice = srcSlice.resolve(src->desc);

        assert(resolvedDstSlice.depth == 1);
        
        auto dstFootprint = dst->getCopyableFootprint(resolvedDstSlice.mipLevel, resolvedDstSlice.arraySlice);
        size_t dstBufferOffset =
            computePlacedBufferOffset(dstFootprint, dstSlice.x, dstSlice.y, dstSlice.z);
        assert((dstBufferOffset & 0x3) == 0);  // per Vulkan spec

        TextureSubresourceSet srcSubresource = TextureSubresourceSet(
            resolvedSrcSlice.mipLevel, 1,
            resolvedSrcSlice.arraySlice, 1
        );

        auto imageCopy =
            vk::BufferImageCopy()
                .setBufferOffset(dstBufferOffset)
                .setBufferRowLength(dstFootprint.width)
                .setBufferImageHeight(dstFootprint.height)
                .setImageSubresource(
                    vk::ImageSubresourceLayers()
                        .setAspectMask(guessImageAspectFlags(src->imageInfo.format))
                        .setMipLevel(resolvedSrcSlice.mipLevel)
                        .setBaseArrayLayer(resolvedSrcSlice.arraySlice)
                        .setLayerCount(1))
                .setImageOffset(vk::Offset3D(resolvedSrcSlice.x, resolvedSrcSlice.y,
                                             resolvedSrcSlice.z))
                .setImageExtent(vk::Extent3D(resolvedSrcSlice.width,
                                             resolvedSrcSlice.height,
                                             resolvedSrcSlice.depth));

        assert(m_CurrentCmdBuf);

        if (m_EnableAutomaticBarriers)
        {
            requireBufferState(dst->buffer, ResourceStates::CopyDest);
            requireTextureState(src, srcSubresource, ResourceStates::CopySource);
            m_BindingStatesDirty = true;
        }
        commitBarriers();

        m_CurrentCmdBuf->referencedResources.push_back(src);
        m_CurrentCmdBuf->referencedResources.push_back(dst);
        m_CurrentCmdBuf->referencedStagingBuffers.push_back(dst->buffer);

        m_CurrentCmdBuf->cmdBuf.copyImageToBuffer(src->image, vk::ImageLayout::eTransferSrcOptimal,
                                      dst->buffer->buffer, 1, &imageCopy);
    }

    void CommandList::copyTexture(ITexture* _dst, const TextureSlice& dstSlice, IStagingTexture* _src, const TextureSlice& srcSlice)
    {
        StagingTexture* src = checked_cast<StagingTexture*>(_src);
        Texture* dst = checked_cast<Texture*>(_dst);

        auto &resolvedDstSlice = dstSlice;
        auto resolvedSrcSlice = srcSlice.resolve(src->desc);

        auto srcFootprint = src->getCopyableFootprint(resolvedSrcSlice.mipLevel,
                                                      resolvedSrcSlice.arraySlice);
        size_t srcBufferOffset =
            computePlacedBufferOffset(srcFootprint, srcSlice.x, srcSlice.y, srcSlice.z);
        assert((srcBufferOffset & 0x3) == 0);  // per vulkan spec

        TextureSubresourceSet dstSubresource = TextureSubresourceSet(
            resolvedDstSlice.mipLevel, 1,
            resolvedDstSlice.arraySlice, 1
        );

        vk::Offset3D dstOffset(resolvedDstSlice.x, resolvedDstSlice.y, resolvedDstSlice.z);

        auto imageCopy =
            vk::BufferImageCopy()
                .setBufferOffset(srcBufferOffset)
                .setBufferRowLength(srcFootprint.width)
                .setBufferImageHeight(srcFootprint.height)
                .setImageSubresource(
                    vk::ImageSubresourceLayers()
                        .setAspectMask(guessImageAspectFlags(dst->imageInfo.format))
                        .setMipLevel(resolvedDstSlice.mipLevel)
                        .setBaseArrayLayer(resolvedDstSlice.arraySlice)
                        .setLayerCount(1))
                .setImageOffset(dstOffset)
                .setImageExtent(vk::Extent3D(resolvedSrcSlice.width,
                                             resolvedSrcSlice.height,
                                             resolvedSrcSlice.depth));

        assert(m_CurrentCmdBuf);

        if (m_EnableAutomaticBarriers)
        {
            requireBufferState(src->buffer, ResourceStates::CopySource);
            requireTextureState(dst, dstSubresource, ResourceStates::CopyDest);
            m_BindingStatesDirty = true;
        }
        commitBarriers();

        m_CurrentCmdBuf->referencedResources.push_back(src);
        m_CurrentCmdBuf->referencedResources.push_back(dst);
        m_CurrentCmdBuf->referencedStagingBuffers.push_back(src->buffer);

        m_CurrentCmdBuf->cmdBuf.copyBufferToImage(src->buffer->buffer,
                                      dst->image, vk::ImageLayout::eTransferDstOptimal,
                                      1, &imageCopy);
    }

} // namespace nvrhi::vulkan
