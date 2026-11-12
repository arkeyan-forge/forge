// Present test (Xbox Dev Mode): which way of presenting to the CoreWindow reaches the screen. The game presents without
// errors yet the console keeps showing the splash screen. This small app (package SSARecomp.PresentTest, no disc image)
// runs presentation experiments in a loop, each a solid colour for 3 s, and logs each one with its parameters to
// LocalState\PresentTest.log. The player reports the colours seen; tools/xbox/presenttest/README.md lists them.
// Raw WinRT ABI through the mingw-w64 headers, as host/uwp/main.cpp.
#define ____FIReference_1_boolean_INTERFACE_DEFINED__

#include <windows.h>
#include <roapi.h>
#include <winstring.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <windows.applicationmodel.core.h>
#include <windows.graphics.display.h>
#include <windows.storage.h>
#include <windows.ui.core.h>
#include <windows.ui.viewmanagement.h>
#include "winrt_extra.h"
#include <plume_render_interface.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace plume
{
    std::unique_ptr<RenderInterface> CreateD3D12Interface();
}

using namespace ABI::Windows;
using ApplicationModel::Core::ICoreApplication;
using ApplicationModel::Core::ICoreApplicationView;
using ApplicationModel::Core::IFrameworkView;
using ApplicationModel::Core::IFrameworkViewSource;
using UI::Core::ICoreWindow;

namespace
{
    constexpr double ExperimentSeconds = 3.0;

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
            if (m_ptr != nullptr)
                m_ptr->Release();
            m_ptr = nullptr;
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
        ~HString() { WindowsDeleteString(m_value); }
        operator HSTRING() const { return m_value; }

    private:
        HSTRING m_value = nullptr;
    };

    template <typename T>
    HRESULT GetFactory(const wchar_t* className, Ref<T>& out)
    {
        HString name(className);
        return RoGetActivationFactory(name, __uuidof(T), out.PutVoid());
    }

    FILE* g_log = stderr;

    void Log(const char* format, ...)
    {
        va_list args;
        va_start(args, format);
        vfprintf(g_log, format, args);
        va_end(args);
        fputc('\n', g_log);
        fflush(g_log);
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
        UINT32 length = 0;
        const wchar_t* text = WindowsGetStringRawBuffer(path, &length);
        std::wstring result(text, length);
        WindowsDeleteString(path);
        return result;
    }

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

    std::atomic<bool> g_closed;
    std::atomic<bool> g_activated;

    class ClosedHandler final
        : public Unknown<Foundation::ITypedEventHandler<UI::Core::CoreWindow*, UI::Core::CoreWindowEventArgs*>>
    {
    public:
        HRESULT STDMETHODCALLTYPE Invoke(ICoreWindow*, UI::Core::ICoreWindowEventArgs*) override
        {
            g_closed = true;
            return S_OK;
        }
    };

    class ActivatedHandler final
        : public Unknown<Foundation::ITypedEventHandler<ApplicationModel::Core::CoreApplicationView*,
              ApplicationModel::Activation::IActivatedEventArgs*>>
    {
    public:
        HRESULT STDMETHODCALLTYPE Invoke(
            ICoreApplicationView* view, ApplicationModel::Activation::IActivatedEventArgs* args) override
        {
            ApplicationModel::Activation::ActivationKind kind{};
            if (args != nullptr)
                args->get_Kind(&kind);
            Ref<ICoreWindow> window;
            if (view != nullptr && SUCCEEDED(view->get_CoreWindow(window.Put())) && window)
                window->Activate();
            Log("view activated (kind %d)", int(kind));
            g_activated = true;
            return S_OK;
        }
    };

    class VisibilityHandler final
        : public Unknown<Foundation::ITypedEventHandler<UI::Core::CoreWindow*, UI::Core::VisibilityChangedEventArgs*>>
    {
    public:
        HRESULT STDMETHODCALLTYPE Invoke(ICoreWindow*, UI::Core::IVisibilityChangedEventArgs* args) override
        {
            boolean visible = false;
            if (args != nullptr)
                args->get_Visible(&visible);
            Log("window %s", visible ? "visible" : "hidden");
            return S_OK;
        }
    };

    // --- Raw D3D12, as Microsoft's UWP DirectX 12 template ---------------------------------------------------------------

    struct RawParams
    {
        DXGI_SWAP_EFFECT swapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        UINT bufferCount = 2;
        bool waitableFlag = false; // DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
        bool waitEachFrame = false; // with it: SetMaximumFrameLatency(1) and the object waited on before each frame
        bool tearingFlag = false; // DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
        bool presentTearing = false; // Present(0, DXGI_PRESENT_ALLOW_TEARING) instead of Present(1, 0)
        bool resizeAfterCreate = false; // ResizeBuffers right after creation, as the game's plume did
        bool fromWorker = false; // Present from another thread than the UI thread
    };

    struct RawDevice
    {
        Ref<IDXGIFactory4> factory;
        Ref<ID3D12Device> device;
        Ref<ID3D12CommandQueue> queue;
        Ref<ID3D12CommandAllocator> allocator;
        Ref<ID3D12GraphicsCommandList> list;
        Ref<ID3D12Fence> fence;
        UINT64 fenceValue = 0;
        HANDLE event = nullptr;
        bool tearingSupported = false;

        bool Init()
        {
            HRESULT hr;
            if (FAILED(hr = CreateDXGIFactory2(0, __uuidof(IDXGIFactory4), factory.PutVoid())))
                return Log("raw: CreateDXGIFactory2 0x%08lX", hr), false;
            if (FAILED(
                    hr = D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), device.PutVoid())))
                return Log("raw: D3D12CreateDevice 0x%08lX", hr), false;
            D3D12_COMMAND_QUEUE_DESC queueDesc{};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), queue.PutVoid());
            device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), allocator.PutVoid());
            device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, allocator.Get(), nullptr,
                __uuidof(ID3D12GraphicsCommandList), list.PutVoid());
            list->Close();
            device->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), fence.PutVoid());
            event = CreateEventExW(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
            Ref<IDXGIFactory5> factory5;
            BOOL tearing = FALSE;
            if (SUCCEEDED(factory.As(factory5)))
                factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearing, sizeof(tearing));
            tearingSupported = tearing;
            Log("raw: device and queue ready, tearing %s", tearingSupported ? "supported" : "not supported");
            return queue && allocator && list && fence;
        }

        void WaitIdle()
        {
            queue->Signal(fence.Get(), ++fenceValue);
            if (fence->GetCompletedValue() < fenceValue)
            {
                fence->SetEventOnCompletion(fenceValue, event);
                WaitForSingleObject(event, 5000);
            }
        }
    };

    struct RawSwapChain
    {
        RawDevice* d = nullptr;
        RawParams params;
        Ref<IDXGISwapChain3> swapChain;
        Ref<ID3D12DescriptorHeap> rtvHeap;
        Ref<ID3D12Resource> buffers[3];
        UINT rtvStride = 0;
        HANDLE waitable = nullptr;

        bool Create(RawDevice& device, ICoreWindow* window, UINT width, UINT height, const RawParams& p)
        {
            d = &device;
            params = p;
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = width;
            desc.Height = height;
            desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = p.bufferCount;
            desc.Scaling = DXGI_SCALING_STRETCH;
            desc.SwapEffect = p.swapEffect;
            desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            desc.Flags = (p.waitableFlag ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT : 0) |
                         (p.tearingFlag ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
            Ref<IDXGISwapChain1> swapChain1;
            HRESULT hr = d->factory->CreateSwapChainForCoreWindow(
                d->queue.Get(), reinterpret_cast<IUnknown*>(window), &desc, nullptr, swapChain1.Put());
            if (FAILED(hr))
                return Log("raw: CreateSwapChainForCoreWindow 0x%08lX", hr), false;
            swapChain1.As(swapChain);
            if (p.waitableFlag)
            {
                if (p.waitEachFrame)
                    swapChain->SetMaximumFrameLatency(1);
                waitable = swapChain->GetFrameLatencyWaitableObject();
            }
            if (p.resizeAfterCreate)
                Log("raw: ResizeBuffers after creation: 0x%08lX",
                    swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, desc.Flags));
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.NumDescriptors = p.bufferCount;
            d->device->CreateDescriptorHeap(&heapDesc, __uuidof(ID3D12DescriptorHeap), rtvHeap.PutVoid());
            rtvStride = d->device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            for (UINT i = 0; i < p.bufferCount; i++)
            {
                swapChain->GetBuffer(i, __uuidof(ID3D12Resource), buffers[i].PutVoid());
                d->device->CreateRenderTargetView(buffers[i].Get(), nullptr, Rtv(i));
            }
            return true;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE Rtv(UINT i)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            handle.ptr += SIZE_T(i) * rtvStride;
            return handle;
        }

        void Barrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
        {
            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = resource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = before;
            barrier.Transition.StateAfter = after;
            d->list->ResourceBarrier(1, &barrier);
        }

        HRESULT Frame(const float colour[4])
        {
            if (waitable != nullptr && params.waitEachFrame)
                WaitForSingleObjectEx(waitable, 1000, TRUE);
            UINT index = swapChain->GetCurrentBackBufferIndex();
            d->allocator->Reset();
            d->list->Reset(d->allocator.Get(), nullptr);
            Barrier(buffers[index].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
            d->list->ClearRenderTargetView(Rtv(index), colour, 0, nullptr);
            Barrier(buffers[index].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            d->list->Close();
            ID3D12CommandList* lists[] = { d->list.Get() };
            d->queue->ExecuteCommandLists(1, lists);
            HRESULT hr =
                params.presentTearing ? swapChain->Present(0, DXGI_PRESENT_ALLOW_TEARING) : swapChain->Present(1, 0);
            d->WaitIdle();
            return hr;
        }

        void Destroy()
        {
            if (d != nullptr)
                d->WaitIdle();
            for (auto& buffer : buffers)
                buffer.Reset();
            rtvHeap.Reset();
            if (waitable != nullptr)
                CloseHandle(waitable);
            waitable = nullptr;
            swapChain.Reset();
        }
    };

    // --- plume, exactly as runtime/gpu/gpu.cpp presents ----------------------------------------------------------------

    struct PlumePath
    {
        std::unique_ptr<plume::RenderInterface> renderInterface;
        std::unique_ptr<plume::RenderDevice> device;
        std::unique_ptr<plume::RenderCommandQueue> queue;
        std::unique_ptr<plume::RenderSwapChain> swapChain;
        std::vector<std::unique_ptr<plume::RenderFramebuffer>> framebuffers;
        std::unique_ptr<plume::RenderCommandSemaphore> acquire;
        std::vector<std::unique_ptr<plume::RenderCommandSemaphore>> release;
        std::unique_ptr<plume::RenderCommandList> list;
        std::unique_ptr<plume::RenderCommandFence> fence;

        bool Create(ICoreWindow* window, UINT width, UINT height)
        {
            using namespace plume;
            renderInterface = CreateD3D12Interface();
            if (renderInterface == nullptr || (device = renderInterface->createDevice()) == nullptr)
                return Log("plume: no device"), false;
            queue = device->createCommandQueue(RenderCommandListType::DIRECT);
            RenderWindow renderWindow{ reinterpret_cast<IUnknown*>(window), width, height };
            swapChain = queue->createSwapChain(RenderSwapChainDesc(renderWindow, RenderFormat::B8G8R8A8_UNORM, 2));
            swapChain->setVsyncEnabled(true);
            acquire = device->createCommandSemaphore();
            if (!swapChain->resize())
                return Log("plume: swap chain resize failed"), false;
            for (uint32_t i = 0; i < swapChain->getTextureCount(); i++)
            {
                const RenderTexture* color = swapChain->getTexture(i);
                RenderFramebufferDesc desc;
                desc.colorAttachments = &color;
                desc.colorAttachmentsCount = 1;
                framebuffers.push_back(device->createFramebuffer(desc));
                release.push_back(device->createCommandSemaphore());
            }
            list = queue->createCommandList();
            fence = device->createCommandFence();
            Log("plume: device %s", device->getDescription().name.c_str());
            return true;
        }

        bool Frame(const float colour[4])
        {
            using namespace plume;
            uint32_t index = 0;
            if (swapChain->isEmpty() || !swapChain->acquireTexture(acquire.get(), &index))
                return false;
            RenderTexture* target = swapChain->getTexture(index);
            list->begin();
            list->barriers(
                RenderBarrierStage::GRAPHICS, RenderTextureBarrier(target, RenderTextureLayout::COLOR_WRITE));
            list->setFramebuffer(framebuffers[index].get());
            list->clearColor(0, RenderColor(colour[0], colour[1], colour[2], colour[3]));
            list->barriers(RenderBarrierStage::NONE, RenderTextureBarrier(target, RenderTextureLayout::PRESENT));
            list->end();
            const RenderCommandList* lists = list.get();
            RenderCommandSemaphore* acquired = acquire.get();
            RenderCommandSemaphore* rendered = release[index].get();
            queue->executeCommandLists(&lists, 1, &acquired, 1, &rendered, 1, fence.get());
            bool presented = swapChain->present(index, &rendered, 1);
            queue->waitForCommandFence(fence.get());
            return presented;
        }

        void Destroy()
        {
            if (queue != nullptr && fence != nullptr)
                queue->waitForCommandFence(fence.get());
            list.reset();
            fence.reset();
            framebuffers.clear();
            release.clear();
            acquire.reset();
            swapChain.reset();
            queue.reset();
            device.reset();
            renderInterface.reset();
        }
    };

    // --- The experiments -------------------------------------------------------------------------------------------------

    enum class Path
    {
        Plume,
        Raw
    };

    struct Experiment
    {
        const char* id;
        const char* colourName;
        float colour[4];
        Path path;
        RawParams raw; // fromWorker also applies to the plume path
        bool boundsModeCoreWindow;
        const char* what;
    };

    // Raw ones first, plume last (the plume path hung after its first run in present test 1).
    const Experiment Experiments[] = {
        { "C", "BLUE", { 0, 0, 1, 1 }, Path::Raw, { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, true, true }, false,
            "raw D3D12, waitable flag waited on each frame, SetMaximumFrameLatency(1), UI thread" },
        { "D", "YELLOW", { 1, 1, 0, 1 }, Path::Raw, { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2 }, false,
            "raw D3D12 as Microsoft's UWP template: FLIP_DISCARD, 2 buffers, no flags, Present(1, 0), UI thread" },
        { "E", "MAGENTA", { 1, 0, 1, 1 }, Path::Raw, { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, false, false, true, true },
            false, "raw D3D12, ALLOW_TEARING flag, Present(0, ALLOW_TEARING), UI thread (skipped when not supported)" },
        { "F", "CYAN", { 0, 1, 1, 1 }, Path::Raw, { DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL, 2 }, false,
            "raw D3D12, FLIP_SEQUENTIAL, 2 buffers, UI thread" },
        { "G", "WHITE", { 1, 1, 1, 1 }, Path::Raw, { DXGI_SWAP_EFFECT_FLIP_DISCARD, 3 }, false,
            "raw D3D12, FLIP_DISCARD, 3 buffers, UI thread" },
        { "H", "ORANGE", { 1, 0.5f, 0, 1 }, Path::Raw,
            { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, false, false, false, false, false, true }, false,
            "raw D3D12 template, Present from a worker thread" },
        { "I", "PURPLE", { 0.5f, 0, 1, 1 }, Path::Raw, { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2 }, true,
            "raw D3D12 template after ApplicationView.SetDesiredBoundsMode(UseCoreWindow), UI thread" },
        { "A", "RED", { 1, 0, 0, 1 }, Path::Raw,
            { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, true, false, true, false, true, true }, false,
            "the game before the fix (raw D3D12 with plume's old swap chain): waitable flag not waited, ALLOW_TEARING "
            "flag, Present(1, 0), ResizeBuffers after creation, worker thread" },
        { "A2", "PINK", { 1, 0.5f, 0.75f, 1 }, Path::Raw,
            { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, false, false, true, false, true, true }, false,
            "as A without the waitable flag" },
        { "A3", "BROWN", { 0.45f, 0.25f, 0.05f, 1 }, Path::Raw,
            { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, true, false, false, false, true, true }, false,
            "as A without the tearing flag" },
        { "A4", "GREEN", { 0, 1, 0, 1 }, Path::Plume, {}, false,
            "plume as the game now: no waitable or tearing flag, Present(1, 0), no ResizeBuffers, UI thread (the fixed "
            "game path)" },
        { "A5", "TEAL", { 0, 0.5f, 0.5f, 1 }, Path::Plume,
            { DXGI_SWAP_EFFECT_FLIP_DISCARD, 2, false, false, false, false, false, true }, false,
            "as A4, Present from a worker thread" },
    };

    // Logs TIMEOUT if an experiment still runs after WatchdogSeconds (a call that never returns cannot be abandoned on
    // the UI thread, but the log then says where it stopped).
    constexpr double WatchdogSeconds = 6.0;

    class Watchdog
    {
    public:
        Watchdog(const char* id, const char* colourName)
            : m_thread([this, id, colourName] {
                  std::unique_lock lock(m_mutex);
                  if (!m_done.wait_for(
                          lock, std::chrono::milliseconds(int(WatchdogSeconds * 1000)), [this] { return m_finished; }))
                      Log("TIMEOUT %s %s: still running after %.0f s", id, colourName, WatchdogSeconds);
              })
        {}
        ~Watchdog()
        {
            {
                std::lock_guard lock(m_mutex);
                m_finished = true;
            }
            m_done.notify_all();
            m_thread.join();
        }

    private:
        std::mutex m_mutex;
        std::condition_variable m_done;
        bool m_finished = false;
        std::thread m_thread;
    };

    double Now() { return GetTickCount64() / 1000.0; }

    class View final : public Inspectable<IFrameworkView>
    {
    public:
        HRESULT STDMETHODCALLTYPE Initialize(ICoreApplicationView* view) override
        {
            EventRegistrationToken token;
            auto* activated = new ActivatedHandler();
            view->add_Activated(activated, &token);
            activated->Release();
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE SetWindow(ICoreWindow* window) override
        {
            m_window = window;
            m_window->AddRef();
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
            auto* visibility = new VisibilityHandler();
            m_window->add_VisibilityChanged(visibility, &token);
            visibility->Release();
            m_window->get_Dispatcher(m_dispatcher.Put());
            SwapChainSize();
            Log("swap chain size %ux%u", m_width, m_height);

            RawDevice raw;
            if (!raw.Init())
                Log("raw D3D12 unavailable: only the plume experiments run");

            // Experiment 0 (GREY): a swap chain created and presented before the Activated event, kept afterwards.
            RawSwapChain early;
            const float grey[4] = { 0.5f, 0.5f, 0.5f, 1 };
            bool earlyOk = raw.queue && early.Create(raw, m_window, m_width, m_height, RawParams{});
            if (earlyOk)
                Log("0 GREY: raw D3D12 template, swap chain created before the Activated event: first present 0x%08lX",
                    early.Frame(grey));
            for (int waited = 0; !g_activated && waited < 5000 && !g_closed; waited += 10)
            {
                m_dispatcher->ProcessEvents(UI::Core::CoreProcessEventsOption_ProcessAllIfPresent);
                if (!g_activated)
                    Sleep(10);
            }
            if (!g_activated)
            {
                Log("no Activated event after 5 s: activating the window anyway");
                m_window->Activate();
            }
            if (earlyOk)
            {
                RunFrames("0", "GREY", [&] { return SUCCEEDED(early.Frame(grey)); });
                early.Destroy();
                Log("0 GREY: released");
            }

            Log("experiments, %.0f s each, in this order:", ExperimentSeconds);
            for (const Experiment& e : Experiments)
                Log("  %s %s: %s", e.id, e.colourName, e.what);

            for (uint32_t cycle = 1; !g_closed; cycle++)
            {
                Log("cycle %u", cycle);
                for (const Experiment& e : Experiments)
                {
                    if (g_closed)
                        break;
                    Log("start %s %s", e.id, e.colourName);
                    Watchdog watchdog(e.id, e.colourName);
                    if (e.path == Path::Raw)
                        RunRaw(raw, e);
                    else
                        RunPlume(e);
                }
            }
            return S_OK;
        }

    private:
        // Calls `frame` for ExperimentSeconds on this (the UI) thread, pumping the window's events between frames.
        template <typename Frame>
        void RunFrames(const char* id, const char* colourName, Frame frame)
        {
            uint32_t ok = 0, failed = 0;
            for (double end = Now() + ExperimentSeconds; Now() < end && !g_closed;)
            {
                m_dispatcher->ProcessEvents(UI::Core::CoreProcessEventsOption_ProcessAllIfPresent);
                (frame() ? ok : failed)++;
            }
            Log("%s %s: %u presents succeeded, %u failed", id, colourName, ok, failed);
        }

        // The same from a worker thread while this thread only pumps events. False when the worker did not stop within
        // 3 s: it is then abandoned, and so must be what `frame` uses (the caller leaks it).
        template <typename Frame>
        bool RunFramesOnWorker(const char* id, const char* colourName, Frame frame)
        {
            struct State
            {
                std::atomic<uint32_t> ok = 0, failed = 0;
                std::atomic<bool> stop = false, stopped = false;
            };
            auto state = std::make_shared<State>();
            std::thread worker([state, frame] {
                while (!state->stop)
                    (frame() ? state->ok : state->failed)++;
                state->stopped = true;
            });
            for (double end = Now() + ExperimentSeconds; Now() < end && !g_closed;)
            {
                m_dispatcher->ProcessEvents(UI::Core::CoreProcessEventsOption_ProcessAllIfPresent);
                Sleep(5);
            }
            state->stop = true;
            for (int waited = 0; !state->stopped && waited < 3000; waited += 10)
                Sleep(10);
            Log("%s %s: %u presents succeeded, %u failed (worker thread)", id, colourName, state->ok.load(),
                state->failed.load());
            if (!state->stopped)
            {
                Log("TIMEOUT %s %s: the worker thread is stuck in a frame: abandoned", id, colourName);
                worker.detach();
                return false;
            }
            worker.join();
            return true;
        }

        void RunRaw(RawDevice& raw, const Experiment& e)
        {
            if (!raw.queue)
                return;
            if (e.raw.tearingFlag && !raw.tearingSupported)
            {
                Log("%s %s: skipped (tearing not supported)", e.id, e.colourName);
                return;
            }
            if (e.boundsModeCoreWindow)
                SetBoundsModeCoreWindow();
            auto* swapChain = new RawSwapChain();
            if (!swapChain->Create(raw, m_window, m_width, m_height, e.raw))
            {
                Log("%s %s: no swap chain", e.id, e.colourName);
                delete swapChain;
                return;
            }
            float colour[4] = { e.colour[0], e.colour[1], e.colour[2], e.colour[3] };
            auto frame = [swapChain, colour] { return SUCCEEDED(swapChain->Frame(colour)); };
            bool finished = true;
            if (e.raw.fromWorker)
                finished = RunFramesOnWorker(e.id, e.colourName, frame);
            else
                RunFrames(e.id, e.colourName, frame);
            if (!finished)
                return; // the stuck worker still uses it
            // Release everything before the next swap chain: the GPU idle, every back buffer and the swap chain.
            swapChain->Destroy();
            delete swapChain;
            Log("%s %s: released", e.id, e.colourName);
        }

        void RunPlume(const Experiment& e)
        {
            auto* plume = new PlumePath();
            if (!plume->Create(m_window, m_width, m_height))
            {
                Log("%s %s: plume path unavailable", e.id, e.colourName);
                plume->Destroy();
                delete plume;
                return;
            }
            float colour[4] = { e.colour[0], e.colour[1], e.colour[2], e.colour[3] };
            auto frame = [plume, colour] { return plume->Frame(colour); };
            bool finished = true;
            if (e.raw.fromWorker)
                finished = RunFramesOnWorker(e.id, e.colourName, frame);
            else
                RunFrames(e.id, e.colourName, frame);
            if (!finished)
                return;
            plume->Destroy();
            delete plume;
            Log("%s %s: released", e.id, e.colourName);
        }

        void SetBoundsModeCoreWindow()
        {
            Ref<UI::ViewManagement::IApplicationViewStatics2> statics;
            Ref<UI::ViewManagement::IApplicationView> view;
            Ref<UI::ViewManagement::IApplicationView2> view2;
            boolean success = false;
            HRESULT hr = E_FAIL;
            if (SUCCEEDED(GetFactory(L"Windows.UI.ViewManagement.ApplicationView", statics)) &&
                SUCCEEDED(statics->GetForCurrentView(view.Put())) && SUCCEEDED(view.As(view2)))
                hr = view2->SetDesiredBoundsMode(UI::ViewManagement::ApplicationViewBoundsMode_UseCoreWindow, &success);
            Log("SetDesiredBoundsMode(UseCoreWindow): 0x%08lX, %s", hr, success ? "accepted" : "refused");
            SwapChainSize();
        }

        // The window's bounds are in view pixels: scaled by the display's DPI (200% on Xbox).
        void SwapChainSize()
        {
            Foundation::Rect bounds{};
            m_window->get_Bounds(&bounds);
            FLOAT dpi = 96.0f;
            Ref<Graphics::Display::IDisplayInformationStatics> displayStatics;
            Ref<Graphics::Display::IDisplayInformation> display;
            if (SUCCEEDED(GetFactory(L"Windows.Graphics.Display.DisplayInformation", displayStatics)) &&
                SUCCEEDED(displayStatics->GetForCurrentView(display.Put())))
                display->get_LogicalDpi(&dpi);
            m_width = UINT(std::lround(bounds.Width * dpi / 96.0f));
            m_height = UINT(std::lround(bounds.Height * dpi / 96.0f));
            if (m_width == 0 || m_height == 0)
            {
                m_width = 1920;
                m_height = 1080;
            }
        }

        ICoreWindow* m_window = nullptr;
        Ref<UI::Core::ICoreDispatcher> m_dispatcher;
        UINT m_width = 1920, m_height = 1080;
    };

    class ViewSource final : public Inspectable<IFrameworkViewSource>
    {
    public:
        HRESULT STDMETHODCALLTYPE CreateView(IFrameworkView** view) override
        {
            *view = new View();
            return S_OK;
        }
    };
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    RoInitialize(RO_INIT_MULTITHREADED);
    std::wstring localState = LocalStatePath();
    if (!localState.empty())
    {
        _wfreopen((localState + L"\\PresentTest.log").c_str(), L"w", stderr); // with plume's messages
        setvbuf(stderr, nullptr, _IONBF, 0);
    }
    Log("Arkeyan present test");

    Ref<ICoreApplication> application;
    HRESULT result = GetFactory(L"Windows.ApplicationModel.Core.CoreApplication", application);
    if (SUCCEEDED(result))
    {
        auto* source = new ViewSource();
        result = application->Run(source);
        source->Release();
    }
    if (FAILED(result))
        Log("CoreApplication.Run failed: 0x%08lX", result);
    RoUninitialize();
    return 0;
}
