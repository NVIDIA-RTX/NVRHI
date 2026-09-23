# Optional memory queries

`IDevice::queryResourceMemoryRequirements(resource, requirements)` reports backing-buffer
memory requirements for buffers, acceleration structures and opacity micromaps. It works
on D3D12 and Vulkan, including through the validation layer. D3D11 returns `false` without
calling its unsupported legacy `getBufferMemoryRequirements` method. Null resources,
unsupported resource types, and acceleration structures without an exposed backing buffer
also return `false`. D3D12 volatile constant buffers return `false` because they use transient
upload suballocations rather than a dedicated backing resource. Imported D3D12 buffers report
the native resource's allocation requirements. The output is unchanged on failure; do not
interpret failure as zero.

Resources must belong to the queried device. The result describes the resource's current
backing buffer, so repeat the query after replacing or compacting a resource. These are
allocation requirements, not driver residency, allocator overhead or a device memory budget.
Virtual resources can have requirements before memory is bound. Several resources can share
one heap; summing their requirements does not measure unique heap allocation. Account for
shared heaps and externally managed pools at the owning application's level.

`IDevice::queryTopLevelAccelStructPrebuildInfo(desc, instanceCount, info)` queries TLAS result,
build-scratch and update-scratch requirements without allocating or submitting work. The
count must not exceed `desc.topLevelMaxInstances`. D3D12 supports this when ray tracing is
available; Vulkan and D3D11 currently return `false` with an informational diagnostic.
Non-TLAS descriptors and unsupported queries leave the output unchanged. Scratch requirements
are build requests, not the size of NVRHI's internal scratch pool.

## Regression tests

Configure NVRHI with `-DNVRHI_BUILD_TESTS=ON -DNVRHI_INSTALL=OFF`, build, and run
`ctest --test-dir <build> --output-on-failure -C Debug`. Each enabled backend gets a separate
test with a 30-second timeout. A Vulkan loader and device are required for its runtime test.
D3D11 uses WARP; D3D12 uses the default adapter. The tests allocate small resources but do not
create windows or submit rendering work. Both raw and validation devices are exercised.

Coverage includes unsupported-backend results, unchanged failure outputs, null/unsupported
resources, buffer replacement sizes, imported D3D12 buffer allocation equivalence, unavailable
D3D12 volatile constant buffers, TLAS capacity validation, validation-layer AS unwrapping,
and native D3D12 prebuild equivalence. D3D12 tests exercise both legacy and requested enhanced
barriers, with native debug-layer error checks when the debug layer is installed. OMM
compaction and GPU residency are not covered.
