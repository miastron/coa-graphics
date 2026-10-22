#include "client_light_stay.h"
#include "client_ripple_sprites.h"
#include "config.h"
#include "d3d9_wrap.h"
#include "engine.h"
#include "fog_data.h"
#include "forever_look.h"
#include "grass_wind.h"
#include "hooks.h"
#include "image_quality.h"
#include "log.h"
#include "noise_volume.h"
#include "overlay.h"
#include "renderer.h"
#include "spell_lights.h"
#include "texture_tint.h"
#include "transparent_fog.h"
#include "water_data.h"
#include "water_renderer.h"

#include <windows.h>

#include <string>

namespace
{
void LogLightStay(LightStayOutcome outcome)
{
    char bytes[64];
    switch (outcome)
    {
    case LightStayOutcome::Applied:
        VF_LOG_INFO("light stay: je -> jmp at 0x%08X; point lights stay enabled while their model is not drawn",
                    static_cast<unsigned>(kStaleLightKeepBranch));
        return;
    case LightStayOutcome::AlreadyInClient:
        VF_LOG_INFO("light stay: the client already keeps off-screen point lights enabled (jmp at 0x%08X)",
                    static_cast<unsigned>(kStaleLightKeepBranch));
        return;
    case LightStayOutcome::CodeDiffers:
        engine::DescribeStaleLightBranch({}, bytes, sizeof(bytes));
        VF_LOG_ERROR("light stay skipped: the code at 0x%08X (%s) is not the 12340 client's je over the call to the "
                     "light enable setter 0x%08X",
                     static_cast<unsigned>(kStaleLightKeepBranch), bytes,
                     static_cast<unsigned>(kPointLightEnableSetter));
        return;
    case LightStayOutcome::WriteFailed:
        VF_LOG_ERROR("light stay skipped: the client code at 0x%08X could not be written",
                     static_cast<unsigned>(kStaleLightKeepBranch));
        return;
    }
}

std::string ModuleDirectory(HMODULE module)
{
    char path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(module, path, MAX_PATH);
    std::string dir(path, n);
    size_t slash = dir.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
}

std::string g_directory;
bool g_engineHooks = false;
bool g_waterHooks = false;

void LoadDataAtFirstDevice()
{
    GlobalFogData().Load(g_directory + "fogdata.bin");
    if (g_engineHooks)
        PrepareAuthoredNoise();
    if (g_waterHooks)
        GlobalWaterData().Load(g_directory + "waterdata.bin");
}

void Attach(HMODULE module)
{
    std::string dir = ModuleDirectory(module);
    g_directory = dir;
    LogOpen((dir + "CoAGraphics.log").c_str());
    GlobalConfig().Load(dir + "CoAGraphics.ini");
    const Config& cfg = GlobalConfig().Get();
    VF_LOG_INFO("CoAGraphics loaded from %s", dir.c_str());
    RunAtFirstDevice(&LoadDataAtFirstDevice);

    if (!engine::IsSupportedClient())
    {
        VF_LOG_INFO("host is not the 3.3.5a (12340) client; engine hooks skipped");
        return;
    }
    if (!cfg.enable)
    {
        VF_LOG_INFO("Enable=0; the client runs unmodified");
        return;
    }
    if (!cfg.hooks)
    {
        VF_LOG_INFO("EngineHooks=0; the client runs unmodified");
        return;
    }
    const bool engineHooks = InstallEngineHooks();
    g_engineHooks = engineHooks;
    AllowFogOnNewDevices(engineHooks);
    if (cfg.lightStay)
        LogLightStay(engine::KeepLightsOnOffScreen({}));
    if (engineHooks)
    {
        InstallTransparentFogHooks();
        InstallSpellLightHooks();
        InstallTreeWindHooks();
        InstallGrassWindHooks();
        EnableForeverLookOnHookedClient();
    }
    g_waterHooks = engineHooks && InstallWaterHooks();
    InstallFarClipHooks();
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        Attach(instance);
    }
    if (reason == DLL_PROCESS_DETACH)
        GlobalClientRippleSprites().Restore();
    return TRUE;
}

extern "C" int __cdecl vf_loader_anchor()
{
    return 1;
}
