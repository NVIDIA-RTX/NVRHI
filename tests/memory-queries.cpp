// Exercises optional queries on real backends, without windows or submitted GPU work.
#include <nvrhi/nvrhi.h>
#include <nvrhi/validation.h>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#if TEST_D3D12
#include <nvrhi/d3d12.h>
#endif
#if TEST_D3D11
#include <nvrhi/d3d11.h>
#endif
#if TEST_D3D11 || TEST_D3D12
#include <wrl/client.h>
#endif
#if TEST_VULKAN
#define VK_NO_PROTOTYPES
#include <nvrhi/vulkan.h>
#if !TEST_SHARED
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#include <vulkan/vulkan.hpp>
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#endif

static void check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

struct Messages : nvrhi::IMessageCallback
{
    unsigned errors = 0, infos = 0;
    void message(nvrhi::MessageSeverity severity, const char* text) override
    {
        if (severity == nvrhi::MessageSeverity::Error || severity == nvrhi::MessageSeverity::Fatal) ++errors;
        if (severity == nvrhi::MessageSeverity::Info) ++infos;
        std::printf("NVRHI: %s\n", text);
    }
};

static void runQueries(nvrhi::IDevice* device, Messages& messages)
{
    nvrhi::MemoryRequirements requirements{123, 456};
    check(!device->queryResourceMemoryRequirements(nullptr, requirements), "null resource is unavailable");
    check(requirements.size == 123 && requirements.alignment == 456, "null query preserves output");
    check(!device->queryResourceMemoryRequirements(device, requirements), "unsupported resource is unavailable");
    check(requirements.size == 123 && requirements.alignment == 456, "unsupported resource preserves output");
    const bool supported = device->getGraphicsAPI() != nvrhi::GraphicsAPI::D3D11;
    for (uint64_t size : {1024ull, 131072ull})
    {
        auto buffer = device->createBuffer(nvrhi::BufferDesc().setByteSize(size));
        check(buffer != nullptr, "create buffer");
        check(device->queryResourceMemoryRequirements(buffer, requirements) == supported, "buffer query capability");
        if (supported)
        {
            auto legacy = device->getBufferMemoryRequirements(buffer);
            check(requirements.size >= size && requirements.size == legacy.size &&
                requirements.alignment == legacy.alignment, "backing-buffer requirements");
        }
        else
            check(requirements.size == 123 && requirements.alignment == 456, "D3D11 preserves unavailable output");
    }

    nvrhi::rt::AccelStructDesc desc;
    desc.isTopLevel = true;
    desc.topLevelMaxInstances = 8;
    desc.buildFlags = nvrhi::rt::AccelStructBuildFlags::AllowUpdate;
    nvrhi::rt::AccelStructPrebuildInfo info{123, 456, 789};
    const bool prebuildSupported = device->getGraphicsAPI() == nvrhi::GraphicsAPI::D3D12 &&
        device->queryFeatureSupport(nvrhi::Feature::RayTracingAccelStruct);
    const auto infosBefore = messages.infos;
    check(device->queryTopLevelAccelStructPrebuildInfo(desc, 4, info) == prebuildSupported, "prebuild capability");
    if (prebuildSupported)
    {
        check(info.resultBytes > 0 && info.scratchBytes > 0, "prebuild bytes");
#if TEST_D3D12
        ID3D12Device* native = device->getNativeObject(nvrhi::ObjectTypes::D3D12_Device);
        Microsoft::WRL::ComPtr<ID3D12Device5> native5;
        check(SUCCEEDED(native->QueryInterface(IID_PPV_ARGS(&native5))), "native ray tracing device");
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
        inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
        inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
        inputs.NumDescs = 4;
        inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO expected = {};
        native5->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &expected);
        check(info.resultBytes == expected.ResultDataMaxSizeInBytes && info.scratchBytes == expected.ScratchDataSizeInBytes &&
            info.updateScratchBytes == expected.UpdateScratchDataSizeInBytes, "prebuild vs native D3D12");
#endif
        const auto original = info;
        desc.buildFlags = desc.buildFlags | nvrhi::rt::AccelStructBuildFlags::AllowEmptyInstances;
        check(device->queryTopLevelAccelStructPrebuildInfo(desc, 4, info) && info.resultBytes == original.resultBytes &&
            info.scratchBytes == original.scratchBytes, "NVRHI-only build flag masked");
        desc.buildFlags = nvrhi::rt::AccelStructBuildFlags::AllowUpdate;
        auto as = device->createAccelStruct(desc);
        check(as && device->queryResourceMemoryRequirements(as, requirements), "AS query (including validation wrapper)");
        check(requirements.size >= info.resultBytes, "AS backing allocation");
    }
    else
    {
        check(info.resultBytes == 123 && info.scratchBytes == 456 && info.updateScratchBytes == 789, "unsupported prebuild preserves output");
        check(messages.infos > infosBefore, "unsupported prebuild emits diagnostic");
    }
    info = {123, 456, 789};
    check(!device->queryTopLevelAccelStructPrebuildInfo(desc, 9, info), "instance count exceeds capacity");
    desc.isTopLevel = false;
    check(!device->queryTopLevelAccelStructPrebuildInfo(desc, 4, info), "BLAS is not TLAS");
    check(info.resultBytes == 123 && info.scratchBytes == 456 && info.updateScratchBytes == 789, "invalid prebuild preserves output");
}

static void runDevice(nvrhi::IDevice* device, Messages& messages)
{
    check(device != nullptr, "NVRHI device");
    runQueries(device, messages);
#if TEST_VALIDATION
    auto validation = nvrhi::validation::createValidationLayer(device);
    runQueries(validation, messages);
#endif
    device->waitForIdle();
    device->runGarbageCollection();
    check(messages.errors == 0, "no validation/backend errors");
}

int main(int argc, char** argv)
{
    try
    {
        check(argc == 2, "specify d3d11, d3d12 or vulkan");
        const std::string backend = argv[1];
        Messages messages;
#if TEST_D3D11
        if (backend == "d3d11")
        {
            Microsoft::WRL::ComPtr<ID3D11Device> native;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
            check(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0,
                D3D11_SDK_VERSION, &native, nullptr, &context)), "D3D11 WARP device");
            nvrhi::d3d11::DeviceDesc desc;
            desc.context = context.Get();
            desc.messageCallback = &messages;
            auto device = nvrhi::d3d11::createDevice(desc);
            runDevice(device, messages);
        }
        else
#endif
#if TEST_D3D12
        if (backend == "d3d12")
        {
            Microsoft::WRL::ComPtr<ID3D12Device> native;
            check(SUCCEEDED(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&native))), "D3D12 device");
            Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            check(SUCCEEDED(native->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&queue))), "D3D12 queue");
            nvrhi::d3d12::DeviceDesc desc;
            desc.pDevice = native.Get();
            desc.pGraphicsCommandQueue = queue.Get();
            desc.errorCB = &messages;
            auto device = nvrhi::d3d12::createDevice(desc);
            runDevice(device, messages);
        }
        else
#endif
#if TEST_VULKAN
        if (backend == "vulkan")
        {
#ifdef _WIN32
            auto loader = LoadLibraryA("vulkan-1.dll");
            check(loader != nullptr, "Vulkan loader");
#define LOAD_VK(name) auto name = reinterpret_cast<PFN_##name>(GetProcAddress(loader, #name)); check(name != nullptr, #name)
#else
            auto loader = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
            check(loader != nullptr, "Vulkan loader");
#define LOAD_VK(name) auto name = reinterpret_cast<PFN_##name>(dlsym(loader, #name)); check(name != nullptr, #name)
#endif
            LOAD_VK(vkGetInstanceProcAddr);
            LOAD_VK(vkCreateInstance);
            LOAD_VK(vkEnumeratePhysicalDevices);
            LOAD_VK(vkGetPhysicalDeviceQueueFamilyProperties);
            LOAD_VK(vkCreateDevice);
            LOAD_VK(vkGetDeviceQueue);
            LOAD_VK(vkDestroyDevice);
            LOAD_VK(vkDestroyInstance);
#undef LOAD_VK
            VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            app.apiVersion = VK_API_VERSION_1_2;
            VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            instanceInfo.pApplicationInfo = &app;
            VkInstance instance;
            check(vkCreateInstance(&instanceInfo, nullptr, &instance) == VK_SUCCESS, "Vulkan instance");
            uint32_t count = 0;
            check(vkEnumeratePhysicalDevices(instance, &count, nullptr) == VK_SUCCESS && count, "Vulkan physical devices");
            std::vector<VkPhysicalDevice> physical(count);
            check(vkEnumeratePhysicalDevices(instance, &count, physical.data()) == VK_SUCCESS, "Vulkan enumeration");
            vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical[0], &count, families.data());
            uint32_t family = 0;
            while (family < count && !(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) ++family;
            check(family < count, "Vulkan graphics queue family");
            float priority = 1.f;
            VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queueInfo.queueFamilyIndex = family;
            queueInfo.queueCount = 1;
            queueInfo.pQueuePriorities = &priority;
            VkPhysicalDeviceVulkan12Features features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
            features.timelineSemaphore = VK_TRUE;
            VkDeviceCreateInfo deviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
            deviceInfo.pNext = &features;
            deviceInfo.queueCreateInfoCount = 1;
            deviceInfo.pQueueCreateInfos = &queueInfo;
            VkDevice native;
            check(vkCreateDevice(physical[0], &deviceInfo, nullptr, &native) == VK_SUCCESS, "Vulkan device");
            nvrhi::vulkan::DeviceDesc desc{};
            desc.instance = instance;
            desc.physicalDevice = physical[0];
            desc.device = native;
            desc.graphicsQueueIndex = int(family);
            vkGetDeviceQueue(native, family, 0, &desc.graphicsQueue);
            desc.errorCB = &messages;
#if !TEST_SHARED
            VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, vkGetInstanceProcAddr, native);
#endif
            {
                auto device = nvrhi::vulkan::createDevice(desc);
                runDevice(device, messages);
            }
            vkDestroyDevice(native, nullptr);
            vkDestroyInstance(instance, nullptr);
#ifdef _WIN32
            FreeLibrary(loader);
#else
            dlclose(loader);
#endif
        }
        else
#endif
            throw std::runtime_error("backend not compiled");
        std::printf("PASS: %s optional memory queries, native/validation paths\n", backend.c_str());
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "FAIL: %s\n", e.what());
        return 1;
    }
}
