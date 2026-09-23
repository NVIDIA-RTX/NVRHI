# Optional memory queries

`IResource::queryMemoryRequirements(requirements)` optionally reports backing-buffer
memory requirements for buffers, acceleration structures and opacity micromaps. It works
on D3D12 and Vulkan, including through the validation layer. The default implementation
returns `false` without modifying the output for heaps and other non-memory resources.
Textures on every backend, and all D3D11 resources, are unsupported: they return `false`
and assert in debug builds through `utils::NotSupported`, like the legacy D3D11 getters.
Acceleration structures without an exposed backing buffer also return `false`. Check for
null before invoking the member. D3D12 volatile constant buffers return `false` because they use transient
upload suballocations rather than a dedicated backing resource. Imported D3D12 buffers report
the native resource's allocation requirements. The output is unchanged on failure; do not
interpret failure as zero.

Virtual dispatch uses the resource's backend context, not a caller-supplied device.
The result describes the resource's current backing buffer, so repeat the query after
replacing or compacting a resource. Vulkan volatile buffers report their multi-version backing storage. These are
allocation requirements, not driver residency, allocator overhead or a device memory budget.
Virtual resources can have requirements before memory is bound. Several resources can share
one heap; summing their requirements does not measure unique heap allocation. Account for
shared heaps and externally managed pools at the owning application's level.

`IDevice::queryTopLevelAccelStructPrebuildInfo(desc, instanceCount, info)` queries TLAS result,
build-scratch and update-scratch requirements without allocating or submitting work. The
intended build count must not exceed `desc.topLevelMaxInstances`. D3D12 and Vulkan support this
when ray tracing is available; otherwise it is unsupported and returns `false` after a debug
assert, as on D3D11. Non-TLAS descriptors, over-capacity counts and unsupported queries return
`false` without changing the output. Scratch requirements are build requests, not the size of
NVRHI's internal scratch pool.

Adding a virtual member to `IResource` changes ABI across the resource hierarchy: rebuild
NVRHI and its consumers together. Existing subclasses may inherit the unsupported default;
wrappers around supported resources should forward the query to their underlying resource.
TLAS prebuild remains a device query because no resource exists yet.

## Regression tests

Configure NVRHI with `-DNVRHI_BUILD_TESTS=ON -DNVRHI_INSTALL=OFF`, build, and run
`ctest --test-dir <build> --output-on-failure -C Debug`. Backend test registration and timeouts are defined in
[tests/CMakeLists.txt](../tests/CMakeLists.txt). A Vulkan loader and device are required for its runtime test.
D3D11 uses WARP; D3D12 uses the default adapter. The tests allocate small resources but do not
create windows or submit rendering work. Raw devices are always exercised; validation devices
are also exercised when `NVRHI_WITH_VALIDATION` is enabled.

Coverage includes default/unsupported-backend results, unchanged failure outputs,
base-resource virtual dispatch, buffer replacement sizes, virtual buffers before/after binding,
imported D3D12/Vulkan buffer allocation equivalence, unavailable D3D12 volatile constants,
Vulkan multi-version volatile buffers, TLAS capacity validation, validation-layer AS forwarding,
and native D3D12 prebuild equivalence. AS and native prebuild checks require a ray-tracing-capable
D3D12 device. D3D12 tests exercise both legacy and requested enhanced barriers, with native
debug-layer error checks when the debug layer is installed. OMM compaction and GPU residency
are not covered.
