#include "renderer.h"

#include "d3d_util.h"
#include "engine_shadows.h"
#include "engine_terrain.h"
#include "shadow_light.h"
#include "fog_data.h"
#include "fog_model.h"
#include "frame_copies.h"
#include "fullscreen_triangle.h"
#include "light_on_screen.h"
#include "god_ray_blur.h"
#include "noise_volume.h"
#include "log.h"
#include "test_seams.h"

#include "ps_composite_low.h"
#include "ps_composite_mid.h"
#include "ps_composite_high.h"
#include "ps_lit_composite_low.h"
#include "ps_lit_composite_mid.h"
#include "ps_lit_composite_high.h"
#include "ps_lit_noisy_composite_high.h"
#include "ps_lit_noisy_composite_low.h"
#include "ps_lit_noisy_composite_mid.h"
#include "ps_lit_noisy_split_composite_high.h"
#include "ps_lit_noisy_split_composite_low.h"
#include "ps_lit_noisy_split_composite_mid.h"
#include "ps_lit_split_composite_high.h"
#include "ps_lit_split_composite_low.h"
#include "ps_lit_split_composite_mid.h"
#include "ps_lit_march_high.h"
#include "ps_lit_march_low.h"
#include "ps_lit_march_mid.h"
#include "ps_lit_noisy_march_high.h"
#include "ps_lit_noisy_march_low.h"
#include "ps_lit_noisy_march_mid.h"
#include "ps_march_high.h"
#include "ps_march_low.h"
#include "ps_march_mid.h"
#include "ps_noisy_composite_high.h"
#include "ps_noisy_composite_low.h"
#include "ps_noisy_composite_mid.h"
#include "ps_noisy_march_high.h"
#include "ps_noisy_march_low.h"
#include "ps_noisy_march_mid.h"
#include "ps_noisy_split_composite_high.h"
#include "ps_noisy_split_composite_low.h"
#include "ps_noisy_split_composite_mid.h"
#include "ps_probe.h"
#include "ps_shadowed_composite_high.h"
#include "ps_shadowed_composite_low.h"
#include "ps_shadowed_composite_mid.h"
#include "ps_shadowed_lit_march_high.h"
#include "ps_shadowed_lit_march_low.h"
#include "ps_shadowed_lit_march_mid.h"
#include "ps_shadowed_lit_noisy_march_high.h"
#include "ps_shadowed_lit_noisy_march_low.h"
#include "ps_shadowed_lit_noisy_march_mid.h"
#include "ps_shadowed_march_high.h"
#include "ps_shadowed_march_low.h"
#include "ps_shadowed_march_mid.h"
#include "ps_shadowed_noisy_march_high.h"
#include "ps_shadowed_noisy_march_low.h"
#include "ps_shadowed_noisy_march_mid.h"
#include "ps_shadowed_noisy_composite_high.h"
#include "ps_shadowed_noisy_composite_low.h"
#include "ps_shadowed_noisy_composite_mid.h"
#include "ps_shadowed_noisy_split_composite_high.h"
#include "ps_shadowed_noisy_split_composite_low.h"
#include "ps_shadowed_noisy_split_composite_mid.h"
#include "ps_shadowed_split_composite_high.h"
#include "ps_shadowed_split_composite_low.h"
#include "ps_shadowed_split_composite_mid.h"
#include "ps_ray_blur.h"
#include "ps_ray_composite.h"
#include "ps_ray_mask.h"
#include "ps_ray_shafts.h"
#include "ps_silhouette_mask.h"
#include "ps_split_composite_high.h"
#include "ps_split_composite_low.h"
#include "ps_split_composite_mid.h"
#include "ps_surface_lights.h"
#include "ps_temporal.h"
#include "ps_history_depth.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <limits>

namespace
{
constexpr UINT kPixelConstants = 105;
constexpr UINT kSurfaceLightRegister = 100;
constexpr UINT kSurfaceLightRegisters = 5;
constexpr DWORD kSceneBeforeLightsStage = 1;
constexpr DWORD kLocalLightStage = 8;
constexpr float kSurfaceDisplayGamma = 2.2f;
constexpr UINT kSampleSideRegister = 99;
constexpr int kQualityLevels = kFogQualityLevels;
constexpr int kMarchLit = 1;
constexpr int kMarchNoisy = 2;
constexpr int kMarchShadowed = 4;
constexpr int kCompositePlain = 0;
constexpr int kCompositeNoisy = 1;
constexpr int kCompositeLit = 2;
constexpr int kCompositeShadowed = 3;
constexpr int kCompositeLitNoisy = 4;
constexpr int kCompositeShadowedNoisy = 5;

template <size_t Families>
void ReleaseShaderFamilies(IDirect3DPixelShader9* (&shaders)[Families][kQualityLevels])
{
    for (auto& family : shaders)
        for (auto*& ps : family)
            SafeRelease(ps);
}

constexpr DWORD kAllStencilBits = 0xFF;
constexpr DWORD kUnmarkedSamples = 0;
constexpr DWORD kNearSamples = 1;
constexpr DWORD kFarSamples = ~kUnmarkedSamples & kAllStencilBits;
constexpr DWORD kPixelsWithoutMatchingFog = 0;
constexpr DWORD kPixelsDrawnFromMatchingFog = 1;
constexpr UINT kGodRayColourRegister = 97;
constexpr float kLeavesUnmatchedPixels = 1.0f;
constexpr float kDrawsUnmatchedPixels = 0.0f;
constexpr DWORD kStages = 15;
constexpr UINT kLayerNoiseRegister = 36;
constexpr DWORD kAuthoredNoiseStage = 10;
constexpr DWORD kFirstWorldShadowStage = 11;
constexpr UINT kSunShadowRegister = 10;
constexpr UINT kSunShadowLightRegister = 54;
constexpr UINT kViewToShadowRegister = 55;
constexpr UINT kViewToShadowRegisters = 3 * kWorldShadowMapCount;
constexpr DWORD kTerrainShadowStage = 5;
constexpr UINT kTerrainShadowRegister = 67;
constexpr int kHardwareShadowModeBit = 8;
constexpr double kMaxNoiseStepSeconds = 0.25;
constexpr UINT kRayScale = 4;
constexpr UINT kRaySourceTexelsPerRayTexel = 2;
constexpr int kShaftRays = 0;
constexpr float kMinViewportDepthExtent = 0.01f;
constexpr float kDeepestWorldDepthInFullRangeViewport = 0.9999995f;
constexpr float kWorldDepthMargin = 2.0e-6f;
constexpr float kSummarySeconds = 60.0f;
constexpr float kProbeSeconds = 60.0f;
constexpr float kProbeDebugSeconds = 30.0f;
constexpr unsigned kProbeFirstFrame = 60;
constexpr unsigned kInfoLevelProbeLimit = 5;
constexpr int kProbeGridSide = 5;
constexpr UINT kProbePoints = kProbeGridSide * kProbeGridSide;
constexpr float kRayFalloff = 8.0f;
constexpr float kRayThreshold = 0.12f;
constexpr float kHistoryMaxSeconds = 0.25f;
constexpr float kHistoryMaxMove = 30.0f;
constexpr float kHistoryMinForwardDot = 0.70710678f;
constexpr float kLoggedStormBlendSteps = 10.0f;
constexpr float kMinLoggedLightWeight = 0.005f;
constexpr unsigned kLocalLightSetSettleFrames = 30;
constexpr uint64_t kLightIdMixMultiplier = 0x9E3779B97F4A7C15ull;
constexpr unsigned kLightSetCaptureShift = 32;
constexpr size_t kCaptureRejectionText = 160;

const D3DRENDERSTATETYPE kRenderStates[] = {
    D3DRS_ZENABLE,          D3DRS_ZWRITEENABLE,  D3DRS_ALPHATESTENABLE,   D3DRS_ALPHABLENDENABLE,
    D3DRS_SRCBLEND,         D3DRS_DESTBLEND,     D3DRS_BLENDOP,           D3DRS_SEPARATEALPHABLENDENABLE,
    D3DRS_CULLMODE,         D3DRS_STENCILENABLE, D3DRS_TWOSIDEDSTENCILMODE, D3DRS_SCISSORTESTENABLE,
    D3DRS_COLORWRITEENABLE, D3DRS_SRGBWRITEENABLE, D3DRS_FOGENABLE,       D3DRS_CLIPPLANEENABLE,
    D3DRS_FILLMODE,         D3DRS_ZFUNC,         D3DRS_STENCILFUNC,       D3DRS_STENCILREF,
    D3DRS_STENCILMASK,      D3DRS_STENCILWRITEMASK, D3DRS_STENCILPASS,    D3DRS_STENCILFAIL,
    D3DRS_STENCILZFAIL,
};

const D3DSAMPLERSTATETYPE kSamplerStates[] = {
    D3DSAMP_ADDRESSU, D3DSAMP_ADDRESSV,  D3DSAMP_ADDRESSW,    D3DSAMP_MAGFILTER,
    D3DSAMP_MINFILTER, D3DSAMP_MIPFILTER, D3DSAMP_SRGBTEXTURE, D3DSAMP_MAXMIPLEVEL,
};

struct Float4
{
    float x, y, z, w;
};

void LogShaderCaps(IDirect3DDevice9* device, LogLevel level)
{
    D3DCAPS9 caps = {};
    const HRESULT result = device->GetDeviceCaps(&caps);
    if (SUCCEEDED(result))
        LogWrite(level, "shader caps: VS 0x%08lX PS 0x%08lX PS3 slots %lu executed %lu temps %d flow %d/%d",
                 caps.VertexShaderVersion, caps.PixelShaderVersion, caps.MaxPixelShader30InstructionSlots,
                 caps.MaxPShaderInstructionsExecuted, caps.PS20Caps.NumTemps,
                 caps.PS20Caps.DynamicFlowControlDepth, caps.PS20Caps.StaticFlowControlDepth);
    else
        LogWrite(level, "shader caps unavailable: HRESULT 0x%08lX", static_cast<unsigned long>(result));
}

long long Ticks()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

double TickSeconds(long long ticks)
{
    static LARGE_INTEGER freq = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return f;
    }();
    return static_cast<double>(ticks) / static_cast<double>(freq.QuadPart);
}

void Normalize3(float* v)
{
    float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1e-6f)
    {
        v[0] /= len;
        v[1] /= len;
        v[2] /= len;
    }
}

RECT ViewportRect(const D3DVIEWPORT9& vp)
{
    return {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
            static_cast<LONG>(vp.Y + vp.Height)};
}

RECT InTarget(const RECT& inViewport, const D3DVIEWPORT9& vp)
{
    const LONG x = static_cast<LONG>(vp.X);
    const LONG y = static_cast<LONG>(vp.Y);
    return {inViewport.left + x, inViewport.top + y, inViewport.right + x, inViewport.bottom + y};
}

bool WorldViewFromCameraRelative(const FrameInputs& in, float* viewToWorld, float* worldToView)
{
    if (!Invert4x4(in.cameraRelativeView, viewToWorld))
        return false;
    viewToWorld[12] = in.camPos[0];
    viewToWorld[13] = in.camPos[1];
    viewToWorld[14] = in.camPos[2];
    return Invert4x4(viewToWorld, worldToView);
}

struct WorldDepthMapping
{
    float atInfinity;
    float perInverseViewDepth;
    float deepest;
};

WorldDepthMapping MapWorldDepth(const float* proj, const D3DVIEWPORT9& vp)
{
    const bool usableRange = vp.MaxZ - vp.MinZ > kMinViewportDepthExtent && vp.MinZ >= 0.0f && vp.MaxZ <= 1.0f;
    const float worldMinZ = usableRange ? vp.MinZ : 0.0f;
    const float worldExtent = usableRange ? vp.MaxZ - vp.MinZ : 1.0f;
    const float worldMaxZ = worldMinZ + worldExtent;
    return {worldMinZ + worldExtent * (1.0f + proj[10]) * 0.5f, worldExtent * proj[14] * 0.5f,
            worldMaxZ >= kDeepestWorldDepthInFullRangeViewport ? kDeepestWorldDepthInFullRangeViewport
                                                               : worldMaxZ + kWorldDepthMargin};
}

enum class FogBlend
{
    GammaFixedFunction,
    LinearOverSceneCopy,
    LinearFixedFunction,
    GammaOverSceneCopy,
};

const char* FogBlendName(FogBlend blend)
{
    static const char* const kNames[] = {"gamma, fixed function", "linear over a scene copy",
                                         "linear, fixed function (no scene copy)", "gamma over a scene copy"};
    return kNames[static_cast<int>(blend)];
}

struct ProbeSample
{
    float rawDepth;
    float viewDepthYards;
    float fogOpacity;
    float depthClass;
};
static_assert(sizeof(ProbeSample) == sizeof(Float4), "ProbeSample is one A32B32G32R32F probe texel");

char DepthClassLetter(float depthClass)
{
    return depthClass > 1.5f ? 's' : (depthClass > 0.5f ? 'f' : 'w');
}

constexpr int kLoggedGradingInputs[] = {8, 16, 24};
constexpr UINT kMinViewportSize = 16;
constexpr float kMinGodRayStrength = 0.005f;

bool g_fogParamsForced = false;
bool g_surfaceLightsKeptInTarget = false;
bool g_surfacesLitOverTheWholeView = false;
bool g_godRaysFromTarget = false;
bool g_raySourceCopyRefused = false;
unsigned g_failedResourceKinds = 0;
unsigned g_resourceCreationAttempts[kFogNoise + 1] = {};
FogParams g_forcedFogParams = {};

bool AttemptCreation(FogResourceKind kind)
{
    if constexpr (kTestSeams)
    {
        ++g_resourceCreationAttempts[kind];
        return (g_failedResourceKinds & kind) == 0;
    }
    else
    {
        return true;
    }
}

D3DFORMAT FilterableRayFormat(IDirect3DDevice9* dev)
{
    constexpr DWORD kUsage = D3DUSAGE_RENDERTARGET | D3DUSAGE_QUERY_FILTER;
    IDirect3D9* d3d = nullptr;
    D3DDEVICE_CREATION_PARAMETERS creation = {};
    D3DDISPLAYMODE mode = {};
    D3DFORMAT format = D3DFMT_A8R8G8B8;
    if (SUCCEEDED(dev->GetDirect3D(&d3d)) && d3d)
    {
        if (SUCCEEDED(dev->GetCreationParameters(&creation)) && SUCCEEDED(dev->GetDisplayMode(0, &mode)) &&
            SUCCEEDED(d3d->CheckDeviceFormat(creation.AdapterOrdinal, creation.DeviceType, mode.Format, kUsage,
                                             D3DRTYPE_TEXTURE, D3DFMT_A16B16G16R16F)))
            format = D3DFMT_A16B16G16R16F;
        d3d->Release();
    }
    return format;
}

const char* DrawnNoiseState(const LayerNoise& drawn, const Config& cfg)
{
    if (drawn.alpha > 0.0f)
        return "";
    return cfg.classicNoise ? " (at its mean: no noise volume)" : " (off: ClassicNoise=0)";
}

void LogAuthoredExtras(const AuthoredFog& fog, const FogParams& drawn, const Config& cfg)
{
    const float* curve = fog.gradingCurve;
    VF_LOG_INFO("  Classic glow %.2f%s at coverage %.2f, grading curve%s at inputs %d/31 %d/31 %d/31: %.3f %.3f "
                "%.3f (ForeverGlow %d, ColorGrading %.2f)",
                fog.glow, fog.hasGlow ? "" : " (no glow data)", fog.coverage,
                fog.hasGradingCurve ? "" : " (no graded light)", kLoggedGradingInputs[0], kLoggedGradingInputs[1],
                kLoggedGradingInputs[2], curve[kLoggedGradingInputs[0]], curve[kLoggedGradingInputs[1]],
                curve[kLoggedGradingInputs[2]], cfg.foreverGlow, cfg.colorGrading);
    for (int i = 0; i < std::min(fog.layerCount, kSceneLayers); ++i)
    {
        const AuthoredNoise& n = fog.layers[i].noise;
        if (n.presence <= 0.0f)
            continue;
        const LayerNoise& shown = drawn.noise[i];
        VF_LOG_INFO("  classic layer %d noise: share %.2f, drawn alpha %.2f%s; octave shares %.2f/%.2f, tiles "
                    "%.0f/%.0f yd, drift (%.1f %.1f %.1f)/(%.1f %.1f %.1f) yd/s, fade %.2f %.2f %.2f",
                    i, n.presence, shown.alpha, DrawnNoiseState(shown, cfg), n.octaveShare[0], n.octaveShare[1],
                    n.tileYards[0], n.tileYards[1], n.velocity[0][0], n.velocity[0][1], n.velocity[0][2],
                    n.velocity[1][0], n.velocity[1][1], n.velocity[1][2], n.fade[0], n.fade[1], n.fade[2]);
    }
}

uint64_t MixedLightId(uintptr_t nativeId)
{
    const uint64_t mixed = (static_cast<uint64_t>(nativeId) + 1) * kLightIdMixMultiplier;
    return mixed ^ (mixed >> 29);
}

uint32_t SpellLightCount(const LocalLightInputs& lights, uint32_t uploaded)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < uploaded; ++i)
        count += lights.pointLights[i].source == LocalLightSource::Spell ? 1 : 0;
    return count;
}

uint64_t UploadedLightSet(const LocalLightInputs& lights, uint32_t uploaded)
{
    uint64_t set = uploaded - SpellLightCount(lights, uploaded) +
                   (static_cast<uint64_t>(lights.capture) << kLightSetCaptureShift);
    for (uint32_t i = 0; i < uploaded; ++i)
        if (lights.pointLights[i].source != LocalLightSource::Spell)
            set += MixedLightId(lights.pointLights[i].nativeId);
    return set;
}

void DescribeLightPool(const LocalLightInputs& lights, uint32_t uploaded, char* text, size_t size)
{
    size_t written = 0;
    text[0] = '\0';
    if (lights.candidateCount > uploaded)
        written += static_cast<size_t>(std::snprintf(text, size, ", %u in view", lights.candidateCount));
    if ((lights.skippedDisabled || lights.skippedNonPoint) && written < size)
        written += static_cast<size_t>(std::snprintf(text + written, size - written,
                                                     ", %u disabled and %u non-point nodes skipped",
                                                     lights.skippedDisabled, lights.skippedNonPoint));
    if (lights.skippedHidden && written < size)
        std::snprintf(text + written, size - written, ", %u hidden with their objects", lights.skippedHidden);
}

const char* CaptureShortfall(LocalLightCapture capture)
{
    return engine::CapturedLightsUsable(capture) ? "incomplete" : "rejected";
}

void DescribeCaptureRejection(LocalLightCapture capture, char* text, size_t size)
{
    text[0] = 0;
    if (capture != LocalLightCapture::Captured)
        std::snprintf(text, size, ", capture %s (%s)", CaptureShortfall(capture),
                      engine::LocalLightCaptureName(capture));
}

float PeakUploadedColor(const LocalPointLight& light)
{
    return std::max({light.uploadedColor[0], light.uploadedColor[1], light.uploadedColor[2]});
}

float CameraDistance(const LocalPointLight& light, const float* camera)
{
    const double dx = static_cast<double>(light.position[0]) - camera[0];
    const double dy = static_cast<double>(light.position[1]) - camera[1];
    const double dz = static_cast<double>(light.position[2]) - camera[2];
    return static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
}

void LogUploadedLight(uint32_t index, const LocalPointLight& light, const float* camera)
{
    VF_LOG_DEBUG("  local light %u: at (%.2f %.2f %.2f), %.1f yd; diffuse (%.4g %.4g %.4g), attenuation %.4g %.4g "
                 "%.4g, enabled %u; uploaded (%.4g %.4g %.4g), reach %.1f yd",
                 index, light.position[0], light.position[1], light.position[2], CameraDistance(light, camera),
                 light.color[0], light.color[1], light.color[2], light.attenuation[0], light.attenuation[1],
                 light.attenuation[2], light.enabled, light.uploadedColor[0], light.uploadedColor[1],
                 light.uploadedColor[2], light.cutoff);
}

struct CharacterMask
{
    Float4 box;
    Float4 focus;
    Float4 heights;
};

CharacterMask MaskTheCharacter(const CharacterOnScreen& character)
{
    if (!character.present)
        return {{1.0f, 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f, -1.0f}, {1.0f, 0.0f, 0.0f, 0.0f}};
    return {{character.pixelMin[0], character.pixelMin[1], character.pixelMax[0], character.pixelMax[1]},
            {character.focusFromCamera[0], character.focusFromCamera[1], character.focusFromCamera[2],
             character.reach * character.reach},
            {-kCharacterBelowFocus, kCharacterAboveFocus, 0.0f, 0.0f}};
}

const char* UnusableTargets(IDirect3DSurface9* target, IDirect3DSurface9* boundDepth, const SceneDepth& depth,
                            D3DSURFACE_DESC& depthDesc)
{
    D3DSURFACE_DESC rtDesc = {};
    if (!target)
        return "no render target";
    if (!depth.texture || boundDepth != depth.bound)
        return "fog depth surface not bound";
    if (FAILED(target->GetDesc(&rtDesc)) || FAILED(depth.bound->GetDesc(&depthDesc)))
        return "surface description failed";
    if (rtDesc.Width != depthDesc.Width || rtDesc.Height != depthDesc.Height)
        return "render target and depth sizes differ";
    if (!SameSampleCount(rtDesc, depthDesc))
        return "render target and depth sample counts differ";
    return nullptr;
}

const char* ViewportOutsideTarget(const D3DVIEWPORT9& vp, const D3DSURFACE_DESC& depthDesc)
{
    if (vp.Width < kMinViewportSize || vp.Height < kMinViewportSize || vp.X + vp.Width > depthDesc.Width ||
        vp.Y + vp.Height > depthDesc.Height)
        return "world viewport outside the render target";
    return nullptr;
}
}

uint32_t LoggedLightsSignature(const AuthoredFog& fog)
{
    uint32_t signature = 0;
    for (int i = 0; i < fog.lightCount; ++i)
        if (fog.lightWeights[i] >= kMinLoggedLightWeight)
            signature = signature * 31u + fog.lightIds[i];
    return signature;
}

FogParams FogParamsFor(const FrameInputs& in, const Config& cfg, const AuthoredFog* authored)
{
    return Seam(g_fogParamsForced, false) ? g_forcedFogParams : BuildFogParams(in, cfg, authored);
}

#ifdef VF_TESTING
void ForceFogParams(const FogParams* fog)
{
    g_fogParamsForced = fog != nullptr;
    g_forcedFogParams = fog ? *fog : FogParams{};
}

void KeepSurfaceLightsInTheTarget(bool keep)
{
    g_surfaceLightsKeptInTarget = keep;
}

void LightSurfacesOverTheWholeView(bool wholeView)
{
    g_surfacesLitOverTheWholeView = wholeView;
}

void TakeGodRaysFromTheTarget(bool fromTarget)
{
    g_godRaysFromTarget = fromTarget;
}

void RefuseRaySourceCopy(bool refused)
{
    g_raySourceCopyRefused = refused;
}

void FailFogResourceCreation(unsigned kinds)
{
    g_failedResourceKinds = kinds;
}

unsigned FogResourceCreationAttempts(FogResourceKind kind)
{
    return g_resourceCreationAttempts[kind];
}
#endif

Renderer::~Renderer()
{
    ReleaseAll();
}

void Renderer::ReleaseDefaultPool()
{
    SafeRelease(m_marchTarget);
    SafeRelease(m_history[0]);
    SafeRelease(m_history[1]);
    SafeRelease(m_historyDepth);
    SafeRelease(m_raySource);
    SafeRelease(m_rays[0]);
    SafeRelease(m_rays[1]);
    SafeRelease(m_raysOpenWithoutTheCharacter[0]);
    SafeRelease(m_raysOpenWithoutTheCharacter[1]);
    SafeRelease(m_sceneCopy);
    SafeRelease(m_sceneBeside);
    SafeRelease(m_unmatchedPixelStencil);
    m_unmatchedPixelStencilW = m_unmatchedPixelStencilH = 0;
    m_unmatchedPixelStencilFailed = false;
    m_targetRetry.Reset();
    m_authoredNoiseRetry.Reset();
    m_densityNoiseRetry.Reset();
    m_localLightRetry.Reset();
    DropPendingDepthProbe();
    SafeRelease(m_probeTarget);
    SafeRelease(m_probeReadback);
    SafeRelease(m_probeCopied);
    m_gpuTimer.Release();
    for (GpuTimer* timer : PassTimers())
        timer->Release();
    m_lateGodRays = {};
    SafeRelease(m_state);
    SafeRelease(m_localLightData);
    m_lowW = m_lowH = m_rayW = m_rayH = 0;
    m_sceneCopyW = m_sceneCopyH = 0;
    m_sceneCopyFailed = false;
    m_probeFailed = false;
    m_historyValid = false;
    m_adaptiveLightingHistory = false;
    m_prevLocalLightCount = 0;
}

void Renderer::ReleaseManagedPool()
{
    SafeRelease(m_densityNoise);
    SafeRelease(m_authoredNoise);
    m_terrainShadows.ReleaseTexture();
}

void Renderer::ReleaseAll()
{
    ReleaseDefaultPool();
    ReleaseManagedPool();
    m_unsupportedShaderDevice = nullptr;
    m_drawnMarch = nullptr;
    m_drawnComposite = nullptr;
    m_drawnSplitComposite = nullptr;
    SafeRelease(m_vs);
    ReleaseShaderFamilies(m_marchShaders);
    SafeRelease(m_temporal);
    SafeRelease(m_historyDepthShader);
    ReleaseShaderFamilies(m_compositeShaders);
    ReleaseSplitComposites();
    m_splitCompositesUnavailable = false;
    SafeRelease(m_rayMask);
    SafeRelease(m_rayBlur);
    SafeRelease(m_rayShafts);
    SafeRelease(m_rayComposite);
    SafeRelease(m_surfaceLights);
    SafeRelease(m_probe);
    SafeRelease(m_decl);
}

void Renderer::LogLightChange(const FrameInputs& in, const AuthoredFog& fog, bool authored)
{
    const LightParamsSelection& selection = in.lightParams;
    const auto loggedStormStep = static_cast<uint32_t>(std::lround(selection.stormBlend * kLoggedStormBlendSteps));
    uint32_t signature = (authored ? 0x80000000u : 0u) ^ LoggedLightsSignature(fog);
    signature = signature * 31u + loggedStormStep;
    signature = signature * 31u + static_cast<uint32_t>(selection.screenEffectSlot);
    signature ^= static_cast<uint32_t>(in.mapId) << 20;
    if (m_lightsLogged && signature == m_lightSignature)
        return;
    m_lightsLogged = true;
    m_lightSignature = signature;
    char lights[160] = {};
    int used = 0;
    for (int i = 0; i < fog.lightCount && used < static_cast<int>(sizeof(lights)) - 24; ++i)
        if (fog.lightWeights[i] >= kMinLoggedLightWeight)
            used += std::snprintf(lights + used, sizeof(lights) - used, "%s%u:%.2f", used ? " " : "",
                                  fog.lightIds[i], fog.lightWeights[i]);
    if (!authored && fog.lightCount == 0)
    {
        VF_LOG_INFO("map %d at (%.0f %.0f %.0f): no Classic fog data, derived layers", in.mapId, in.camPos[0],
                    in.camPos[1], in.camPos[2]);
        return;
    }
    if (!authored)
    {
        VF_LOG_INFO("map %d at (%.0f %.0f %.0f): Classic lights %s (coverage %.2f) without Classic fog, derived layers",
                    in.mapId, in.camPos[0], in.camPos[1], in.camPos[2], lights, fog.coverage);
        return;
    }
    VF_LOG_INFO("map %d at (%.0f %.0f %.0f): Classic lights %s, %d layers, storm %.1f, screen effect slot %d",
                in.mapId, in.camPos[0], in.camPos[1], in.camPos[2], lights, fog.layerCount,
                loggedStormStep / kLoggedStormBlendSteps, selection.screenEffectSlot);
}

void Renderer::LogFirstLocalLightRejection(LocalLightCapture capture)
{
    const uint32_t reason = 1u << static_cast<uint32_t>(capture);
    if (capture == LocalLightCapture::Captured || (m_loggedLocalLightRejections & reason))
        return;
    m_loggedLocalLightRejections |= reason;
    VF_LOG_INFO("local lights: capture %s (%s), first on frame %u", CaptureShortfall(capture),
                engine::LocalLightCaptureName(capture), m_frame);
}

void Renderer::LogUploadedLocalLights(const FrameInputs& in, const Config& cfg, uint32_t uploaded)
{
    if (!LogEnabled(LogLevel::Info))
        return;
    LogFirstLocalLightRejection(in.localLights.capture);
    const uint64_t set = UploadedLightSet(in.localLights, uploaded);
    if (set != m_pendingLocalLightSet)
    {
        m_pendingLocalLightSet = set;
        m_pendingLocalLightFrames = 0;
    }
    const bool detailed = LogEnabled(LogLevel::Debug);
    const bool alreadyLogged = set == m_loggedLocalLightSet && (m_loggedLocalLightDetail || !detailed);
    if (alreadyLogged || ++m_pendingLocalLightFrames < kLocalLightSetSettleFrames)
        return;
    m_loggedLocalLightSet = set;
    m_loggedLocalLightDetail = detailed || uploaded == 0;
    if (uploaded == 0)
    {
        char rejection[kCaptureRejectionText];
        DescribeCaptureRejection(in.localLights.capture, rejection, sizeof(rejection));
        VF_LOG_INFO("local lights: none uploaded%s%s", cfg.localLights ? "" : " (LocalLights=0)", rejection);
        return;
    }
    const LocalPointLight* lights = in.localLights.pointLights;
    uint32_t brightest = 0;
    float nearest = std::numeric_limits<float>::infinity();
    for (uint32_t i = 0; i < uploaded; ++i)
    {
        if (PeakUploadedColor(lights[i]) > PeakUploadedColor(lights[brightest]))
            brightest = i;
        nearest = std::min(nearest, CameraDistance(lights[i], in.camPos));
    }
    const float* colour = lights[brightest].uploadedColor;
    char pool[kCaptureRejectionText];
    DescribeLightPool(in.localLights, uploaded, pool, sizeof(pool));
    const PointLightUpload upload = LocalLightUpload(cfg, in.mapId);
    VF_LOG_INFO("local lights: %u uploaded (%u from spells), brightest %s (%.4g %.4g %.4g), nearest %.1f yd, reach "
                "x%.3g%s",
                uploaded, SpellLightCount(in.localLights, uploaded), upload.linear ? "linear" : "gamma", colour[0],
                colour[1], colour[2], nearest, upload.reach, pool);
    if (detailed)
        for (uint32_t i = 0; i < uploaded; ++i)
            LogUploadedLight(i, lights[i], in.camPos);
}

bool Renderer::Skip(const char* reason)
{
    m_skip = reason;
    m_historyValid = false;
    return false;
}

bool Renderer::NotReady(const char* reason)
{
    m_skip = reason;
    return false;
}

bool Renderer::EnsureShaders(IDirect3DDevice9* dev)
{
    if (m_unsupportedShaderDevice == dev)
        return Skip("required shader unsupported");
    if (m_vs)
        return true;
    if (!m_shaderRetry.Ready())
        return Skip("shader creation failed");
    struct PixelShaderRequest
    {
        const char* name;
        const BYTE* code;
        IDirect3DPixelShader9** output;
    };
    const PixelShaderRequest pixels[] = {
        {"ps_march_low", g_ps_march_low, &m_marchShaders[0][0]},
        {"ps_march_mid", g_ps_march_mid, &m_marchShaders[0][1]},
        {"ps_march_high", g_ps_march_high, &m_marchShaders[0][2]},
        {"ps_lit_march_low", g_ps_lit_march_low, &m_marchShaders[1][0]},
        {"ps_lit_march_mid", g_ps_lit_march_mid, &m_marchShaders[1][1]},
        {"ps_lit_march_high", g_ps_lit_march_high, &m_marchShaders[1][2]},
        {"ps_noisy_march_low", g_ps_noisy_march_low, &m_marchShaders[2][0]},
        {"ps_noisy_march_mid", g_ps_noisy_march_mid, &m_marchShaders[2][1]},
        {"ps_noisy_march_high", g_ps_noisy_march_high, &m_marchShaders[2][2]},
        {"ps_lit_noisy_march_low", g_ps_lit_noisy_march_low, &m_marchShaders[3][0]},
        {"ps_lit_noisy_march_mid", g_ps_lit_noisy_march_mid, &m_marchShaders[3][1]},
        {"ps_lit_noisy_march_high", g_ps_lit_noisy_march_high, &m_marchShaders[3][2]},
        {"ps_shadowed_march_low", g_ps_shadowed_march_low, &m_marchShaders[4][0]},
        {"ps_shadowed_march_mid", g_ps_shadowed_march_mid, &m_marchShaders[4][1]},
        {"ps_shadowed_march_high", g_ps_shadowed_march_high, &m_marchShaders[4][2]},
        {"ps_shadowed_noisy_march_low", g_ps_shadowed_noisy_march_low, &m_marchShaders[6][0]},
        {"ps_shadowed_noisy_march_mid", g_ps_shadowed_noisy_march_mid, &m_marchShaders[6][1]},
        {"ps_shadowed_noisy_march_high", g_ps_shadowed_noisy_march_high, &m_marchShaders[6][2]},
        {"ps_shadowed_lit_march_low", g_ps_shadowed_lit_march_low, &m_marchShaders[5][0]},
        {"ps_shadowed_lit_march_mid", g_ps_shadowed_lit_march_mid, &m_marchShaders[5][1]},
        {"ps_shadowed_lit_march_high", g_ps_shadowed_lit_march_high, &m_marchShaders[5][2]},
        {"ps_shadowed_lit_noisy_march_low", g_ps_shadowed_lit_noisy_march_low, &m_marchShaders[7][0]},
        {"ps_shadowed_lit_noisy_march_mid", g_ps_shadowed_lit_noisy_march_mid, &m_marchShaders[7][1]},
        {"ps_shadowed_lit_noisy_march_high", g_ps_shadowed_lit_noisy_march_high, &m_marchShaders[7][2]},
        {"ps_temporal", g_ps_temporal, &m_temporal},
        {"ps_history_depth", g_ps_history_depth, &m_historyDepthShader},
        {"ps_composite_low", g_ps_composite_low, &m_compositeShaders[0][0]},
        {"ps_composite_mid", g_ps_composite_mid, &m_compositeShaders[0][1]},
        {"ps_composite_high", g_ps_composite_high, &m_compositeShaders[0][2]},
        {"ps_noisy_composite_low", g_ps_noisy_composite_low, &m_compositeShaders[1][0]},
        {"ps_noisy_composite_mid", g_ps_noisy_composite_mid, &m_compositeShaders[1][1]},
        {"ps_noisy_composite_high", g_ps_noisy_composite_high, &m_compositeShaders[1][2]},
        {"ps_lit_composite_low", g_ps_lit_composite_low, &m_compositeShaders[2][0]},
        {"ps_lit_composite_mid", g_ps_lit_composite_mid, &m_compositeShaders[2][1]},
        {"ps_lit_composite_high", g_ps_lit_composite_high, &m_compositeShaders[2][2]},
        {"ps_shadowed_composite_low", g_ps_shadowed_composite_low, &m_compositeShaders[3][0]},
        {"ps_shadowed_composite_mid", g_ps_shadowed_composite_mid, &m_compositeShaders[3][1]},
        {"ps_shadowed_composite_high", g_ps_shadowed_composite_high, &m_compositeShaders[3][2]},
        {"ps_lit_noisy_composite_low", g_ps_lit_noisy_composite_low, &m_compositeShaders[4][0]},
        {"ps_lit_noisy_composite_mid", g_ps_lit_noisy_composite_mid, &m_compositeShaders[4][1]},
        {"ps_lit_noisy_composite_high", g_ps_lit_noisy_composite_high, &m_compositeShaders[4][2]},
        {"ps_shadowed_noisy_composite_low", g_ps_shadowed_noisy_composite_low, &m_compositeShaders[5][0]},
        {"ps_shadowed_noisy_composite_mid", g_ps_shadowed_noisy_composite_mid, &m_compositeShaders[5][1]},
        {"ps_shadowed_noisy_composite_high", g_ps_shadowed_noisy_composite_high, &m_compositeShaders[5][2]},
        {"ps_ray_mask", g_ps_ray_mask, &m_rayMask},
        {"ps_ray_blur", g_ps_ray_blur, &m_rayBlur},
        {"ps_ray_shafts", g_ps_ray_shafts, &m_rayShafts},
        {"ps_ray_composite", g_ps_ray_composite, &m_rayComposite},
        {"ps_surface_lights", g_ps_surface_lights, &m_surfaceLights},
    };
    const char* failedName = "vs_fullscreen";
    LARGE_INTEGER creationStart;
    QueryPerformanceCounter(&creationStart);
    HRESULT result = AttemptCreation(kFogShaders)
                         ? CreateFullscreenVertexShader(dev, &m_vs)
                         : D3DERR_OUTOFVIDEOMEMORY;
    if (SUCCEEDED(result))
        for (const PixelShaderRequest& shader : pixels)
        {
            failedName = shader.name;
            result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(shader.code), shader.output);
            if (FAILED(result))
                break;
        }
    if (SUCCEEDED(result))
    {
        failedName = "fullscreen vertex declaration";
        result = CreateFullscreenVertexDeclaration(dev, &m_decl);
    }
    if (FAILED(result))
    {
        const bool unsupported = (result == D3DERR_INVALIDCALL || result == D3DERR_NOTAVAILABLE ||
                                  result == E_INVALIDARG) && dev->TestCooperativeLevel() == D3D_OK;
        VF_LOG_ERROR("shader initialization failed: %s HRESULT 0x%08lX; %s", failedName,
                     static_cast<unsigned long>(result), unsupported ? "unsupported on this device" : "will retry");
        LogShaderCaps(dev, LogLevel::Error);
        ReleaseAll();
        if (unsupported)
            m_unsupportedShaderDevice = dev;
        else
            m_shaderRetry.Failed();
        return Skip(unsupported ? "required shader unsupported" : "shader creation failed");
    }
    LARGE_INTEGER creationEnd;
    LARGE_INTEGER frequency;
    QueryPerformanceCounter(&creationEnd);
    QueryPerformanceFrequency(&frequency);
    VF_LOG_INFO("fog shaders: %zu pixel shaders created in %.1f ms", std::size(pixels),
                1000.0 * static_cast<double>(creationEnd.QuadPart - creationStart.QuadPart) /
                    static_cast<double>(frequency.QuadPart));
    LogShaderCaps(dev, LogLevel::Info);
    return true;
}

void Renderer::ReleaseSplitComposites()
{
    SafeRelease(m_silhouetteMask);
    ReleaseShaderFamilies(m_splitCompositeShaders);
}

bool Renderer::EnsureSplitComposites(IDirect3DDevice9* dev)
{
    if (m_silhouetteMask)
        return true;
    if (m_splitCompositesUnavailable)
        return false;
    const BYTE* const code[][kQualityLevels] = {
        {g_ps_split_composite_low, g_ps_split_composite_mid, g_ps_split_composite_high},
        {g_ps_noisy_split_composite_low, g_ps_noisy_split_composite_mid, g_ps_noisy_split_composite_high},
        {g_ps_lit_split_composite_low, g_ps_lit_split_composite_mid, g_ps_lit_split_composite_high},
        {g_ps_shadowed_split_composite_low, g_ps_shadowed_split_composite_mid, g_ps_shadowed_split_composite_high},
        {g_ps_lit_noisy_split_composite_low, g_ps_lit_noisy_split_composite_mid, g_ps_lit_noisy_split_composite_high},
        {g_ps_shadowed_noisy_split_composite_low, g_ps_shadowed_noisy_split_composite_mid,
         g_ps_shadowed_noisy_split_composite_high},
    };
    static_assert(std::size(code) == kCompositeFamilies, "one split composite array per shader family");
    HRESULT result = D3D_OK;
    for (size_t family = 0; family < kCompositeFamilies && SUCCEEDED(result); ++family)
        for (int quality = 0; quality < kQualityLevels && SUCCEEDED(result); ++quality)
            result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(code[family][quality]),
                                            &m_splitCompositeShaders[family][quality]);
    if (SUCCEEDED(result))
        result = dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_silhouette_mask), &m_silhouetteMask);
    if (SUCCEEDED(result))
        return true;
    ReleaseSplitComposites();
    if (dev->TestCooperativeLevel() == D3D_OK)
    {
        m_splitCompositesUnavailable = true;
        VF_LOG_ERROR("the sample-split composite could not be created (HRESULT 0x%08lX); multisampled silhouettes take "
                     "the fog of one sample",
                     static_cast<unsigned long>(result));
    }
    return false;
}

IDirect3DPixelShader9* const* Renderer::MarchShaders(bool shadowed, bool lit, bool noisy) const
{
    return m_marchShaders[(lit ? kMarchLit : 0) | (noisy ? kMarchNoisy : 0) | (shadowed ? kMarchShadowed : 0)];
}

IDirect3DPixelShader9* Renderer::CompositeShader(const CompositeChoice& choice, bool splitSamples) const
{
    const int index = std::clamp(choice.quality, 1, kQualityLevels) - 1;
    const int family = choice.shadowed ? (choice.noisy ? kCompositeShadowedNoisy : kCompositeShadowed)
                       : choice.lit    ? (choice.noisy ? kCompositeLitNoisy : kCompositeLit)
                       : choice.noisy  ? kCompositeNoisy
                                       : kCompositePlain;
    return (splitSamples ? m_splitCompositeShaders : m_compositeShaders)[family][index];
}

void Renderer::MarkSilhouetteSamples(IDirect3DDevice9* dev, IDirect3DSurface9* sampleDepth, const D3DVIEWPORT9& vp)
{
    D3DVIEWPORT9 fullDepthRange = vp;
    fullDepthRange.MinZ = 0.0f;
    fullDepthRange.MaxZ = 1.0f;
    const D3DRECT world = {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
                           static_cast<LONG>(vp.Y + vp.Height)};
    DWORD colourWrites = 0;
    dev->GetRenderState(D3DRS_COLORWRITEENABLE, &colourWrites);
    dev->SetDepthStencilSurface(sampleDepth);
    dev->SetViewport(&fullDepthRange);
    dev->Clear(1, &world, D3DCLEAR_STENCIL, 0, 1.0f, kUnmarkedSamples);
    dev->SetPixelShader(m_silhouetteMask);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
    dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
    dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    dev->SetRenderState(D3DRS_STENCILREF, kNearSamples);
    dev->SetRenderState(D3DRS_STENCILMASK, kAllStencilBits);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, kAllStencilBits);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_INVERT);
    dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_REPLACE);
    dev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    DrawFullscreenTriangle(dev);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, colourWrites);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
}

void Renderer::DrawSamplesMarked(IDirect3DDevice9* dev, DWORD marker, IDirect3DPixelShader9* shader,
                                 const float* side)
{
    dev->SetRenderState(D3DRS_STENCILREF, marker);
    dev->SetPixelShader(shader);
    if (side)
        dev->SetPixelShaderConstantF(kSampleSideRegister, side, 1);
    DrawFullscreenTriangle(dev);
}

void Renderer::DrawCompositeBySampleDepth(IDirect3DDevice9* dev, IDirect3DSurface9* sampleDepth,
                                          const D3DVIEWPORT9& vp, const CompositeChoice& choice, bool overwrites)
{
    const float ownSideDrawn = overwrites ? 1.0f : 0.0f;
    const Float4 nearSide = {0.0f, ownSideDrawn, 0.0f, 0.0f};
    const Float4 farSide = {1.0f, ownSideDrawn, 0.0f, 0.0f};
    MarkSilhouetteSamples(dev, sampleDepth, vp);
    if (overwrites)
    {
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        dev->SetDepthStencilSurface(nullptr);
        dev->SetPixelShader(CompositeShader(choice, false));
        DrawFullscreenTriangle(dev);
        dev->SetDepthStencilSurface(sampleDepth);
        dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    }
    else
        DrawSamplesMarked(dev, kUnmarkedSamples, CompositeShader(choice, false), nullptr);
    DrawSamplesMarked(dev, kNearSamples, CompositeShader(choice, true), &nearSide.x);
    DrawSamplesMarked(dev, kFarSamples, CompositeShader(choice, true), &farSide.x);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetDepthStencilSurface(nullptr);
    dev->SetViewport(&vp);
}

bool Renderer::EnsureUnmatchedPixelStencil(IDirect3DDevice9* dev, UINT w, UINT h)
{
    if (m_unmatchedPixelStencil && m_unmatchedPixelStencilW == w && m_unmatchedPixelStencilH == h)
        return true;
    SafeRelease(m_unmatchedPixelStencil);
    m_unmatchedPixelStencilW = m_unmatchedPixelStencilH = 0;
    if (m_unmatchedPixelStencilFailed)
        return false;
    if (FAILED(dev->CreateDepthStencilSurface(w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE,
                                              &m_unmatchedPixelStencil, nullptr)))
    {
        m_unmatchedPixelStencilFailed = true;
        VF_LOG_ERROR("the stencil that sets apart composite pixels without matching fog could not be created "
                     "(%ux%u); the composite marches them in its one pass",
                     w, h);
        return false;
    }
    m_unmatchedPixelStencilW = w;
    m_unmatchedPixelStencilH = h;
    return true;
}

void Renderer::DrawCompositeWithUnmatchedPixelsApart(IDirect3DDevice9* dev, const D3DVIEWPORT9& vp,
                                                     const float* godRayColour)
{
    const Float4 leavingUnmatchedPixels = {godRayColour[0], godRayColour[1], godRayColour[2], kLeavesUnmatchedPixels};
    const Float4 drawingUnmatchedPixels = {godRayColour[0], godRayColour[1], godRayColour[2], kDrawsUnmatchedPixels};
    const D3DRECT world = {static_cast<LONG>(vp.X), static_cast<LONG>(vp.Y), static_cast<LONG>(vp.X + vp.Width),
                           static_cast<LONG>(vp.Y + vp.Height)};
    dev->SetDepthStencilSurface(m_unmatchedPixelStencil);
    dev->Clear(1, &world, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, kPixelsWithoutMatchingFog);
    dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_ALWAYS);
    dev->SetRenderState(D3DRS_STENCILREF, kPixelsDrawnFromMatchingFog);
    dev->SetRenderState(D3DRS_STENCILMASK, kAllStencilBits);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, kAllStencilBits);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_REPLACE);
    dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
    dev->SetPixelShaderConstantF(kGodRayColourRegister, &leavingUnmatchedPixels.x, 1);
    DrawFullscreenTriangle(dev);
    dev->SetRenderState(D3DRS_STENCILFUNC, D3DCMP_EQUAL);
    dev->SetRenderState(D3DRS_STENCILREF, kPixelsWithoutMatchingFog);
    dev->SetRenderState(D3DRS_STENCILPASS, D3DSTENCILOP_KEEP);
    dev->SetRenderState(D3DRS_STENCILWRITEMASK, 0);
    dev->SetPixelShaderConstantF(kGodRayColourRegister, &drawingUnmatchedPixels.x, 1);
    DrawFullscreenTriangle(dev);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetDepthStencilSurface(nullptr);
}

bool Renderer::EnsureStateBlock(IDirect3DDevice9* dev)
{
    if (m_state)
        return true;
    if (FAILED(dev->BeginStateBlock()))
        return Skip("state block recording failed");
    for (D3DRENDERSTATETYPE rs : kRenderStates)
        dev->SetRenderState(rs, 0);
    for (DWORD stage = 0; stage < kStages; ++stage)
    {
        dev->SetTexture(stage, nullptr);
        for (D3DSAMPLERSTATETYPE ss : kSamplerStates)
            dev->SetSamplerState(stage, ss, 0);
    }
    float zeros[kPixelConstants * 4] = {};
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
    dev->SetPixelShaderConstantF(0, zeros, kPixelConstants);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSource(0, nullptr, 0, 0);
    dev->SetStreamSourceFreq(0, 1);
    D3DVIEWPORT9 vp = {0, 0, 1, 1, 0.0f, 1.0f};
    dev->SetViewport(&vp);
    RECT scissor = {0, 0, 1, 1};
    dev->SetScissorRect(&scissor);
    if (FAILED(dev->EndStateBlock(&m_state)) || !m_state)
    {
        m_state = nullptr;
        return Skip("state block recording failed");
    }
    return true;
}

bool Renderer::EnsureTargets(IDirect3DDevice9* dev, UINT lowW, UINT lowH, UINT rayW, UINT rayH)
{
    if (m_marchTarget && m_lowW == lowW && m_lowH == lowH && m_rayW == rayW && m_rayH == rayH)
        return true;
    if (!m_targetRetry.Ready())
        return Skip("render target creation failed");
    SafeRelease(m_marchTarget);
    SafeRelease(m_history[0]);
    SafeRelease(m_history[1]);
    SafeRelease(m_historyDepth);
    SafeRelease(m_raySource);
    SafeRelease(m_rays[0]);
    SafeRelease(m_rays[1]);
    SafeRelease(m_raysOpenWithoutTheCharacter[0]);
    SafeRelease(m_raysOpenWithoutTheCharacter[1]);
    m_historyValid = false;

    D3DFORMAT fogFormat = D3DFMT_A16B16G16R16F;
    const bool attempted = AttemptCreation(kFogTargets);
    bool ok = attempted && CreateTarget(dev, lowW, lowH, fogFormat, &m_marchTarget);
    if (attempted && !ok)
    {
        fogFormat = D3DFMT_A8R8G8B8;
        ok = CreateTarget(dev, lowW, lowH, fogFormat, &m_marchTarget);
    }
    const D3DFORMAT rayFormat = FilterableRayFormat(dev);
    ok = ok && CreateTarget(dev, lowW, lowH, fogFormat, &m_history[0]) &&
         CreateTarget(dev, lowW, lowH, fogFormat, &m_history[1]) &&
         CreateTarget(dev, lowW, lowH, D3DFMT_A8R8G8B8, &m_historyDepth) &&
         CreateTarget(dev, rayW * kRaySourceTexelsPerRayTexel, rayH * kRaySourceTexelsPerRayTexel, D3DFMT_A8R8G8B8,
                      &m_raySource) &&
         CreateTarget(dev, rayW, rayH, rayFormat, &m_rays[0]) &&
         CreateTarget(dev, rayW, rayH, rayFormat, &m_rays[1]);
    if (!ok)
    {
        VF_LOG_ERROR("render target creation failed (%ux%u)", lowW, lowH);
        SafeRelease(m_marchTarget);
        SafeRelease(m_history[0]);
        SafeRelease(m_history[1]);
        SafeRelease(m_historyDepth);
        SafeRelease(m_raySource);
        SafeRelease(m_rays[0]);
        SafeRelease(m_rays[1]);
        SafeRelease(m_raysOpenWithoutTheCharacter[0]);
        SafeRelease(m_raysOpenWithoutTheCharacter[1]);
        m_targetRetry.Failed();
        return Skip("render target creation failed");
    }

    if (SupportsTwoTargets(dev) &&
        !(CreateTarget(dev, rayW, rayH, rayFormat, &m_raysOpenWithoutTheCharacter[0]) &&
          CreateTarget(dev, rayW, rayH, rayFormat, &m_raysOpenWithoutTheCharacter[1])))
    {
        SafeRelease(m_raysOpenWithoutTheCharacter[0]);
        SafeRelease(m_raysOpenWithoutTheCharacter[1]);
    }
    if (!m_raysOpenWithoutTheCharacter[0])
        VF_LOG_INFO("god rays: no second ray target, so the own character casts shafts");

    m_lowW = lowW;
    m_lowH = lowH;
    m_rayW = rayW;
    m_rayH = rayH;
    VF_LOG_INFO("targets: fog %ux%u (%s), depth history rgba8, rays %ux%u (%s) from a %ux%u scene copy", lowW, lowH,
                fogFormat == D3DFMT_A16B16G16R16F ? "fp16" : "rgba8", rayW, rayH,
                rayFormat == D3DFMT_A16B16G16R16F ? "fp16" : "rgba8", rayW * kRaySourceTexelsPerRayTexel,
                rayH * kRaySourceTexelsPerRayTexel);
    return true;
}

bool Renderer::EnsureSceneCopy(IDirect3DDevice9* dev, IDirect3DSurface9* target, UINT w, UINT h)
{
    if (m_sceneCopy && m_sceneCopyW == w && m_sceneCopyH == h)
        return true;
    SafeRelease(m_sceneCopy);
    SafeRelease(m_sceneBeside);
    m_sceneCopyW = m_sceneCopyH = 0;
    if (m_sceneCopyFailed)
        return false;
    D3DSURFACE_DESC desc = {};
    target->GetDesc(&desc);
    if (!CreateTarget(dev, w, h, desc.Format, &m_sceneCopy) &&
        !CreateTarget(dev, w, h, D3DFMT_A8R8G8B8, &m_sceneCopy))
    {
        m_sceneCopyFailed = true;
        VF_LOG_ERROR("scene copy creation failed (%ux%u); fog blends in gamma space", w, h);
        return false;
    }
    m_sceneCopyW = w;
    m_sceneCopyH = h;
    return true;
}

bool Renderer::EnsureSceneBeside(IDirect3DDevice9* dev)
{
    if (m_sceneBeside)
        return true;
    D3DSURFACE_DESC desc = {};
    return m_sceneCopy && SUCCEEDED(m_sceneCopy->GetLevelDesc(0, &desc)) &&
           CreateTarget(dev, m_sceneCopyW, m_sceneCopyH, desc.Format, &m_sceneBeside);
}

bool Renderer::CopyWorldViewport(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp,
                                 ColourCopySite site)
{
    return CopyWorldArea(dev, target, vp, WholeViewport(vp).inViewport, site);
}

bool Renderer::CopyWorldArea(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp,
                             const RECT& inViewport, ColourCopySite site)
{
    if (!EnsureSceneCopy(dev, target, vp.Width, vp.Height))
        return false;
    const RECT inTarget = InTarget(inViewport, vp);
    IDirect3DSurface9* sceneSurface = nullptr;
    const bool copied = SUCCEEDED(m_sceneCopy->GetSurfaceLevel(0, &sceneSurface)) &&
                        SUCCEEDED(dev->StretchRect(target, &inTarget, sceneSurface, &inViewport, D3DTEXF_POINT));
    SafeRelease(sceneSurface);
    if (copied)
        GlobalFrameCopies().CountColourCopy(site);
    return copied;
}

bool Renderer::CopySceneForTheComposite(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp)
{
    if (LogEnabled(LogLevel::Info))
        m_sceneCopyTimer.Begin(dev);
    const bool copied = CopyWorldViewport(dev, target, vp, ColourCopySite::FogComposite);
    if (copied)
        m_sceneCopyTimer.End();
    else
        m_sceneCopyTimer.Cancel();
    return copied;
}

bool Renderer::DepthProbeDue(long long now) const
{
    const bool debugLog = LogEnabled(LogLevel::Debug);
    const float probeIntervalSeconds = debugLog ? kProbeDebugSeconds : kProbeSeconds;
    return LogEnabled(LogLevel::Info) && !m_probeFailed && !m_pendingProbe.issued && m_frame >= kProbeFirstFrame &&
           (debugLog || m_probeAttempts < kInfoLevelProbeLimit) &&
           (m_probeAttempts == 0 || TickSeconds(now - m_probeTicks) > probeIntervalSeconds);
}

bool Renderer::EnsureDepthProbe(IDirect3DDevice9* dev)
{
    if (m_probeFailed)
        return false;
    const bool ready =
        (m_probe || SUCCEEDED(dev->CreatePixelShader(reinterpret_cast<const DWORD*>(g_ps_probe), &m_probe))) &&
        (m_probeTarget || CreateTarget(dev, kProbePoints, 1, D3DFMT_A32B32G32R32F, &m_probeTarget)) &&
        (m_probeReadback || SUCCEEDED(dev->CreateOffscreenPlainSurface(kProbePoints, 1, D3DFMT_A32B32G32R32F,
                                                                        D3DPOOL_SYSTEMMEM, &m_probeReadback,
                                                                        nullptr))) &&
        (m_probeCopied || SUCCEEDED(dev->CreateQuery(D3DQUERYTYPE_EVENT, &m_probeCopied)));
    if (ready)
        return true;
    SafeRelease(m_probeTarget);
    SafeRelease(m_probeReadback);
    SafeRelease(m_probeCopied);
    m_probeFailed = true;
    VF_LOG_INFO("depth probe unavailable (no probe shader, float render target or event query)");
    return false;
}

void Renderer::IssueDepthProbe(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* fog,
                               const D3DVIEWPORT9& vp, float deepestWorldDepth, float dayFraction)
{
    if (!EnsureDepthProbe(dev))
        return;
    SetTarget(dev, m_probeTarget);
    D3DVIEWPORT9 probeVp = {0, 0, kProbePoints, 1, 0.0f, 1.0f};
    dev->SetViewport(&probeVp);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetPixelShader(m_probe);
    BindTexture(dev, 0, depthTexture, false);
    BindTexture(dev, 1, fog, false);
    DrawFullscreenTriangle(dev);

    IDirect3DSurface9* surface = nullptr;
    const bool copying = SUCCEEDED(m_probeTarget->GetSurfaceLevel(0, &surface)) &&
                         SUCCEEDED(dev->GetRenderTargetData(surface, m_probeReadback)) &&
                         SUCCEEDED(m_probeCopied->Issue(D3DISSUE_END));
    SafeRelease(surface);
    if (!copying)
    {
        VF_LOG_INFO("depth probe %u: readback failed", m_probeAttempts);
        return;
    }
    m_pendingProbe = {true, m_probeAttempts, dayFraction, vp.MinZ, vp.MaxZ, deepestWorldDepth};
}

void Renderer::DropPendingDepthProbe()
{
    if (m_pendingProbe.issued)
        VF_LOG_INFO("depth probe %u: readback dropped with the device resources", m_pendingProbe.attempt);
    m_pendingProbe = {};
}

void Renderer::LogFinishedDepthProbe()
{
    if (!m_pendingProbe.issued)
        return;
    const HRESULT copied = m_probeCopied->GetData(nullptr, 0, 0);
    if (copied == S_FALSE)
        return;
    const PendingDepthProbe probe = m_pendingProbe;
    m_pendingProbe = {};
    D3DLOCKED_RECT locked = {};
    if (copied != S_OK || FAILED(m_probeReadback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
    {
        VF_LOG_INFO("depth probe %u: readback failed", probe.attempt);
        return;
    }
    const auto* samples = static_cast<const ProbeSample*>(locked.pBits);
    VF_LOG_INFO("depth probe %u: day %.4f, viewport depth %.4f..%.4f, world depth up to %.7f; per point raw depth / yd "
                "/ fog opacity (w world, f beyond the far clip, s sky)",
                probe.attempt, probe.dayFraction, probe.viewportMinZ, probe.viewportMaxZ, probe.deepestWorldDepth);
    for (int row = 0; row < kProbeGridSide; ++row)
    {
        char line[256] = {};
        int used = 0;
        for (int col = 0; col < kProbeGridSide; ++col)
        {
            const ProbeSample& sample = samples[row * kProbeGridSide + col];
            used += std::snprintf(line + used, sizeof(line) - used, "  %.7f/%.0f/%.2f %c", sample.rawDepth,
                                  sample.viewDepthYards, sample.fogOpacity, DepthClassLetter(sample.depthClass));
        }
        VF_LOG_INFO("  row %d:%s", row, line);
    }
    m_probeReadback->UnlockRect();
}

void Renderer::BindTexture(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* tex, bool linear)
{
    DWORD filter = linear ? D3DTEXF_LINEAR : D3DTEXF_POINT;
    dev->SetTexture(stage, tex);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSW, D3DTADDRESS_CLAMP);
    dev->SetSamplerState(stage, D3DSAMP_MAGFILTER, filter);
    dev->SetSamplerState(stage, D3DSAMP_MINFILTER, filter);
    dev->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    dev->SetSamplerState(stage, D3DSAMP_SRGBTEXTURE, FALSE);
    dev->SetSamplerState(stage, D3DSAMP_MAXMIPLEVEL, 0);
}

void Renderer::BindWrappedVolume(IDirect3DDevice9* dev, DWORD stage, IDirect3DVolumeTexture9* volume)
{
    BindTexture(dev, stage, volume, true);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
    dev->SetSamplerState(stage, D3DSAMP_ADDRESSW, D3DTADDRESS_WRAP);
}

void Renderer::BindSunShadows(IDirect3DDevice9* dev, const FrameInputs& in, const WorldShadowInputs& shadows,
                              const float* shadowToLight, const float* control)
{
    float toShadowLightInView[3];
    TransformDirection(shadowToLight, in.cameraRelativeView, toShadowLightInView);
    Normalize3(toShadowLightInView);
    const Float4 light = {toShadowLightInView[0], toShadowLightInView[1], toShadowLightInView[2], 0.0f};
    dev->SetPixelShaderConstantF(kSunShadowRegister, control, 1);
    dev->SetPixelShaderConstantF(kSunShadowLightRegister, &light.x, 1);
    BindWorldShadowMaps(dev, shadows);
}

void Renderer::BindWorldShadowMaps(IDirect3DDevice9* dev, const WorldShadowInputs& shadows)
{
    dev->SetPixelShaderConstantF(kViewToShadowRegister, &shadows.viewToShadow[0][0], kViewToShadowRegisters);
    for (int i = 0; i < kWorldShadowMapCount; ++i)
        BindTexture(dev, kFirstWorldShadowStage + i, shadows.textures[i], shadows.hardwareComparison);
}

bool Renderer::UpdateTerrainShadows(IDirect3DDevice9* dev, const FrameInputs& in, const float* shadowToLight,
                                    const Config& cfg)
{
    static const bool supported = engine::TerrainContractSupported();
    if (!cfg.lightShafts || !supported)
        return false;
    const int loadedBefore = m_terrainShadows.Window().LoadedChunks();
    const bool active = m_terrainShadows.Update(dev, in.camPos, in.mapId, shadowToLight, engine::ClientTerrain(),
                                                TickSeconds(Ticks()));
    const int loaded = m_terrainShadows.Window().LoadedChunks();
    if ((loadedBefore == 0) != (loaded == 0))
        VF_LOG_INFO("terrain shadows: %d of %d chunks around the camera loaded on map %d", loaded,
                    kTerrainWindowChunks * kTerrainWindowChunks, in.mapId);
    return active;
}

void Renderer::BindTerrainShadows(IDirect3DDevice9* dev, bool active)
{
    const TerrainHeightWindow& window = m_terrainShadows.Window();
    const Float4 control = {static_cast<float>(window.FirstVertexRow()), static_cast<float>(window.FirstVertexCol()),
                            active ? 1.0f : 0.0f, 0.0f};
    dev->SetPixelShaderConstantF(kTerrainShadowRegister, &control.x, 1);
    BindTexture(dev, kTerrainShadowStage, active ? m_terrainShadows.Texture() : nullptr, false);
}

FogParams Renderer::DrawableFog(IDirect3DDevice9* dev, const FogParams& fog)
{
    if (!AnyLayerNoise(fog))
        return fog;
    if (!m_authoredNoise && m_authoredNoiseRetry.Ready() &&
        !(AttemptCreation(kFogNoise) && CreateAuthoredNoise(dev, &m_authoredNoise)))
        m_authoredNoiseRetry.Failed();
    return m_authoredNoise ? fog : WithMeanNoise(fog);
}

void Renderer::UploadLayerNoise(IDirect3DDevice9* dev, const FogParams& fog, const float* camera,
                                long long now)
{
    const double elapsed = m_noiseTicks ? std::min(TickSeconds(now - m_noiseTicks), kMaxNoiseStepSeconds) : 0.0;
    m_noiseTicks = now;
    m_noiseScroll.Advance(fog, camera, elapsed);
    LayerNoiseRegisters registers[kSceneLayers];
    m_noiseScroll.Registers(fog, registers);
    static_assert(sizeof(registers) == 4 * kSceneLayers * sizeof(Float4), "noise registers of the scene layers");
    dev->SetPixelShaderConstantF(kLayerNoiseRegister, &registers[0].octaveOffsetAndInverseTile[0][0],
                                 4 * kSceneLayers);
    BindWrappedVolume(dev, kAuthoredNoiseStage, m_authoredNoise);
}

IDirect3DTexture9* Renderer::FilterWithHistory(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture,
                                               const float* viewToPreviousClip, bool historyValid,
                                               float historyWeight)
{
    const int write = m_historyIndex ^ 1;
    SetTarget(dev, m_history[write]);
    dev->SetPixelShader(m_temporal);
    const Float4 temporal = {historyWeight, historyValid ? 1.0f : 0.0f, m_adaptiveLightingHistory ? 1.0f : 0.0f,
                             0.0f};
    dev->SetPixelShaderConstantF(9, viewToPreviousClip, 4);
    dev->SetPixelShaderConstantF(13, &temporal.x, 1);
    BindTexture(dev, 0, m_marchTarget, false);
    BindTexture(dev, 1, m_history[m_historyIndex], false);
    BindTexture(dev, 2, depthTexture, false);
    BindTexture(dev, 3, m_historyDepth, false);
    DrawFullscreenTriangle(dev);

    BindTexture(dev, 3, nullptr, false);
    SetTarget(dev, m_historyDepth);
    dev->SetPixelShader(m_historyDepthShader);
    BindTexture(dev, 0, depthTexture, false);
    DrawFullscreenTriangle(dev);
    m_historyIndex = write;
    return m_history[write];
}

void Renderer::LogFrameSummary(IDirect3DDevice9* dev, long long now, const FrameInputs& in, const Config& cfg,
                               const FogParams& fog, const AuthoredFog& authored, const D3DSURFACE_DESC& depthDesc,
                               const float* viewToWorld, const float* toLightInView, const float* sunPx,
                               float rayStrength)
{
    const D3DVIEWPORT9& vp = in.viewport;
    const bool viewChanged = std::fabs(in.farClip - m_loggedFarClip) > 1.0f || vp.Width != m_loggedViewport.Width ||
                             vp.Height != m_loggedViewport.Height || vp.MinZ != m_loggedViewport.MinZ ||
                             vp.MaxZ != m_loggedViewport.MaxZ;
    if (m_logged >= 1 && !viewChanged && TickSeconds(now - m_summaryTicks) <= kSummarySeconds &&
        !(LogEnabled(LogLevel::Debug) && m_frame % 600u == 0))
        return;
    ++m_logged;
    m_summaryTicks = now;
    m_loggedFarClip = in.farClip;
    m_loggedViewport = vp;
    const float* proj = in.glProjection;
    const float projectionNear = -proj[14] / (1.0f + proj[10]);
    const float projectionFar = proj[14] / (1.0f - proj[10]);
    char gpuTime[96] = {};
    if (LogEnabled(LogLevel::Info))
        DescribeGpuTime(m_gpuTimer, dev, gpuTime, sizeof(gpuTime));
    VF_LOG_INFO("frame %u: viewport %lu,%lu %lux%lu of %ux%u depth %.4f..%.4f; near %.3f far %.1f (farclip %.1f) "
                "maxdist %.0f%s%s",
                m_frame, vp.X, vp.Y, vp.Width, vp.Height, depthDesc.Width, depthDesc.Height, vp.MinZ, vp.MaxZ,
                projectionNear, projectionFar, in.farClip, fog.maxDistance, gpuTime[0] ? ", " : "", gpuTime);
    char passTimes[160] = {};
    if (LogEnabled(LogLevel::Info))
    {
        const auto timers = PassTimers();
        DescribeGpuTimes(dev, timers.data(), timers.size(), passTimes, sizeof(passTimes));
    }
    if (passTimes[0])
        VF_LOG_INFO("  fog pass gpu ms (median): %s", passTimes);
    LogSurfaceLightCoverage();
    VF_LOG_INFO("  camera (%.2f %.2f %.2f) inverse-view origin (%.2f %.2f %.2f) target (%.2f %.2f %.2f) focus "
                "(%.2f %.2f %.2f)",
                in.camPos[0], in.camPos[1], in.camPos[2], viewToWorld[12], viewToWorld[13], viewToWorld[14],
                in.camTarget[0], in.camTarget[1], in.camTarget[2], in.cameraFocus[0], in.cameraFocus[1],
                in.cameraFocus[2]);
    VF_LOG_INFO("  day %.4f %s toLight (%.3f %.3f %.3f) view (%.3f %.3f %.3f) vis %.2f above %.2f "
                "sunPx (%.0f %.0f) rays %.2f",
                in.dayFraction, in.lightIsMoon ? "moon" : "sun", in.toLight[0], in.toLight[1], in.toLight[2],
                toLightInView[0], toLightInView[1], toLightInView[2], fog.lightVisibility, fog.lightAboveHorizon,
                sunPx[0], sunPx[1], rayStrength);
    VF_LOG_INFO("  map %d fog %08X start %.1f end %.1f zone %.1f sun %08X direct %08X ambient %08X refZ %.1f "
                "glow %.2f, direct light vs Classic %.2f",
                in.mapId, in.fogColor, in.fogStart, in.fogEnd, in.zoneFogDistance, in.sunColor, in.directColor,
                in.ambientColor, fog.referenceZ, in.clientGlowAmount, fog.directLightMatch);
    char localPoints[32] = "not captured";
    if (cfg.localLights)
        std::snprintf(localPoints, sizeof(localPoints), "%u", in.localLights.pointLightCount);
    char rejection[kCaptureRejectionText];
    DescribeCaptureRejection(in.localLights.capture, rejection, sizeof(rejection));
    VF_LOG_INFO("  local points %s enabled %d%s; interior %d blend %.3f", localPoints, cfg.localLights, rejection,
                in.localLights.cameraInterior, in.localLights.interiorBlend);
    for (int i = 0; i < kFogLayers; ++i)
    {
        const FogLayer& l = fog.layers[i];
        VF_LOG_INFO("  layer %d (%s): start %.0f density %.6f curve %.2f^%.2f g %.2f diffuse %.2f %.2f %.2f "
                    "emissive %.2f %.2f %.2f upper %.1f/%.4f lower %.1f/%.4f shadowed %.0f limit %.0f",
                    i, fog.authored && i < kSceneLayers ? "classic" : "derived", l.start, l.density, l.strength,
                    l.exponent, l.g, l.diffuse[0], l.diffuse[1], l.diffuse[2], l.emissive[0], l.emissive[1],
                    l.emissive[2], l.upperHeight, l.upperFalloff, l.lowerHeight, l.lowerFalloff, l.shadowed,
                    std::min(l.endDistance, 99999.0f));
    }
    const float exposure = fog.authored ? cfg.classicExposure : cfg.exposure;
    const LevelRayFog closeRay = LevelRayFogAt(fog, kLevelRayLogDistances[0]);
    const LevelRayFog midRay = LevelRayFogAt(fog, kLevelRayLogDistances[1]);
    const LevelRayFog farRay = LevelRayFogAt(fog, kLevelRayLogDistances[2]);
    VF_LOG_INFO("  level ray at %.0f/%.0f/%.0f yd: opacity %.3f %.3f %.3f, ambient glow %.4f %.4f %.4f "
                "(exposure %.2f), %u point lights",
                kLevelRayLogDistances[0], kLevelRayLogDistances[1], kLevelRayLogDistances[2], closeRay.opacity,
                midRay.opacity, farRay.opacity, closeRay.emission * exposure, midRay.emission * exposure,
                farRay.emission * exposure, exposure, in.localLights.pointLightCount);
    if (authored.lightCount > 0)
        LogAuthoredExtras(authored, fog, cfg);
}

bool Renderer::ReadyToRender(IDirect3DDevice9* dev, const SceneDepth& depth, const D3DVIEWPORT9& vp)
{
    m_skip = "";
    if (dev->TestCooperativeLevel() != D3D_OK)
        return NotReady("device not ready");
    if (!EnsureShaders(dev) || !EnsureStateBlock(dev))
        return false;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* boundDepth = nullptr;
    dev->GetRenderTarget(0, &target);
    dev->GetDepthStencilSurface(&boundDepth);
    D3DSURFACE_DESC depthDesc = {};
    const char* unusable = UnusableTargets(target, boundDepth, depth, depthDesc);
    if (!unusable)
        unusable = ViewportOutsideTarget(vp, depthDesc);
    SafeRelease(target);
    SafeRelease(boundDepth);
    return unusable ? NotReady(unusable) : true;
}

namespace
{
volatile bool g_injectedPassFault = false;
volatile bool g_compositeInOnePass = false;

void FaultWhenInjected()
{
    if (Seam<bool>(g_injectedPassFault, false))
        RaiseException(EXCEPTION_ACCESS_VIOLATION, 0, 0, nullptr);
}
}

#ifdef VF_TESTING
void Renderer::InjectPassFault(bool faulting)
{
    g_injectedPassFault = faulting;
}

void Renderer::DrawCompositeInOnePass(bool onePass)
{
    g_compositeInOnePass = onePass;
}

FogPassTimes Renderer::TakePassTimes()
{
    FogPassTimes times;
    times.fog = m_gpuTimer.TakeInterval();
    times.march = m_marchTimer.TakeInterval();
    times.temporal = m_temporalTimer.TakeInterval();
    times.sceneCopy = m_sceneCopyTimer.TakeInterval();
    times.surfaceLights = m_surfaceLightTimer.TakeInterval();
    times.composite = m_compositeTimer.TakeInterval();
    return times;
}
#endif

template <typename Passes>
bool Renderer::WithClientStateSaved(IDirect3DDevice9* dev, const SceneDepth& depth, Passes passes)
{
    IDirect3DSurface9* saved[4] = {};
    for (DWORD i = 0; i < 4; ++i)
        dev->GetRenderTarget(i, &saved[i]);
    IDirect3DSurface9* savedDepth = nullptr;
    dev->GetDepthStencilSurface(&savedDepth);
    IDirect3DVertexBuffer9* stream = nullptr;
    UINT streamOffset = 0;
    UINT streamStride = 0;

    bool ok = false;
    bool captured = false;
    __try
    {
        D3DSURFACE_DESC depthDesc = {};
        if (const char* unusable = UnusableTargets(saved[0], savedDepth, depth, depthDesc))
            Skip(unusable);
        else
        {
            dev->GetStreamSource(0, &stream, &streamOffset, &streamStride);
            m_state->Capture();
            captured = true;
            for (DWORD i = 1; i < 4; ++i)
                if (saved[i])
                    dev->SetRenderTarget(i, nullptr);
            ok = passes(saved[0], depthDesc);
        }
    }
    __finally
    {
        if (captured)
        {
            dev->SetRenderTarget(0, saved[0]);
            for (DWORD i = 1; i < 4; ++i)
                if (saved[i])
                    dev->SetRenderTarget(i, saved[i]);
            m_state->Apply();
            dev->SetDepthStencilSurface(savedDepth);
            dev->SetStreamSource(0, stream, streamOffset, streamStride);
        }
        engine::ReleaseWorldShadows(m_worldShadows);
        SafeRelease(stream);
        for (auto*& s : saved)
            SafeRelease(s);
        SafeRelease(savedDepth);
    }
    return ok;
}

bool Renderer::Render(IDirect3DDevice9* dev, const SceneDepth& depth, const FrameInputs& in, const Config& cfg,
                      FogPass pass)
{
    m_skip = "";
    m_stockFogFit = {};
    if (m_lateGodRays.pending)
        m_gpuTimer.Cancel();
    m_lateGodRays = {};
    if (dev->TestCooperativeLevel() != D3D_OK)
        return Skip("device not ready");
    LogFinishedDepthProbe();
    if (!EnsureShaders(dev) || !EnsureStateBlock(dev))
        return false;
    return WithClientStateSaved(dev, depth, [&](IDirect3DSurface9* target, const D3DSURFACE_DESC& depthDesc) {
        return RenderPasses(dev, depth, target, depthDesc, in, ConfigForMap(cfg, in.mapId), pass);
    });
}

bool Renderer::RenderGodRaysAfterWorld(IDirect3DDevice9* dev, const SceneDepth& depth)
{
    const bool historyValid = m_historyValid;
    const bool drawn = DrawLateGodRays(dev, depth);
    m_historyValid = historyValid;
    return drawn;
}

bool Renderer::DrawLateGodRays(IDirect3DDevice9* dev, const SceneDepth& depth)
{
    m_skip = "";
    const GodRayFrame rays = m_lateGodRays;
    m_lateGodRays = {};
    if (!rays.pending)
        return true;
    if (dev->TestCooperativeLevel() != D3D_OK || !m_state)
    {
        m_gpuTimer.Cancel();
        return Skip("device not ready");
    }
    m_gpuTimer.Resume();
    const bool drawn =
        WithClientStateSaved(dev, depth, [&](IDirect3DSurface9* target, const D3DSURFACE_DESC& depthDesc) {
            return DrawGodRaysOverScene(dev, depth, target, depthDesc, rays);
        });
    if (drawn)
        m_gpuTimer.End();
    else
        m_gpuTimer.Cancel();
    return drawn;
}

void Renderer::PrepareFullscreenPasses(IDirect3DDevice9* dev)
{
    dev->SetDepthStencilSurface(nullptr);
    dev->SetVertexShader(m_vs);
    dev->SetVertexDeclaration(m_decl);
    dev->SetStreamSourceFreq(0, 1);
    dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
    dev->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, FALSE);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
    dev->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE);
    dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
    dev->SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
    dev->SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
}

namespace
{
constexpr float kInvisibleLightShare = 0.03f;
constexpr float kLargestShareLitInPlace = 0.75f;
constexpr float kFullLightShare = 0.1f;
constexpr float kSunShareOfIlluminant = 0.5f;
constexpr float kDarkestIlluminant = 0.02f;
constexpr float kLumaRed = 0.2126f;
constexpr float kLumaGreen = 0.7152f;
constexpr float kLumaBlue = 0.0722f;

float LinearLuma(uint32_t packed)
{
    float rgb[3];
    UnpackColor(packed, rgb);
    return kLumaRed * std::pow(rgb[0], kSurfaceDisplayGamma) + kLumaGreen * std::pow(rgb[1], kSurfaceDisplayGamma) +
           kLumaBlue * std::pow(rgb[2], kSurfaceDisplayGamma);
}

float StrongestSurfaceLight(const Float4* lightData, uint32_t count)
{
    float strongest = 0.0f;
    for (uint32_t i = 0; i < count; ++i)
        strongest = std::max(strongest, lightData[i * 3 + 1].w);
    return strongest;
}

float SurfaceLightVisibility(const Float4* lightData, uint32_t count, uint32_t ambient, uint32_t direct)
{
    float brightest = 0.0f;
    for (uint32_t i = 0; i < count; ++i)
    {
        const Float4& colour = lightData[i * 3 + 1];
        brightest = std::max(brightest, std::max(colour.x, std::max(colour.y, colour.z)));
    }
    const float illuminant =
        std::max(LinearLuma(ambient) + kSunShareOfIlluminant * LinearLuma(direct), kDarkestIlluminant);
    const float share = brightest / illuminant;
    const float t = std::clamp((share - kInvisibleLightShare) / (kFullLightShare - kInvisibleLightShare), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

SurfaceLightArea AreaOfSurfaceLights(const Float4* lightData, uint32_t count, const float* glProjection,
                                     const D3DVIEWPORT9& vp)
{
    if (Seam(g_surfacesLitOverTheWholeView, false))
        return WholeViewport(vp);
    SurfaceLight lights[kMaxLocalPointLights];
    for (uint32_t i = 0; i < count; ++i)
    {
        const Float4& position = lightData[i * 3];
        const Float4& colour = lightData[i * 3 + 1];
        const Float4& attenuation = lightData[i * 3 + 2];
        lights[i] = {{position.x, position.y, position.z}, position.w, {colour.x, colour.y, colour.z}, colour.w,
                     {attenuation.x, attenuation.y, attenuation.z}};
    }
    return ProjectSurfaceLights(lights, count, glProjection, vp);
}
}

void Renderer::CountSurfaceLightCoverage(const SurfaceLightArea& area)
{
    ++m_surfaceLightCoverage.frames;
    if (area.Empty())
        return;
    ++m_surfaceLightCoverage.lit;
    if (area.share >= kLargestShareLitInPlace)
        ++m_surfaceLightCoverage.overTheWholeView;
    m_surfaceLightCoverage.share += area.share;
    m_surfaceLightCoverage.shareOfLightsApart += area.shareOfLightsApart;
}

void Renderer::LogSurfaceLightCoverage()
{
    const SurfaceLightCoverage coverage = m_surfaceLightCoverage;
    m_surfaceLightCoverage = {};
    if (coverage.lit == 0)
        return;
    VF_LOG_INFO("  surface lights: %u of %u frames, their rectangle %.0f%% of the view on average (the lights' own "
                "rectangles add up to %.0f%%), %u frames at %.0f%% or more",
                coverage.lit, coverage.frames, 100.0 * coverage.share / coverage.lit,
                100.0 * coverage.shareOfLightsApart / coverage.lit, coverage.overTheWholeView,
                100.0 * kLargestShareLitInPlace);
}

void Renderer::LightTheTarget(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* target,
                              const SurfaceLightFrame& frame)
{
    if (LogEnabled(LogLevel::Info))
        m_surfaceLightTimer.Begin(dev);
    if (!CopyWorldArea(dev, target, frame.viewport, frame.area.inViewport, ColourCopySite::SurfaceLights))
    {
        m_surfaceLightTimer.Cancel();
        return;
    }
    DrawSurfaceLights(dev, depthTexture, frame,
                      {target, m_sceneCopy, frame.viewport, InTarget(frame.area.inViewport, frame.viewport), false,
                       false});
    m_surfaceLightTimer.End();
}

IDirect3DTexture9* Renderer::LightTheSceneCopy(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture,
                                               const SurfaceLightFrame& frame)
{
    IDirect3DSurface9* scene = nullptr;
    IDirect3DSurface9* beside = nullptr;
    IDirect3DTexture9* litScene = m_sceneCopy;
    if (SUCCEEDED(m_sceneCopy->GetSurfaceLevel(0, &scene)) && SUCCEEDED(m_sceneBeside->GetSurfaceLevel(0, &beside)))
    {
        const D3DVIEWPORT9 textureViewport = {0, 0, frame.viewport.Width, frame.viewport.Height, 0.0f, 1.0f};
        const RECT& area = frame.area.inViewport;
        if (LogEnabled(LogLevel::Info))
            m_surfaceLightTimer.Begin(dev);
        if (frame.area.share >= kLargestShareLitInPlace)
        {
            DrawSurfaceLights(dev, depthTexture, frame,
                              {beside, m_sceneCopy, textureViewport, ViewportRect(textureViewport), true, true});
            litScene = m_sceneBeside;
            m_surfaceLightTimer.End();
        }
        else if (SUCCEEDED(dev->StretchRect(scene, &area, beside, &area, D3DTEXF_POINT)))
        {
            GlobalFrameCopies().CountColourCopy(ColourCopySite::SurfaceLights);
            DrawSurfaceLights(dev, depthTexture, frame, {scene, m_sceneBeside, textureViewport, area, true, false});
            m_surfaceLightTimer.End();
        }
        else
            m_surfaceLightTimer.Cancel();
    }
    SafeRelease(scene);
    SafeRelease(beside);
    return litScene;
}

void Renderer::DrawSurfaceLights(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture,
                                 const SurfaceLightFrame& frame, const SurfaceLightDraw& draw)
{
    dev->SetRenderTarget(0, draw.onto);
    dev->SetViewport(&draw.viewport);
    dev->SetScissorRect(&draw.area);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,
                        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    dev->SetPixelShader(m_surfaceLights);
    const WorldShadowInputs& clientShadows = *frame.clientShadows;
    Float4 constants[kSurfaceLightRegisters] = {
        {frame.strength, frame.reach, draw.ontoViewportTexture ? 1.0f : 0.0f, draw.keepsUnlitPixels ? 1.0f : 0.0f},
        {},
        {},
        {frame.toLightInView[0], frame.toLightInView[1], frame.toLightInView[2], 0.0f},
        {clientShadows.count > 0 ? 1.0f : 0.0f, clientShadows.count > 1 ? 1.0f : 0.0f,
         clientShadows.hardwareComparison ? 1.0f : 0.0f, clientShadows.texelSize[0][0]},
    };
    float* const ambient = &constants[1].x;
    float* const direct = &constants[2].x;
    UnpackColor(frame.ambientColor, ambient);
    UnpackColor(frame.directColor, direct);
    for (int channel = 0; channel < 3; ++channel)
    {
        ambient[channel] = std::pow(ambient[channel], kSurfaceDisplayGamma);
        direct[channel] = std::pow(direct[channel], kSurfaceDisplayGamma);
    }
    dev->SetPixelShaderConstantF(kSurfaceLightRegister, &constants[0].x, kSurfaceLightRegisters);
    BindTexture(dev, 0, depthTexture, false);
    BindTexture(dev, kSceneBeforeLightsStage, draw.sceneBeforeLights, false);
    BindTexture(dev, kLocalLightStage, m_localLightData, false);
    BindWorldShadowMaps(dev, clientShadows);
    DrawFullscreenTriangle(dev);
    ++m_surfaceLightDraws;
    m_drawnSurfaceLightArea = frame.area;
}

bool Renderer::CopySceneForGodRays(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp,
                                   IDirect3DTexture9* sceneTexture)
{
    IDirect3DSurface9* raySurface = nullptr;
    IDirect3DSurface9* sceneSurface = nullptr;
    const RECT world = ViewportRect(vp);
    const RECT sceneWorld = {0, 0, static_cast<LONG>(vp.Width), static_cast<LONG>(vp.Height)};
    const bool fromTexture = sceneTexture && !Seam(g_godRaysFromTarget, false);
    HRESULT result = m_raySource->GetSurfaceLevel(0, &raySurface);
    if (SUCCEEDED(result) && fromTexture)
        result = sceneTexture->GetSurfaceLevel(0, &sceneSurface);
    if (SUCCEEDED(result))
    {
        IDirect3DSurface9* const source = sceneSurface ? sceneSurface : target;
        const RECT* const sourceRect = sceneSurface ? &sceneWorld : &world;
        result = Seam(g_raySourceCopyRefused, false)
                     ? D3DERR_INVALIDCALL
                     : dev->StretchRect(source, sourceRect, raySurface, nullptr, D3DTEXF_LINEAR);
    }
    SafeRelease(sceneSurface);
    SafeRelease(raySurface);
    if (SUCCEEDED(result))
    {
        GlobalFrameCopies().CountColourCopy(ColourCopySite::GodRaySource);
        return true;
    }
    if (!m_raySourceCopyFailureLogged)
        VF_LOG_ERROR("god rays skipped: the scene copy they are drawn from failed (HRESULT 0x%08lX); no god rays "
                     "while it fails",
                     static_cast<unsigned long>(result));
    m_raySourceCopyFailureLogged = true;
    return false;
}

void Renderer::DrawGodRayMask(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, const GodRayFrame& rays)
{
    const D3DVIEWPORT9& vp = rays.viewport;
    IDirect3DTexture9* const* openWithoutTheCharacter =
        m_raysOpenWithoutTheCharacter[0] ? m_raysOpenWithoutTheCharacter : m_rays;
    SetTarget(dev, m_rays[1]);
    SetSecondTarget(dev, m_raysOpenWithoutTheCharacter[1]);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE1, 0xF);
    dev->SetPixelShader(m_rayMask);
    const CharacterMask character = MaskTheCharacter(rays.character);
    const Float4 mask[5] = {
        {rays.toLightInView[0], rays.toLightInView[1], rays.toLightInView[2], kRayFalloff},
        {0.0f, kRayThreshold, 1.0f / m_rayW, 1.0f / m_rayH},
        character.box,
        character.focus,
        character.heights,
    };
    dev->SetPixelShaderConstantF(9, &mask[0].x, 5);
    BindTexture(dev, 0, depthTexture, false);
    BindTexture(dev, 1, m_raySource, true);
    DrawFullscreenTriangle(dev);

    const float sunUv[2] = {(rays.sunPx[0] - vp.X) / vp.Width, (rays.sunPx[1] - vp.Y) / vp.Height};
    dev->SetPixelShader(m_rayBlur);
    for (int pass = 0; pass < god_ray_blur::kPasses; ++pass)
    {
        const god_ray_blur::Pass blurPass = god_ray_blur::PassAt(pass);
        SetTarget(dev, m_rays[pass == 0 ? 0 : 1]);
        SetSecondTarget(dev, m_raysOpenWithoutTheCharacter[pass == 0 ? 0 : 1]);
        const Float4 blur[3] = {
            {sunUv[0], sunUv[1], blurPass.stepTowardSun, blurPass.normalisation},
            {static_cast<float>(m_rayW), static_cast<float>(m_rayH), 1.0f / m_rayW, 1.0f / m_rayH},
            {blurPass.decayPerTap, 0.0f, 0.0f, 0.0f},
        };
        dev->SetPixelShaderConstantF(9, &blur[0].x, 3);
        BindTexture(dev, 0, m_rays[pass == 0 ? 1 : 0], true);
        BindTexture(dev, 1, openWithoutTheCharacter[pass == 0 ? 1 : 0], true);
        DrawFullscreenTriangle(dev);
    }
    SetSecondTarget(dev, nullptr);

    SetTarget(dev, m_rays[kShaftRays]);
    dev->SetPixelShader(m_rayShafts);
    const Float4 shafts[2] = {
        {sunUv[0] * m_rayW, sunUv[1] * m_rayH, 0.0f, 0.0f},
        {static_cast<float>(m_rayW), static_cast<float>(m_rayH), 1.0f / m_rayW, 1.0f / m_rayH},
    };
    dev->SetPixelShaderConstantF(9, &shafts[0].x, 2);
    BindTexture(dev, 0, m_rays[1 - kShaftRays], true);
    BindTexture(dev, 1, openWithoutTheCharacter[1 - kShaftRays], true);
    DrawFullscreenTriangle(dev);
}

bool Renderer::DrawGodRaysOverScene(IDirect3DDevice9* dev, const SceneDepth& depth, IDirect3DSurface9* target,
                                    const D3DSURFACE_DESC& depthDesc, const GodRayFrame& rays)
{
    const D3DVIEWPORT9& vp = rays.viewport;
    if (!m_raySource || !m_rays[0] || !m_rays[1] || vp.X + vp.Width > depthDesc.Width ||
        vp.Y + vp.Height > depthDesc.Height)
        return Skip("god ray targets unavailable");
    PrepareFullscreenPasses(dev);
    dev->SetPixelShaderConstantF(0, &rays.common[0][0], 9);
    DrawGodRayMask(dev, depth.texture, rays);
    if (!CopyWorldViewport(dev, target, vp, ColourCopySite::LateGodRays))
        return Skip("no scene copy for the god rays");
    const RECT world = ViewportRect(vp);
    dev->SetRenderTarget(0, target);
    dev->SetViewport(&vp);
    dev->SetScissorRect(&world);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,
                        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    dev->SetPixelShader(m_rayComposite);
    const Float4 composite[3] = {
        {0.0f, rays.strength, 0.0f, 0.0f},
        {rays.colour[0], rays.colour[1], rays.colour[2], 0.0f},
        {rays.sunPx[0], rays.sunPx[1], 0.0f, rays.glowToCompensate},
    };
    dev->SetPixelShaderConstantF(96, &composite[0].x, 3);
    const CharacterMask character = MaskTheCharacter(rays.character);
    dev->SetPixelShaderConstantF(100, &character.box.x, 3);
    BindTexture(dev, 0, depth.texture, false);
    BindTexture(dev, 2, m_rays[kShaftRays], true);
    BindTexture(dev, 3, m_sceneCopy, false);
    DrawFullscreenTriangle(dev);
    return true;
}

bool Renderer::RenderPasses(IDirect3DDevice9* dev, const SceneDepth& depth, IDirect3DSurface9* target,
                            const D3DSURFACE_DESC& depthDesc, const FrameInputs& in, const Config& cfg,
                            FogPass pass)
{
    const D3DVIEWPORT9 vp = in.viewport;
    if (const char* outside = ViewportOutsideTarget(vp, depthDesc))
        return Skip(outside);

    const UINT scale = cfg.quality == 1 ? 4u : 2u;
    const UINT lowW = (vp.Width + scale - 1) / scale;
    const UINT lowH = (vp.Height + scale - 1) / scale;
    const UINT rayW = std::max(1u, static_cast<UINT>(vp.Width) / kRayScale);
    const UINT rayH = std::max(1u, static_cast<UINT>(vp.Height) / kRayScale);
    if (!EnsureTargets(dev, lowW, lowH, rayW, rayH))
        return false;

    float viewToWorld[16];
    float worldToView[16];
    if (!WorldViewFromCameraRelative(in, viewToWorld, worldToView))
        return Skip("view matrix not invertible");

    AuthoredFog authored = {};
    const bool hasAuthored =
        cfg.dataMode == 1 && GlobalFogData().Resolve(in.mapId, in.camPos, in.dayFraction, in.lightParams, authored);
    const FogParams fog = DrawableFog(dev, FogParamsFor(in, cfg, hasAuthored ? &authored : nullptr));
    const bool samplesNoise = AnyLayerNoise(fog);
    LogLightChange(in, authored, hasAuthored);
    const float* proj = in.glProjection;
    const WorldDepthMapping worldDepth = MapWorldDepth(proj, vp);

    float toLightInView[3];
    TransformDirection(in.toLight, in.cameraRelativeView, toLightInView);
    Normalize3(toLightInView);

    float clientShadowVector[3];
    engine::ReadClientShadowVector(clientShadowVector);
    const SunShadowPlan shadowPlan = PlanSunShadows(clientShadowVector, in.toLight);
    const float* shadowToLight = shadowPlan.toLight;

    const bool lightsMayReachSurfaces = cfg.localLights && in.localLights.pointLightCount > 0 &&
                                        (cfg.surfaceLights > 0.0f || cfg.spellSurfaceLights > 0.0f);
    if (cfg.worldShadows && (cfg.lightShafts || lightsMayReachSurfaces))
        engine::AcquireWorldShadows(dev, m_worldShadows);
    static const WorldShadowInputs noWorldShadows;
    const WorldShadowInputs& shadows = cfg.lightShafts ? m_worldShadows : noWorldShadows;
    const bool terrainShadows = UpdateTerrainShadows(dev, in, shadowToLight, cfg);
    const int shadowMode = shadows.count | (shadows.hardwareComparison ? kHardwareShadowModeBit : 0);

    float common[9][4] = {
        {static_cast<float>(vp.X), static_cast<float>(vp.Y), static_cast<float>(vp.Width),
         static_cast<float>(vp.Height)},
        {static_cast<float>(scale), static_cast<float>(m_frame % 1024u), 1.0f / depthDesc.Width,
         1.0f / depthDesc.Height},
        {proj[0], proj[5], proj[8], proj[9]},
        {worldDepth.atInfinity, worldDepth.perInverseViewDepth, fog.maxDistance, worldDepth.deepest},
        {},
        {},
        {},
        {},
        {static_cast<float>(lowW), static_cast<float>(lowH), 1.0f / lowW, 1.0f / lowH},
    };
    std::memcpy(common[4], viewToWorld, sizeof(viewToWorld));

    const long long now = Ticks();
    const float dx = in.camPos[0] - m_prevCam[0];
    const float dy = in.camPos[1] - m_prevCam[1];
    const float dz = in.camPos[2] - m_prevCam[2];
    const float forwardDot = worldToView[2] * m_prevWorldToView[2] +
                             worldToView[6] * m_prevWorldToView[6] + worldToView[10] * m_prevWorldToView[10];
    const bool historyValid = m_historyValid && cfg.temporal > 0.0f && m_prevScale == scale &&
                              SameFogSettings(cfg, m_prevConfig) && in.mapId == m_prevMap &&
                              in.lightParams.screenEffectSlot == m_prevLightSlot &&
                              shadowMode == m_prevShadowMode && forwardDot > kHistoryMinForwardDot &&
                              std::memcmp(m_prevProj, proj, sizeof(m_prevProj)) == 0 &&
                              std::memcmp(&m_prevViewport, &vp, sizeof(vp)) == 0 &&
                              TickSeconds(now - m_prevTicks) < kHistoryMaxSeconds &&
                              dx * dx + dy * dy + dz * dz < kHistoryMaxMove * kHistoryMaxMove;
    float reproj[16];
    float prevViewProj[16];
    Mul4x4(m_prevWorldToView, m_prevProj, prevViewProj);
    Mul4x4(viewToWorld, prevViewProj, reproj);

    const LightOnScreen celestialOnScreen = ProjectLightOntoViewport(toLightInView, proj, vp);
    const float sunPx[2] = {celestialOnScreen.pixel[0], celestialOnScreen.pixel[1]};
    const bool sunInFront = celestialOnScreen.inFront;
    const float sunNdcExtent = std::max(std::fabs(celestialOnScreen.ndc[0]), std::fabs(celestialOnScreen.ndc[1]));
    const float sunScreenFade = sunInFront ? std::clamp(1.6f - sunNdcExtent, 0.0f, 1.0f) : 0.0f;
    const float rayStrength = cfg.godRays * fog.lightVisibility * sunScreenFade;
    const bool rays = rayStrength > kMinGodRayStrength;
    const bool raysAfterWorld = rays && pass == FogPass::BeforeTransparents;
    const bool raysNow = rays && !raysAfterWorld;
    GodRayFrame godRays;
    std::memcpy(godRays.common, common, sizeof(common));
    godRays.viewport = vp;
    std::memcpy(godRays.toLightInView, toLightInView, sizeof(toLightInView));
    std::memcpy(godRays.sunPx, sunPx, sizeof(sunPx));
    godRays.strength = rayStrength;
    std::memcpy(godRays.colour, fog.rayColor, sizeof(godRays.colour));
    godRays.glowToCompensate = cfg.glowCompensation ? in.clientGlowAmount : 0.0f;
    godRays.character = ProjectCharacterOntoViewport(in.cameraFocus, in.camPos, in.cameraRelativeView, proj, vp);

    LogFrameSummary(dev, now, in, cfg, fog, authored, depthDesc, viewToWorld, toLightInView, sunPx, rayStrength);

    if (LogEnabled(LogLevel::Info))
        m_gpuTimer.Begin(dev);
    if (!depth.Refresh(dev, DepthRefreshSite::Fog))
    {
        m_gpuTimer.Cancel();
        return Skip("multisampled depth copy failed");
    }
    IDirect3DTexture9* const depthTexture = depth.texture;
    PrepareFullscreenPasses(dev);
    FaultWhenInjected();
    dev->SetPixelShaderConstantF(0, &common[0][0], 9);

    SetTarget(dev, m_marchTarget);
    const Float4 celestialLight = {toLightInView[0], toLightInView[1], toLightInView[2], fog.lightAboveHorizon};
    const Float4 march = {cfg.temporal > 0.0f ? 1.0f : 0.0f, fog.maxDistance, fog.horizonStart, fog.farClip};
    dev->SetPixelShaderConstantF(9, &celestialLight.x, 1);
    dev->SetPixelShaderConstantF(11, &march.x, 1);
    static_assert(sizeof(FogLayer) == 6 * sizeof(Float4), "FogLayer is six shader registers");
    FogLayer marchLayers[kFogLayers];
    PackMarchLayers(fog, marchLayers);
    dev->SetPixelShaderConstantF(12, &marchLayers[0].start, 6 * kFogLayers);
    const Float4 sunShadow = {cfg.lightShafts ? 1.0f : 0.0f, shadows.hardwareComparison ? 1.0f : 0.0f,
                              shadows.count > 1 ? shadowPlan.mapWeight : 0.0f,
                              shadows.count > 0 ? shadowPlan.mapWeight : 0.0f};
    BindSunShadows(dev, in, shadows, shadowToLight, &sunShadow.x);
    BindTerrainShadows(dev, terrainShadows);
    uint32_t pointLightCount = cfg.localLights ? std::min(in.localLights.pointLightCount, kMaxLocalPointLights) : 0;
    Float4 localConstants[32] = {};
    for (uint32_t i = 0; i < pointLightCount; ++i)
    {
        const LocalPointLight& light = in.localLights.pointLights[i];
        float relative[3] = {light.position[0] - in.camPos[0], light.position[1] - in.camPos[1],
                              light.position[2] - in.camPos[2]};
        float position[3];
        TransformDirection(relative, in.cameraRelativeView, position);
        localConstants[i * 3] = {position[0], position[1], position[2], light.cutoff};
        localConstants[i * 3 + 1] = {light.uploadedColor[0], light.uploadedColor[1], light.uploadedColor[2],
                                     LocalLightSurfaceStrength(cfg, light)};
        localConstants[i * 3 + 2] = {light.attenuation[0], light.attenuation[1], light.attenuation[2],
                                     LocalLightFogGain(fog, light)};
    }
    if (pointLightCount > 0)
    {
        if (!m_localLightData && m_localLightRetry.Ready() &&
            !(AttemptCreation(kFogNoise) &&
              SUCCEEDED(dev->CreateTexture(32, 1, 1, D3DUSAGE_DYNAMIC, D3DFMT_A32B32G32R32F, D3DPOOL_DEFAULT,
                                           &m_localLightData, nullptr))))
            m_localLightRetry.Failed();
        D3DLOCKED_RECT locked = {};
        if (m_localLightData && SUCCEEDED(m_localLightData->LockRect(0, &locked, nullptr, D3DLOCK_DISCARD)))
        {
            std::memcpy(locked.pBits, localConstants, sizeof(localConstants));
            m_localLightData->UnlockRect(0);
        }
        else
            pointLightCount = 0;
    }
    float marchLightLimit = 0.0f;
    for (uint32_t i = 0; i < pointLightCount; ++i)
    {
        const Float4& positionRadius = localConstants[i * 3];
        const double x = positionRadius.x;
        const double y = positionRadius.y;
        const double z = positionRadius.z;
        const float limit = static_cast<float>(std::sqrt(x * x + y * y + z * z) + positionRadius.w + 0.001);
        marchLightLimit = std::max(marchLightLimit, std::nextafter(limit, std::numeric_limits<float>::infinity()));
    }
    LogUploadedLocalLights(in, cfg, pointLightCount);
    const Float4 localControl = {static_cast<float>(pointLightCount), marchLightLimit, cfg.localLightPhase, 0.0f};
    const bool marchesLocalLights = pointLightCount > 0;
    const bool tracesSunShadows = cfg.lightShafts;
    m_drawnMarch = MarchShaders(tracesSunShadows, marchesLocalLights, samplesNoise)[std::clamp(cfg.quality, 1, 3) - 1];
    dev->SetPixelShader(m_drawnMarch);
    dev->SetPixelShaderConstantF(53, &localControl.x, 1);
    BindTexture(dev, 8, m_localLightData, false);
    if (cfg.noiseAmount > 0.0f && !m_densityNoise && m_densityNoiseRetry.Ready() &&
        !(AttemptCreation(kFogNoise) && CreateDensityNoise(dev, &m_densityNoise)))
        m_densityNoiseRetry.Failed();
    BindWrappedVolume(dev, 9, m_densityNoise);
    UploadLayerNoise(dev, fog, in.camPos, now);
    const double windPeriod = static_cast<double>(kDensityNoiseSize) / std::max(cfg.noiseScale, 0.001f);
    const Float4 variation = {m_densityNoise ? cfg.noiseAmount : 0.0f, cfg.noiseScale,
                              static_cast<float>(std::fmod(TickSeconds(now) * cfg.noiseWindSpeed, windPeriod)), 0.0f};
    dev->SetPixelShaderConstantF(78, &variation.x, 1);
    BindTexture(dev, 0, depthTexture, false);
    const bool timesPasses = LogEnabled(LogLevel::Info);
    if (timesPasses)
        m_marchTimer.Begin(dev);
    DrawFullscreenTriangle(dev);
    m_marchTimer.End();

    m_adaptiveLightingHistory = pointLightCount > 0 || m_prevLocalLightCount > 0 || shadows.count > 0 ||
                                cfg.noiseAmount > 0.0f || samplesNoise;
    const bool temporalFiltering = cfg.temporal > 0.0f;
    if (timesPasses && temporalFiltering)
        m_temporalTimer.Begin(dev);
    IDirect3DTexture9* const fogResult =
        temporalFiltering ? FilterWithHistory(dev, depthTexture, reproj, historyValid, cfg.temporal) : m_marchTarget;
    m_temporalTimer.End();

    const bool compositeBlendsInShader = fog.linear || raysNow;
    const float surfaceLightStrength =
        StrongestSurfaceLight(localConstants, pointLightCount) > 0.0f
            ? SurfaceLightVisibility(localConstants, pointLightCount, in.ambientColor, in.directColor)
            : 0.0f;
    const SurfaceLightFrame surfaceLights = {
        vp,
        surfaceLightStrength > 0.0f ? AreaOfSurfaceLights(localConstants, pointLightCount, proj, vp)
                                    : SurfaceLightArea(),
        surfaceLightStrength,
        marchLightLimit,
        in.ambientColor,
        in.directColor,
        {toLightInView[0], toLightInView[1], toLightInView[2]},
        &m_worldShadows};
    const bool lightsReachSurfaces = !surfaceLights.area.Empty();
    CountSurfaceLightCoverage(surfaceLights.area);
    const bool lightsGoIntoTheSceneCopy = lightsReachSurfaces && compositeBlendsInShader &&
                                          !Seam(g_surfaceLightsKeptInTarget, false) &&
                                          EnsureSceneCopy(dev, target, vp.Width, vp.Height) && EnsureSceneBeside(dev);
    if (lightsReachSurfaces && !lightsGoIntoTheSceneCopy)
        LightTheTarget(dev, depthTexture, target, surfaceLights);
    IDirect3DTexture9* blendScene = nullptr;
    if (compositeBlendsInShader && CopySceneForTheComposite(dev, target, vp))
        blendScene = lightsGoIntoTheSceneCopy ? LightTheSceneCopy(dev, depthTexture, surfaceLights) : m_sceneCopy;
    if (lightsReachSurfaces)
        BindWorldShadowMaps(dev, shadows);
    const bool sceneCopyReady = blendScene != nullptr;
    const bool raysCopied = rays && CopySceneForGodRays(dev, target, vp, blendScene);
    const bool raysDrawnNow = raysNow && raysCopied;
    const bool raysDrawnAfterWorld = raysAfterWorld && raysCopied;
    if (raysDrawnNow)
        DrawGodRayMask(dev, depthTexture, godRays);

    FogBlend blend = fog.linear ? FogBlend::LinearFixedFunction : FogBlend::GammaFixedFunction;
    if (sceneCopyReady)
        blend = fog.linear ? FogBlend::LinearOverSceneCopy : FogBlend::GammaOverSceneCopy;
    const bool sceneBlend = sceneCopyReady;
    const float blendMode = static_cast<float>(blend);
    if (blendMode != m_loggedBlendMode)
    {
        m_loggedBlendMode = blendMode;
        VF_LOG_INFO("fog blend: %s", FogBlendName(blend));
    }

    const RECT world = ViewportRect(vp);
    dev->SetRenderTarget(0, target);
    dev->SetViewport(&vp);
    dev->SetScissorRect(&world);
    dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
    dev->SetRenderState(D3DRS_ALPHABLENDENABLE, sceneBlend ? FALSE : TRUE);
    dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
    dev->SetRenderState(D3DRS_COLORWRITEENABLE,
                        D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE);
    const CompositeChoice compositeChoice = {tracesSunShadows, marchesLocalLights, samplesNoise, cfg.quality};
    const bool splitSamples = depth.Multisampled() && EnsureSplitComposites(dev);
    m_drawnComposite = CompositeShader(compositeChoice, false);
    m_drawnSplitComposite = splitSamples ? CompositeShader(compositeChoice, true) : nullptr;
    dev->SetPixelShader(m_drawnComposite);
    dev->SetPixelShaderConstantF(9, &celestialLight.x, 1);
    dev->SetPixelShaderConstantF(kSunShadowRegister, &sunShadow.x, 1);
    dev->SetPixelShaderConstantF(11, &march.x, 1);
    FogLayer compositeLayers[kFogLayers];
    PackMarchLayers(fog, compositeLayers);
    dev->SetPixelShaderConstantF(12, &compositeLayers[0].start, 6 * kFogLayers);
    m_drawnGlowCompensation = cfg.glowCompensation && sceneBlend ? in.clientGlowAmount : 0.0f;
    const Float4 composite[3] = {
        {fog.authored ? cfg.classicExposure : cfg.exposure, raysDrawnNow && sceneBlend ? rayStrength : 0.0f,
         static_cast<float>(cfg.debugView), blendMode},
        {fog.rayColor[0], fog.rayColor[1], fog.rayColor[2], kDrawsUnmatchedPixels},
        {sunPx[0], sunPx[1], cfg.sunMarker && sunInFront ? 1.0f : 0.0f, m_drawnGlowCompensation},
    };
    dev->SetPixelShaderConstantF(96, &composite[0].x, 3);
    const CharacterMask character = MaskTheCharacter(godRays.character);
    dev->SetPixelShaderConstantF(100, &character.box.x, 3);
    BindTexture(dev, 0, depthTexture, false);
    BindTexture(dev, 1, fogResult, false);
    BindTexture(dev, 2, m_rays[kShaftRays], true);
    BindTexture(dev, 3, blendScene, false);
    BindTexture(dev, 4, m_marchTarget, false);
    if (timesPasses)
        m_compositeTimer.Begin(dev);
    const bool unmatchedPixelsApart = !depth.Multisampled() && !Seam<bool>(g_compositeInOnePass, false) &&
                                      EnsureUnmatchedPixelStencil(dev, depthDesc.Width, depthDesc.Height);
    m_compositePasses = splitSamples ? 3 : (unmatchedPixelsApart ? 2 : 1);
    if (splitSamples)
        DrawCompositeBySampleDepth(dev, depth.bound, vp, compositeChoice, sceneBlend);
    else if (unmatchedPixelsApart)
        DrawCompositeWithUnmatchedPixelsApart(dev, vp, &composite[1].x);
    else
        DrawFullscreenTriangle(dev);
    m_compositeTimer.End();
    if (raysDrawnAfterWorld)
    {
        m_lateGodRays = godRays;
        m_lateGodRays.pending = true;
        m_gpuTimer.Pause();
    }
    else
        m_gpuTimer.End();
    if (pass == FogPass::BeforeTransparents)
        m_stockFogFit = FitStockFog(fog, in, {composite[0].x, composite[2].w, pointLightCount, cfg.localLightPhase});

    if (DepthProbeDue(now))
    {
        m_probeTicks = now;
        ++m_probeAttempts;
        IssueDepthProbe(dev, depthTexture, fogResult, vp, worldDepth.deepest, in.dayFraction);
    }

    std::memcpy(m_prevWorldToView, worldToView, sizeof(m_prevWorldToView));
    std::memcpy(m_prevProj, in.glProjection, sizeof(m_prevProj));
    std::memcpy(m_prevCam, in.camPos, sizeof(m_prevCam));
    m_prevViewport = vp;
    m_prevScale = scale;
    m_prevConfig = cfg;
    m_prevMap = in.mapId;
    m_prevLightSlot = in.lightParams.screenEffectSlot;
    m_prevShadowMode = shadowMode;
    m_prevLocalLightCount = pointLightCount;
    m_prevTicks = now;
    m_historyValid = temporalFiltering;
    ++m_frame;
    return true;
}
