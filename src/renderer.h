#pragma once

#include "config.h"
#include "engine.h"
#include "engine_shadows.h"
#include "fog_data.h"
#include "fog_model.h"
#include "gpu_timing.h"
#include "msaa_depth.h"
#include "retry_gate.h"
#include "terrain_shadows.h"
#include "transparent_fog.h"

#include <d3d9.h>

#ifdef VF_TESTING
void ForceFogParams(const FogParams* fog);
#endif
enum FogResourceKind : unsigned
{
    kFogShaders = 1,
    kFogTargets = 2,
    kFogNoise = 4,
};

#ifdef VF_TESTING
void FailFogResourceCreation(unsigned kinds);
unsigned FogResourceCreationAttempts(FogResourceKind kind);
#endif
FogParams FogParamsFor(const FrameInputs& in, const Config& cfg, const AuthoredFog* authored);
uint32_t LoggedLightsSignature(const AuthoredFog& fog);

constexpr int kFogQualityLevels = 3;
constexpr int kMarchFamilies = 8;
constexpr int kCompositeFamilies = 4;

class Renderer
{
public:
    ~Renderer();

    void ReleaseDefaultPool();
    void ReleaseManagedPool();
    void ReleaseAll();

#ifdef VF_TESTING
    static void InjectPassFault(bool faulting);
#endif
    bool Render(IDirect3DDevice9* dev, const SceneDepth& depth, const FrameInputs& in, const Config& cfg,
                FogPass pass = FogPass::WholeFrame);
    bool RenderGodRaysAfterWorld(IDirect3DDevice9* dev, const SceneDepth& depth);
    bool ReadyToRender(IDirect3DDevice9* dev, const SceneDepth& depth, const D3DVIEWPORT9& vp);

    const StockFogFit& LastStockFogFit() const { return m_stockFogFit; }
    const char* LastSkipReason() const { return m_skip; }
    bool AdaptiveLightingHistory() const { return m_adaptiveLightingHistory; }
    const void* DensityNoise() const { return m_densityNoise; }
    const void* AuthoredNoise() const { return m_authoredNoise; }
    IDirect3DPixelShader9* DrawnMarch() const { return m_drawnMarch; }
    IDirect3DPixelShader9* DrawnComposite() const { return m_drawnComposite; }
    IDirect3DPixelShader9* DrawnSplitComposite() const { return m_drawnSplitComposite; }
    float DrawnGlowCompensation() const { return m_drawnGlowCompensation; }
    unsigned SurfaceLightDraws() const { return m_surfaceLightDraws; }

private:
    struct PendingDepthProbe
    {
        bool issued = false;
        unsigned attempt = 0;
        float dayFraction = 0.0f;
        float viewportMinZ = 0.0f;
        float viewportMaxZ = 0.0f;
        float deepestWorldDepth = 0.0f;
    };

    struct CompositeChoice
    {
        bool shadowed;
        bool lit;
        bool noisy;
        int quality;
    };

    struct GodRayFrame
    {
        bool pending = false;
        float common[9][4] = {};
        D3DVIEWPORT9 viewport = {};
        float toLightInView[3] = {};
        float sunPx[2] = {};
        float strength = 0.0f;
        float colour[3] = {};
        float glowToCompensate = 0.0f;
    };

    struct SurfaceLightFrame
    {
        D3DVIEWPORT9 viewport;
        float strength;
        float reach;
        uint32_t ambientColor;
        uint32_t directColor;
        float toLightInView[3];
    };

    template <typename Passes>
    bool WithClientStateSaved(IDirect3DDevice9* dev, const SceneDepth& depth, Passes passes);
    void PrepareFullscreenPasses(IDirect3DDevice9* dev);
    void LightSurfaces(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DSurface9* target,
                       const SurfaceLightFrame& frame);
    void CopySceneForGodRays(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp);
    void DrawGodRayMask(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, const GodRayFrame& rays);
    bool DrawGodRaysOverScene(IDirect3DDevice9* dev, const SceneDepth& depth, IDirect3DSurface9* target,
                              const D3DSURFACE_DESC& depthDesc, const GodRayFrame& rays);
    bool EnsureShaders(IDirect3DDevice9* dev);
    bool EnsureSplitComposites(IDirect3DDevice9* dev);
    void ReleaseSplitComposites();
    IDirect3DPixelShader9* const* MarchShaders(bool shadowed, bool lit, bool noisy) const;
    IDirect3DPixelShader9* CompositeShader(const CompositeChoice& choice, bool splitSamples) const;
    void MarkSilhouetteSamples(IDirect3DDevice9* dev, IDirect3DSurface9* sampleDepth, const D3DVIEWPORT9& vp);
    void DrawSamplesMarked(IDirect3DDevice9* dev, DWORD marker, IDirect3DPixelShader9* shader, const float* side);
    void DrawCompositeBySampleDepth(IDirect3DDevice9* dev, IDirect3DSurface9* sampleDepth, const D3DVIEWPORT9& vp,
                                    const CompositeChoice& choice, bool overwrites);
    bool EnsureStateBlock(IDirect3DDevice9* dev);
    bool EnsureTargets(IDirect3DDevice9* dev, UINT lowW, UINT lowH, UINT rayW, UINT rayH);
    bool EnsureSceneCopy(IDirect3DDevice9* dev, IDirect3DSurface9* target, UINT w, UINT h);
    bool CopyWorldViewport(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DVIEWPORT9& vp);
    bool Skip(const char* reason);
    bool NotReady(const char* reason);
    void LogLightChange(const FrameInputs& in, const AuthoredFog& fog, bool authored);
    void LogFirstLocalLightRejection(LocalLightCapture capture);
    void LogUploadedLocalLights(const FrameInputs& in, const Config& cfg, uint32_t uploaded);
    bool DepthProbeDue(long long now) const;
    bool EnsureDepthProbe(IDirect3DDevice9* dev);
    void IssueDepthProbe(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture, IDirect3DTexture9* fog,
                         const D3DVIEWPORT9& vp, float deepestWorldDepth, float dayFraction);
    void LogFinishedDepthProbe();
    void DropPendingDepthProbe();
    IDirect3DTexture9* FilterWithHistory(IDirect3DDevice9* dev, IDirect3DTexture9* depthTexture,
                                         const float* viewToPreviousClip, bool historyValid, float historyWeight);
    void LogFrameSummary(IDirect3DDevice9* dev, long long now, const FrameInputs& in, const Config& cfg,
                         const FogParams& fog, const AuthoredFog& authored, const D3DSURFACE_DESC& depthDesc,
                         const float* viewToWorld, const float* toLightInView, const float* sunPx, float rayStrength);
    void BindTexture(IDirect3DDevice9* dev, DWORD stage, IDirect3DBaseTexture9* tex, bool linear);
    void BindWrappedVolume(IDirect3DDevice9* dev, DWORD stage, IDirect3DVolumeTexture9* volume);
    FogParams DrawableFog(IDirect3DDevice9* dev, const FogParams& fog);
    void BindSunShadows(IDirect3DDevice9* dev, const FrameInputs& in, const WorldShadowInputs& shadows,
                        const float* shadowToLight, const float* control);
    bool UpdateTerrainShadows(IDirect3DDevice9* dev, const FrameInputs& in, const float* shadowToLight,
                              const Config& cfg);
    void BindTerrainShadows(IDirect3DDevice9* dev, bool active);
    void UploadLayerNoise(IDirect3DDevice9* dev, const FogParams& fog, const float* camera, long long now);
    bool RenderPasses(IDirect3DDevice9* dev, const SceneDepth& depth, IDirect3DSurface9* target,
                      const D3DSURFACE_DESC& depthDesc, const FrameInputs& in, const Config& cfg, FogPass pass);

    IDirect3DVertexShader9* m_vs = nullptr;
    IDirect3DDevice9* m_unsupportedShaderDevice = nullptr;
    RetryGate m_shaderRetry;
    RetryGate m_targetRetry;
    RetryGate m_authoredNoiseRetry;
    RetryGate m_densityNoiseRetry;
    RetryGate m_localLightRetry;
    IDirect3DPixelShader9* m_marchShaders[kMarchFamilies][kFogQualityLevels] = {};
    IDirect3DPixelShader9* m_temporal = nullptr;
    IDirect3DPixelShader9* m_historyDepthShader = nullptr;
    IDirect3DPixelShader9* m_compositeShaders[kCompositeFamilies][kFogQualityLevels] = {};
    IDirect3DPixelShader9* m_silhouetteMask = nullptr;
    IDirect3DPixelShader9* m_splitCompositeShaders[kCompositeFamilies][kFogQualityLevels] = {};
    bool m_splitCompositesUnavailable = false;
    IDirect3DPixelShader9* m_drawnMarch = nullptr;
    IDirect3DPixelShader9* m_drawnComposite = nullptr;
    IDirect3DPixelShader9* m_drawnSplitComposite = nullptr;
    float m_drawnGlowCompensation = 0.0f;
    IDirect3DPixelShader9* m_rayMask = nullptr;
    IDirect3DPixelShader9* m_rayBlur = nullptr;
    IDirect3DPixelShader9* m_rayComposite = nullptr;
    IDirect3DPixelShader9* m_surfaceLights = nullptr;
    IDirect3DPixelShader9* m_probe = nullptr;
    IDirect3DVertexDeclaration9* m_decl = nullptr;
    IDirect3DStateBlock9* m_state = nullptr;

    IDirect3DTexture9* m_marchTarget = nullptr;
    IDirect3DTexture9* m_history[2] = {};
    IDirect3DTexture9* m_historyDepth = nullptr;
    IDirect3DTexture9* m_rays[2] = {};
    IDirect3DTexture9* m_sceneCopy = nullptr;
    IDirect3DTexture9* m_localLightData = nullptr;
    IDirect3DVolumeTexture9* m_densityNoise = nullptr;
    IDirect3DVolumeTexture9* m_authoredNoise = nullptr;
    TerrainShadowMap m_terrainShadows;
    AuthoredNoiseScroll m_noiseScroll;
    long long m_noiseTicks = 0;
    IDirect3DTexture9* m_probeTarget = nullptr;
    IDirect3DSurface9* m_probeReadback = nullptr;
    IDirect3DQuery9* m_probeCopied = nullptr;
    PendingDepthProbe m_pendingProbe;
    GpuTimer m_gpuTimer{"fog"};
    WorldShadowInputs m_worldShadows;
    StockFogFit m_stockFogFit;
    GodRayFrame m_lateGodRays;
    UINT m_lowW = 0;
    UINT m_lowH = 0;
    UINT m_rayW = 0;
    UINT m_rayH = 0;
    UINT m_sceneCopyW = 0;
    UINT m_sceneCopyH = 0;
    bool m_sceneCopyFailed = false;
    bool m_probeFailed = false;

    int m_historyIndex = 0;
    bool m_historyValid = false;
    bool m_adaptiveLightingHistory = false;
    uint32_t m_prevLocalLightCount = 0;
    Config m_prevConfig;
    int m_prevMap = -1;
    int m_prevLightSlot = -1;
    int m_prevShadowMode = -1;
    float m_prevWorldToView[16] = {};
    float m_prevProj[16] = {};
    float m_prevCam[3] = {};
    D3DVIEWPORT9 m_prevViewport = {};
    UINT m_prevScale = 0;
    long long m_prevTicks = 0;
    unsigned m_frame = 0;
    unsigned m_surfaceLightDraws = 0;
    unsigned m_logged = 0;
    long long m_summaryTicks = 0;
    long long m_probeTicks = 0;
    unsigned m_probeAttempts = 0;
    bool m_lightsLogged = false;
    float m_loggedFarClip = 0.0f;
    float m_loggedBlendMode = -1.0f;
    D3DVIEWPORT9 m_loggedViewport = {};
    uint32_t m_lightSignature = 0;
    uint64_t m_pendingLocalLightSet = 0;
    unsigned m_pendingLocalLightFrames = 0;
    uint64_t m_loggedLocalLightSet = 0;
    bool m_loggedLocalLightDetail = true;
    uint32_t m_loggedLocalLightRejections = 0;
    const char* m_skip = "";
};
