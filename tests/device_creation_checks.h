#pragma once

namespace device_creation_checks
{
using Direct3DCreate = IDirect3D9*(WINAPI*)(UINT);

constexpr DWORD kClientDeviceFlags =
    D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
constexpr UINT kWidth = 1280;
constexpr UINT kHeight = 720;
constexpr D3DFORMAT kDxvkDisplayFormats[] = {D3DFMT_X8R8G8B8, D3DFMT_X1R5G5B5, D3DFMT_R5G6B5, D3DFMT_A2R10G10B10};
constexpr int kReszDepthCopy = static_cast<int>(DepthCopyMethod::Resz);
constexpr int kAcceptDepth = 0;
constexpr int kRefuseAnyDepth = 2;

struct DeviceScript
{
    Direct3DCreate realCreate = nullptr;
    D3DFORMAT intzAdapterFormat = D3DFMT_UNKNOWN;
    D3DFORMAT stencilDepthAdapterFormat = D3DFMT_UNKNOWN;
    D3DFORMAT matchAdapterFormat = D3DFMT_UNKNOWN;
    D3DFORMAT matchTargetFormat = D3DFMT_UNKNOWN;
};

DeviceScript g_script;

bool IsDxvkDisplayFormat(D3DFORMAT format)
{
    for (D3DFORMAT display : kDxvkDisplayFormats)
        if (display == format)
            return true;
    return false;
}

void RecordFormatQuery(D3DFORMAT adapterFormat, DWORD usage, D3DRESOURCETYPE type, D3DFORMAT format)
{
    if (usage == D3DUSAGE_DEPTHSTENCIL && type == D3DRTYPE_TEXTURE && format == kIntz)
        g_script.intzAdapterFormat = adapterFormat;
    if (usage == D3DUSAGE_DEPTHSTENCIL && type == D3DRTYPE_SURFACE && format == D3DFMT_D24S8)
        g_script.stencilDepthAdapterFormat = adapterFormat;
}

class ScriptedDirect3D9 final : public IDirect3D9
{
public:
    explicit ScriptedDirect3D9(IDirect3D9* real) : m_real(real) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** out) override
    {
        if (!out)
            return E_POINTER;
        if (riid == __uuidof(IUnknown) || riid == __uuidof(IDirect3D9))
        {
            AddRef();
            *out = this;
            return S_OK;
        }
        return m_real->QueryInterface(riid, out);
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&m_ref)); }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const LONG r = InterlockedDecrement(&m_ref);
        if (r == 0)
        {
            m_real->Release();
            delete this;
        }
        return static_cast<ULONG>(r);
    }
    HRESULT STDMETHODCALLTYPE RegisterSoftwareDevice(void* init) override
    {
        return m_real->RegisterSoftwareDevice(init);
    }
    UINT STDMETHODCALLTYPE GetAdapterCount() override { return m_real->GetAdapterCount(); }
    HRESULT STDMETHODCALLTYPE GetAdapterIdentifier(UINT a, DWORD f, D3DADAPTER_IDENTIFIER9* id) override
    {
        return m_real->GetAdapterIdentifier(a, f, id);
    }
    UINT STDMETHODCALLTYPE GetAdapterModeCount(UINT a, D3DFORMAT f) override
    {
        return m_real->GetAdapterModeCount(a, f);
    }
    HRESULT STDMETHODCALLTYPE EnumAdapterModes(UINT a, D3DFORMAT f, UINT m, D3DDISPLAYMODE* mode) override
    {
        return m_real->EnumAdapterModes(a, f, m, mode);
    }
    HRESULT STDMETHODCALLTYPE GetAdapterDisplayMode(UINT a, D3DDISPLAYMODE* mode) override
    {
        return m_real->GetAdapterDisplayMode(a, mode);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceType(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT bf, BOOL w) override
    {
        return m_real->CheckDeviceType(a, t, af, bf, w);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormat(UINT a, D3DDEVTYPE t, D3DFORMAT af, DWORD u, D3DRESOURCETYPE r,
                                                D3DFORMAT f) override
    {
        RecordFormatQuery(af, u, r, f);
        if (!IsDxvkDisplayFormat(af))
            return D3DERR_NOTAVAILABLE;
        return m_real->CheckDeviceFormat(a, t, af, u, r, f);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceMultiSampleType(UINT a, D3DDEVTYPE t, D3DFORMAT f, BOOL w,
                                                         D3DMULTISAMPLE_TYPE ms, DWORD* q) override
    {
        return m_real->CheckDeviceMultiSampleType(a, t, f, w, ms, q);
    }
    HRESULT STDMETHODCALLTYPE CheckDepthStencilMatch(UINT a, D3DDEVTYPE t, D3DFORMAT af, D3DFORMAT rf,
                                                     D3DFORMAT df) override
    {
        g_script.matchAdapterFormat = af;
        g_script.matchTargetFormat = rf;
        if (!IsDxvkDisplayFormat(af))
            return D3DERR_NOTAVAILABLE;
        return m_real->CheckDepthStencilMatch(a, t, af, rf, df);
    }
    HRESULT STDMETHODCALLTYPE CheckDeviceFormatConversion(UINT a, D3DDEVTYPE t, D3DFORMAT s, D3DFORMAT d) override
    {
        return m_real->CheckDeviceFormatConversion(a, t, s, d);
    }
    HRESULT STDMETHODCALLTYPE GetDeviceCaps(UINT a, D3DDEVTYPE t, D3DCAPS9* caps) override
    {
        return m_real->GetDeviceCaps(a, t, caps);
    }
    HMONITOR STDMETHODCALLTYPE GetAdapterMonitor(UINT a) override { return m_real->GetAdapterMonitor(a); }
    HRESULT STDMETHODCALLTYPE CreateDevice(UINT a, D3DDEVTYPE t, HWND window, DWORD flags,
                                           D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out) override
    {
        D3DPRESENT_PARAMETERS shownInWindow = *pp;
        shownInWindow.Windowed = TRUE;
        shownInWindow.FullScreen_RefreshRateInHz = 0;
        const HRESULT hr = m_real->CreateDevice(a, t, window, flags, &shownInWindow, out);
        shownInWindow.Windowed = pp->Windowed;
        shownInWindow.FullScreen_RefreshRateInHz = pp->FullScreen_RefreshRateInHz;
        *pp = shownInWindow;
        return hr;
    }

private:
    ~ScriptedDirect3D9() = default;

    LONG m_ref = 1;
    IDirect3D9* m_real;
};

IDirect3D9* WINAPI CreateScriptedDirect3D9(UINT sdkVersion)
{
    IDirect3D9* real = g_script.realCreate(sdkVersion);
    return real ? new ScriptedDirect3D9(real) : nullptr;
}

IDirect3D9* WrapScriptedDirect3D9(Direct3DCreate realCreate)
{
    g_script = {};
    g_script.realCreate = realCreate;
    return vf_test_wrap_direct3d9(CreateScriptedDirect3D9, D3D_SDK_VERSION);
}

D3DPRESENT_PARAMETERS ClientParameters(HWND window, BOOL windowed, D3DFORMAT backBufferFormat,
                                       D3DMULTISAMPLE_TYPE samples)
{
    D3DPRESENT_PARAMETERS pp = {};
    pp.Windowed = windowed;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferWidth = kWidth;
    pp.BackBufferHeight = kHeight;
    pp.BackBufferFormat = backBufferFormat;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = samples;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = kClientDepthFormat;
    pp.hDeviceWindow = window;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    return pp;
}

D3DFORMAT BoundDepthFormat(IDirect3DDevice9* dev)
{
    IDirect3DSurface9* depth = nullptr;
    D3DSURFACE_DESC desc = {};
    if (FAILED(dev->GetDepthStencilSurface(&depth)) || !depth)
        return D3DFMT_UNKNOWN;
    depth->GetDesc(&desc);
    depth->Release();
    return desc.Format;
}

Config DeviceConfig(bool multisampling)
{
    Config cfg;
    cfg.overlay = false;
    cfg.multisampling = multisampling;
    return cfg;
}

void ReleaseDevice(IDirect3DDevice9*& dev)
{
    if (dev)
        dev->Release();
    dev = nullptr;
}

void CheckDisplayFormats()
{
    Check(vf_test_display_format_for(D3DFMT_A8R8G8B8) == D3DFMT_X8R8G8B8 &&
              vf_test_display_format_for(D3DFMT_A1R5G5B5) == D3DFMT_X1R5G5B5,
          "a fullscreen A8R8G8B8 or A1R5G5B5 back buffer is shown in the X8R8G8B8 or X1R5G5B5 display format");
    Check(vf_test_display_format_for(D3DFMT_X8R8G8B8) == D3DFMT_X8R8G8B8 &&
              vf_test_display_format_for(D3DFMT_R5G6B5) == D3DFMT_R5G6B5 &&
              vf_test_display_format_for(D3DFMT_A2R10G10B10) == D3DFMT_A2R10G10B10,
          "fullscreen back buffer formats that are display formats keep their format");
}

void CheckFullscreenAlphaBackBufferKeepsTheFog(Direct3DCreate realCreate, HWND window)
{
    IDirect3D9* d3d = WrapScriptedDirect3D9(realCreate);
    Config cfg = DeviceConfig(false);
    vf_test_set_config(&cfg);
    D3DPRESENT_PARAMETERS pp = ClientParameters(window, FALSE, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE);
    IDirect3DDevice9* dev = nullptr;
    const HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, kClientDeviceFlags, &pp, &dev);
    Check(SUCCEEDED(hr) && dev && g_script.intzAdapterFormat == D3DFMT_X8R8G8B8 && BoundDepthFormat(dev) == kIntz &&
              !pp.Windowed && pp.EnableAutoDepthStencil,
          "a fullscreen A8R8G8B8 device asks for INTZ with its X8R8G8B8 display format, which DXVK accepts, and "
          "binds the fog's INTZ depth");
    ReleaseDevice(dev);
    d3d->Release();
}

void CheckFullscreenAlphaBackBufferAsksForStencilDepth(Direct3DCreate realCreate, HWND window)
{
    IDirect3D9* d3d = WrapScriptedDirect3D9(realCreate);
    Config cfg = DeviceConfig(true);
    vf_test_set_config(&cfg);
    vf_test_force_depth_copy_method(kReszDepthCopy);
    D3DPRESENT_PARAMETERS pp = ClientParameters(window, FALSE, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_4_SAMPLES);
    IDirect3DDevice9* dev = nullptr;
    const HRESULT hr = d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, kClientDeviceFlags, &pp, &dev);
    vf_test_force_depth_copy_method(kDepthCopyMethodFromDriver);
    Check(SUCCEEDED(hr) && dev && g_script.stencilDepthAdapterFormat == D3DFMT_X8R8G8B8 &&
              g_script.matchAdapterFormat == D3DFMT_X8R8G8B8 && g_script.matchTargetFormat == D3DFMT_A8R8G8B8,
          "a fullscreen 4x A8R8G8B8 device asks for the D24S8 depth with its X8R8G8B8 display format and its "
          "A8R8G8B8 back buffer as the render target");
    ReleaseDevice(dev);
    d3d->Release();
}

void CheckGameKeepsItsDepthWhenNoFogDepthCanBeBound(Direct3DCreate realCreate, HWND window)
{
    IDirect3D9* d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    Config cfg = DeviceConfig(false);
    vf_test_set_config(&cfg);
    D3DPRESENT_PARAMETERS pp = ClientParameters(window, TRUE, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE);
    IDirect3DDevice9* dev = nullptr;
    vf_test_refuse_fog_depth(kRefuseAnyDepth);
    const HRESULT created = d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, kClientDeviceFlags, &pp, &dev);
    vf_test_refuse_fog_depth(kAcceptDepth);
    Check(SUCCEEDED(created) && dev && BoundDepthFormat(dev) == kClientDepthFormat && pp.EnableAutoDepthStencil &&
              pp.AutoDepthStencilFormat == kClientDepthFormat,
          "when neither the fog's INTZ depth nor a fallback depth can be created, CreateDevice gives the game a "
          "device with its own automatic depth");
    ReleaseDevice(dev);
    d3d->Release();
}

void CheckGameKeepsItsDepthThroughAResetWithoutFogDepth(Direct3DCreate realCreate, HWND window)
{
    IDirect3D9* d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    Config cfg = DeviceConfig(false);
    vf_test_set_config(&cfg);
    D3DPRESENT_PARAMETERS pp = ClientParameters(window, TRUE, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE);
    IDirect3DDevice9* dev = nullptr;
    const HRESULT created = d3d->CreateDevice(0, D3DDEVTYPE_HAL, window, kClientDeviceFlags, &pp, &dev);
    const bool fogDepthBound = dev && BoundDepthFormat(dev) == kIntz;
    vf_test_refuse_fog_depth(kRefuseAnyDepth);
    const HRESULT refused = dev ? dev->Reset(&pp) : D3DERR_INVALIDCALL;
    const D3DFORMAT keptDepth = dev ? BoundDepthFormat(dev) : D3DFMT_UNKNOWN;
    vf_test_refuse_fog_depth(kAcceptDepth);
    const HRESULT recovered = dev ? dev->Reset(&pp) : D3DERR_INVALIDCALL;
    const D3DFORMAT returnedDepth = dev ? BoundDepthFormat(dev) : D3DFMT_UNKNOWN;
    Check(SUCCEEDED(created) && fogDepthBound && SUCCEEDED(refused) && keptDepth == kClientDepthFormat &&
              SUCCEEDED(recovered) && returnedDepth == kIntz,
          "a Reset that can create neither the fog's INTZ depth nor a fallback depth leaves the game its own "
          "automatic depth, and the next Reset binds the fog's INTZ depth again");
    ReleaseDevice(dev);
    d3d->Release();
}

void CheckDeviceCreation(Direct3DCreate realCreate, HWND window)
{
    CheckDisplayFormats();
    Config before;
    vf_test_get_config(&before);
    CheckFullscreenAlphaBackBufferKeepsTheFog(realCreate, window);
    CheckFullscreenAlphaBackBufferAsksForStencilDepth(realCreate, window);
    CheckGameKeepsItsDepthWhenNoFogDepthCanBeBound(realCreate, window);
    CheckGameKeepsItsDepthThroughAResetWithoutFogDepth(realCreate, window);
    vf_test_set_config(&before);
}
}
