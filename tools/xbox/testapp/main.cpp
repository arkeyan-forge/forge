// Xbox Dev Mode smoke test for the Arkeyan UWP pipeline (see tools/xbox/README.md).
// A CoreApplication view that clears the screen with D3D12, shows gamepad input as colours, and logs platform
// probes (system, memory, D3D12 capabilities, audio, drives/USB, ISO access, HID devices) to
// LocalState\ssa-xbox-test.log. Raw WinRT ABI through the mingw-w64 headers: no C++/WinRT, no Windows SDK.

// mingw-w64 (widl) declares IReference<boolean> and IReference<BYTE>, both IReference<unsigned char>, which C++
// rejects as a redefinition. Nothing here uses IReference<boolean>, so skip that declaration.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__

#include <windows.h>
#include <appmodel.h>
#include <roapi.h>
#include <winstring.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <xaudio2.h>
#include <cpuid.h>
#include <windows.applicationmodel.core.h>
#include <windows.ui.core.h>
#include <windows.gaming.input.h>
#include <windows.storage.h>
#include <windows.system.profile.h>
#include <windows.devices.enumeration.h>
#include "winrt_extra.h"

#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cwctype>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace ABI::Windows;
using ApplicationModel::Core::ICoreApplication;
using ApplicationModel::Core::ICoreApplicationExit;
using ApplicationModel::Core::ICoreApplicationView;
using ApplicationModel::Core::IFrameworkView;
using ApplicationModel::Core::IFrameworkViewSource;
using Gaming::Input::GamepadButtons;
using Gaming::Input::GamepadReading;
using UI::Core::ICoreWindow;

namespace
{

    template <typename T>
    class Ref
    {
    public:
        Ref() = default;
        Ref(const Ref&) = delete;
        Ref& operator=(const Ref&) = delete;
        ~Ref() { Reset(); }

        void Reset()
        {
            if (m_ptr)
            {
                m_ptr->Release();
                m_ptr = nullptr;
            }
        }
        void Attach(T* ptr)
        {
            Reset();
            m_ptr = ptr;
        }
        T** Put()
        {
            Reset();
            return &m_ptr;
        }
        void** PutVoid() { return reinterpret_cast<void**>(Put()); }
        T* Get() const { return m_ptr; }
        T* operator->() const { return m_ptr; }
        explicit operator bool() const { return m_ptr != nullptr; }

        template <typename U>
        HRESULT As(Ref<U>& out) const
        {
            return m_ptr->QueryInterface(__uuidof(U), out.PutVoid());
        }

    private:
        T* m_ptr = nullptr;
    };

    class HString
    {
    public:
        explicit HString(const wchar_t* text) { WindowsCreateString(text, UINT32(wcslen(text)), &m_value); }
        HString(const HString&) = delete;
        HString& operator=(const HString&) = delete;
        ~HString() { WindowsDeleteString(m_value); }
        operator HSTRING() const { return m_value; }

    private:
        HSTRING m_value = nullptr;
    };

    std::wstring TakeHString(HSTRING value)
    {
        UINT32 length = 0;
        const wchar_t* text = WindowsGetStringRawBuffer(value, &length);
        std::wstring result(text, length);
        WindowsDeleteString(value);
        return result;
    }

    std::string Utf8(const std::wstring& text)
    {
        if (text.empty())
            return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size_t(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    template <typename T>
    HRESULT GetFactory(const wchar_t* className, Ref<T>& out)
    {
        HString name(className);
        return RoGetActivationFactory(name, __uuidof(T), out.PutVoid());
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Log

    std::mutex g_logMutex;
    FILE* g_logFile = nullptr;
    LARGE_INTEGER g_startTime;
    LARGE_INTEGER g_frequency;

    double Seconds()
    {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return double(now.QuadPart - g_startTime.QuadPart) / double(g_frequency.QuadPart);
    }

    [[gnu::format(printf, 1, 2)]] void Log(const char* format, ...)
    {
        char message[1024];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        char line[1100];
        snprintf(line, sizeof(line), "[%8.3f] %s\n", Seconds(), message);
        std::lock_guard lock(g_logMutex);
        OutputDebugStringA(line);
        if (g_logFile)
        {
            fputs(line, g_logFile);
            fflush(g_logFile);
        }
    }

    std::wstring LocalStatePath()
    {
        Ref<Storage::IApplicationDataStatics> statics;
        Ref<Storage::IApplicationData> data;
        Ref<Storage::IStorageFolder> folder;
        Ref<Storage::IStorageItem> item;
        HSTRING path = nullptr;
        if (FAILED(GetFactory(L"Windows.Storage.ApplicationData", statics)) ||
            FAILED(statics->get_Current(data.Put())) || FAILED(data->get_LocalFolder(folder.Put())) ||
            FAILED(folder.As(item)) || FAILED(item->get_Path(&path)))
            return {};
        return TakeHString(path);
    }

    // ---------------------------------------------------------------------------------------------------------------
    // Probe status, shown as squares on screen (README: "Reading the test screen").

    enum Check
    {
        CheckLog,
        CheckD3D12,
        CheckGamepad,
        CheckMemory,
        CheckAudio,
        CheckDrives,
        CheckIso,
        CheckHid,
        CheckCount
    };

    enum class Status
    {
        Pending,
        Ok,
        Warning,
        Failed
    };

    std::atomic<Status> g_status[CheckCount];
    std::atomic<bool> g_quit{ false };

    void SetStatus(Check check, Status status) { g_status[check] = status; }

    // ---------------------------------------------------------------------------------------------------------------
    // COM object helpers for the objects the OS calls back into.

    template <typename Interface>
    class Unknown : public Interface
    {
    public:
        virtual ~Unknown() = default;

        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
        {
            if (iid == __uuidof(Interface) || iid == __uuidof(IUnknown) || iid == __uuidof(IAgileObject) ||
                (std::is_base_of_v<IInspectable, Interface> && iid == __uuidof(IInspectable)))
            {
                *out = static_cast<Interface*>(this);
                AddRef();
                return S_OK;
            }
            *out = nullptr;
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }
        ULONG STDMETHODCALLTYPE Release() override
        {
            ULONG refs = --m_refs;
            if (refs == 0)
                delete this;
            return refs;
        }

    private:
        std::atomic<ULONG> m_refs{ 1 };
    };

    template <typename Interface>
    class Inspectable : public Unknown<Interface>
    {
    public:
        HRESULT STDMETHODCALLTYPE GetIids(ULONG* count, IID** iids) override
        {
            *count = 0;
            *iids = nullptr;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetRuntimeClassName(HSTRING* name) override
        {
            *name = nullptr;
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE GetTrustLevel(TrustLevel* level) override
        {
            *level = BaseTrust;
            return S_OK;
        }
    };

    using ClosedHandlerBase = Foundation::ITypedEventHandler<UI::Core::CoreWindow*, UI::Core::CoreWindowEventArgs*>;

    class ClosedHandler final : public Unknown<ClosedHandlerBase>
    {
    public:
        HRESULT STDMETHODCALLTYPE Invoke(ICoreWindow*, UI::Core::ICoreWindowEventArgs*) override
        {
            Log("window closed");
            g_quit = true;
            return S_OK;
        }
    };

    // On Xbox an unhandled back request (gamepad B) suspends the app to Home; B is a game button, so take it.
    class BackHandler final : public Unknown<UI::Core::IBackRequestedEventHandler>
    {
    public:
        HRESULT STDMETHODCALLTYPE Invoke(IInspectable*, UI::Core::IBackRequestedEventArgs* args) override
        {
            args->put_Handled(TRUE);
            return S_OK;
        }
    };

    // ---------------------------------------------------------------------------------------------------------------
    // D3D12 renderer: clears the back buffer and a few rectangles (no shaders needed).

    struct ColorRect
    {
        D3D12_RECT rect;
        float color[4];
    };

    class Renderer
    {
    public:
        static constexpr UINT BufferCount = 2;

        bool Init(IUnknown* window, UINT width, UINT height)
        {
            HRESULT hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), m_factory.PutVoid());
            if (FAILED(hr))
                return Fail("CreateDXGIFactory2", hr);

            Ref<IDXGIAdapter1> adapter;
            if (SUCCEEDED(m_factory->EnumAdapters1(0, adapter.Put())))
            {
                DXGI_ADAPTER_DESC1 desc{};
                adapter->GetDesc1(&desc);
                Log("adapter: %s vendor=%04x device=%04x dedicatedVRAM=%llu MB sharedRAM=%llu MB",
                    Utf8(desc.Description).c_str(), desc.VendorId, desc.DeviceId,
                    (unsigned long long)(desc.DedicatedVideoMemory >> 20),
                    (unsigned long long)(desc.SharedSystemMemory >> 20));
                Ref<IDXGIAdapter3> adapter3;
                DXGI_QUERY_VIDEO_MEMORY_INFO info{};
                if (SUCCEEDED(adapter.As(adapter3)) &&
                    SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
                    Log("video memory (local): budget=%llu MB usage=%llu MB", (unsigned long long)(info.Budget >> 20),
                        (unsigned long long)(info.CurrentUsage >> 20));
            }

            hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), m_device.PutVoid());
            if (FAILED(hr))
                return Fail("D3D12CreateDevice(FL 11_0)", hr);
            LogCapabilities();

            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (FAILED(hr = m_device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), m_queue.PutVoid())))
                return Fail("CreateCommandQueue", hr);

            DXGI_SWAP_CHAIN_DESC1 swapDesc{};
            swapDesc.Width = width;
            swapDesc.Height = height;
            swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            swapDesc.SampleDesc.Count = 1;
            swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapDesc.BufferCount = BufferCount;
            swapDesc.Scaling = DXGI_SCALING_STRETCH;
            swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            Ref<IDXGISwapChain1> swapChain1;
            if (FAILED(hr = m_factory->CreateSwapChainForCoreWindow(
                           m_queue.Get(), window, &swapDesc, nullptr, swapChain1.Put())))
                return Fail("CreateSwapChainForCoreWindow", hr);
            if (FAILED(hr = swapChain1.As(m_swapChain)))
                return Fail("IDXGISwapChain3", hr);

            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = BufferCount;
            if (FAILED(hr = m_device->CreateDescriptorHeap(
                           &heapDesc, __uuidof(ID3D12DescriptorHeap), m_rtvHeap.PutVoid())))
                return Fail("CreateDescriptorHeap", hr);
            m_rtvStride = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            for (UINT i = 0; i < BufferCount; i++)
            {
                if (FAILED(hr = m_swapChain->GetBuffer(i, __uuidof(ID3D12Resource), m_buffers[i].PutVoid())))
                    return Fail("GetBuffer", hr);
                m_device->CreateRenderTargetView(m_buffers[i].Get(), nullptr, Rtv(i));
            }

            if (FAILED(hr = m_device->CreateCommandAllocator(
                           D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), m_allocator.PutVoid())))
                return Fail("CreateCommandAllocator", hr);
            if (FAILED(hr = m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_allocator.Get(), nullptr,
                           __uuidof(ID3D12GraphicsCommandList), m_list.PutVoid())))
                return Fail("CreateCommandList", hr);
            m_list->Close();
            if (FAILED(hr = m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), m_fence.PutVoid())))
                return Fail("CreateFence", hr);
            m_fenceEvent = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
            Log("D3D12 swap chain %ux%u ready", width, height);
            return true;
        }

        bool Frame(const float background[4], const std::vector<ColorRect>& rects)
        {
            UINT index = m_swapChain->GetCurrentBackBufferIndex();
            m_allocator->Reset();
            m_list->Reset(m_allocator.Get(), nullptr);
            Transition(index, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            m_list->ClearRenderTargetView(Rtv(index), background, 0, nullptr);
            for (const ColorRect& rect : rects)
                m_list->ClearRenderTargetView(Rtv(index), rect.color, 1, &rect.rect);
            Transition(index, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            m_list->Close();
            ID3D12CommandList* lists[] = { m_list.Get() };
            m_queue->ExecuteCommandLists(1, lists);

            HRESULT hr = m_swapChain->Present(1, 0);
            if (FAILED(hr))
                return Fail("Present", hr);
            if (!m_framesPresented++)
                Log("first frame presented");

            // Wait for the GPU every frame: trivial workload, keeps the sample free of per-frame resource tracking.
            m_queue->Signal(m_fence.Get(), ++m_fenceValue);
            if (m_fence->GetCompletedValue() < m_fenceValue)
            {
                m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
                WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
            }
            return true;
        }

    private:
        bool Fail(const char* what, HRESULT hr)
        {
            Log("D3D12: %s failed: 0x%08lx", what, (unsigned long)hr);
            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE Rtv(UINT index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += SIZE_T(index) * m_rtvStride;
            return handle;
        }

        void Transition(UINT index, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = m_buffers[index].Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            m_list->ResourceBarrier(1, &barrier);
        }

        void LogCapabilities()
        {
            D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0,
                D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
            D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels{ UINT(std::size(levels)), levels, D3D_FEATURE_LEVEL_11_0 };
            if (SUCCEEDED(
                    m_device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevels, sizeof(featureLevels))))
                Log("D3D12 max feature level: %x", unsigned(featureLevels.MaxSupportedFeatureLevel));

            // The runtime rejects shader models it does not know, so walk down from the newest one in the headers.
            for (int model = D3D_SHADER_MODEL_6_7; model >= D3D_SHADER_MODEL_6_0; model--)
            {
                D3D12_FEATURE_DATA_SHADER_MODEL shaderModel{ D3D_SHADER_MODEL(model) };
                if (SUCCEEDED(
                        m_device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel))))
                {
                    Log("D3D12 highest shader model: %x", unsigned(shaderModel.HighestShaderModel));
                    break;
                }
                if (model == D3D_SHADER_MODEL_6_0)
                    Log("D3D12 shader model 6.0 not supported (DXBC/SM 5.1 only)");
            }

            D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
            if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof(options))))
                Log("D3D12 options: resourceBindingTier=%d resourceHeapTier=%d tiledResourcesTier=%d "
                    "typedUAVLoadAdditionalFormats=%d "
                    "ROVs=%d conservativeRasterTier=%d standardSwizzle64KB=%d",
                    options.ResourceBindingTier, options.ResourceHeapTier, options.TiledResourcesTier,
                    options.TypedUAVLoadAdditionalFormats, options.ROVsSupported, options.ConservativeRasterizationTier,
                    options.StandardSwizzle64KBSupported);
            D3D12_FEATURE_DATA_D3D12_OPTIONS1 options1{};
            if (SUCCEEDED(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &options1, sizeof(options1))))
                Log("D3D12 wave ops=%d lanes=%u..%u", options1.WaveOps, options1.WaveLaneCountMin,
                    options1.WaveLaneCountMax);
            D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature{ D3D_ROOT_SIGNATURE_VERSION_1_1 };
            if (SUCCEEDED(
                    m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature))))
                Log("D3D12 root signature version: %x", unsigned(rootSignature.HighestVersion));
        }

        Ref<IDXGIFactory4> m_factory;
        Ref<ID3D12Device> m_device;
        Ref<ID3D12CommandQueue> m_queue;
        Ref<IDXGISwapChain3> m_swapChain;
        Ref<ID3D12DescriptorHeap> m_rtvHeap;
        Ref<ID3D12Resource> m_buffers[BufferCount];
        Ref<ID3D12CommandAllocator> m_allocator;
        Ref<ID3D12GraphicsCommandList> m_list;
        Ref<ID3D12Fence> m_fence;
        HANDLE m_fenceEvent = nullptr;
        UINT64 m_fenceValue = 0;
        UINT m_rtvStride = 0;
        UINT64 m_framesPresented = 0;
    };

    // ---------------------------------------------------------------------------------------------------------------
    // Probes (worker thread)

    constexpr unsigned long long MiB = 1ull << 20;

    void LogMemoryState(const char* when)
    {
        Ref<System::IMemoryManagerStatics> memoryManager;
        UINT64 usage = 0, limit = 0;
        if (SUCCEEDED(GetFactory(L"Windows.System.MemoryManager", memoryManager)))
        {
            memoryManager->get_AppMemoryUsage(&usage);
            memoryManager->get_AppMemoryUsageLimit(&limit);
        }
        MEMORYSTATUSEX status{};
        status.dwLength = sizeof(status);
        GlobalMemoryStatusEx(&status);
        Log("memory (%s): AppMemoryUsage=%llu MB AppMemoryUsageLimit=%llu MB physTotal=%llu MB physAvail=%llu MB "
            "commitTotal=%llu MB commitAvail=%llu MB",
            when, (unsigned long long)(usage / MiB), (unsigned long long)(limit / MiB), status.ullTotalPhys / MiB,
            status.ullAvailPhys / MiB, status.ullTotalPageFile / MiB, status.ullAvailPageFile / MiB);
    }

    void ProbeSystem()
    {
        Ref<System::Profile::IAnalyticsInfoStatics> analytics;
        Ref<System::Profile::IAnalyticsVersionInfo> version;
        HSTRING family = nullptr, familyVersion = nullptr;
        if (SUCCEEDED(GetFactory(L"Windows.System.Profile.AnalyticsInfo", analytics)) &&
            SUCCEEDED(analytics->get_VersionInfo(version.Put())) && SUCCEEDED(version->get_DeviceFamily(&family)) &&
            SUCCEEDED(version->get_DeviceFamilyVersion(&familyVersion)))
        {
            unsigned long long v = std::stoull(TakeHString(familyVersion));
            Log("device family: %s %llu.%llu.%llu.%llu", Utf8(TakeHString(family)).c_str(), v >> 48, (v >> 32) & 0xffff,
                (v >> 16) & 0xffff, v & 0xffff);
        }

        wchar_t name[256];
        UINT32 length = UINT32(std::size(name));
        if (GetCurrentPackageFullName(&length, name) == ERROR_SUCCESS)
            Log("package full name: %s", Utf8(name).c_str());
        length = UINT32(std::size(name));
        if (GetCurrentPackageFamilyName(&length, name) == ERROR_SUCCESS)
            Log("package family name: %s", Utf8(name).c_str());

        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        Log("CPU: %lu logical processors, hardware_concurrency=%u, page=%lu, allocation granularity=%lu",
            info.dwNumberOfProcessors, std::thread::hardware_concurrency(), info.dwPageSize,
            info.dwAllocationGranularity);

        unsigned int regs[4][4] = {};
        char brand[49] = {};
        for (unsigned int i = 0; i < 3; i++)
        {
            unsigned int a, b, c, d;
            __cpuid(0x80000002 + i, a, b, c, d);
            memcpy(brand + i * 16 + 0, &a, 4);
            memcpy(brand + i * 16 + 4, &b, 4);
            memcpy(brand + i * 16 + 8, &c, 4);
            memcpy(brand + i * 16 + 12, &d, 4);
        }
        __cpuid(1, regs[0][0], regs[0][1], regs[0][2], regs[0][3]);
        __cpuid_count(7, 0, regs[1][0], regs[1][1], regs[1][2], regs[1][3]);
        bool osAvx = false;
        if (regs[0][2] & bit_OSXSAVE)
        {
            unsigned int lo, hi;
            __asm__ volatile("xgetbv" : "=a"(lo), "=d"(hi) : "c"(0));
            osAvx = (lo & 6) == 6;
        }
        Log("CPU: %s | SSE4.1=%d SSE4.2=%d AVX=%d(os %d) F16C=%d FMA=%d AVX2=%d BMI1=%d BMI2=%d MOVBE=%d", brand,
            !!(regs[0][2] & bit_SSE4_1), !!(regs[0][2] & bit_SSE4_2), !!(regs[0][2] & bit_AVX), osAvx,
            !!(regs[0][2] & bit_F16C), !!(regs[0][2] & bit_FMA), !!(regs[1][1] & bit_AVX2), !!(regs[1][1] & bit_BMI),
            !!(regs[1][1] & bit_BMI2), !!(regs[0][2] & bit_MOVBE));
    }

    void ProbeMemory()
    {
        LogMemoryState("start");
        Status status = Status::Ok;

        // The runtime keeps the 4 GB guest address space in one reservation (memory.cpp).
        constexpr SIZE_T GuestSize = SIZE_T(4) << 30;
        constexpr SIZE_T Step = SIZE_T(256) << 20;
        auto* guest = static_cast<uint8_t*>(VirtualAlloc(nullptr, GuestSize, MEM_RESERVE, PAGE_NOACCESS));
        Log("reserve 4 GB: %s (error %lu)", guest ? "ok" : "FAILED", guest ? 0ul : GetLastError());
        if (!guest)
            status = Status::Failed;

        // Commit charge without touching: does an eager 4 GB commit fit the app limit?
        SIZE_T committed = 0;
        while (guest && committed < GuestSize && VirtualAlloc(guest + committed, Step, MEM_COMMIT, PAGE_READWRITE))
            committed += Step;
        Log("commit inside the reservation: %llu MB of 4096 MB", (unsigned long long)(committed / MiB));
        LogMemoryState("after commit");
        if (guest)
            VirtualFree(guest, 0, MEM_RELEASE);
        if (committed < GuestSize)
            status = Status::Warning;

        // Commit limit: separate 256 MB commits until failure (capped at 16 GB), then release.
        std::vector<void*> blocks;
        while (blocks.size() < 64 && !g_quit)
        {
            void* block = VirtualAlloc(nullptr, Step, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (!block)
                break;
            blocks.push_back(block);
        }
        Log("commit limit probe: %llu MB committed before failure (cap 16384 MB)",
            (unsigned long long)(blocks.size() * Step / MiB));
        LogMemoryState("at commit limit");
        for (void* block : blocks)
            VirtualFree(block, 0, MEM_RELEASE);

        // Resident memory: write every page of up to 2 GB.
        constexpr SIZE_T TouchSize = SIZE_T(2) << 30;
        auto* touch = static_cast<uint8_t*>(VirtualAlloc(nullptr, TouchSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        if (touch)
        {
            double start = Seconds();
            for (SIZE_T offset = 0; offset < TouchSize; offset += 4096)
                touch[offset] = 1;
            Log("touched 2048 MB in %.2f s", Seconds() - start);
            LogMemoryState("after touching 2 GB");
            VirtualFree(touch, 0, MEM_RELEASE);
        }
        else
        {
            Log("commit 2 GB for the touch test FAILED (error %lu)", GetLastError());
            status = Status::Warning;
        }
        SetStatus(CheckMemory, status);
    }

    void ProbeAudio()
    {
        Ref<IXAudio2> xaudio;
        HRESULT hr = XAudio2Create(xaudio.Put(), 0, XAUDIO2_DEFAULT_PROCESSOR);
        if (FAILED(hr))
        {
            Log("XAudio2Create failed: 0x%08lx", (unsigned long)hr);
            SetStatus(CheckAudio, Status::Failed);
            return;
        }
        IXAudio2MasteringVoice* master = nullptr;
        hr = xaudio->CreateMasteringVoice(&master);
        if (FAILED(hr))
        {
            Log("XAudio2 CreateMasteringVoice failed: 0x%08lx", (unsigned long)hr);
            SetStatus(CheckAudio, Status::Failed);
            return;
        }
        XAUDIO2_VOICE_DETAILS details{};
        master->GetVoiceDetails(&details);
        DWORD channelMask = 0;
        master->GetChannelMask(&channelMask);
        Log("XAudio2 2.9 mastering voice: %u channels, %u Hz, channel mask 0x%lx (nothing is played)",
            details.InputChannels, details.InputSampleRate, channelMask);
        master->DestroyVoice();
        SetStatus(CheckAudio, Status::Ok);
    }

    bool EndsWithIso(const std::wstring& name)
    {
        if (name.size() < 4)
            return false;
        std::wstring ext = name.substr(name.size() - 4);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return wchar_t(std::towlower(c)); });
        return ext == L".iso";
    }

    // Lists a directory with FindFirstFileExW (plain) or FindFirstFileExFromAppW (brokered). Returns false on error.
    bool ListDirectory(const std::wstring& directory, bool fromApp, std::vector<std::wstring>* isoFiles, bool verbose)
    {
        std::wstring pattern = directory + L"\\*";
        WIN32_FIND_DATAW data;
        HANDLE find = fromApp ? FindFirstFileExFromAppW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch,
                                    nullptr, FIND_FIRST_EX_LARGE_FETCH)
                              : FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch,
                                    nullptr, FIND_FIRST_EX_LARGE_FETCH);
        const char* api = fromApp ? "FindFirstFileExFromAppW" : "FindFirstFileExW";
        if (find == INVALID_HANDLE_VALUE)
        {
            DWORD error = GetLastError();
            if (verbose)
                Log("  %s(%s) failed: error %lu", api, Utf8(pattern).c_str(), error);
            return false;
        }
        int count = 0;
        do
        {
            std::wstring name = data.cFileName;
            if (name == L"." || name == L"..")
                continue;
            bool isDirectory = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
            unsigned long long size = (unsigned long long)data.nFileSizeHigh << 32 | data.nFileSizeLow;
            if (verbose && count < 40)
                Log("  %s %s %s", api, Utf8(directory + L"\\" + name).c_str(),
                    isDirectory ? "<dir>" : (std::to_string(size) + " bytes").c_str());
            count++;
            if (!isDirectory && isoFiles && EndsWithIso(name))
                isoFiles->push_back(directory + L"\\" + name);
        } while (FindNextFileW(find, &data));
        FindClose(find);
        if (verbose)
            Log("  %s(%s): %d entries", api, Utf8(directory).c_str(), count);
        return true;
    }

    bool ProbeIsoFile(const std::wstring& path)
    {
        Log("ISO candidate: %s", Utf8(path).c_str());
        HANDLE file = CreateFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
        Log("  CreateFile2: %s (error %lu)", file != INVALID_HANDLE_VALUE ? "ok" : "failed",
            file != INVALID_HANDLE_VALUE ? 0ul : GetLastError());
        HANDLE brokered = CreateFile2FromAppW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
        Log("  CreateFile2FromAppW: %s (error %lu)", brokered != INVALID_HANDLE_VALUE ? "ok" : "failed",
            brokered != INVALID_HANDLE_VALUE ? 0ul : GetLastError());
        if (file == INVALID_HANDLE_VALUE)
            std::swap(file, brokered);
        if (brokered != INVALID_HANDLE_VALUE)
            CloseHandle(brokered);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        LARGE_INTEGER size{};
        GetFileSizeEx(file, &size);
        Log("  size: %lld bytes", (long long)size.QuadPart);

        // Same layouts as runtime/vfs.cpp: XDVDFS volume descriptor at sector 32 of the game partition.
        constexpr char Magic[] = "MICROSOFT*XBOX*MEDIA";
        constexpr long long PartitionOffsets[] = { 0x0, 0x2080000, 0xFD90000, 0x18300000 };
        bool found = false;
        for (long long partition : PartitionOffsets)
        {
            char header[20] = {};
            DWORD read = 0;
            LARGE_INTEGER position{};
            position.QuadPart = partition + 32 * 2048;
            if (SetFilePointerEx(file, position, nullptr, FILE_BEGIN) &&
                ReadFile(file, header, sizeof(header), &read, nullptr) && read == sizeof(header) &&
                memcmp(header, Magic, sizeof(header)) == 0)
            {
                Log("  XDVDFS game partition at 0x%llx", (unsigned long long)partition);
                found = true;
                break;
            }
        }
        if (!found)
            Log("  no XDVDFS header found");

        // Sequential read speed, 4 MB requests (very large single reads can fail with ERROR_NO_SYSTEM_RESOURCES).
        constexpr DWORD Chunk = 4 << 20;
        std::vector<char> buffer(Chunk);
        LARGE_INTEGER zero{};
        SetFilePointerEx(file, zero, nullptr, FILE_BEGIN);
        unsigned long long total = 0;
        double start = Seconds();
        while (total < (256ull << 20) && !g_quit)
        {
            DWORD read = 0;
            if (!ReadFile(file, buffer.data(), Chunk, &read, nullptr) || read == 0)
                break;
            total += read;
        }
        double elapsed = Seconds() - start;
        Log("  read %llu MB in %.2f s (%.1f MB/s)", total / MiB, elapsed,
            elapsed > 0 ? double(total) / MiB / elapsed : 0.0);
        CloseHandle(file);
        return found;
    }

    void ProbeFiles(const std::wstring& localState)
    {
        std::vector<std::wstring> isoFiles;

        // Data mode "embedded": the ISO ships inside the package next to the executable.
        wchar_t exePath[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exePath, MAX_PATH);
        std::wstring installDir = exePath;
        installDir.resize(installDir.find_last_of(L'\\'));
        Log("install dir: %s", Utf8(installDir).c_str());
        ListDirectory(installDir, false, &isoFiles, true);
        if (!localState.empty())
            ListDirectory(localState, false, &isoFiles, true);

        // Data mode "external": an ISO on a USB drive, reachable with broadFileSystemAccess through the FromApp APIs.
        DWORD drives = GetLogicalDrives();
        std::string letters;
        for (int i = 0; i < 26; i++)
            if (drives & (1u << i))
                letters += char('A' + i);
        Log("GetLogicalDrives: %s", letters.empty() ? "(none)" : letters.c_str());

        bool anyDrive = false;
        for (int i = 2; i < 26; i++)
        {
            // Always try D:-H: in case the drive mask hides removable drives from the AppContainer.
            if (!(drives & (1u << i)) && (i < 3 || i > 7))
                continue;
            std::wstring root = std::wstring(1, wchar_t(L'A' + i)) + L":";
            bool plain = ListDirectory(root, false, nullptr, false);
            bool brokered = ListDirectory(root, true, &isoFiles, true);
            if (plain || brokered)
            {
                Log("drive %s: plain listing %s, FromApp listing %s", Utf8(root).c_str(), plain ? "ok" : "denied",
                    brokered ? "ok" : "denied");
                anyDrive = anyDrive || brokered;
                ListDirectory(root + L"\\SSA", true, &isoFiles, true);
            }
        }
        SetStatus(CheckDrives, anyDrive ? Status::Ok : Status::Warning);

        bool isoOk = false;
        for (const std::wstring& iso : isoFiles)
        {
            if (g_quit)
                break;
            isoOk = ProbeIsoFile(iso) || isoOk;
        }
        if (isoFiles.empty())
            Log("no *.iso found (looked in the install dir, LocalState, <drive>:\\ and <drive>:\\SSA)");
        SetStatus(CheckIso, isoOk ? Status::Ok : (isoFiles.empty() ? Status::Warning : Status::Failed));
    }

    void ProbeHidDevices()
    {
        // The Portal of Power is a HID device (VID 0x1430). Enumeration only: opening it needs a DeviceCapability.
        Ref<Devices::Enumeration::IDeviceInformationStatics> statics;
        Ref<Foundation::IAsyncOperation<Devices::Enumeration::DeviceInformationCollection*>> operation;
        HString aqs(L"System.Devices.InterfaceClassGuid:=\"{4D1E55B2-F16F-11CF-88CB-001111000030}\"");
        HRESULT hr = GetFactory(L"Windows.Devices.Enumeration.DeviceInformation", statics);
        if (SUCCEEDED(hr))
            hr = statics->FindAllAsyncAqsFilter(aqs, operation.Put());
        Ref<IAsyncInfo> info;
        if (SUCCEEDED(hr))
            hr = operation.As(info);
        AsyncStatus status = Started;
        for (int i = 0; SUCCEEDED(hr) && status == Started && i < 500; i++)
        {
            info->get_Status(&status);
            if (status == Started)
                Sleep(10);
        }
        Ref<Foundation::Collections::IVectorView<Devices::Enumeration::DeviceInformation*>> devices;
        if (SUCCEEDED(hr) && status == Completed)
            hr = operation->GetResults(devices.Put());
        else if (SUCCEEDED(hr))
            hr = E_FAIL;
        if (FAILED(hr))
        {
            Log("HID enumeration failed: 0x%08lx (async status %d)", (unsigned long)hr, int(status));
            SetStatus(CheckHid, Status::Failed);
            return;
        }

        UINT32 count = 0;
        devices->get_Size(&count);
        Log("HID device interfaces: %u", count);
        bool portal = false;
        for (UINT32 i = 0; i < count && i < 32; i++)
        {
            Ref<Devices::Enumeration::IDeviceInformation> device;
            HSTRING id = nullptr, name = nullptr;
            if (FAILED(devices->GetAt(i, device.Put())) || FAILED(device->get_Id(&id)) ||
                FAILED(device->get_Name(&name)))
                continue;
            std::string idText = Utf8(TakeHString(id));
            Log("  HID: %s | %s", Utf8(TakeHString(name)).c_str(), idText.c_str());
            std::transform(idText.begin(), idText.end(), idText.begin(), [](char c) { return char(toupper(c)); });
            portal = portal || idText.find("VID_1430") != std::string::npos;
        }
        Log("Portal of Power (VID 1430) %s", portal ? "seen" : "not seen");
        SetStatus(CheckHid, portal ? Status::Ok : Status::Warning);
    }

    void RunProbes(std::wstring localState)
    {
        RoInitialize(RO_INIT_MULTITHREADED);
        ProbeSystem();
        ProbeAudio();
        ProbeHidDevices();
        ProbeFiles(localState);
        if (!g_quit)
            ProbeMemory();
        Log("probes finished");
        RoUninitialize();
    }

    // ---------------------------------------------------------------------------------------------------------------
    // View

    const char* ButtonNames[] = { "Menu", "View", "A", "B", "X", "Y", "DPadUp", "DPadDown", "DPadLeft", "DPadRight",
        "LeftShoulder", "RightShoulder", "LeftThumbstick", "RightThumbstick" };

    class View final : public Inspectable<IFrameworkView>
    {
    public:
        explicit View(std::wstring localState) : m_localState(std::move(localState)) {}

        HRESULT STDMETHODCALLTYPE Initialize(ICoreApplicationView*) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE SetWindow(ICoreWindow* window) override
        {
            window->AddRef();
            m_window.Attach(window);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Load(HSTRING) override { return S_OK; }
        HRESULT STDMETHODCALLTYPE Uninitialize() override { return S_OK; }

        HRESULT STDMETHODCALLTYPE Run() override
        {
            EventRegistrationToken token;
            auto* closed = new ClosedHandler();
            m_window->add_Closed(closed, &token);
            closed->Release();

            Ref<UI::Core::ISystemNavigationManagerStatics> navigationStatics;
            Ref<UI::Core::ISystemNavigationManager> navigation;
            if (SUCCEEDED(GetFactory(L"Windows.UI.Core.SystemNavigationManager", navigationStatics)) &&
                SUCCEEDED(navigationStatics->GetForCurrentView(navigation.Put())))
            {
                auto* back = new BackHandler();
                navigation->add_BackRequested(back, &token);
                back->Release();
            }
            else
                Log("SystemNavigationManager unavailable: B may send the app to Home");

            m_window->Activate();
            Foundation::Rect bounds{};
            m_window->get_Bounds(&bounds);
            Log("CoreWindow activated, bounds %.0fx%.0f", bounds.Width, bounds.Height);

            Ref<UI::Core::ICoreDispatcher> dispatcher;
            m_window->get_Dispatcher(dispatcher.Put());

            Renderer renderer;
            bool rendering = renderer.Init(reinterpret_cast<IUnknown*>(m_window.Get()), 1920, 1080);
            SetStatus(CheckD3D12, rendering ? Status::Ok : Status::Failed);

            Ref<Gaming::Input::IGamepadStatics> gamepads;
            if (FAILED(GetFactory(L"Windows.Gaming.Input.Gamepad", gamepads)))
            {
                Log("Windows.Gaming.Input.Gamepad unavailable");
                SetStatus(CheckGamepad, Status::Failed);
            }

            std::thread probes(RunProbes, m_localState);

            UINT32 lastCount = 0;
            unsigned lastButtons = 0;
            double exitHeldSince = -1;
            while (!g_quit)
            {
                dispatcher->ProcessEvents(UI::Core::CoreProcessEventsOption_ProcessAllIfPresent);

                GamepadReading reading{};
                UINT32 count = 0;
                Ref<Foundation::Collections::IVectorView<Gaming::Input::Gamepad*>> list;
                Ref<Gaming::Input::IGamepad> pad;
                if (gamepads && SUCCEEDED(gamepads->get_Gamepads(list.Put())) && SUCCEEDED(list->get_Size(&count)) &&
                    count && SUCCEEDED(list->GetAt(0, pad.Put())))
                    pad->GetCurrentReading(&reading);
                if (count != lastCount)
                {
                    Log("gamepads connected: %u", count);
                    lastCount = count;
                    if (count)
                        SetStatus(CheckGamepad, Status::Ok);
                }

                unsigned buttons = unsigned(reading.Buttons);
                if (buttons != lastButtons)
                {
                    std::string names;
                    for (int i = 0; i < int(std::size(ButtonNames)); i++)
                        if (buttons & (1u << i))
                            names += std::string(names.empty() ? "" : "+") + ButtonNames[i];
                    Log("buttons: %s", names.empty() ? "(none)" : names.c_str());
                    lastButtons = buttons;
                }

                // View + Menu held for 2 seconds quits (the Guide button / Home also works but only suspends).
                constexpr unsigned ExitCombo = Gaming::Input::GamepadButtons_View | Gaming::Input::GamepadButtons_Menu;
                if ((buttons & ExitCombo) == ExitCombo)
                {
                    if (exitHeldSince < 0)
                        exitHeldSince = Seconds();
                    else if (Seconds() - exitHeldSince > 2.0)
                    {
                        Log("exit requested from the gamepad");
                        g_quit = true;
                    }
                }
                else
                    exitHeldSince = -1;

                if (rendering)
                {
                    float background[4];
                    std::vector<ColorRect> rects;
                    Compose(count != 0, reading, background, rects);
                    rendering = renderer.Frame(background, rects);
                }
                else
                    Sleep(16);
            }

            probes.join();
            Log("exiting");
            Ref<ICoreApplicationExit> exit;
            if (SUCCEEDED(GetFactory(L"Windows.ApplicationModel.Core.CoreApplication", exit)))
                exit->Exit();
            return S_OK;
        }

    private:
        static void Compose(bool hasPad, const GamepadReading& r, float background[4], std::vector<ColorRect>& rects)
        {
            auto pressed = [&](GamepadButtons b) { return (unsigned(r.Buttons) & unsigned(b)) != 0; };
            float red = 0.08f, green = 0.08f, blue = 0.10f;
            if (hasPad)
            {
                red = 0.0f, green = 0.22f, blue = 0.28f;
                if (pressed(Gaming::Input::GamepadButtons_A))
                    green += 0.6f;
                if (pressed(Gaming::Input::GamepadButtons_B))
                    red += 0.7f;
                if (pressed(Gaming::Input::GamepadButtons_X))
                    blue += 0.7f;
                if (pressed(Gaming::Input::GamepadButtons_Y))
                    red += 0.6f, green += 0.6f;
            }
            background[0] = std::min(red, 1.0f);
            background[1] = std::min(green, 1.0f);
            background[2] = std::min(blue, 1.0f);
            background[3] = 1.0f;

            static const float StatusColors[][4] = { { 0.35f, 0.35f, 0.35f, 1 }, { 0.1f, 0.8f, 0.2f, 1 },
                { 0.95f, 0.75f, 0.1f, 1 }, { 0.9f, 0.1f, 0.1f, 1 } };
            for (int i = 0; i < CheckCount; i++)
            {
                ColorRect rect{ { 64 + i * 96, 64, 64 + i * 96 + 64, 128 }, {} };
                memcpy(rect.color, StatusColors[int(g_status[i].load())], sizeof(rect.color));
                rects.push_back(rect);
            }
            if (!hasPad)
                return;

            const float white[4] = { 1, 1, 1, 1 };
            auto add = [&](LONG left, LONG top, LONG right, LONG bottom) {
                ColorRect rect{ { left, top, right, bottom }, {} };
                memcpy(rect.color, white, sizeof(white));
                rects.push_back(rect);
            };
            add(64, 960, 64 + LONG(r.LeftTrigger * 800), 992);
            add(1856 - LONG(r.RightTrigger * 800), 960, 1856, 992);
            LONG lx = 660 + LONG(r.LeftThumbstickX * 200), ly = 540 - LONG(r.LeftThumbstickY * 200);
            LONG rx = 1260 + LONG(r.RightThumbstickX * 200), ry = 540 - LONG(r.RightThumbstickY * 200);
            add(lx - 16, ly - 16, lx + 16, ly + 16);
            add(rx - 16, ry - 16, rx + 16, ry + 16);
        }

        std::wstring m_localState;
        Ref<ICoreWindow> m_window;
    };

    class ViewSource final : public Inspectable<IFrameworkViewSource>
    {
    public:
        explicit ViewSource(std::wstring localState) : m_localState(std::move(localState)) {}
        HRESULT STDMETHODCALLTYPE CreateView(IFrameworkView** view) override
        {
            *view = new View(m_localState);
            return S_OK;
        }

    private:
        std::wstring m_localState;
    };

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    QueryPerformanceFrequency(&g_frequency);
    QueryPerformanceCounter(&g_startTime);
    RoInitialize(RO_INIT_MULTITHREADED);

    std::wstring localState = LocalStatePath();
    if (!localState.empty())
        g_logFile = _wfopen((localState + L"\\ssa-xbox-test.log").c_str(), L"w");
    SetStatus(CheckLog, g_logFile ? Status::Ok : Status::Failed);
    Log("Arkeyan Xbox test app, built " __DATE__ " " __TIME__);
    Log("LocalState: %s", localState.empty() ? "(unavailable: no package identity?)" : Utf8(localState).c_str());

    Ref<ICoreApplication> application;
    HRESULT hr = GetFactory(L"Windows.ApplicationModel.Core.CoreApplication", application);
    if (SUCCEEDED(hr))
    {
        auto* source = new ViewSource(localState);
        hr = application->Run(source);
        source->Release();
    }
    if (FAILED(hr))
        Log("CoreApplication.Run failed: 0x%08lx", (unsigned long)hr);

    if (g_logFile)
        fclose(g_logFile);
    RoUninitialize();
    return 0;
}
