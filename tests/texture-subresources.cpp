// Exercises TextureSubresourceSet::resolve, which needs no backend.
#include <nvrhi/nvrhi.h>
#include <cstdio>
#include <stdexcept>

static void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    try
    {
        using nvrhi::TextureSubresourceSet;
        nvrhi::TextureDesc desc;
        desc.dimension = nvrhi::TextureDimension::Texture2DArray;
        desc.width = 256;
        desc.height = 256;
        desc.mipLevels = 9;
        desc.arraySize = 4;

        TextureSubresourceSet set = nvrhi::AllSubresources.resolve(desc, false);
        check(set == TextureSubresourceSet(0, 9, 0, 4), "all subresources");

        set = TextureSubresourceSet(1, TextureSubresourceSet::AllMipLevels,
            1, TextureSubresourceSet::AllArraySlices).resolve(desc, false);
        check(set == TextureSubresourceSet(1, 8, 1, 3), "non-zero base with AllMipLevels and AllArraySlices");

        set = TextureSubresourceSet(3, 100, 2, 100).resolve(desc, false);
        check(set == TextureSubresourceSet(3, 6, 2, 2), "counts past the end are clamped");

        set = TextureSubresourceSet(9, 1, 4, 1).resolve(desc, false);
        check(set.numMipLevels == 0 && set.numArraySlices == 0, "base at the end");

        set = TextureSubresourceSet(10, 1, 5, 1).resolve(desc, false);
        check(set.numMipLevels == 0 && set.numArraySlices == 0, "base past the end");

        set = TextureSubresourceSet(2, TextureSubresourceSet::AllMipLevels, 0, 1).resolve(desc, true);
        check(set == TextureSubresourceSet(2, 1, 0, 1), "single mip level");

        std::printf("PASS: texture subresource set resolve\n");
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
