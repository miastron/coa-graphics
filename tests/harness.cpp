#include "character_on_screen.h"
#include "client_camera_focus.h"
#include "client_game_clock.h"
#include "client_light_stay.h"
#include "client_ripple_sprites.h"
#include "config.h"
#include "engine.h"
#include "engine_shadows.h"
#include "fog_data.h"
#include "fog_model.h"
#include "forever_look.h"
#include "fullscreen_triangle.h"
#include "frame_copies.h"
#include "gpu_timing.h"
#include "grading_renderer.h"
#include "image_quality.h"
#include "ini_section.h"
#include "light_on_screen.h"
#include "local_light_stabiliser.h"
#include "log.h"
#include "msaa_depth.h"
#include "spell_lights.h"
#include "surface_light_area.h"
#include "texture_filtering.h"
#include "grass_density.h"
#include "grass_doodads.h"
#include "engine_grass.h"
#include "grass_wind.h"
#include "snow_cover.h"
#include "snow_decals.h"
#include "snow_footprints.h"
#include "snow_shaders.h"
#include "snow_surface.h"
#include "wdbc.h"
#include "wind_model.h"
#include "texture_tint.h"
#include "tree_wind.h"
#include "noise_volume.h"
#include "water_contacts.h"
#include "water_data.h"
#include "water_fft.h"
#include "water_ripples.h"
#include "water_spectrum.h"
#include "status_log.h"
#include "target_facing.h"
#include "terrain_shadows.h"
#include "transparent_fog.h"

#include <windows.h>
#include <d3d9.h>
#include <share.h>
#include <wincodec.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

extern "C" __declspec(dllimport) IDirect3D9* __cdecl vf_test_wrap_direct3d9(IDirect3D9*(WINAPI*)(UINT), UINT);
extern "C" __declspec(dllimport) void __cdecl vf_test_set_config(const Config*);
extern "C" __declspec(dllimport) void __cdecl vf_test_get_config(Config*);
extern "C" __declspec(dllimport) int __cdecl vf_test_render(const FrameInputs*, const char**);
extern "C" __declspec(dllimport) int __cdecl vf_test_adaptive_lighting_history();
extern "C" __declspec(dllimport) int __cdecl vf_test_fog_history_valid();
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_ray_source_copy(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_managed_noise(const void** density, const void** authored);
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_fog_resources(unsigned kinds);
extern "C" __declspec(dllimport) unsigned __cdecl vf_test_fog_resource_attempts(unsigned kind);
extern "C" __declspec(dllimport) void __cdecl vf_test_drawn_fog_shaders(IDirect3DPixelShader9**,
                                                                        IDirect3DPixelShader9**,
                                                                        IDirect3DPixelShader9**);
extern "C" __declspec(dllimport) void __cdecl vf_test_force_depth_write(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_suppress_depth_write(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_overlay_visible();
extern "C" __declspec(dllimport) void __cdecl vf_test_draw_overlay();
extern "C" __declspec(dllimport) int __cdecl vf_test_settings_window(float*);
extern "C" __declspec(dllimport) int __cdecl vf_test_assign_water_data(const WaterPreset*, int,
                                                                       const WaterFftTile*, int,
                                                                       const WaterMaskView*, int);
extern "C" __declspec(dllimport) int __cdecl vf_test_load_water_data(const char*);
extern "C" __declspec(dllimport) int __cdecl vf_test_water_begin(const FrameInputs*, const WaterInputs*, const char**);
extern "C" __declspec(dllimport) void __cdecl vf_test_water_tag(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_water_untag();
extern "C" __declspec(dllimport) int __cdecl vf_test_water_end(const char**);
extern "C" __declspec(dllimport) void __cdecl vf_test_set_water_seconds(double);
extern "C" __declspec(dllimport) void __cdecl vf_test_disable_wave_simulation(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_force_packed_water_depth(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_transparent_liquids_queued(const void*);
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_water_in_window(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_water_abort();
extern "C" __declspec(dllimport) void __cdecl vf_test_force_water_summary();
extern "C" __declspec(dllimport) int __cdecl vf_test_water_data_loaded();
extern "C" __declspec(dllimport) unsigned __cdecl vf_test_water_resources_held();
extern "C" __declspec(dllimport) int __cdecl vf_test_water_mask_pool();
extern "C" __declspec(dllimport) void __cdecl vf_test_use_water_hook_client(const FrameInputs*, const WaterInputs*);
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_water_pass_begin(const void*);
extern "C" __declspec(dllimport) int __cdecl vf_test_hook_water_draw_tag(const void*);
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_water_draw_untag();
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_water_pass_end();
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_frame_end();
extern "C" __declspec(dllimport) int __cdecl vf_test_water_armed();
extern "C" __declspec(dllimport) int __cdecl vf_test_water_status(const char**);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_water_pass_thunk(uintptr_t);
extern "C" __declspec(dllimport) void __cdecl vf_test_water_pass_begin_reuses_argument_slot(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_particle_render_thunks(uintptr_t, uintptr_t, const void**,
                                                                            const void**);
extern "C" __declspec(dllimport) void __cdecl vf_test_model_animate_thunks(uintptr_t, uintptr_t, const float*,
                                                                          const void**, const void**);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_grass_thunks(
    uintptr_t, uintptr_t, float*, const void*, void (*)(unsigned, const float*, unsigned), const void**);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_grass_doodad_thunks(
    uintptr_t, uintptr_t, uintptr_t, bool (*)(uint32_t, ModelBands&), bool (*)(unsigned, unsigned, float*), int,
    const void**, const void**);
extern "C" __declspec(dllimport) void __cdecl vf_test_set_grass_density(float, float, float);
extern "C" __declspec(dllimport) void __cdecl vf_test_enable_grass_shaders(int);
extern "C" __declspec(dllimport) uint32_t __cdecl vf_test_publish_spell_lights(LocalPointLight*, uint32_t, float);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_mesh_batch_draw_thunk(uintptr_t);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_target_circle_thunk(uintptr_t, uintptr_t);
extern "C" __declspec(dllimport) int __cdecl vf_test_begin_target_arrow(IDirect3DDevice9*);
extern "C" __declspec(dllimport) void __cdecl vf_test_end_target_arrow(IDirect3DDevice9*);
extern "C" __declspec(dllimport) int __cdecl vf_test_texture_tint(uintptr_t, int, float*);
extern "C" __declspec(dllimport) void __cdecl vf_test_record_fog_frame(int, int, const char*);
extern "C" __declspec(dllimport) void __cdecl vf_test_use_fog_hook_client(const FrameInputs*);
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_stock_fog(engine::StockFog*, const engine::StockFog*);
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_frame_begin();
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_liquid_end();
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_world_done();
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_m2_batch_fog(M2BatchFogArgs*);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_m2_batch_fog_thunk(uintptr_t);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_m2_unfogged_batch_thunk(uintptr_t);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_glare_pass_thunk(uintptr_t);
extern "C" __declspec(dllimport) const void* __cdecl vf_test_world_text_draw_thunk(uintptr_t);
extern "C" __declspec(dllimport) void __cdecl vf_test_log_transparent_fog_stats();
extern "C" __declspec(dllimport) void __cdecl vf_test_clear_transparent_fog_failure();
extern "C" __declspec(dllimport) void __cdecl vf_test_inject_fog_pass_fault(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_refuse_fog_depth(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_fog_data_loaded();
extern "C" __declspec(dllimport) void __cdecl vf_test_force_fog_params(const FogParams*);
extern "C" __declspec(dllimport) void __cdecl vf_test_keep_surface_lights_in_target(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_light_surfaces_over_the_whole_view(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_take_god_rays_from_target(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_water_mask_uploads(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_water_masks_uploaded(int*);
extern "C" __declspec(dllimport) void __cdecl vf_test_force_water_shading_variant(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_water_shading_variant();
extern "C" __declspec(dllimport) void __cdecl vf_test_force_depth_copy_method(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_override_client_shadow_vector(const float*);
extern "C" __declspec(dllimport) void __cdecl vf_test_override_world_shadows(const WorldShadowInputs*);
extern "C" __declspec(dllimport) int __cdecl vf_test_read_scene_depth(const DepthTexel*, int, float*);
extern "C" __declspec(dllimport) void __cdecl vf_test_multisampling(MultisamplingStatus*);
extern "C" __declspec(dllimport) int __cdecl vf_test_refresh_scene_depth();
extern "C" __declspec(dllimport) void __cdecl vf_test_take_frame_copies(FrameCopyCounts*);
extern "C" __declspec(dllimport) void __cdecl vf_test_probe_depth_copy(IDirect3D9*, DepthCopyProbe*);
extern "C" __declspec(dllimport) const char* __cdecl vf_test_load_nvapi(LibraryLoader);
extern "C" __declspec(dllimport) const char* __cdecl vf_test_log_path();
extern "C" __declspec(dllimport) D3DFORMAT __cdecl vf_test_display_format_for(D3DFORMAT);
extern "C" __declspec(dllimport) void __cdecl vf_test_water_ripple_stats(WaterRippleStats*);
extern "C" __declspec(dllimport) void __cdecl vf_test_water_ripple_shading(WaterRippleShading*);
extern "C" __declspec(dllimport) unsigned __cdecl vf_test_water_contact_reads();
extern "C" __declspec(dllimport) void __cdecl vf_test_refuse_water_contacts(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_inject_water_ripple_fault(int);
extern "C" __declspec(dllimport) int __cdecl vf_test_bind_client_ripple_gate(volatile int32_t*, uintptr_t,
                                                                             const unsigned char*, size_t);
extern "C" __declspec(dllimport) void __cdecl vf_test_update_client_ripple_sprites(int, int, double);
extern "C" __declspec(dllimport) void __cdecl vf_test_use_forever_look_frame(const ForeverLookFrame*);
extern "C" __declspec(dllimport) void __cdecl vf_test_forever_look_world_done();
extern "C" __declspec(dllimport) int __cdecl vf_test_delivered_glow(float*);
extern "C" __declspec(dllimport) void __cdecl vf_test_grading_stats(GradingStats*);
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_grading_scene_copies(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_grading_pass(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_image_stats(ImageStats*);
extern "C" __declspec(dllimport) void __cdecl vf_test_fail_image_pass(int);
extern "C" __declspec(dllimport) void __cdecl vf_test_image_status(ImageQualityStatus*);
extern "C" __declspec(dllimport) void __cdecl vf_test_image_world_done();
extern "C" __declspec(dllimport) unsigned __cdecl vf_test_surface_light_draws();
extern "C" __declspec(dllimport) float __cdecl vf_test_drawn_surface_light_share();
extern "C" __declspec(dllimport) void __cdecl vf_test_composite_in_one_pass(int);
extern "C" __declspec(dllimport) unsigned __cdecl vf_test_composite_passes();
extern "C" __declspec(dllimport) void __cdecl vf_test_take_fog_pass_times(FogPassTimes*);
extern "C" __declspec(dllimport) int __cdecl vf_test_forever_look_guards(engine::CodeRange*, int);
extern "C" __declspec(dllimport) void __cdecl vf_test_forever_look_status(ForeverLookStatus*);
extern "C" __declspec(dllimport) void __cdecl vf_test_use_world_hook_client(const FrameInputs*, int);
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_opaque_done();
extern "C" __declspec(dllimport) void __cdecl vf_test_hook_world_done();
extern "C" __declspec(dllimport) void __cdecl vf_test_simulate_fog_hook_failure(int);
extern "C" __declspec(dllimport) float __cdecl vf_test_drawn_glow_compensation();
extern "C" __declspec(dllimport) uint32_t __cdecl vf_test_logged_lights_signature(const AuthoredFog*);

namespace
{
constexpr D3DFORMAT kIntz = static_cast<D3DFORMAT>(MAKEFOURCC('I', 'N', 'T', 'Z'));
constexpr D3DFORMAT kClientDepthFormat = D3DFMT_D24X8;
constexpr float kPi = 3.14159265f;
constexpr float kNear = 0.4f;
constexpr float kFar = 1000.0f;
constexpr float kFovY = 0.9f;
constexpr float kIdealWorldMaxZ = 1.0f;
constexpr float kClientWorldMaxZ = 0.94f;
constexpr float kClientWdlRawDepth = 0.9985f;
constexpr float kNoWdlPatch = 0.0f;
constexpr float kDisplayGamma = 2.2f;
constexpr int kReferenceIntegrationSteps = 8192;
constexpr BYTE kKeyHeld = 0x80;
constexpr UINT kKeyRepeatCountOne = 1;
constexpr UINT kKeyReleaseTransition = 0xC0000000u;
constexpr wchar_t kHotkeyCharacter = L'\x16';
constexpr int kOverlaySettleFrames = 3;
constexpr UINT kOverlayProbeLeft = 40;
constexpr UINT kOverlayProbeTop = 60;
constexpr UINT kOverlayProbeRight = 400;
constexpr UINT kOverlayProbeBottom = 300;
constexpr int kOverlayTitleBarX = 200;
constexpr int kOverlayTitleBarY = 40;
constexpr UINT kSkyRowsFraction = 4;
constexpr int kMaxAdjacentSkyColumnDifference = 2;
constexpr int kBesideOverlayX = 1100;
constexpr int kBesideOverlayY = 600;
constexpr int kOverlayBodyX = 480;
constexpr int kOverlayBodyY = 75;
constexpr int kDensitySliderY = 197;
constexpr int kDensitySliderGrabX = 60;
constexpr int kDensitySliderDragX = 250;
constexpr float kDensityAfterDragAtLeast = 2.5f;
constexpr float kPanelLinePixels = 16.0f;
constexpr float kPanelFirstWidth = 30.0f * kPanelLinePixels;
constexpr float kPanelMinWidth = 20.0f * kPanelLinePixels;
constexpr float kPanelMinHeight = 16.0f * kPanelLinePixels;
constexpr float kPanelMargin = 2.0f * kPanelLinePixels;
constexpr int kGripInset = 4;
constexpr int kGripDragX = 160;
constexpr int kGripDragY = 24;
constexpr int kGripShrink = -2000;
constexpr float kPanelSizeTolerance = 1.5f;
constexpr float kBesideShrunkPanelGap = 4.0f;
constexpr UINT kVoidProbeRows = 120;
constexpr int kMaxVoidMirrorDifference = 3;

int g_failures = 0;

void Check(bool ok, const char* what)
{
    std::printf("%s %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++g_failures;
}

struct Vec3
{
    float x, y, z;
};

Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 Norm(Vec3 a)
{
    float l = std::sqrt(Dot(a, a));
    return {a.x / l, a.y / l, a.z / l};
}

constexpr Vec3 kWorldUp = {0, 0, 1};
constexpr Vec3 kGameLikeWorldOffset = {-9100.0f, -100.0f, 80.0f};

float EyeHeightAboveGround(Vec3 eye)
{
    return eye.z - kGameLikeWorldOffset.z;
}

void CameraRelativeLookAt(Vec3 eye, Vec3 at, float* view)
{
    Vec3 forward = Norm(Sub(at, eye));
    Vec3 right = Norm(Cross(forward, kWorldUp));
    Vec3 up = Cross(right, forward);
    float rowVectors[16] = {right.x, up.x, forward.x, 0, right.y, up.y, forward.y, 0,
                            right.z, up.z, forward.z, 0, 0,       0,    0,         1};
    std::memcpy(view, rowVectors, sizeof(rowVectors));
}

void EngineGlDepthProjection(float yScale, float aspect, float zNear, float zFar, float* m)
{
    float p[16] = {yScale / aspect, 0, 0, 0, 0, yScale, 0, 0, 0, 0, (zFar + zNear) / (zFar - zNear), 1,
                   0, 0, -2.0f * zFar * zNear / (zFar - zNear), 0};
    std::memcpy(m, p, sizeof(p));
}

void EngineProjection(float aspect, float* m)
{
    EngineGlDepthProjection(1.0f / std::tan(kFovY * 0.5f), aspect, kNear, kFar, m);
}

void RemapToD3DDepthRange(const float* glDepthProjection, float* d3dProjection)
{
    std::memcpy(d3dProjection, glDepthProjection, sizeof(float) * 16);
    d3dProjection[10] = (1.0f + glDepthProjection[10]) * 0.5f;
    d3dProjection[14] = glDepthProjection[14] * 0.5f;
}

struct SceneVertex
{
    float x, y, z;
    DWORD color;
};

void AddQuad(std::vector<SceneVertex>& v, Vec3 a, Vec3 b, Vec3 c, Vec3 d, DWORD color,
             Vec3 offset = kGameLikeWorldOffset)
{
    for (Vec3 p : {a, b, c, a, c, d})
    {
        Vec3 w = Add(p, offset);
        v.push_back({w.x, w.y, w.z, color});
    }
}

void AddBox(std::vector<SceneVertex>& v, Vec3 lo, Vec3 hi, DWORD color)
{
    Vec3 p[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                 {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    AddQuad(v, p[0], p[1], p[5], p[4], color);
    AddQuad(v, p[1], p[2], p[6], p[5], color);
    AddQuad(v, p[2], p[3], p[7], p[6], color);
    AddQuad(v, p[3], p[0], p[4], p[7], color);
    AddQuad(v, p[4], p[5], p[6], p[7], color);
}

std::vector<SceneVertex> BuildScene()
{
    std::vector<SceneVertex> v;
    AddQuad(v, {-2000, -2000, 0}, {2000, -2000, 0}, {2000, 2000, 0}, {-2000, 2000, 0}, 0xFF4F6B3A);
    for (int i = 0; i < 14; ++i)
    {
        float x = 40.0f + i * 22.0f;
        float y = (i % 2 ? -1.0f : 1.0f) * (8.0f + (i * 7) % 30);
        AddBox(v, {x, y, 0}, {x + 4, y + 4, 25.0f + (i * 13) % 30}, 0xFF6E5A48);
    }
    AddBox(v, {600, -300, 0}, {640, 300, 90}, 0xFF5A5A62);
    return v;
}

bool SavePng(const std::wstring& path, UINT w, UINT h, const std::vector<unsigned char>& bgra)
{
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return false;
    IWICStream* stream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    bool ok = SUCCEEDED(factory->CreateStream(&stream)) &&
              SUCCEEDED(stream->InitializeFromFilename(path.c_str(), GENERIC_WRITE)) &&
              SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
              SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
              SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && SUCCEEDED(frame->Initialize(nullptr)) &&
              SUCCEEDED(frame->SetSize(w, h));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    ok = ok && SUCCEEDED(frame->SetPixelFormat(&format)) &&
         SUCCEEDED(frame->WritePixels(h, w * 4, static_cast<UINT>(bgra.size()), const_cast<BYTE*>(bgra.data()))) &&
         SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
    if (frame)
        frame->Release();
    if (encoder)
        encoder->Release();
    if (stream)
        stream->Release();
    factory->Release();
    return ok;
}

struct Image
{
    UINT w = 0, h = 0;
    std::vector<unsigned char> bgra;

    const unsigned char* At(UINT x, UINT y) const { return &bgra[(static_cast<size_t>(y) * w + x) * 4]; }
    float Luma(UINT x, UINT y) const
    {
        const unsigned char* p = At(x, y);
        return (0.114f * p[0] + 0.587f * p[1] + 0.299f * p[2]) / 255.0f;
    }
};

IDirect3DSurface9* ResolvedCopy(IDirect3DDevice9* dev, IDirect3DSurface9* target, const D3DSURFACE_DESC& desc)
{
    IDirect3DSurface9* resolved = nullptr;
    if (SUCCEEDED(dev->CreateRenderTarget(desc.Width, desc.Height, desc.Format, D3DMULTISAMPLE_NONE, 0, FALSE,
                                          &resolved, nullptr)) &&
        SUCCEEDED(dev->StretchRect(target, nullptr, resolved, nullptr, D3DTEXF_NONE)))
        return resolved;
    if (resolved)
        resolved->Release();
    target->AddRef();
    return target;
}

Image Capture(IDirect3DDevice9* dev)
{
    Image img;
    IDirect3DSurface9* bb = nullptr;
    IDirect3DSurface9* sys = nullptr;
    dev->GetRenderTarget(0, &bb);
    D3DSURFACE_DESC desc;
    bb->GetDesc(&desc);
    if (desc.MultiSampleType != D3DMULTISAMPLE_NONE)
    {
        IDirect3DSurface9* resolved = ResolvedCopy(dev, bb, desc);
        bb->Release();
        bb = resolved;
    }
    dev->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, D3DPOOL_SYSTEMMEM, &sys, nullptr);
    dev->GetRenderTargetData(bb, sys);
    D3DLOCKED_RECT lr;
    sys->LockRect(&lr, nullptr, D3DLOCK_READONLY);
    img.w = desc.Width;
    img.h = desc.Height;
    img.bgra.resize(static_cast<size_t>(img.w) * img.h * 4);
    for (UINT y = 0; y < img.h; ++y)
    {
        std::memcpy(&img.bgra[static_cast<size_t>(y) * img.w * 4], static_cast<unsigned char*>(lr.pBits) + y * lr.Pitch,
                    img.w * 4);
        for (UINT x = 0; x < img.w; ++x)
            img.bgra[(static_cast<size_t>(y) * img.w + x) * 4 + 3] = 255;
    }
    sys->UnlockRect();
    sys->Release();
    bb->Release();
    return img;
}

struct Rgb
{
    float r, g, b;
};

Rgb SampleRgb(const Image& img, int cx, int cy, int radius)
{
    Rgb sum = {};
    int n = 0;
    for (int y = cy - radius; y <= cy + radius; ++y)
        for (int x = cx - radius; x <= cx + radius; ++x)
        {
            const unsigned char* p = img.At(static_cast<UINT>(x), static_cast<UINT>(y));
            sum.r += p[2];
            sum.g += p[1];
            sum.b += p[0];
            ++n;
        }
    return {sum.r / n, sum.g / n, sum.b / n};
}

constexpr DWORD kFogPassTextureStages = 11;
constexpr UINT kFogPassPixelConstants = 105;

struct Sentinel
{
    DWORD renderStates[10];
    DWORD samplers[kFogPassTextureStages][4];
    IDirect3DBaseTexture9* textures[kFogPassTextureStages];
    IDirect3DVertexShader9* vs;
    IDirect3DPixelShader9* ps;
    IDirect3DVertexDeclaration9* decl;
    IDirect3DVertexBuffer9* stream;
    UINT streamOffset, streamStride;
    float constants[kFogPassPixelConstants * 4];
    D3DVIEWPORT9 viewport;
    RECT scissor;
    IDirect3DSurface9* rt;
    IDirect3DSurface9* ds;
};

const D3DRENDERSTATETYPE kSentinelStates[10] = {
    D3DRS_ZENABLE,  D3DRS_ZWRITEENABLE,      D3DRS_ALPHABLENDENABLE, D3DRS_SRCBLEND,  D3DRS_DESTBLEND,
    D3DRS_CULLMODE, D3DRS_SCISSORTESTENABLE, D3DRS_COLORWRITEENABLE, D3DRS_FOGENABLE, D3DRS_STENCILENABLE,
};
const D3DSAMPLERSTATETYPE kSentinelSamplers[4] = {D3DSAMP_ADDRESSU, D3DSAMP_MAGFILTER, D3DSAMP_MINFILTER,
                                                  D3DSAMP_MIPFILTER};

void ReadSentinel(IDirect3DDevice9* dev, Sentinel& s)
{
    std::memset(&s, 0, sizeof(s));
    for (int i = 0; i < 10; ++i)
        dev->GetRenderState(kSentinelStates[i], &s.renderStates[i]);
    for (DWORD t = 0; t < kFogPassTextureStages; ++t)
    {
        for (int i = 0; i < 4; ++i)
            dev->GetSamplerState(t, kSentinelSamplers[i], &s.samplers[t][i]);
        dev->GetTexture(t, &s.textures[t]);
    }
    dev->GetVertexShader(&s.vs);
    dev->GetPixelShader(&s.ps);
    dev->GetVertexDeclaration(&s.decl);
    dev->GetStreamSource(0, &s.stream, &s.streamOffset, &s.streamStride);
    dev->GetPixelShaderConstantF(0, s.constants, kFogPassPixelConstants);
    dev->GetViewport(&s.viewport);
    dev->GetScissorRect(&s.scissor);
    dev->GetRenderTarget(0, &s.rt);
    dev->GetDepthStencilSurface(&s.ds);
}

void ReleaseSentinel(Sentinel& s)
{
    for (IDirect3DBaseTexture9* texture : s.textures)
        if (texture)
            texture->Release();
    IUnknown* refs[] = {s.vs, s.ps, s.decl, s.stream, s.rt, s.ds};
    for (IUnknown* r : refs)
        if (r)
            r->Release();
}

void ReportSentinelDifferences(const Sentinel& a, const Sentinel& b)
{
    for (int i = 0; i < 10; ++i)
        if (a.renderStates[i] != b.renderStates[i])
            std::printf("     render state %d: %lu -> %lu\n", kSentinelStates[i], a.renderStates[i], b.renderStates[i]);
    for (DWORD t = 0; t < kFogPassTextureStages; ++t)
    {
        for (int i = 0; i < 4; ++i)
            if (a.samplers[t][i] != b.samplers[t][i])
                std::printf("     sampler %d state %d: %lu -> %lu\n", t, kSentinelSamplers[i], a.samplers[t][i],
                            b.samplers[t][i]);
        if (a.textures[t] != b.textures[t])
            std::printf("     texture %d: %p -> %p\n", t, static_cast<void*>(a.textures[t]),
                        static_cast<void*>(b.textures[t]));
    }
    if (a.vs != b.vs || a.ps != b.ps || a.decl != b.decl)
        std::printf("     shaders/decl: vs %p->%p ps %p->%p decl %p->%p\n", static_cast<void*>(a.vs),
                    static_cast<void*>(b.vs), static_cast<void*>(a.ps), static_cast<void*>(b.ps),
                    static_cast<void*>(a.decl), static_cast<void*>(b.decl));
    if (a.stream != b.stream || a.streamOffset != b.streamOffset || a.streamStride != b.streamStride)
        std::printf("     stream 0: %p+%u/%u -> %p+%u/%u\n", static_cast<void*>(a.stream), a.streamOffset,
                    a.streamStride, static_cast<void*>(b.stream), b.streamOffset, b.streamStride);
    for (UINT i = 0; i < kFogPassPixelConstants * 4; ++i)
        if (a.constants[i] != b.constants[i])
        {
            std::printf("     pixel constant c%d differs\n", i / 4);
            break;
        }
    if (std::memcmp(&a.viewport, &b.viewport, sizeof(a.viewport)) != 0)
        std::printf("     viewport %lu,%lu %lux%lu -> %lu,%lu %lux%lu\n", a.viewport.X, a.viewport.Y, a.viewport.Width,
                    a.viewport.Height, b.viewport.X, b.viewport.Y, b.viewport.Width, b.viewport.Height);
    if (std::memcmp(&a.scissor, &b.scissor, sizeof(a.scissor)) != 0)
        std::printf("     scissor differs\n");
    if (a.rt != b.rt || a.ds != b.ds)
        std::printf("     targets: rt %p->%p ds %p->%p\n", static_cast<void*>(a.rt), static_cast<void*>(b.rt),
                    static_cast<void*>(a.ds), static_cast<void*>(b.ds));
}

bool SameSentinel(const Sentinel& a, const Sentinel& b)
{
    return std::memcmp(a.renderStates, b.renderStates, sizeof(a.renderStates)) == 0 &&
           std::memcmp(a.samplers, b.samplers, sizeof(a.samplers)) == 0 &&
           std::memcmp(a.textures, b.textures, sizeof(a.textures)) == 0 && a.vs == b.vs && a.ps == b.ps &&
           a.decl == b.decl && a.stream == b.stream && a.streamOffset == b.streamOffset &&
           a.streamStride == b.streamStride && std::memcmp(a.constants, b.constants, sizeof(a.constants)) == 0 &&
           std::memcmp(&a.viewport, &b.viewport, sizeof(a.viewport)) == 0 &&
           std::memcmp(&a.scissor, &b.scissor, sizeof(a.scissor)) == 0 && a.rt == b.rt && a.ds == b.ds;
}

struct Harness
{
    HWND window = nullptr;
    IDirect3D9* d3d = nullptr;
    IDirect3DDevice9* dev = nullptr;
    D3DPRESENT_PARAMETERS pp = {};
    std::vector<SceneVertex> scene = BuildScene();
    IDirect3DTexture9* dummyTexture = nullptr;
    IDirect3DVertexBuffer9* dummyBuffer = nullptr;
    IDirect3DVertexShader9* engineVs = nullptr;
    IDirect3DPixelShader9* enginePs = nullptr;
    IDirect3DVertexDeclaration9* engineDecl = nullptr;
    D3DCOLOR clearColor = 0xFF6FA0DC;
    DWORD clearFlags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL;

    void CreateEngineObjects()
    {
        dev->CreateTexture(4, 4, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &dummyTexture, nullptr);
        dev->CreateVertexBuffer(256, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &dummyBuffer, nullptr);
        static const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            D3DDECL_END(),
        };
        dev->CreateVertexDeclaration(elements, &engineDecl);
    }

    void ReleaseEngineObjects()
    {
        if (dummyTexture)
            dummyTexture->Release();
        if (dummyBuffer)
            dummyBuffer->Release();
        if (engineDecl)
            engineDecl->Release();
        dummyTexture = nullptr;
        dummyBuffer = nullptr;
        engineDecl = nullptr;
    }

    void BeginFrame()
    {
        dev->BeginScene();
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        for (DWORD t = 0; t < kFogPassTextureStages; ++t)
            dev->SetTexture(t, nullptr);
        dev->Clear(0, nullptr, clearFlags, clearColor, 1.0f, 0);
    }

    void DrawScene(Vec3 eye, const float* view, const float* engineProj, const D3DVIEWPORT9& vp)
    {
        float d3dProj[16];
        RemapToD3DDepthRange(engineProj, d3dProj);
        D3DMATRIX cameraRelativeWorld = {};
        cameraRelativeWorld._11 = cameraRelativeWorld._22 = cameraRelativeWorld._33 = cameraRelativeWorld._44 = 1.0f;
        cameraRelativeWorld._41 = -eye.x;
        cameraRelativeWorld._42 = -eye.y;
        cameraRelativeWorld._43 = -eye.z;
        dev->SetTransform(D3DTS_WORLD, &cameraRelativeWorld);
        dev->SetTransform(D3DTS_VIEW, reinterpret_cast<const D3DMATRIX*>(view));
        dev->SetTransform(D3DTS_PROJECTION, reinterpret_cast<const D3DMATRIX*>(d3dProj));
        dev->SetViewport(&vp);
        dev->SetVertexShader(nullptr);
        dev->SetPixelShader(nullptr);
        dev->SetRenderState(D3DRS_LIGHTING, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
        dev->SetTexture(0, nullptr);
        dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
        dev->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(scene.size() / 3), scene.data(),
                             sizeof(SceneVertex));
    }

    void DrawPretransformedQuadAtRawDepth(float x0, float y0, float x1, float y1, float rawDepth,
                                         DWORD color = 0xFF808890)
    {
        struct ScreenVertex
        {
            float x, y, z, rhw;
            DWORD color;
        };
        const ScreenVertex quad[6] = {
            {x0, y0, rawDepth, 1, color}, {x1, y0, rawDepth, 1, color}, {x0, y1, rawDepth, 1, color},
            {x1, y0, rawDepth, 1, color}, {x1, y1, rawDepth, 1, color}, {x0, y1, rawDepth, 1, color},
        };
        D3DVIEWPORT9 vp = {};
        dev->GetViewport(&vp);
        D3DVIEWPORT9 fullDepthRange = vp;
        fullDepthRange.MinZ = 0.0f;
        fullDepthRange.MaxZ = 1.0f;
        dev->SetViewport(&fullDepthRange);
        dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
        dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 2, quad, sizeof(ScreenVertex));
        dev->SetViewport(&vp);
    }

    void DrawDistantTerrainPatch(float wdlRawDepth)
    {
        DrawPretransformedQuadAtRawDepth(0.0f, 0.0f, 200.0f, 100.0f, wdlRawDepth);
    }

    void SetEngineState(const D3DVIEWPORT9& vp)
    {
        dev->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        dev->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
        dev->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_DESTCOLOR);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
        dev->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
        dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0x7);
        dev->SetRenderState(D3DRS_FOGENABLE, TRUE);
        dev->SetRenderState(D3DRS_STENCILENABLE, TRUE);
        for (DWORD t = 0; t < kFogPassTextureStages; ++t)
        {
            dev->SetTexture(t, dummyTexture);
            dev->SetSamplerState(t, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
            dev->SetSamplerState(t, D3DSAMP_MAGFILTER, D3DTEXF_ANISOTROPIC);
            dev->SetSamplerState(t, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
            dev->SetSamplerState(t, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
        }
        float constants[kFogPassPixelConstants * 4];
        for (UINT i = 0; i < kFogPassPixelConstants * 4; ++i)
            constants[i] = 0.25f * i - 3.0f;
        dev->SetPixelShaderConstantF(0, constants, kFogPassPixelConstants);
        dev->SetVertexDeclaration(engineDecl);
        dev->SetStreamSource(0, dummyBuffer, 16, 16);
        dev->SetViewport(&vp);
        RECT scissor = {3, 5, 700, 400};
        dev->SetScissorRect(&scissor);
    }
};

FrameInputs MakeInputs(const float* view, const float* proj, Vec3 eye, Vec3 at, const D3DVIEWPORT9& vp)
{
    FrameInputs in = {};
    std::memcpy(in.cameraRelativeView, view, sizeof(in.cameraRelativeView));
    std::memcpy(in.glProjection, proj, sizeof(in.glProjection));
    in.camPos[0] = eye.x;
    in.camPos[1] = eye.y;
    in.camPos[2] = eye.z;
    Vec3 f = Norm(Sub(at, eye));
    in.camTarget[0] = eye.x + f.x * 6;
    in.camTarget[1] = eye.y + f.y * 6;
    in.camTarget[2] = eye.z - 6;
    in.cameraFocus[0] = eye.x;
    in.cameraFocus[1] = eye.y;
    in.cameraFocus[2] = eye.z;
    in.viewport = vp;
    in.dayFraction = 0.75f;
    Vec3 sun = Norm({1.0f, 0.12f, 0.16f});
    in.toLight[0] = sun.x;
    in.toLight[1] = sun.y;
    in.toLight[2] = sun.z;
    in.lightIsMoon = false;
    in.fogColor = 0xFF9DB2C8;
    in.sunColor = 0xFFFFD9A0;
    in.directColor = 0xFFE8D8C0;
    in.ambientColor = 0xFF404858;
    in.fogStart = 150.0f;
    in.fogEnd = 600.0f;
    in.zoneFogDistance = 600.0f;
    in.farClip = kFar;
    in.inLiquid = false;
    in.mapId = -1;
    return in;
}

float SmoothStep(float e0, float e1, float x)
{
    float t = std::fmin(std::fmax((x - e0) / (e1 - e0), 0.0f), 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float ReferenceUnshadowedSkyTransmittance(const FrameInputs& in, const Config& cfg, const AuthoredFog* authored,
                                          float px, float py)
{
    FogParams fog = BuildFogParams(in, cfg, authored);
    const float* P = in.glProjection;
    const D3DVIEWPORT9& vp = in.viewport;
    float ndcX = (px - vp.X) / vp.Width * 2.0f - 1.0f;
    float ndcY = 1.0f - (py - vp.Y) / vp.Height * 2.0f;
    Vec3 ray = {(ndcX - P[8]) / P[0], (ndcY - P[9]) / P[5], 1.0f};
    float len = std::sqrt(Dot(ray, ray));
    Vec3 v = {ray.x / len, ray.y / len, ray.z / len};
    const float* m = in.cameraRelativeView;
    Vec3 dirW = {v.x * m[0] + v.y * m[1] + v.z * m[2], v.x * m[4] + v.y * m[5] + v.z * m[6],
                 v.x * m[8] + v.y * m[9] + v.z * m[10]};
    const double upward = std::fabs(dirW.z);
    double tau = 0.0;
    for (const FogLayer& layer : fog.layers)
    {
        const double begin = std::fmax(layer.start, 0.0f);
        const double end = std::fmin(layer.endDistance, fog.maxDistance);
        if (layer.density == 0.0f || end <= begin)
            continue;
        const double dt = (end - begin) / kReferenceIntegrationSteps;
        const double skyScale = std::exp(-upward * layer.skyFalloff);
        for (int sample = 0; sample < kReferenceIntegrationSteps; ++sample)
        {
            const double distance = begin + (sample + 0.5) * dt;
            const double height = in.camPos[2] + upward * distance;
            const double ramp = std::fmin((distance - layer.start) / fog.maxDistance, 1.0);
            const double curve = 1.0 + layer.strength * std::pow(ramp, layer.exponent);
            const double heightFactor = std::exp(std::fmin((layer.upperHeight - height) * layer.upperFalloff, 0.0) +
                                                 std::fmin((height - layer.lowerHeight) * layer.lowerFalloff, 0.0));
            tau += layer.density * skyScale * dt * curve * heightFactor;
        }
    }
    return static_cast<float>(std::exp(-tau));
}

const AuthoredLayer* FarWall(const AuthoredFog& fog)
{
    for (int i = 0; i < fog.layerCount; ++i)
        if (fog.layers[i].start >= 1000.0f)
            return &fog.layers[i];
    return nullptr;
}

const AuthoredLayer* MidHaze(const AuthoredFog& fog)
{
    for (int i = 0; i < fog.layerCount; ++i)
        if (fog.layers[i].start < 1.0f && std::fabs(fog.layers[i].g - 0.5f) < 0.01f)
            return &fog.layers[i];
    return nullptr;
}

float WeightOf(const AuthoredFog& fog, uint32_t light)
{
    for (int i = 0; i < fog.lightCount; ++i)
        if (fog.lightIds[i] == light)
            return fog.lightWeights[i];
    return 0.0f;
}

constexpr int kEasternKingdoms = 0;
constexpr int kKalimdor = 1;
constexpr int kMapWithoutClassicData = 9999;
constexpr int kOutland = 530;
constexpr int kNorthrend = 571;
const LightParamsSelection kClearWeather = {};
constexpr uint32_t kEasternKingdomsGlobalLight = 1;
constexpr uint32_t kStormwindLight = 77;
constexpr float kDayFraction1800 = 0.75f;
constexpr float kDayFraction1845 = 2250.0f / 2880.0f;

AuthoredFog BlendOfLights(std::initializer_list<std::pair<uint32_t, float>> lights)
{
    AuthoredFog fog = {};
    for (const auto& light : lights)
    {
        fog.lightIds[fog.lightCount] = light.first;
        fog.lightWeights[fog.lightCount] = light.second;
        ++fog.lightCount;
    }
    return fog;
}

void CheckNegligibleLightsLeaveTheLightLogAlone()
{
    const AuthoredFog blend = BlendOfLights({{4, 0.84f}, {8, 0.16f}});
    const AuthoredFog withNegligible = BlendOfLights({{4, 0.84f}, {8, 0.16f}, {1, 0.001f}});
    const AuthoredFog withVisible = BlendOfLights({{4, 0.80f}, {8, 0.15f}, {1, 0.05f}});
    const uint32_t plain = vf_test_logged_lights_signature(&blend);
    Check(plain == vf_test_logged_lights_signature(&withNegligible) &&
              plain != vf_test_logged_lights_signature(&withVisible),
          "a light whose blend weight rounds to 0.00 neither logs a light change as it enters or leaves the blend, nor "
          "hides one that carries weight");
}

void CheckClassicData(const FogData& data)
{
    const float harbourAtStormwindLightEdge[3] = {-8565.21f, 993.46f, 104.96f};
    AuthoredFog fog = {};
    bool ok = data.Resolve(kEasternKingdoms, harbourAtStormwindLightEdge, kDayFraction1800, kClearWeather, fog);
    float globalWeight = WeightOf(fog, kEasternKingdomsGlobalLight);
    float stormwindWeight = WeightOf(fog, kStormwindLight);
    const AuthoredLayer* wall = FarWall(fog);
    const AuthoredLayer* haze = MidHaze(fog);
    std::printf("     harbour at 18:00: lights 1:%.3f 77:%.3f, %d layers, far wall density %.4f, haze density %.4f\n",
                globalWeight, stormwindWeight, fog.layerCount, wall ? wall->density : -1.0f,
                haze ? haze->density : -1.0f);
    Check(ok && fog.layerCount == 3 && std::fabs(globalWeight + stormwindWeight - 1.0f) < 1e-4f &&
              stormwindWeight > 0.05f && stormwindWeight < 0.1f,
          "Classic light blend at the harbour (light 77 falloff edge)");
    Check(wall && haze && std::fabs(wall->start - 3000.0f) < 0.5f &&
              std::fabs(wall->density - (globalWeight * 0.6f + stormwindWeight * 1.0f)) < 1e-4f &&
              std::fabs(haze->density - (globalWeight * 0.12f + stormwindWeight * 0.125f)) < 1e-4f,
          "Classic layers blended by light weight at a key time");

    AuthoredFog mid = {};
    data.Resolve(kEasternKingdoms, harbourAtStormwindLightEdge, kDayFraction1845, kClearWeather, mid);
    float expected = WeightOf(mid, kEasternKingdomsGlobalLight) * 0.8f + WeightOf(mid, kStormwindLight) * 0.75f;
    const AuthoredLayer* midWall = FarWall(mid);
    std::printf("     harbour at 18:45: far wall density %.4f (expected %.4f)\n", midWall ? midWall->density : -1.0f,
                expected);
    Check(midWall && std::fabs(midWall->density - expected) < 1e-4f,
          "Classic keys interpolated between 18:00 and 19:30");

    AuthoredFog none = {};
    Check(!data.Resolve(kMapWithoutClassicData, harbourAtStormwindLightEdge, kDayFraction1800, kClearWeather, none),
          "maps without Classic lights fall back to derived layers");
}

constexpr float kNoon = 0.5f;
constexpr uint32_t kElwynnZoneLight = 16849;
constexpr uint32_t kOrgrimmarDragZoneLight = 16845;
constexpr uint32_t kOrgrimmarZoneLight = 239;
constexpr int kUnusedLightParamsSlot = 6;
const float kOpenSeaWestOfElwynn[3] = {-10500.0f, 2500.0f, 50.0f};

bool Near(float a, float b)
{
    return std::fabs(a - b) < 1e-4f;
}

LightParamsSelection Storm(float blend)
{
    LightParamsSelection selection;
    selection.stormBlend = blend;
    return selection;
}

LightParamsSelection ScreenEffectSlot(int slot, float stormBlend)
{
    LightParamsSelection selection = Storm(stormBlend);
    selection.screenEffectSlot = slot;
    return selection;
}

void CheckStormBlendsLayersByClassicIndex(const FogData& data)
{
    AuthoredFog clear = {};
    AuthoredFog storm = {};
    AuthoredFog half = {};
    bool resolved = data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, kClearWeather, clear) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, Storm(1.0f), storm) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, Storm(0.5f), half);
    std::printf("     open sea at noon, densities by Classic layer index: clear %.3f %.3f %.3f, storm %.3f %.3f %.3f, "
                "half storm %.3f %.3f %.3f (far wall start %.0f)\n",
                clear.layers[0].density, clear.layers[1].density, clear.layers[2].density, storm.layers[0].density,
                storm.layers[1].density, storm.layers[2].density, half.layers[0].density, half.layers[1].density,
                half.layers[2].density, half.layers[1].start);
    Check(resolved && clear.lightCount == 1 && clear.lightIds[0] == kEasternKingdomsGlobalLight &&
              Near(clear.layers[1].start, 3000.0f) && Near(clear.layers[1].density, 0.3f) &&
              Near(clear.layers[2].density, 0.1f),
          "open sea resolves only the Eastern Kingdoms light");
    Check(storm.layerCount == 3 && storm.layers[0].density == 0.0f && Near(storm.layers[1].start, 500.0f) &&
              Near(storm.layers[1].density, 0.75f) && Near(storm.layers[2].density, 0.3f),
          "a full storm uses the storm layers at their Classic indices");
    Check(Near(half.layers[0].density, 0.015f) && Near(half.layers[0].g, clear.layers[0].g) &&
              Near(half.layers[1].start, 1750.0f) && Near(half.layers[1].density, 0.525f) &&
              Near(half.layers[2].density, 0.2f),
          "a half storm blends matching layers and thins the clear-only haze");
}

void CheckScreenEffectLightSlot(const FogData& data)
{
    AuthoredFog ghost = {};
    AuthoredFog unused = {};
    AuthoredFog clear = {};
    const LightParamsSelection ghostDuringStorm = ScreenEffectSlot(FogData::kDeathSlot, 1.0f);
    const LightParamsSelection unusedSlot = ScreenEffectSlot(kUnusedLightParamsSlot, 0.0f);
    bool resolved = data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, ghostDuringStorm, ghost) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, unusedSlot, unused) &&
                    data.Resolve(kEasternKingdoms, kOpenSeaWestOfElwynn, kNoon, kClearWeather, clear);
    std::printf("     ghost at noon: densities %.3f %.3f %.3f, start %.0f\n", ghost.layers[0].density,
                ghost.layers[1].density, ghost.layers[2].density, ghost.layers[2].start);
    Check(resolved && ghost.layerCount == 3 && ghost.layers[0].density == 0.0f && ghost.layers[1].density == 0.0f &&
              Near(ghost.layers[2].start, 75.0f) && Near(ghost.layers[2].density, 1.0f),
          "the ghost screen effect selects the death slot over the storm");
    Check(Near(unused.layers[1].density, clear.layers[1].density) &&
              Near(unused.layers[2].density, clear.layers[2].density),
          "a screen effect slot the light leaves empty keeps its weather layers");
}

void CheckZoneLights(const FogData& data)
{
    const float elwynnInterior[3] = {-9847.0f, 49.0f, 50.0f};
    const float elwynnFiftyYardsInsideWestEdge[3] = {-9847.0f, 664.0f, 50.0f};
    const float orgrimmarDragInsideOrgrimmar[3] = {1847.0f, -4499.0f, 20.0f};
    AuthoredFog inside = {};
    AuthoredFog edge = {};
    AuthoredFog drag = {};
    bool resolved = data.Resolve(kEasternKingdoms, elwynnInterior, kNoon, kClearWeather, inside) &&
                    data.Resolve(kEasternKingdoms, elwynnFiftyYardsInsideWestEdge, kNoon, kClearWeather, edge) &&
                    data.Resolve(kKalimdor, orgrimmarDragInsideOrgrimmar, kNoon, kClearWeather, drag);
    const float edgeZoneWeight = WeightOf(edge, kElwynnZoneLight);
    std::printf("     Elwynn zone light: interior %.2f (far wall %.2f), 50 yd inside the edge %.2f; Drag %.2f, "
                "Orgrimmar %.2f\n",
                WeightOf(inside, kElwynnZoneLight), inside.layers[1].density, edgeZoneWeight,
                WeightOf(drag, kOrgrimmarDragZoneLight), WeightOf(drag, kOrgrimmarZoneLight));
    Check(resolved && Near(WeightOf(inside, kElwynnZoneLight), 1.0f) &&
              WeightOf(inside, kEasternKingdomsGlobalLight) == 0.0f && Near(inside.layers[1].density, 0.2f),
          "inside its outline the Elwynn zone light replaces the Eastern Kingdoms light");
    Check(edgeZoneWeight > 0.4f && edgeZoneWeight < 0.6f &&
              Near(edgeZoneWeight + WeightOf(edge, kEasternKingdomsGlobalLight), 1.0f),
          "a zone light fades in over 100 yd inside its outline");
    Check(Near(WeightOf(drag, kOrgrimmarDragZoneLight), 1.0f) && WeightOf(drag, kOrgrimmarZoneLight) == 0.0f,
          "the innermost of nested zone outlines wins");
}

float SunlitLevelRayOpacity(const FogParams& fog, float camZ, float distance)
{
    const float dt = distance / kReferenceIntegrationSteps;
    double tau = 0.0;
    for (int i = 0; i < kReferenceIntegrationSteps; ++i)
    {
        const float t = (i + 0.5f) * dt;
        for (const FogLayer& l : fog.layers)
        {
            if (t < l.start || t > l.endDistance)
                continue;
            float curve =
                1.0f + l.strength * std::pow(std::fmin(std::fmax(t - l.start, 0.0f) / fog.maxDistance, 1.0f) + 1e-6f,
                                             l.exponent);
            float heightF = std::fmin(std::exp((l.upperHeight - camZ) * l.upperFalloff), 1.0f) *
                            std::fmin(std::exp((camZ - l.lowerHeight) * l.lowerFalloff), 1.0f);
            float shadow = 1.0f + (l.shadowDensity - 1.0f) * l.shadowed * (1.0f - fog.lightAboveHorizon);
            tau += l.density * curve * heightF * shadow * dt;
        }
    }
    return static_cast<float>(1.0 - std::exp(-tau));
}

constexpr float kContinentFogStart = 197.9f;
constexpr float kContinentFarClip = 791.6f;
constexpr Vec3 kHarbourEye = {-8576.0f, 1007.0f, 104.0f};
constexpr float kHarbourDayFraction = 0.7802f;
constexpr Vec3 kHarbourToLight = {0.673f, 0.673f, 0.307f};
constexpr float kHarbourNear = 0.2f;
constexpr float kHarbourFar = 791.6f;
constexpr float kHarbourLoggedP11 = 1.511f;
constexpr D3DCOLOR kHarbourSky = 0xFFFFD890;
constexpr float kDegree = kPi / 180.0f;

Vec3 Dir(float azimuthDeg, float elevationDeg)
{
    float a = azimuthDeg * kDegree;
    float e = elevationDeg * kDegree;
    return {std::cos(e) * std::cos(a), std::cos(e) * std::sin(a), std::sin(e)};
}

FrameInputs HarbourSunsetInputs(const float* view, const float* proj, Vec3 at, const D3DVIEWPORT9& vp,
                                float stormBlend)
{
    const Vec3 toLight = Norm(kHarbourToLight);
    FrameInputs in = MakeInputs(view, proj, kHarbourEye, at, vp);
    Vec3 clientTargetOneYardAhead = Add(kHarbourEye, Norm(Sub(at, kHarbourEye)));
    in.camTarget[0] = clientTargetOneYardAhead.x;
    in.camTarget[1] = clientTargetOneYardAhead.y;
    in.camTarget[2] = clientTargetOneYardAhead.z;
    in.dayFraction = kHarbourDayFraction;
    in.toLight[0] = toLight.x;
    in.toLight[1] = toLight.y;
    in.toLight[2] = toLight.z;
    in.lightIsMoon = false;
    in.fogColor = 0xFF574C5C;
    in.sunColor = 0xFFFFE7B6;
    in.directColor = 0xFFFF7400;
    in.ambientColor = 0xFF676680;
    in.fogStart = kContinentFogStart;
    in.fogEnd = 791.7f;
    in.zoneFogDistance = 791.7f;
    in.farClip = kHarbourFar;
    in.inLiquid = false;
    in.mapId = kEasternKingdoms;
    in.lightParams = Storm(stormBlend);
    return in;
}

FrameInputs ContinentFrame(int map, Vec3 eye, float dayFraction, Vec3 toLight, bool lightIsMoon)
{
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    const D3DVIEWPORT9 vp = {0, 0, 1280, 720, 0.0f, kClientWorldMaxZ};
    FrameInputs in = MakeInputs(identity, identity, eye, Add(eye, {100, 0, 0}), vp);
    in.mapId = map;
    in.dayFraction = dayFraction;
    Vec3 l = Norm(toLight);
    in.toLight[0] = l.x;
    in.toLight[1] = l.y;
    in.toLight[2] = l.z;
    in.lightIsMoon = lightIsMoon;
    in.fogStart = kContinentFogStart;
    in.fogEnd = kContinentFarClip;
    in.zoneFogDistance = kContinentFarClip;
    in.farClip = kContinentFarClip;
    return in;
}

void CheckAuthoredFogFollowsClientDirectLight(const FogData& data)
{
    const Config cfg = {};
    const Vec3 goldshire = {-9456.8f, 54.7f, 59.6f};
    const float eightPm = 0.8337f;
    const Vec3 lowSun = {0.704f, 0.704f, 0.086f};
    constexpr uint32_t kClientStormDirectLight = 0xFF656565;
    constexpr uint32_t kWhiteDirectLight = 0xFFFFFFFF;
    FrameInputs storm = ContinentFrame(kEasternKingdoms, goldshire, eightPm, lowSun, false);
    storm.directColor = kClientStormDirectLight;
    storm.lightParams = Storm(1.0f);
    FrameInputs bright = storm;
    bright.directColor = kWhiteDirectLight;
    AuthoredFog fog = {};
    bool resolved = data.Resolve(kEasternKingdoms, storm.camPos, eightPm, storm.lightParams, fog);
    FogParams dim = BuildFogParams(storm, cfg, &fog);
    FogParams full = BuildFogParams(bright, cfg, &fog);
    const float diffuseRatio = dim.layers[2].diffuse[0] / std::fmax(full.layers[2].diffuse[0], 1e-6f);
    std::printf("     Goldshire storm 20:00: client direct light vs Classic %.2f (white light %.2f), "
                "sun scatter %.2f -> %.2f\n",
                dim.directLightMatch, full.directLightMatch, full.layers[2].diffuse[0], dim.layers[2].diffuse[0]);
    Check(resolved && dim.directLightMatch > 0.35f && dim.directLightMatch < 0.47f &&
              Near(diffuseRatio, dim.directLightMatch),
          "storm fog scatters the client's dimmer storm sunlight, not Classic's");
    Check(full.directLightMatch == 1.0f, "a client direct light brighter than Classic's never brightens the fog");

    const Vec3 darkshire = {-10559.2f, -1196.6f, 28.3f};
    const float sixPm = 0.7511f;
    const Vec3 eveningSun = {0.641f, 0.641f, 0.423f};
    constexpr uint32_t kClientDuskwoodDirectLight = 0xFF344A5C;
    FrameInputs clearDuskwood = ContinentFrame(kEasternKingdoms, darkshire, sixPm, eveningSun, false);
    clearDuskwood.directColor = kClientDuskwoodDirectLight;
    AuthoredFog duskwoodFog = {};
    bool duskwoodResolved =
        data.Resolve(kEasternKingdoms, clearDuskwood.camPos, sixPm, clearDuskwood.lightParams, duskwoodFog);
    FogParams duskwood = BuildFogParams(clearDuskwood, cfg, &duskwoodFog);
    std::printf("     Darkshire clear 18:00: direct light match %.5f, Classic RGB %.4f %.4f %.4f\n",
                duskwood.directLightMatch, duskwoodFog.classicDirectLight[0], duskwoodFog.classicDirectLight[1],
                duskwoodFog.classicDirectLight[2]);
    Check(duskwoodResolved && std::fabs(duskwood.directLightMatch - 0.342745f) < 0.00001f,
          "clear Darkshire fog follows the client's dimmer direct light in linear colour space");

    AuthoredFog missingDirectLight = duskwoodFog;
    missingDirectLight.hasClassicDirectLight = false;
    const FogParams uncalibrated = BuildFogParams(clearDuskwood, cfg, &missingDirectLight);
    bool diffuseMatched = true;
    bool mediumPreserved = true;
    for (int i = 0; i < kSceneLayers; ++i)
    {
        const FogLayer& actual = duskwood.layers[i];
        const FogLayer& original = uncalibrated.layers[i];
        mediumPreserved &= actual.density == original.density && actual.g == original.g &&
                           actual.start == original.start && actual.endDistance == original.endDistance &&
                           actual.strength == original.strength && actual.exponent == original.exponent &&
                           actual.upperHeight == original.upperHeight && actual.upperFalloff == original.upperFalloff &&
                           actual.lowerHeight == original.lowerHeight && actual.lowerFalloff == original.lowerFalloff &&
                           actual.shadowDensity == original.shadowDensity && actual.shadowed == original.shadowed;
        for (int channel = 0; channel < 3; ++channel)
        {
            diffuseMatched &= Near(actual.diffuse[channel], original.diffuse[channel] * duskwood.directLightMatch);
            mediumPreserved &= actual.emissive[channel] == original.emissive[channel] &&
                               actual.shadowEmissive[channel] == original.shadowEmissive[channel];
        }
    }
    Check(diffuseMatched, "direct-light calibration scales every authored diffuse channel without shifting its hue");
    Check(mediumPreserved &&
              Near(SunlitLevelRayOpacity(duskwood, clearDuskwood.camPos[2], 1000.0f),
                   SunlitLevelRayOpacity(uncalibrated, clearDuskwood.camPos[2], 1000.0f)),
          "clear-light calibration preserves fog density, height, phase, opacity and ambient emission");
    Check(std::memcmp(duskwood.rayColor, uncalibrated.rayColor, sizeof(duskwood.rayColor)) == 0 &&
              std::memcmp(&duskwood.layers[kDistanceFogLayer], &uncalibrated.layers[kDistanceFogLayer],
                          sizeof(FogLayer)) == 0,
          "authored direct-light calibration preserves the halo hue and already client-lit distance fog");
    Check(uncalibrated.directLightMatch == 1.0f,
          "missing Classic direct-light data preserves the authored diffuse contribution");

    Config gamma = cfg;
    gamma.colorSpace = 0;
    const FogParams gammaDuskwood = BuildFogParams(clearDuskwood, gamma, &duskwoodFog);
    Check(std::fabs(gammaDuskwood.directLightMatch - 0.647823f) < 0.00001f,
          "clear direct-light matching uses the selected gamma colour space consistently");

    FrameInputs clearWhite = clearDuskwood;
    clearWhite.directColor = kWhiteDirectLight;
    Check(BuildFogParams(clearWhite, cfg, &duskwoodFog).directLightMatch == 1.0f,
          "clear client lighting brighter than Classic never boosts authored scattering");
    FrameInputs dark = clearDuskwood;
    dark.directColor = 0xFF000000;
    const FogParams noDirectLight = BuildFogParams(dark, cfg, &duskwoodFog);
    Check(noDirectLight.directLightMatch == 0.0f && noDirectLight.layers[0].diffuse[2] == 0.0f &&
              noDirectLight.layers[0].emissive[2] == duskwood.layers[0].emissive[2] &&
              noDirectLight.layers[0].density == duskwood.layers[0].density,
          "black client direct light removes direct scattering while retaining the medium and its ambient light");

    AuthoredFog nearBlackReference = duskwoodFog;
    for (float& channel : nearBlackReference.classicDirectLight)
        channel = 0.0001f;
    Check(BuildFogParams(clearDuskwood, cfg, &nearBlackReference).directLightMatch == 1.0f,
          "an unusably dark Classic reference keeps the existing authored-light fallback");

    FrameInputs mixedWeather = clearDuskwood;
    mixedWeather.lightParams = Storm(0.5f);
    const FogParams mixed = BuildFogParams(mixedWeather, cfg, &duskwoodFog);
    FrameInputs selectedEffect = clearDuskwood;
    selectedEffect.lightParams = ScreenEffectSlot(FogData::kDeathSlot, 1.0f);
    AuthoredFog selectedFog = {};
    const bool selectedResolved = data.Resolve(kEasternKingdoms, selectedEffect.camPos, sixPm,
                                              selectedEffect.lightParams, selectedFog);
    const FogParams selected = BuildFogParams(selectedEffect, cfg, &selectedFog);
    FrameInputs selectedStorm = selectedEffect;
    selectedStorm.lightParams = Storm(1.0f);
    const FogParams selectedReference = BuildFogParams(selectedStorm, cfg, &selectedFog);
    Check(Near(mixed.directLightMatch, duskwood.directLightMatch) && selectedResolved &&
              Near(selected.directLightMatch, selectedReference.directLightMatch),
          "resolved weather and screen-effect lighting receive one consistent direct-light calibration");
}

void CheckDenseClassicFogAtHarbourSunset(const FogData& data)
{
    const Config cfg = {};
    FrameInputs harbour = ContinentFrame(kEasternKingdoms, kHarbourEye, kHarbourDayFraction, kHarbourToLight, false);
    AuthoredFog fog = {};
    bool resolved = data.Resolve(kEasternKingdoms, harbour.camPos, harbour.dayFraction, kClearWeather, fog);
    FogParams p = BuildFogParams(harbour, cfg, resolved ? &fog : nullptr);
    float horizon = SunlitLevelRayOpacity(p, harbour.camPos[2], p.maxDistance);
    std::printf("     harbour 18:43: coverage %.2f, distance fog density %.6f, level-ray opacity %.3f\n", fog.coverage,
                p.layers[kDistanceFogLayer].density, horizon);
    Check(resolved && p.layers[kDistanceFogLayer].density == 0.0f && horizon > 0.97f,
          "dense Classic layers hide the far clip without the distance fog");
}

void CheckThinClassicFogAtHyjalMidnight(const FogData& data)
{
    const Config cfg = {};
    const Vec3 hyjal = {5458.0f, -2934.0f, 1481.0f};
    const float midnight = 0.0f;
    const Vec3 toMoon = {0.63f, 0.63f, 0.455f};
    FrameInputs frame = ContinentFrame(kKalimdor, hyjal, midnight, toMoon, true);
    AuthoredFog fog = {};
    bool resolved = data.Resolve(kKalimdor, frame.camPos, frame.dayFraction, kClearWeather, fog);
    FogParams p = BuildFogParams(frame, cfg, resolved ? &fog : nullptr);
    float horizon = SunlitLevelRayOpacity(p, frame.camPos[2], p.maxDistance);
    std::printf("     Hyjal 00:00: coverage %.2f, distance fog density %.6f, level-ray opacity %.3f\n", fog.coverage,
                p.layers[kDistanceFogLayer].density, horizon);
    Check(resolved && p.layers[kDistanceFogLayer].density > 0.0f && horizon > 0.94f,
          "thin Classic layers keep enough distance fog to hide the far clip");
}

constexpr uint32_t kFoglessBlastedLandsLight = 19;

float SceneLayersDensity(const FogParams& p)
{
    float density = 0.0f;
    for (int i = 0; i < kSceneLayers; ++i)
        density += p.layers[i].density;
    return density;
}

void CheckFogThinsIntoFoglessClassicLight(const FogData& data)
{
    const Config cfg = {};
    const Vec3 foglessBlastedLandsLight19 = {-12041.6f, -2450.3f, 0.0f};
    const float afterNoon = 0.5212f;
    const Vec3 toSun = {0.106f, 0.106f, 0.989f};
    const float offsets[] = {0.0f, -455.0f, -465.0f};
    FrameInputs frames[3];
    AuthoredFog fogs[3] = {};
    FogParams params[3];
    bool resolved = true;
    for (int i = 0; i < 3; ++i)
    {
        frames[i] = ContinentFrame(kEasternKingdoms, Add(foglessBlastedLandsLight19, {0, offsets[i], 20.0f}),
                                   afterNoon, toSun, false);
        resolved = data.Resolve(kEasternKingdoms, frames[i].camPos, afterNoon, kClearWeather, fogs[i]) && resolved;
        params[i] = BuildFogParams(frames[i], cfg, &fogs[i]);
    }
    const float innerOpacity = SunlitLevelRayOpacity(params[1], frames[1].camPos[2], 450.0f);
    const float outerOpacity = SunlitLevelRayOpacity(params[2], frames[2].camPos[2], 450.0f);
    std::printf("     light 19 at noon: centre scene density %.6f, distance fog %.6f; light 19 weight %.2f / %.2f, "
                "opacity at 450 yd %.3f / %.3f\n",
                SceneLayersDensity(params[0]), params[0].layers[kDistanceFogLayer].density,
                WeightOf(fogs[1], kFoglessBlastedLandsLight), WeightOf(fogs[2], kFoglessBlastedLandsLight),
                innerOpacity, outerOpacity);
    Check(resolved && std::fabs(innerOpacity - outerOpacity) < 0.08f,
          "fog thins continuously into a Classic light without fog");
    Check(Near(WeightOf(fogs[0], kFoglessBlastedLandsLight), 1.0f) && SceneLayersDensity(params[0]) == 0.0f &&
              params[0].layers[kDistanceFogLayer].density > 0.0f,
          "inside a Classic light without fog only the distance fog hides the far clip");
}

constexpr uint32_t kFirstClientLight = 1000000;
constexpr uint32_t kNorthrendGlobalClientLight = kFirstClientLight + 1406;

bool OnlyClientLights(const AuthoredFog& fog)
{
    for (int i = 0; i < fog.lightCount; ++i)
        if (fog.lightIds[i] < kFirstClientLight)
            return false;
    return fog.lightCount > 0;
}

void CheckOutlandAndNorthrendHaveTheirOwnLights(const FogData& data)
{
    const float hellfirePeninsula[3] = {-3005.0f, 4469.0f, -18.0f};
    const float howlingFjord[3] = {1350.0f, -5000.0f, 210.0f};
    AuthoredFog outland = {};
    AuthoredFog northrend = {};
    const bool outlandFog = data.Resolve(kOutland, hellfirePeninsula, kDayFraction1800, kClearWeather, outland);
    const bool northrendFog = data.Resolve(kNorthrend, howlingFjord, kDayFraction1800, kClearWeather, northrend);
    std::printf("     Hellfire Peninsula: %d client lights (first %u), coverage %.2f, %d layers; Howling Fjord: light "
                "%u weight %.2f, %d layers\n",
                outland.lightCount, outland.lightCount ? outland.lightIds[0] : 0u, outland.coverage,
                outland.layerCount, northrend.lightCount ? northrend.lightIds[0] : 0u,
                WeightOf(northrend, kNorthrendGlobalClientLight), northrend.layerCount);
    Check(outlandFog && OnlyClientLights(outland) && outland.lightCount > 1 &&
              std::fabs(outland.coverage - 1.0f) < 1e-4f && outland.layerCount > 0,
          "Hellfire Peninsula blends its own client lights, converted from the 3.3.5 Light table");
    Check(northrendFog && OnlyClientLights(northrend) &&
              std::fabs(WeightOf(northrend, kNorthrendGlobalClientLight) - 1.0f) < 1e-4f && northrend.layerCount > 0,
          "Northrend's global client light covers Howling Fjord away from its sphere lights");
}

void CheckRegionalFogSettings()
{
    Check(Config().azerothDensity == 1.0f && Config().outlandDensity == 0.15f && Config().northrendDensity == 0.15f,
          "Outland and Northrend start at density 0.15, as testers found their converted fog too strong");
    Config cfg = {};
    cfg.density = 0.8f;
    cfg.azerothDensity = 1.5f;
    cfg.outlandDensity = 0.5f;
    cfg.northrendDensity = 0.25f;
    const Config azeroth = ConfigForMap(cfg, kEasternKingdoms);
    const Config dungeon = ConfigForMap(cfg, kMapWithoutClassicData);
    const Config outland = ConfigForMap(cfg, kOutland);
    const Config northrend = ConfigForMap(cfg, kNorthrend);
    std::printf("     density 0.8 by region: Azeroth %.2f, other map %.2f, Outland %.2f, Northrend %.2f\n",
                azeroth.density, dungeon.density, outland.density, northrend.density);
    Check(std::fabs(azeroth.density - 1.2f) < 1e-5f && std::fabs(dungeon.density - 1.2f) < 1e-5f &&
              std::fabs(outland.density - 0.4f) < 1e-5f && std::fabs(northrend.density - 0.2f) < 1e-5f,
          "each region scales Density by its own density: Azeroth on every map but Outland and Northrend");
    Check(outland.dataMode == 1 && northrend.dataMode == 1 && UsesFogData(cfg, kOutland) &&
              UsesFogData(cfg, kNorthrend),
          "Outland and Northrend borrow the Classic fog by default");

    cfg.outlandClassicFog = 0;
    Check(ConfigForMap(cfg, kOutland).dataMode == 0 && !UsesFogData(cfg, kOutland) &&
              ConfigForMap(cfg, kNorthrend).dataMode == 1 && ConfigForMap(cfg, kEasternKingdoms).dataMode == 1,
          "OutlandClassicFog=0 gives Outland the derived layers and leaves Northrend and Azeroth alone");
    cfg.outlandClassicFog = 1;
    cfg.northrendClassicFog = 0;
    Check(ConfigForMap(cfg, kNorthrend).dataMode == 0 && !UsesFogData(cfg, kNorthrend) &&
              ConfigForMap(cfg, kOutland).dataMode == 1 && UsesFogData(cfg, kEasternKingdoms),
          "NorthrendClassicFog=0 gives Northrend the derived layers and leaves Outland and Azeroth alone");
    cfg.dataMode = 0;
    Check(ConfigForMap(cfg, kOutland).dataMode == 0, "DataMode=0 keeps the derived layers in every region");
}

void CheckOutlandDensityThinsTheConvertedFog(const FogData& data)
{
    const Vec3 hellfirePeninsula = {-3005.0f, 4469.0f, -18.0f};
    const Vec3 toSun = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    const FrameInputs frame = ContinentFrame(kOutland, hellfirePeninsula, evening, toSun, false);
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kOutland, frame.camPos, evening, kClearWeather, fog);
    Config full = {};
    full.outlandDensity = 1.0f;
    Config halved = full;
    halved.outlandDensity = 0.5f;
    const FogParams fullParams = BuildFogParams(frame, ConfigForMap(full, kOutland), &fog);
    const FogParams halvedParams = BuildFogParams(frame, ConfigForMap(halved, kOutland), &fog);
    const float fullDensity = SceneLayersDensity(fullParams);
    const float halvedDensity = SceneLayersDensity(halvedParams);
    std::printf("     Hellfire Peninsula at dusk: scene layer density %.6f at OutlandDensity 1, %.6f at 0.5\n",
                fullDensity, halvedDensity);
    Check(resolved && fullDensity > 0.0f && std::fabs(halvedDensity - 0.5f * fullDensity) < 1e-6f,
          "OutlandDensity 0.5 halves the converted layers in Hellfire Peninsula");
}

void CheckLayerSlidersScaleTheClassicLayers(const FogData& data)
{
    const Vec3 hellfirePeninsula = {-3005.0f, 4469.0f, -18.0f};
    const Vec3 toSun = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    const FrameInputs frame = ContinentFrame(kOutland, hellfirePeninsula, evening, toSun, false);
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kOutland, frame.camPos, evening, kClearWeather, fog);
    const Config defaults;
    Config moved = defaults;
    moved.haze = 2.0f * defaults.haze;
    moved.farFog = 0.0f;
    moved.groundFog = 0.5f * defaults.groundFog;
    const FogParams authored = BuildFogParams(frame, defaults, &fog);
    const FogParams changed = BuildFogParams(frame, moved, &fog);
    std::printf("     Hellfire Peninsula layers haze/far wall/ground: %.6f %.6f %.6f by default, %.6f %.6f %.6f with "
                "Haze x2, Distance fog 0, Ground fog x0.5\n",
                authored.layers[0].density, authored.layers[1].density, authored.layers[2].density,
                changed.layers[0].density, changed.layers[1].density, changed.layers[2].density);
    Check(resolved && fog.layerCount == kSceneLayers &&
              Near(authored.layers[0].density, fog.layers[0].density * 0.01f * defaults.density) &&
              Near(changed.layers[0].density, 2.0f * authored.layers[0].density) &&
              changed.layers[1].density == 0.0f && authored.layers[1].density > 0.0f &&
              Near(changed.layers[2].density, 0.5f * authored.layers[2].density),
          "Haze, Distance fog and Ground fog scale the Classic haze, far wall and ground layers relative to their "
          "defaults, which keep the layers as authored");
}

void CheckConvertedGroundFogFollowsTheHeightOfItsLight(const FogData& data)
{
    const float boneWastes[3] = {-3005.0f, 4469.0f, -18.0f};
    const float groundFogBelowTheGround = 150.0f;
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kOutland, boneWastes, kDayFraction1800, kClearWeather, fog);
    const AuthoredLayer& ground = fog.layers[kSceneLayers - 1];
    std::printf("     Bone Wastes at %.0f yd: converted ground fog from %.0f yd up to %.0f yd\n",
                boneWastes[2], ground.lowerHeight, ground.upperHeight);
    Check(resolved && fog.layerCount == kSceneLayers &&
              ground.upperHeight < boneWastes[2] - groundFogBelowTheGround,
          "a converted light lifts or lowers the absolute heights of its Classic layers by its height above the "
          "Classic light, so the Bone Wastes' ground fog stays below their ground as the Classic ground fog stays "
          "below its own light");

    const float netherstormLight[3] = {3519.1f, 6896.3f, 142.1f};
    const float azsharaLightAtNoHeight[3] = {3551.2f, -6765.3f, 0.0f};
    const uint32_t netherstormLightId = kFirstClientLight + 500;
    const uint32_t azsharaLightId = 238;
    AuthoredFog converted = {};
    AuthoredFog classic = {};
    const bool bothResolved =
        data.Resolve(kOutland, netherstormLight, kDayFraction1800, kClearWeather, converted) &&
        data.Resolve(kKalimdor, azsharaLightAtNoHeight, kDayFraction1800, kClearWeather, classic);
    std::printf("     Netherstorm light 500 (weight %.2f) haze from %.0f to %.0f yd; its Classic light 238 at Z 0 "
                "(weight %.2f) from %.0f to %.0f yd\n",
                WeightOf(converted, netherstormLightId), converted.layers[0].upperHeight,
                converted.layers[0].lowerHeight, WeightOf(classic, azsharaLightId), classic.layers[0].upperHeight,
                classic.layers[0].lowerHeight);
    Check(bothResolved && WeightOf(converted, netherstormLightId) > 0.0f &&
              Near(WeightOf(classic, azsharaLightId), 1.0f) &&
              converted.layers[0].upperHeight == classic.layers[0].upperHeight &&
              converted.layers[0].lowerHeight == classic.layers[0].lowerHeight,
          "a converted light keeps the absolute heights of its Classic layers when either light has no height (Z 0, "
          "as 234 Classic lights have), so Netherstorm keeps the haze of Azshara's light 238");
}

void CheckConvertedLightsCarryNoClassicNoise(const FogData& data)
{
    const float places[][3] = {{-3005.0f, 4469.0f, -18.0f}, {3054.0f, 3654.0f, 142.0f}, {1350.0f, -5000.0f, 210.0f}};
    const int maps[] = {kOutland, kOutland, kNorthrend};
    int layers = 0;
    int noisy = 0;
    for (int place = 0; place < static_cast<int>(std::size(maps)); ++place)
    {
        AuthoredFog fog = {};
        if (!data.Resolve(maps[place], places[place], kDayFraction1800, kClearWeather, fog))
            continue;
        for (int i = 0; i < fog.layerCount; ++i)
            noisy += fog.layers[i].noise.presence > 0.0f ? 1 : 0;
        layers += fog.layerCount;
    }
    std::printf("     Bone Wastes, Netherstorm, Howling Fjord: %d of %d converted layers carry noise\n", noisy,
                layers);
    Check(layers == 3 * kSceneLayers && noisy == 0,
          "the converted lights of Outland and Northrend carry no Classic noise, which showed nowhere there but made "
          "Netherstorm look worse");
}

bool ScattersNoSunOrMoon(const FogParams& fog)
{
    for (const FogLayer& layer : fog.layers)
        for (float c : layer.diffuse)
            if (c != 0.0f)
                return false;
    return fog.lightVisibility == 0.0f;
}

float SceneLayersRedDiffuse(const FogParams& fog)
{
    float red = 0.0f;
    for (int i = 0; i < kSceneLayers; ++i)
        red += fog.layers[i].diffuse[0];
    return red;
}

void CheckSunlessSkiesScatterNoSunOrMoon(const FogData& data)
{
    const Vec3 netherstorm = {3054.0f, 3654.0f, 142.0f};
    const Vec3 shattrathCity = {-1829.0f, 5248.0f, -46.0f};
    const Vec3 toLight = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    const Config cfg = ConfigForMap(Config{}, kOutland);
    const FrameInputs sunOverNetherstorm = ContinentFrame(kOutland, netherstorm, evening, toLight, false);
    const FrameInputs moonOverNetherstorm = ContinentFrame(kOutland, netherstorm, evening, toLight, true);
    const FrameInputs sunOverShattrath = ContinentFrame(kOutland, shattrathCity, evening, toLight, false);
    AuthoredFog sunless = {};
    AuthoredFog sunlit = {};
    const bool resolved = data.Resolve(kOutland, sunOverNetherstorm.camPos, evening, kClearWeather, sunless) &&
                          data.Resolve(kOutland, sunOverShattrath.camPos, evening, kClearWeather, sunlit);
    const FogParams sun = BuildFogParams(sunOverNetherstorm, cfg, &sunless);
    const FogParams moon = BuildFogParams(moonOverNetherstorm, cfg, &sunless);
    const FogParams shattrath = BuildFogParams(sunOverShattrath, cfg, &sunlit);
    std::printf("     Netherstorm: sunless %.2f, scene diffuse red %.5f, distance fog %.5f, rays %.2f, density %.6f; "
                "Shattrath City: sunless %.2f, scene diffuse red %.5f, rays %.2f\n",
                sunless.sunless, SceneLayersRedDiffuse(sun), sun.layers[kDistanceFogLayer].diffuse[0],
                sun.lightVisibility, SceneLayersDensity(sun), sunlit.sunless, SceneLayersRedDiffuse(shattrath),
                shattrath.lightVisibility);
    Check(resolved && Near(sunless.sunless, 1.0f) && sun.authored && SceneLayersDensity(sun) > 0.0f &&
              ScattersNoSunOrMoon(sun) && ScattersNoSunOrMoon(moon),
          "Netherstorm, whose sky the client draws without sun and moon, scatters neither in its converted layers "
          "nor in its distance fog and casts no god rays, by day or by night");
    Check(resolved && sunlit.sunless == 0.0f && SceneLayersRedDiffuse(shattrath) > 0.0f &&
              shattrath.lightVisibility > 0.0f,
          "Shattrath City, whose sky shows the sun, keeps its sun scatter and god rays");
}

struct Landmark
{
    const char* name;
    int map;
    float position[3];
};

constexpr uint32_t kSunKeepingCopyOfNorthrendGlobalLight = kFirstClientLight + kNorthrendGlobalClientLight;
constexpr uint32_t kNoSunLayerFlag = 0x10;

template <size_t N>
bool LandmarksAtSunless(const FogData& data, const Landmark (&landmarks)[N], float sunless, const char* label)
{
    bool all = true;
    std::printf("     %s, sunless at 18:00:", label);
    for (const Landmark& landmark : landmarks)
    {
        AuthoredFog fog = {};
        const bool resolved = data.Resolve(landmark.map, landmark.position, kDayFraction1800, kClearWeather, fog);
        std::printf(" %s %.2f (%u)", landmark.name, fog.sunless, fog.lightCount ? fog.lightIds[0] : 0u);
        all = all && resolved && Near(fog.sunless, sunless);
    }
    std::printf("\n");
    return all;
}

AuthoredLayer WithoutTheSunFlag(AuthoredLayer layer)
{
    layer.sunless = 0.0f;
    layer.flags &= ~kNoSunLayerFlag;
    return layer;
}

bool SameFogApartFromTheSun(const AuthoredFog& a, const AuthoredFog& b)
{
    bool same = a.layerCount == b.layerCount && a.layerCount > 0 && a.lightCount == b.lightCount &&
                std::equal(a.lightWeights, a.lightWeights + a.lightCount, b.lightWeights) &&
                a.coverage == b.coverage && a.hasClassicDirectLight == b.hasClassicDirectLight &&
                std::equal(std::begin(a.classicDirectLight), std::end(a.classicDirectLight), b.classicDirectLight) &&
                a.hasGlow == b.hasGlow && a.glow == b.glow && a.hasGradingCurve == b.hasGradingCurve &&
                std::equal(std::begin(a.gradingCurve), std::end(a.gradingCurve), b.gradingCurve) &&
                a.clearSky == b.clearSky;
    for (int i = 0; same && i < a.layerCount; ++i)
    {
        const AuthoredLayer x = WithoutTheSunFlag(a.layers[i]);
        const AuthoredLayer y = WithoutTheSunFlag(b.layers[i]);
        same = same && std::memcmp(&x, &y, sizeof(AuthoredLayer)) == 0;
    }
    return same;
}

void CheckGrizzlyHillsSholazarAndBoreanTundraKeepTheSun(const FogData& data)
{
    const Landmark sunKept[] = {
        {"Grizzlemaw", kNorthrend, {4022.6f, -3820.9f, 114.6f}},
        {"Amberpine Lodge", kNorthrend, {3456.5f, -2778.6f, 200.4f}},
        {"Warsong Hold", kNorthrend, {2741.3f, 6097.2f, 76.9f}},
        {"Valiance Keep", kNorthrend, {2213.9f, 5273.1f, 11.3f}},
        {"Transitus Shield", kNorthrend, {3587.3f, 6661.5f, 195.4f}},
        {"Nesingwary Base Camp", kNorthrend, {5595.1f, 5831.6f, -67.5f}},
        {"Rainspeaker Canopy", kNorthrend, {5670.8f, 4561.3f, -136.5f}},
    };
    const Landmark sunless[] = {
        {"Shadow Vault", kNorthrend, {8447.4f, 2698.1f, 656.8f}},
        {"Balargarde Fortress", kNorthrend, {7060.0f, 4256.8f, 676.9f}},
        {"The Fleshwerks", kNorthrend, {6490.0f, 3427.7f, 595.7f}},
        {"Dun Niffelem", kNorthrend, {7201.8f, -2721.2f, 764.2f}},
        {"K3", kNorthrend, {6122.2f, -1061.5f, 402.6f}},
        {"Howling Fjord", kNorthrend, {1350.0f, -5000.0f, 210.0f}},
        {"Wintergrasp", kNorthrend, {5100.0f, 2750.0f, 443.7f}},
        {"Moonrest Gardens", kNorthrend, {3323.2f, 2717.3f, 36.5f}},
        {"Naxxramas", kNorthrend, {3668.7f, -1262.5f, 243.6f}},
        {"Light's Breach", kNorthrend, {5148.5f, -2182.6f, 236.6f}},
        {"Telredor", kOutland, {278.6f, 6001.3f, 144.7f}},
    };
    Check(LandmarksAtSunless(data, sunKept, 0.0f, "Grizzly Hills, Borean Tundra, Coldarra, Sholazar Basin"),
          "Grizzly Hills, Borean Tundra with Coldarra and Sholazar Basin keep the sun and moon again: their zone "
          "outlines hold a copy of Northrend's global light that keeps the sun, and their own skies keep it in "
          "their sphere lights");
    Check(LandmarksAtSunless(data, sunless, 1.0f,
                             "Icecrown, Storm Peaks, Howling Fjord, Wintergrasp, Dragonblight, Zul'Drak, Zangarmarsh"),
          "Icecrown, the Storm Peaks, the Howling Fjord and Zangarmarsh stay sunless, and so do Wintergrasp, "
          "Dragonblight and Zul'Drak about 300 to 520 yd past the Borean and Grizzly Hills outlines");

    const float* grizzlemaw = sunKept[0].position;
    const float* warsongHold = sunKept[2].position;
    const float* howlingFjord = sunless[5].position;
    AuthoredFog grizzly = {};
    AuthoredFog borean = {};
    AuthoredFog fjord = {};
    const bool resolved = data.Resolve(kNorthrend, grizzlemaw, kDayFraction1800, kClearWeather, grizzly) &&
                          data.Resolve(kNorthrend, warsongHold, kDayFraction1800, kClearWeather, borean) &&
                          data.Resolve(kNorthrend, howlingFjord, kDayFraction1800, kClearWeather, fjord);
    std::printf("     sun-keeping copy %u: weight %.2f at Grizzlemaw, %.2f at Warsong Hold, %.2f at the Howling "
                "Fjord, where light %u has %.2f\n",
                kSunKeepingCopyOfNorthrendGlobalLight, WeightOf(grizzly, kSunKeepingCopyOfNorthrendGlobalLight),
                WeightOf(borean, kSunKeepingCopyOfNorthrendGlobalLight),
                WeightOf(fjord, kSunKeepingCopyOfNorthrendGlobalLight), kNorthrendGlobalClientLight,
                WeightOf(fjord, kNorthrendGlobalClientLight));
    Check(resolved && Near(WeightOf(grizzly, kSunKeepingCopyOfNorthrendGlobalLight), 1.0f) &&
              Near(WeightOf(borean, kSunKeepingCopyOfNorthrendGlobalLight), 1.0f) &&
              WeightOf(fjord, kSunKeepingCopyOfNorthrendGlobalLight) == 0.0f &&
              SameFogApartFromTheSun(grizzly, fjord) && SameFogApartFromTheSun(borean, fjord),
          "inside the Grizzly Hills and Borean Tundra outlines the copy replaces Northrend's global light with the "
          "same layers, glow, direct light and grading; outside them, as at the Howling Fjord, the global light "
          "stays");

    AuthoredFog grizzlyGhost = {};
    AuthoredFog fjordGhost = {};
    AuthoredFog grizzlyHalfStorm = {};
    const LightParamsSelection ghost = ScreenEffectSlot(FogData::kDeathSlot, 0.0f);
    const bool resolvedSlots =
        data.Resolve(kNorthrend, grizzlemaw, kDayFraction1800, ghost, grizzlyGhost) &&
        data.Resolve(kNorthrend, howlingFjord, kDayFraction1800, ghost, fjordGhost) &&
        data.Resolve(kNorthrend, grizzlemaw, kDayFraction1800, Storm(0.5f), grizzlyHalfStorm);
    std::printf("     Grizzlemaw: sunless %.2f as a ghost (Howling Fjord %.2f), %.2f at half storm\n",
                grizzlyGhost.sunless, fjordGhost.sunless, grizzlyHalfStorm.sunless);
    Check(resolvedSlots && Near(grizzlyGhost.sunless, 1.0f) && Near(fjordGhost.sunless, 1.0f) &&
              SameFogApartFromTheSun(grizzlyGhost, fjordGhost) && Near(grizzlyHalfStorm.sunless, 0.0f),
          "the copy keeps the sun in its clear and storm slots alike, so Grizzlemaw stays sunlit at half storm, "
          "while its ghost slot is 1406's own: the spirit sky over Grizzlemaw stays sunless as at the Howling Fjord");

    const Vec3 toLight = {0.607f, 0.607f, 0.513f};
    const Vec3 eye = {grizzlemaw[0], grizzlemaw[1], grizzlemaw[2]};
    const Config cfg = ConfigForMap(Config{}, kNorthrend);
    const FogParams sun = BuildFogParams(ContinentFrame(kNorthrend, eye, kDayFraction1800, toLight, false), cfg,
                                         &grizzly);
    const FogParams moon = BuildFogParams(ContinentFrame(kNorthrend, eye, kDayFraction1800, toLight, true), cfg,
                                          &grizzly);
    std::printf("     Grizzlemaw: scene diffuse red %.5f by the sun, %.5f by the moon, rays %.2f\n",
                SceneLayersRedDiffuse(sun), SceneLayersRedDiffuse(moon), sun.lightVisibility);
    Check(resolved && sun.authored && SceneLayersRedDiffuse(sun) > 0.0f && SceneLayersRedDiffuse(moon) > 0.0f &&
              sun.lightVisibility > 0.0f,
          "Grizzlemaw in Grizzly Hills scatters the sun and the moon in its converted layers and casts god rays");
}

AuthoredFog WithoutClearSky(AuthoredFog fog)
{
    fog.clearSky = 0.0f;
    for (AuthoredLayer& layer : fog.layers)
        layer.clearSky = 0.0f;
    return fog;
}

void CheckHellfireKeepsOnlyAThinGroundFogUnderAClearSky(const FogData& data)
{
    const Vec3 hellfirePeninsula = {-258.0f, 1800.0f, 105.0f};
    const Vec3 netherstorm = {3054.0f, 3654.0f, 142.0f};
    const Vec3 toLight = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    const float oneDegreeUpward = 0.0175f;
    const float clearAtOneDegree = 1.0e-6f;
    const float groundThinEnoughForDistanceFog = 0.01f;
    const uint32_t hellfireLight = kFirstClientLight + 520;
    const Config cfg = ConfigForMap(Config{}, kOutland);
    const FrameInputs frame = ContinentFrame(kOutland, hellfirePeninsula, evening, toLight, false);
    AuthoredFog hellfire = {};
    AuthoredFog other = {};
    const bool resolved = data.Resolve(kOutland, frame.camPos, evening, kClearWeather, hellfire) &&
                          data.Resolve(kOutland, &netherstorm.x, evening, kClearWeather, other);
    AuthoredFog thinGround = hellfire;
    thinGround.layers[kSceneLayers - 1].density *= groundThinEnoughForDistanceFog;
    const AuthoredFog hellfireWithSky = WithoutClearSky(hellfire);
    const AuthoredFog thinGroundWithSky = WithoutClearSky(thinGround);
    const FogParams fog = BuildFogParams(frame, cfg, &hellfire);
    const FogParams skyFogged = BuildFogParams(frame, cfg, &hellfireWithSky);
    const FogParams thin = BuildFogParams(frame, cfg, &thinGround);
    const FogParams thinSkyFogged = BuildFogParams(frame, cfg, &thinGroundWithSky);
    const FogLayer& ground = fog.layers[kSceneLayers - 1];
    const float groundSkyScale = std::exp(-oneDegreeUpward * ground.skyFalloff);
    const float foggedSkyScale = std::exp(-oneDegreeUpward * skyFogged.layers[kSceneLayers - 1].skyFalloff);
    std::printf("     Hellfire Peninsula (light 520, weight %.2f): haze %.6f, far wall %.6f, ground %.6f, distance fog "
                "%.6f, ground fog on the sky 1 degree up x%.2g (x%.2g without the clear sky); with a hundredth of "
                "the ground fog the distance fog is %.6f, %.6f without the clear sky\n",
                WeightOf(hellfire, hellfireLight), fog.layers[0].density, fog.layers[1].density, ground.density,
                fog.layers[kDistanceFogLayer].density, groundSkyScale, foggedSkyScale,
                thin.layers[kDistanceFogLayer].density, thinSkyFogged.layers[kDistanceFogLayer].density);
    Check(resolved && Near(WeightOf(hellfire, hellfireLight), 1.0f) && Near(hellfire.clearSky, 1.0f) &&
              fog.layers[0].density == 0.0f && fog.layers[1].density == 0.0f && ground.density > 0.0f &&
              fog.layers[kDistanceFogLayer].density == 0.0f && groundSkyScale < clearAtOneDegree &&
              ScattersNoSunOrMoon(fog),
          "Hellfire Peninsula keeps only its ground fog, without sun, and leaves its sky untouched: no haze, far "
          "wall or distance fog, and no ground fog on the sky from a degree above the horizon");
    Check(resolved && foggedSkyScale == 1.0f && thin.layers[kDistanceFogLayer].density == 0.0f &&
              thinSkyFogged.layers[kDistanceFogLayer].density > 0.0f && other.clearSky == 0.0f &&
              other.layerCount > 0,
          "it is Hellfire's clear sky that keeps the ground fog off the sky and the distance fog away even where "
          "the ground fog is thin; Netherstorm's sky stays as it was");
}

void CheckMoonScattersOnItsOwn(const FogData& data)
{
    const Vec3 goldshire = {-9456.8f, 54.7f, 59.6f};
    const Vec3 toLight = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    const FrameInputs sunLit = ContinentFrame(kEasternKingdoms, goldshire, evening, toLight, false);
    const FrameInputs moonLit = ContinentFrame(kEasternKingdoms, goldshire, evening, toLight, true);
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kEasternKingdoms, sunLit.camPos, evening, kClearWeather, fog);
    const Config defaults;
    Config noSun = defaults;
    noSun.sunScatter = 0.0f;
    const FogParams sun = BuildFogParams(sunLit, defaults, &fog);
    const FogParams moon = BuildFogParams(moonLit, defaults, &fog);
    const FogParams darkSun = BuildFogParams(sunLit, noSun, &fog);
    const FogParams moonWithoutSun = BuildFogParams(moonLit, noSun, &fog);
    bool share = resolved && sun.authored;
    bool own = share;
    int lit = 0;
    for (int i = 0; i < kFogLayers; ++i)
        for (int c = 0; c < 3; ++c)
        {
            if (sun.layers[i].diffuse[c] <= 0.0f)
                continue;
            ++lit;
            share = share && Near(moon.layers[i].diffuse[c], defaults.moonScatter * sun.layers[i].diffuse[c]);
            own = own && darkSun.layers[i].diffuse[c] == 0.0f &&
                  Near(moonWithoutSun.layers[i].diffuse[c], moon.layers[i].diffuse[c]);
        }
    std::printf("     Goldshire haze diffuse red: sun %.6f, moon %.6f at MoonScatter %.2f; with SunScatter 0: sun "
                "%.6f, moon %.6f\n",
                sun.layers[0].diffuse[0], moon.layers[0].diffuse[0], defaults.moonScatter,
                darkSun.layers[0].diffuse[0], moonWithoutSun.layers[0].diffuse[0]);
    Check(defaults.moonScatter == 0.2f && lit > 0 && share && own,
          "the moon scatters MoonScatter (0.2 by default) of the sun's light in the Classic and distance layers, on "
          "its own: SunScatter 0 darkens the sun's halo and leaves the moon's");
}

struct RegionLights
{
    int map;
    float intensity;
};

void CheckLocalLightsGlowByRegion()
{
    const Vec3 eye = {-3005.0f, 4469.0f, -18.0f};
    const Vec3 toLight = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    bool followsRegion = true;
    bool spellsKeepTheirGain = true;
    for (float density : {0.15f, 0.5f, 1.0f, 2.0f})
    {
        Config cfg;
        cfg.azerothDensity = density;
        cfg.outlandDensity = density;
        cfg.northrendDensity = density;
        cfg.azerothLocalLights = 1.5f;
        cfg.outlandLocalLights = 2.5f;
        cfg.northrendLocalLights = 4.0f;
        const RegionLights regions[] = {{kEasternKingdoms, 1.5f}, {kOutland, 2.5f}, {kNorthrend, 4.0f}};
        for (const RegionLights& region : regions)
        {
            const FrameInputs frame = ContinentFrame(region.map, eye, evening, toLight, false);
            const FogParams fog = BuildFogParams(frame, ConfigForMap(cfg, region.map), nullptr);
            followsRegion = followsRegion && Near(fog.localLightGain * density, region.intensity);
            LocalPointLight lamp;
            LocalPointLight spell;
            spell.source = LocalLightSource::Spell;
            spellsKeepTheirGain = spellsKeepTheirGain && Near(LocalLightFogGain(fog, lamp), fog.localLightGain) &&
                                  Near(LocalLightFogGain(fog, spell) * density, 1.0f);
        }
    }
    const Config defaults;
    const FogParams outland = BuildFogParams(ContinentFrame(kOutland, eye, evening, toLight, false),
                                             ConfigForMap(defaults, kOutland), nullptr);
    const FogParams azeroth = BuildFogParams(ContinentFrame(kEasternKingdoms, eye, evening, toLight, false),
                                             ConfigForMap(defaults, kEasternKingdoms), nullptr);
    std::printf("     local light gain by default: Outland %.3f, Azeroth %.3f\n", outland.localLightGain,
                azeroth.localLightGain);
    Check(followsRegion && Near(azeroth.localLightGain, 1.0f) && defaults.outlandLocalLights == 8.0f &&
              defaults.northrendLocalLights == 8.0f && Near(outland.localLightGain, 8.0f / 0.15f),
          "the local lights scatter with their region's intensity over the region's density, so the density "
          "sliders leave the lamps' glow alone; Outland and Northrend start at 8, as upstream's testers needed about "
          "eight times the old world's intensity there, and Azeroth stays as before by default");
    Check(spellsKeepTheirGain,
          "spell lights scatter over the region's density alone, whatever its lamp intensity, while the lamps "
          "keep the region's gain");
}

void CheckOutlandSkyColourTintsTheConvertedFog(const FogData& data)
{
    const Vec3 hellfirePeninsula = {-3005.0f, 4469.0f, -18.0f};
    const Vec3 toSun = {0.607f, 0.607f, 0.513f};
    const float evening = 0.7279f;
    FrameInputs frame = ContinentFrame(kOutland, hellfirePeninsula, evening, toSun, false);
    frame.fogColor = 0xFF7A3A2Au;
    AuthoredFog fog = {};
    const bool resolved = data.Resolve(kOutland, frame.camPos, evening, kClearWeather, fog);
    Config asConverted = {};
    asConverted.outlandSkyColor = 0.0f;
    Config sky = asConverted;
    sky.outlandSkyColor = 1.0f;
    Config half = asConverted;
    half.outlandSkyColor = 0.5f;
    const FogParams converted = BuildFogParams(frame, ConfigForMap(asConverted, kOutland), &fog);
    const FogParams own = BuildFogParams(frame, ConfigForMap(sky, kOutland), &fog);
    const FogParams mixed = BuildFogParams(frame, ConfigForMap(half, kOutland), &fog);
    float clientFog[3];
    UnpackColor(frame.fogColor, clientFog);
    for (float& c : clientFog)
        c = std::pow(c, 2.2f);
    bool takesClientFog = true;
    bool halfway = true;
    const auto luminance = [](const float* rgb) { return 0.2126f * rgb[0] + 0.7152f * rgb[1] + 0.0722f * rgb[2]; };
    const auto hasClientHue = [&](const float* rgb) {
        const float scale = luminance(rgb) / luminance(clientFog);
        return std::fabs(rgb[0] - clientFog[0] * scale) < 1e-4f && std::fabs(rgb[1] - clientFog[1] * scale) < 1e-4f &&
               std::fabs(rgb[2] - clientFog[2] * scale) < 1e-4f;
    };
    for (int i = 0; i < fog.layerCount; ++i)
    {
        takesClientFog = takesClientFog && hasClientHue(own.layers[i].emissive) &&
                         hasClientHue(own.layers[i].shadowEmissive) &&
                         std::fabs(luminance(own.layers[i].emissive) - luminance(converted.layers[i].emissive)) < 1e-4f;
        for (int c = 0; c < 3; ++c)
            halfway = halfway && std::fabs(mixed.layers[i].emissive[c] - 0.5f * (converted.layers[i].emissive[c] +
                                                                                    own.layers[i].emissive[c])) < 1e-5f;
    }
    std::printf("     Hellfire Peninsula layer 1 emissive: converted %.3f %.3f %.3f, client fog hue %.3f %.3f %.3f\n",
                converted.layers[1].emissive[0], converted.layers[1].emissive[1], converted.layers[1].emissive[2],
                own.layers[1].emissive[0], own.layers[1].emissive[1], own.layers[1].emissive[2]);
    Check(resolved && fog.layerCount > 0 && takesClientFog && halfway &&
              std::fabs(converted.layers[1].emissive[0] - own.layers[1].emissive[0]) > 0.01f,
          "OutlandSkyColor mixes the layers' ambient colours toward the hue of the client's fog colour at their own "
          "brightness: 1 takes that hue, 0.5 lands halfway, 0 keeps the converted colours");

    FrameInputs stormwind = ContinentFrame(kEasternKingdoms, {-8565.21f, 993.46f, 104.96f}, evening, toSun, false);
    stormwind.fogColor = frame.fogColor;
    AuthoredFog classic = {};
    data.Resolve(kEasternKingdoms, stormwind.camPos, evening, kClearWeather, classic);
    const FogParams plain = BuildFogParams(stormwind, ConfigForMap(asConverted, kEasternKingdoms), &classic);
    const FogParams tinted = BuildFogParams(stormwind, ConfigForMap(sky, kEasternKingdoms), &classic);
    Check(std::memcmp(plain.layers, tinted.layers, sizeof(plain.layers)) == 0,
          "the sky colour settings leave the Classic fog of the old continents as authored");
}

float BoxMean7x7(const Image& img, UINT cx, UINT cy, int ch)
{
    double sum = 0.0;
    for (UINT y = cy - 3; y <= cy + 3; ++y)
        for (UINT x = cx - 3; x <= cx + 3; ++x)
            sum += img.At(x, y)[ch];
    return static_cast<float>(sum / 49.0 / 255.0);
}

float HighlightRollOff(float x, float knee)
{
    float span = std::fmax(1.0f - knee, 1e-4f);
    return x <= knee ? x : knee + span * (1.0f - std::exp(-(x - knee) / span));
}

float ExpectedLinearComposite(float sceneEncoded, float radianceEncoded, float transmittance)
{
    float scene = std::pow(sceneEncoded, kDisplayGamma);
    float radiance = std::pow(radianceEncoded, kDisplayGamma);
    return std::pow(HighlightRollOff(scene * transmittance + radiance, std::fmax(0.8f, scene)), 1.0f / kDisplayGamma);
}

float PreInvertClientGlow(float composited, float glow)
{
    return (std::sqrt(1.0f + 4.0f * glow * composited) - 1.0f) / (2.0f * glow);
}

void CheckLinearComposite(Harness& h, const Config& cfg, Vec3 eye, Vec3 at, const float* proj,
                          const D3DVIEWPORT9& world)
{
    constexpr int kConvergenceFrames = 16;
    const char* skip = "";
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    auto renderConvergedWithGlow = [&](const Config& c, float glow) {
        vf_test_set_config(&c);
        Image img;
        for (int frame = 0; frame < kConvergenceFrames; ++frame)
        {
            h.BeginFrame();
            h.DrawScene(eye, view, proj, world);
            FrameInputs in = MakeInputs(view, proj, eye, at, world);
            in.clientGlowAmount = glow;
            vf_test_render(&in, &skip);
            if (frame == kConvergenceFrames - 1)
                img = Capture(h.dev);
            h.dev->EndScene();
        }
        return img;
    };
    auto renderConverged = [&](const Config& c) { return renderConvergedWithGlow(c, 0.0f); };
    h.BeginFrame();
    h.DrawScene(eye, view, proj, world);
    Image scene = Capture(h.dev);
    h.dev->EndScene();

    Config c = cfg;
    c.godRays = 0.0f;
    c.temporal = 0.85f;
    Config none = c;
    none.density = 0.0f;
    Image unfogged = renderConverged(none);
    int maxDiff = 0;
    for (UINT y = 0; y < 688; ++y)
        for (UINT x = 0; x < 1280; ++x)
            for (int ch = 0; ch < 3; ++ch)
                maxDiff = std::max(maxDiff, std::abs(unfogged.At(x, y)[ch] - scene.At(x, y)[ch]));
    std::printf("     zero fog: largest channel change %d/255\n", maxDiff);
    Check(maxDiff <= 1, "zero fog leaves the world viewport unchanged (linear composite)");

    Config radianceCfg = c;
    radianceCfg.debugView = 1;
    Image radiance = renderConverged(radianceCfg);
    Config transmittanceCfg = c;
    transmittanceCfg.debugView = 2;
    Image transmittance = renderConverged(transmittanceCfg);
    Image composited = renderConverged(c);
    const UINT points[3][2] = {{101, 101}, {640, 330}, {640, 600}};
    float worst = 0.0f;
    for (const auto& p : points)
        for (int ch = 0; ch < 3; ++ch)
        {
            float expected = ExpectedLinearComposite(BoxMean7x7(scene, p[0], p[1], ch),
                                                     BoxMean7x7(radiance, p[0], p[1], ch),
                                                     BoxMean7x7(transmittance, p[0], p[1], 2));
            worst = std::fmax(worst, std::fabs(BoxMean7x7(composited, p[0], p[1], ch) - expected));
        }
    std::printf("     linear composite vs scene * T + L: largest difference %.1f/255\n", worst * 255.0f);
    Check(worst < 4.0f / 255.0f, "fog blends over the scene in linear light");

    const float clientGlow = 0.65f;
    Image compensated = renderConvergedWithGlow(c, clientGlow);
    float glowWorst = 0.0f;
    for (const auto& p : points)
        for (int ch = 0; ch < 3; ++ch)
        {
            float composite = BoxMean7x7(composited, p[0], p[1], ch);
            float fogOpacity = 1.0f - BoxMean7x7(transmittance, p[0], p[1], 2);
            float expected = composite + (PreInvertClientGlow(composite, clientGlow) - composite) * fogOpacity;
            glowWorst = std::fmax(glowWorst, std::fabs(BoxMean7x7(compensated, p[0], p[1], ch) - expected));
        }
    Config noGlow = c;
    noGlow.glowCompensation = false;
    Image uncompensated = renderConvergedWithGlow(noGlow, clientGlow);
    float offDiff = 0.0f;
    for (const auto& p : points)
        for (int ch = 0; ch < 3; ++ch)
            offDiff = std::fmax(offDiff, std::fabs(BoxMean7x7(uncompensated, p[0], p[1], ch) -
                                                   BoxMean7x7(composited, p[0], p[1], ch)));
    std::printf("     glow compensation: largest difference %.1f/255; switched off %.1f/255\n", glowWorst * 255.0f,
                offDiff * 255.0f);
    Check(glowWorst < 4.0f / 255.0f && offDiff < 1.0f / 255.0f,
          "fog is pre-compensated for the client's glow, and GlowCompensation 0 turns it off");

    Config gamma = c;
    gamma.colorSpace = 0;
    Image gammaImage = renderConverged(gamma);
    double gammaDiff = 0.0;
    for (UINT y = 0; y < 688; ++y)
        for (UINT x = 0; x < 1280; ++x)
            gammaDiff += std::fabs(gammaImage.Luma(x, y) - scene.Luma(x, y));
    gammaDiff /= 1280.0 * 688.0;
    std::printf("     gamma mode: mean luma change %.4f\n", gammaDiff);
    Check(gammaDiff > 0.01 && gammaDiff < 0.5, "gamma mode (ColorSpace 0) still blends the fog");
    vf_test_set_config(&cfg);
}

struct ExposureVariant
{
    float exposure;
    float sunScatter;
};

void SaveClassicSunJustAboveViewCaptures(Harness& h, const Config& cfg, Vec3 eye, const FrameInputs& in,
                                         const float* proj, const D3DVIEWPORT9& world, const std::wstring& outDir)
{
    const char* skip = "";
    Vec3 towardSun = Add(eye, {in.toLight[0] * 100.0f, in.toLight[1] * 100.0f, -2.0f});
    float facingSunView[16];
    CameraRelativeLookAt(eye, towardSun, facingSunView);
    FrameInputs sunJustAboveView = MakeInputs(facingSunView, proj, eye, towardSun, world);
    sunJustAboveView.mapId = kEasternKingdoms;
    sunJustAboveView.toLight[2] = 0.45f;
    float len = std::sqrt(sunJustAboveView.toLight[0] * sunJustAboveView.toLight[0] +
                          sunJustAboveView.toLight[1] * sunJustAboveView.toLight[1] + 0.2025f);
    for (float& v : sunJustAboveView.toLight)
        v /= len;
    const ExposureVariant variants[] = {{1.0f, 1.0f}, {0.6f, 1.0f}, {0.6f, 0.6f}, {0.45f, 0.8f}};
    for (const ExposureVariant& v : variants)
    {
        Config sunCfg = cfg;
        sunCfg.dataMode = 1;
        sunCfg.exposure = v.exposure;
        sunCfg.sunScatter = v.sunScatter;
        vf_test_set_config(&sunCfg);
        for (int frame = 0; frame < 8; ++frame)
        {
            h.BeginFrame();
            h.DrawScene(eye, facingSunView, proj, world);
            vf_test_render(&sunJustAboveView, &skip);
            if (frame == 7)
            {
                Image sunImage = Capture(h.dev);
                wchar_t name[96];
                swprintf(name, 96, L"\\classic-sun-exposure%.2f-sun%.2f.png", v.exposure, v.sunScatter);
                SavePng(outDir + name, sunImage.w, sunImage.h, sunImage.bgra);
            }
            h.dev->EndScene();
        }
    }
}

std::wstring FullPath(const std::wstring& path)
{
    wchar_t full[MAX_PATH] = {};
    return GetFullPathNameW(path.c_str(), MAX_PATH, full, nullptr) ? std::wstring(full) : path;
}

std::string NarrowPath(const std::wstring& path)
{
    char narrow[MAX_PATH] = {};
    WideCharToMultiByte(CP_ACP, 0, path.c_str(), -1, narrow, MAX_PATH, nullptr, nullptr);
    return narrow;
}

std::wstring g_harnessLog;
std::vector<std::string> g_groupFilters;

bool GroupSelected(const std::string& group)
{
    if (g_groupFilters.empty())
        return true;
    std::string lowered = group;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const std::string& filter : g_groupFilters)
        if (lowered.find(filter) != std::string::npos)
            return true;
    return false;
}

#define RUN_GROUP(...)                      \
    do                                      \
    {                                       \
        if (GroupSelected(#__VA_ARGS__))    \
        {                                   \
            __VA_ARGS__;                    \
        }                                   \
    } while (0)

std::string ReadText(const std::wstring& path)
{
    std::string text;
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f)
        return text;
    char chunk[4096];
    for (size_t n; (n = std::fread(chunk, 1, sizeof(chunk), f)) > 0;)
        text.append(chunk, n);
    std::fclose(f);
    return text;
}

void CheckFrameCopySummary()
{
    FrameCopyCounters counters;
    for (int frame = 0; frame < 4; ++frame)
    {
        counters.CountFrame();
        counters.CountColourCopy(ColourCopySite::WaterBegin);
        counters.CountDepthRefresh(DepthRefreshSite::Fog, frame % 2 == 0, true);
    }
    counters.CountColourCopy(ColourCopySite::SurfaceLights);
    char text[768];
    DescribeFrameCopies(counters.Counts(), text, sizeof(text));
    std::printf("     %s\n", text);
    Check(std::strstr(text, "over 4 frames") && std::strstr(text, "water begin 1.00, surface lights 0.25") &&
              std::strstr(text, "fog 1.00 (0.50, 1.00)") && std::strstr(text, "water end 0.00 (0.00, 0.00)"),
          "the frame copy summary gives each site's colour copies and depth refreshes per frame, the refreshes with "
          "no depth write since the last and the multisampled copies");
    counters.Reset();
    DescribeFrameCopies(counters.Counts(), text, sizeof(text));
    Check(std::strstr(text, "over 0 frames") && std::strstr(text, "fog 0.00 (0.00, 0.00)"),
          "an empty interval describes zero copies instead of dividing by no frames");
}

bool SameHotkey(const Hotkey& a, const Hotkey& b)
{
    return a.virtualKey == b.virtualKey && a.ctrl == b.ctrl && a.shift == b.shift && a.alt == b.alt;
}

void CheckOverlayKeyNames()
{
    Hotkey key = {};
    Check(ParseHotkey("Ctrl+F7", key) && SameHotkey(key, {VK_F7, true, false, false}), "OverlayKey Ctrl+F7 parses");
    Check(ParseHotkey(" shift + ALT + pageup ", key) && SameHotkey(key, {VK_PRIOR, false, true, true}),
          "OverlayKey modifiers and key names ignore case and spaces");
    Check(ParseHotkey("o", key) && SameHotkey(key, {'O', false, false, false}) && ParseHotkey("F24", key) &&
              key.virtualKey == VK_F24,
          "OverlayKey takes letters and F1-F24");
    Hotkey untouched = kDefaultOverlayHotkey;
    const char* const malformed[] = {"", "Ctrl+", "F25", "F0", "Ctrl+F7+F8", "Win+F7", "Space"};
    bool rejected = true;
    for (const char* text : malformed)
        rejected = !ParseHotkey(text, untouched) && rejected;
    Check(rejected && SameHotkey(untouched, kDefaultOverlayHotkey), "malformed OverlayKey values are rejected");
    Check(HotkeyName(kDefaultOverlayHotkey) == "Ctrl+F7" &&
              HotkeyName({VK_HOME, false, true, true}) == "Shift+Alt+Home",
          "overlay key names read back as written");
}

std::string ProfileText(const std::string& path, const std::string& key)
{
    char buf[512];
    GetPrivateProfileStringA("CoAGraphics", key.c_str(), "<missing>", buf, sizeof(buf), path.c_str());
    return buf;
}

std::string IniSectionText(const IniSection& ini, const std::string& key)
{
    std::string text;
    return ini.Find(key.c_str(), text) ? text : "<missing>";
}

void CheckIniSectionMatchesProfileApi(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const std::wstring fixture = FullPath(outDir + L"\\profile_fixture.ini");
    const char fixtureText[] =
        "[Other]\r\nElsewhere=7\r\nShared=other\r\n"
        "[CoAGraphics]\r\n"
        "; Commented=1\r\n"
        "Plain=1\r\n"
        "  Spaced  =  42  \r\n"
        "MiXeD=3\r\n"
        "Twice=1\r\nTwice=2\r\n"
        "Quoted=\"0.5\"\r\n"
        "Single='9'\r\n"
        "Mismatched=\"4\r\n"
        "Empty=\r\n"
        "Negative=-5\r\n"
        "Decimal=1.5\r\n"
        "Trailing=12abc\r\n"
        "Letters=abc\r\n"
        "Plus=+3\r\n"
        "Padded=  -7\r\n"
        "Tabbed=\t8\t\r\n"
        "Hotkey=Ctrl+F7\r\n"
        "NoEquals\r\n"
        "Embedded=a=b\r\n"
        "[coagraphics]\r\n"
        "AfterRepeat=1\r\n"
        "Plain=2\r\n"
        "[Last]\r\nTail=1";
    std::FILE* file = _wfopen(fixture.c_str(), L"wb");
    Check(file != nullptr && std::fwrite(fixtureText, 1, sizeof(fixtureText) - 1, file) == sizeof(fixtureText) - 1,
          "profile fixture written");
    if (file)
        std::fclose(file);

    const std::string path = NarrowPath(fixture);
    const IniSection ini(path, "CoAGraphics");
    const char* keys[] = {"Plain",   "spaced",  "MIXED",   "Twice",   "Quoted",    "Single",     "Mismatched",
                          "Empty",   "Negative", "Decimal", "Trailing", "Letters",  "Plus",       "Padded",
                          "Tabbed",  "Hotkey",  "NoEquals", "Embedded", "Commented", "Elsewhere", "Shared",
                          "Missing", "AfterRepeat", "Tail"};
    std::string differences;
    constexpr int kFallback = 31337;
    for (const char* key : keys)
    {
        const std::string ours = IniSectionText(ini, key);
        const std::string theirs = ProfileText(path, key);
        if (ours != theirs)
            differences += std::string(differences.empty() ? "" : ", ") + key + " text '" + ours + "' against '" +
                           theirs + "'";
        const int ourInt = ini.Int(key, kFallback);
        const int theirInt = static_cast<int>(GetPrivateProfileIntA("CoAGraphics", key, kFallback, path.c_str()));
        if (ourInt != theirInt)
            differences += std::string(differences.empty() ? "" : ", ") + key + " int " + std::to_string(ourInt) +
                           " against " + std::to_string(theirInt);
    }
    if (!differences.empty())
        std::printf("     INI section differs from the profile API: %s\n", differences.c_str());
    Check(differences.empty(), "a section read once gives the profile API's text and integers for every key");

    const std::string shipped = NarrowPath(shippedIni);
    const IniSection shippedSection(shipped, "CoAGraphics");
    std::string shippedDifferences;
    for (const std::string& key : SettingKeys())
        if (IniSectionText(shippedSection, key) != ProfileText(shipped, key))
            shippedDifferences += (shippedDifferences.empty() ? "" : ", ") + key;
    Check(shippedDifferences.empty(), "the shipped INI reads the same through the section and the profile API");
}

constexpr float kShippedFarClipMax = 1583.0f;

void CheckRegionLocalLightsSetting(const std::wstring& outDir, const std::wstring& shippedIni)
{
    ConfigStore shipped;
    shipped.Load(NarrowPath(shippedIni));
    const Config& s = shipped.Get();
    std::printf("     shipped region local lights: Azeroth %.2f, Outland %.2f, Northrend %.2f\n", s.azerothLocalLights,
                s.outlandLocalLights, s.northrendLocalLights);
    Check(s.azerothLocalLights == 1.0f && s.outlandLocalLights == 8.0f && s.northrendLocalLights == 8.0f &&
              ReadText(shippedIni).find("; Local light intensity per region, 0..128 (the settings window's sliders "
                                        "reach 96),") != std::string::npos &&
              kRegionLocalLightsSliderMax == 96.0f && kRegionLocalLightsMax == 128.0f,
          "the shipped INI starts Outland's and Northrend's local lights at 8 and Azeroth's at 1, and documents "
          "their range 0..128 and the sliders' 96");

    const std::wstring savedIni = FullPath(outDir + L"\\region-local-lights.ini");
    CopyFileW(shippedIni.c_str(), savedIni.c_str(), FALSE);
    WritePrivateProfileStringW(L"CoAGraphics", L"AzerothLocalLights", L"12", savedIni.c_str());
    WritePrivateProfileStringW(L"CoAGraphics", L"OutlandLocalLights", L"96", savedIni.c_str());
    WritePrivateProfileStringW(L"CoAGraphics", L"NorthrendLocalLights", L"200", savedIni.c_str());
    ConfigStore written;
    written.Load(NarrowPath(savedIni));
    const Config& w = written.Get();
    std::printf("     written 12, 96 and 200: Azeroth %.2f, Outland %.2f, Northrend %.2f\n", w.azerothLocalLights,
                w.outlandLocalLights, w.northrendLocalLights);
    Check(w.azerothLocalLights == 12.0f && w.outlandLocalLights == 96.0f && w.northrendLocalLights == 128.0f,
          "region local lights read from the INI up to 128 and clamp to 128 above it");
}

void CheckShippedIniMatchesSettingsTable(const std::wstring& shippedIni)
{
    const std::string text = ReadText(shippedIni);
    std::string missing;
    for (const std::string& key : SettingKeys())
        if (text.find("\n" + key + "=") == std::string::npos)
            missing += (missing.empty() ? "" : ", ") + key;
    if (!missing.empty())
        std::printf("     keys missing from the shipped INI: %s\n", missing.c_str());
    Check(missing.empty(), "every key of the settings table is written in the shipped INI");

    const SettingRange temporal = FloatSettingRange(&Config::temporal);
    const SettingRange phase = FloatSettingRange(&Config::localLightPhase);
    const SettingRange parting = FloatSettingRange(&Config::grassPartingRadiusScale);
    Check(temporal.lo == 0.0f && temporal.hi == 0.97f && phase.lo == -0.9f && phase.hi == 0.9f &&
              parting.lo == 0.5f && parting.hi == 6.0f,
          "the settings window takes its slider ranges from the settings table");

    ConfigStore store;
    store.Load(NarrowPath(shippedIni));
    Config expected;
    expected.farClipMax = kShippedFarClipMax;
    const std::string changes = SettingChanges(expected, store.Get());
    if (!changes.empty())
        std::printf("     shipped INI differs from the defaults: %s\n", changes.c_str());
    Check(changes.empty(),
          "the shipped INI parses to the Config defaults except the far clip it lifts, so a missing key and a shipped "
          "key agree");
}

void CheckLightProjectsOntoTheViewport()
{
    const D3DVIEWPORT9 offset = {10, 20, 800, 600, 0.0f, 1.0f};
    float projection[16] = {};
    projection[0] = 1.2f;
    projection[5] = 2.1f;
    projection[8] = 0.02f;
    projection[9] = -0.03f;
    const Vec3 toLight = Norm({0.2f, 0.3f, 0.9f});
    const float toLightInView[3] = {toLight.x, toLight.y, toLight.z};
    const LightOnScreen onScreen = ProjectLightOntoViewport(toLightInView, projection, offset);
    const float ndcX = toLight.x / toLight.z * projection[0] + projection[8];
    const float ndcY = toLight.y / toLight.z * projection[5] + projection[9];
    const float column = offset.X + (ndcX * 0.5f + 0.5f) * offset.Width;
    const float row = offset.Y + (0.5f - ndcY * 0.5f) * offset.Height;
    const float sideways[3] = {1.0f, 0.0f, 0.5f * kLightInFrontViewZ};
    const float behind[3] = {0.0f, 0.6f, -0.8f};
    Check(onScreen.inFront && Near(onScreen.pixel[0], column) && Near(onScreen.pixel[1], row) &&
              Near(onScreen.ndc[0], ndcX) && Near(onScreen.ndc[1], ndcY) &&
              !ProjectLightOntoViewport(sideways, projection, offset).inFront &&
              !ProjectLightOntoViewport(behind, projection, offset).inFront,
          "the sun or moon projects onto the viewport where the god rays and the sun marker put it, and a light "
          "beside or behind the camera is not in front");
}

void CheckSettingsSaveKeepsTheIni(const std::wstring& outDir, const std::wstring& shippedIni)
{
    const std::wstring savedIni = FullPath(outDir + L"\\saved.ini");
    Check(CopyFileW(shippedIni.c_str(), savedIni.c_str(), FALSE) != FALSE, "shipped CoAGraphics.ini copied");
    WritePrivateProfileStringW(L"CoAGraphics", L"MaterialFog", L"0", savedIni.c_str());
    ConfigStore store;
    store.Load(NarrowPath(savedIni));
    const Config shipped = store.Get();
    Check(!store.HasUnsavedChanges(), "a freshly loaded INI has no unsaved changes");
    ConfigStore shippedStore;
    shippedStore.Load(NarrowPath(shippedIni));
    const std::string shippedText = ReadText(shippedIni);
    Check(SameLiveSettings(shipped, shippedStore.Get()) && shippedText.find("MaterialFog") == std::string::npos,
          "the removed MaterialFog key is gone from the shipped INI and ignored in an old one");
    Check(shippedText.find("\nLightShafts=1") != std::string::npos &&
              shippedText.find("\nWorldShadows=1") != std::string::npos && shipped.lightShafts && shipped.worldShadows,
          "the shipped INI turns LightShafts and WorldShadows on");

    Config edited = shipped;
    edited.enable = false;
    edited.overlay = false;
    edited.quality = 3;
    edited.density = 2.5f;
    edited.temporal = 0.5f;
    edited.localLights = !shipped.localLights;
    edited.lightShafts = !shipped.lightShafts;
    edited.farClipMax = 900.0f;
    edited.debugView = 2;
    edited.godRays = 99.0f;
    store.Apply(edited);
    const Config applied = store.Get();
    Check(applied.enable == shipped.enable && applied.overlay == shipped.overlay && applied.godRays == 4.0f &&
              store.HasUnsavedChanges(),
          "overlay edits keep the restart-only switches and are clamped like the INI");
    Check(store.Save() && !store.HasUnsavedChanges(), "overlay settings save to the INI");

    ConfigStore reloaded;
    reloaded.Load(NarrowPath(savedIni));
    const Config& r = reloaded.Get();
    Check(SameLiveSettings(r, applied) && r.quality == 3 && r.density == 2.5f && r.farClipMax == 900.0f &&
              r.localLights == edited.localLights && r.lightShafts == edited.lightShafts &&
              r.enable == shipped.enable && r.overlay == shipped.overlay,
          "saved settings reload unchanged and the restart-only keys stay as they were");
    const std::string text = ReadText(savedIni);
    Check(text.find("; CoAGraphics settings.") != std::string::npos &&
              text.find("\nDensity=2.5") != std::string::npos && text.find("\nHaze=1.0") != std::string::npos &&
              text.find("\nEnable=1") != std::string::npos &&
              text.find("; Global density multiplier") != std::string::npos &&
              text.find("\nLightShafts=0") != std::string::npos && text.find("\nMaterialFog=0") != std::string::npos,
          "saving rewrites only the changed lines and keeps the comments and unknown keys");

    Config scratch = store.Get();
    scratch.density = 0.1f;
    store.Apply(scratch);
    store.Revert();
    Check(store.Get().density == 2.5f && !store.HasUnsavedChanges(), "Revert goes back to the saved settings");

    Config typed = store.Get();
    typed.haze = 1.23456f;
    store.Apply(typed);
    WritePrivateProfileStringW(L"CoAGraphics", L"Quality", L"1", savedIni.c_str());
    const bool mergedSave = store.Save();
    ConfigStore afterMerge;
    afterMerge.Load(NarrowPath(savedIni));
    Check(mergedSave && store.Get().quality == 1 && afterMerge.Get().quality == 1 && afterMerge.Get().density == 2.5f,
          "Save keeps a hand edit of a setting the window did not change");
    Check(store.Get().haze == afterMerge.Get().haze && !store.HasUnsavedChanges() &&
              ReadText(savedIni).find("\nHaze=1.235") != std::string::npos,
          "Save records the value it wrote, rounded like the INI");
}

struct ClientInput
{
    int keyDowns = 0;
    int keyUps = 0;
    int characters = 0;
    int mouseDowns = 0;
    int mouseUps = 0;
    int mouseMoves = 0;
};

ClientInput g_clientInput;

LRESULT CALLBACK ClientWindowProc(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
        ++g_clientInput.keyDowns;
    if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
        ++g_clientInput.keyUps;
    if (msg == WM_CHAR)
        ++g_clientInput.characters;
    if (msg == WM_LBUTTONDOWN)
        ++g_clientInput.mouseDowns;
    if (msg == WM_LBUTTONUP)
        ++g_clientInput.mouseUps;
    if (msg == WM_MOUSEMOVE)
        ++g_clientInput.mouseMoves;
    return DefWindowProcW(window, msg, wParam, lParam);
}

void HoldModifiers(bool ctrl, bool shift, bool alt)
{
    BYTE keys[256] = {};
    GetKeyboardState(keys);
    keys[VK_CONTROL] = ctrl ? kKeyHeld : 0;
    keys[VK_SHIFT] = shift ? kKeyHeld : 0;
    keys[VK_MENU] = alt ? kKeyHeld : 0;
    SetKeyboardState(keys);
}

LPARAM KeyPressedParameter(unsigned virtualKey)
{
    return static_cast<LPARAM>(kKeyRepeatCountOne | (MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC) << 16));
}

LPARAM KeyReleasedParameter(unsigned virtualKey)
{
    return static_cast<LPARAM>(static_cast<UINT>(KeyPressedParameter(virtualKey)) | kKeyReleaseTransition);
}

void PressKey(HWND window, unsigned virtualKey)
{
    SendMessageW(window, WM_KEYDOWN, virtualKey, KeyPressedParameter(virtualKey));
    SendMessageW(window, WM_KEYUP, virtualKey, KeyReleasedParameter(virtualKey));
}

void HoldHotkeyDown(HWND window, const Hotkey& key)
{
    HoldModifiers(key.ctrl, key.shift, key.alt);
    SendMessageW(window, WM_KEYDOWN, key.virtualKey, KeyPressedParameter(key.virtualKey));
    HoldModifiers(false, false, false);
}

void TypeCharacter(HWND window, unsigned virtualKey, wchar_t character)
{
    SendMessageW(window, WM_CHAR, character, KeyPressedParameter(virtualKey));
}

void PressHotkey(HWND window, const Hotkey& key)
{
    HoldModifiers(key.ctrl, key.shift, key.alt);
    PressKey(window, key.virtualKey);
    HoldModifiers(false, false, false);
}

LPARAM ClientPointOfBackBufferPixel(const Harness& h, int x, int y)
{
    RECT client = {};
    GetClientRect(h.window, &client);
    return MAKELPARAM(MulDiv(x, client.right, static_cast<int>(h.pp.BackBufferWidth)),
                      MulDiv(y, client.bottom, static_cast<int>(h.pp.BackBufferHeight)));
}

void DrawOverlayFrames(int frames)
{
    for (int i = 0; i < frames; ++i)
        vf_test_draw_overlay();
}

void ClickAfterHover(HWND window, LPARAM point)
{
    SendMessageW(window, WM_MOUSEMOVE, 0, point);
    DrawOverlayFrames(1);
    SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, point);
    SendMessageW(window, WM_LBUTTONUP, 0, point);
    DrawOverlayFrames(2);
}

double MeanLumaChange(const Image& a, const Image& b, UINT x0, UINT y0, UINT x1, UINT y1)
{
    double sum = 0.0;
    for (UINT y = y0; y < y1; ++y)
        for (UINT x = x0; x < x1; ++x)
            sum += std::fabs(a.Luma(x, y) - b.Luma(x, y));
    return sum / (static_cast<double>(x1 - x0) * (y1 - y0));
}

double OverlayProbeChange(const Image& a, const Image& b)
{
    return MeanLumaChange(a, b, kOverlayProbeLeft, kOverlayProbeTop, kOverlayProbeRight, kOverlayProbeBottom);
}

void CheckHotkeyCharactersSwallowedOnlyWhileTheHotkeyIsTyped(Harness& h)
{
    const Hotkey key = kDefaultOverlayHotkey;
    g_clientInput = {};
    HoldHotkeyDown(h.window, key);
    TypeCharacter(h.window, key.virtualKey, kHotkeyCharacter);
    const int hotkeyCharacters = g_clientInput.characters;
    SendMessageW(h.window, WM_KILLFOCUS, 0, 0);
    TypeCharacter(h.window, 'W', L'w');
    std::printf("     characters the client saw: %d for the overlay key, %d typed after the focus left with it held\n",
                hotkeyCharacters, g_clientInput.characters - hotkeyCharacters);
    Check(vf_test_overlay_visible() == 1 && hotkeyCharacters == 0 && g_clientInput.characters == 1,
          "the overlay key's own character is swallowed, but once the focus leaves while the key is held, typed "
          "characters reach the client again");
    PressHotkey(h.window, key);

    g_clientInput = {};
    HoldHotkeyDown(h.window, key);
    SendMessageW(h.window, WM_KEYDOWN, 'W', KeyPressedParameter('W'));
    TypeCharacter(h.window, 'W', L'w');
    SendMessageW(h.window, WM_KEYUP, 'W', KeyReleasedParameter('W'));
    SendMessageW(h.window, WM_KEYUP, key.virtualKey, KeyReleasedParameter(key.virtualKey));
    const bool openWhileHeld = vf_test_overlay_visible() == 1;
    const ClientInput whileHeld = g_clientInput;
    PressHotkey(h.window, key);
    Check(openWhileHeld && whileHeld.characters == 1 && whileHeld.keyDowns == 1 && whileHeld.keyUps == 1 &&
              vf_test_overlay_visible() == 0,
          "a key pressed while the overlay key is held reaches the client with its character, and the overlay "
          "key's release still does not");
}

void CheckOverlayInput(Harness& h)
{
    const Hotkey key = kDefaultOverlayHotkey;
    g_clientInput = {};
    PressHotkey(h.window, key);
    Check(vf_test_overlay_visible() == 1 && g_clientInput.keyDowns == 0,
          "the overlay key opens the overlay without reaching the client");
    PressKey(h.window, 'W');
    Check(g_clientInput.keyDowns == 1, "other keys reach the client while the overlay is open");

    DrawOverlayFrames(kOverlaySettleFrames);
    ClickAfterHover(h.window, ClientPointOfBackBufferPixel(h, kOverlayTitleBarX, kOverlayTitleBarY));
    Check(g_clientInput.mouseDowns == 0, "clicks on the overlay window do not reach the client");
    Check(g_clientInput.mouseMoves == 1, "mouse moves over the overlay window still reach the client");
    ClickAfterHover(h.window, ClientPointOfBackBufferPixel(h, kBesideOverlayX, kBesideOverlayY));
    Check(g_clientInput.mouseDowns == 1, "clicks beside the overlay window reach the client");

    PressHotkey(h.window, key);
    Check(vf_test_overlay_visible() == 0 && g_clientInput.keyDowns == 1, "the overlay key closes the overlay");

    Config off = {};
    off.overlay = false;
    vf_test_set_config(&off);
    PressHotkey(h.window, key);
    Check(vf_test_overlay_visible() == 0 && g_clientInput.keyDowns == 2,
          "with Overlay=0 the overlay key reaches the client");
    Config on = {};
    vf_test_set_config(&on);
}

void DragAcross(HWND window, LPARAM from, LPARAM to)
{
    SendMessageW(window, WM_MOUSEMOVE, 0, from);
    DrawOverlayFrames(1);
    SendMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, from);
    DrawOverlayFrames(2);
    SendMessageW(window, WM_MOUSEMOVE, MK_LBUTTON, to);
    DrawOverlayFrames(2);
    SendMessageW(window, WM_LBUTTONUP, 0, to);
    DrawOverlayFrames(2);
}

void CheckOverlayWidgets(Harness& h)
{
    Config start = {};
    vf_test_set_config(&start);
    PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawOverlayFrames(kOverlaySettleFrames);
    DragAcross(h.window, ClientPointOfBackBufferPixel(h, kDensitySliderGrabX, kDensitySliderY),
               ClientPointOfBackBufferPixel(h, kDensitySliderDragX, kDensitySliderY));
    Config dragged = {};
    vf_test_get_config(&dragged);
    std::printf("     Density after dragging its slider: %.2f\n", dragged.density);
    Check(dragged.density > kDensityAfterDragAtLeast, "the Density slider drags although its section is named Density");

    g_clientInput = {};
    const LPARAM panelBody = ClientPointOfBackBufferPixel(h, kOverlayBodyX, kOverlayBodyY);
    ClickAfterHover(h.window, panelBody);
    DrawOverlayFrames(kOverlaySettleFrames);
    PressKey(h.window, VK_TAB);
    DrawOverlayFrames(kOverlaySettleFrames);
    PressKey(h.window, 'W');
    std::printf("     key presses the client saw for Tab then W: %d\n", g_clientInput.keyDowns);
    Check(g_clientInput.keyDowns == 2, "Tab with the overlay focused does not take the keyboard from the client");

    g_clientInput = {};
    const LPARAM beside = ClientPointOfBackBufferPixel(h, kBesideOverlayX, kBesideOverlayY);
    SendMessageW(h.window, WM_MOUSEMOVE, 0, beside);
    DrawOverlayFrames(kOverlaySettleFrames);
    SendMessageW(h.window, WM_MOUSEMOVE, 0, panelBody);
    SendMessageW(h.window, WM_LBUTTONDOWN, MK_LBUTTON, panelBody);
    DrawOverlayFrames(2);
    SendMessageW(h.window, WM_LBUTTONUP, 0, panelBody);
    std::printf("     button presses / releases the client saw: %d / %d\n", g_clientInput.mouseDowns,
                g_clientInput.mouseUps);
    Check(g_clientInput.mouseDowns == 1 && g_clientInput.mouseUps == 1,
          "a button whose press reached the client is released to the client");
    PressHotkey(h.window, kDefaultOverlayHotkey);
    vf_test_set_config(&start);

    Config pauseKey = {};
    pauseKey.overlayKey = {VK_PAUSE, true, false, false};
    vf_test_set_config(&pauseKey);
    HoldModifiers(true, false, false);
    PressKey(h.window, VK_CANCEL);
    const bool opened = vf_test_overlay_visible() == 1;
    PressKey(h.window, VK_CANCEL);
    HoldModifiers(false, false, false);
    Check(opened && vf_test_overlay_visible() == 0,
          "Ctrl+Pause toggles the overlay although Windows reports it as Cancel");
    vf_test_set_config(&start);
}

bool FogDrawnAfterReset(Harness& h, const D3DVIEWPORT9& vp, Vec3 eye, Vec3 at, const float* view,
                        const float* proj, bool& reset)
{
    h.ReleaseEngineObjects();
    reset = SUCCEEDED(h.dev->Reset(&h.pp));
    h.CreateEngineObjects();
    h.BeginFrame();
    h.DrawScene(eye, view, proj, vp);
    FrameInputs in = MakeInputs(view, proj, eye, at, vp);
    const char* skip = "";
    const bool drawn = vf_test_render(&in, &skip) != 0;
    h.dev->EndScene();
    return drawn;
}

void CheckFogReturnsAfterRefusedDepth(Harness& h, const D3DVIEWPORT9& vp, Vec3 eye, Vec3 at, const float* view,
                                      const float* proj)
{
    bool refusedReset = false;
    bool retriedReset = false;
    vf_test_refuse_fog_depth(1);
    const bool drawnWhileRefused = FogDrawnAfterReset(h, vp, eye, at, view, proj, refusedReset);
    vf_test_refuse_fog_depth(0);
    const bool drawnAfterRetry = FogDrawnAfterReset(h, vp, eye, at, view, proj, retriedReset);
    Check(refusedReset && retriedReset && !drawnWhileRefused && drawnAfterRetry,
          "a Reset whose fog depth cannot be created leaves the fog off, and the next Reset creates it again");
}

void DrawEngineFrameWithoutPresent(Harness& h, const D3DVIEWPORT9& world)
{
    h.BeginFrame();
    h.SetEngineState(world);
    h.dev->EndScene();
}

void CheckOverlayDraw(Harness& h, const D3DVIEWPORT9& world, const std::wstring& outDir)
{
    IDirect3DIndexBuffer9* engineIndices = nullptr;
    h.dev->CreateIndexBuffer(64, D3DUSAGE_WRITEONLY, D3DFMT_INDEX16, D3DPOOL_MANAGED, &engineIndices, nullptr);
    PressHotkey(h.window, kDefaultOverlayHotkey);
    Image before;
    Image after;
    bool statesKept = true;
    bool indicesKept = engineIndices != nullptr;
    for (int frame = 0; frame < kOverlaySettleFrames; ++frame)
    {
        DrawEngineFrameWithoutPresent(h, world);
        h.dev->SetIndices(engineIndices);
        before = Capture(h.dev);
        Sentinel s0;
        ReadSentinel(h.dev, s0);
        vf_test_draw_overlay();
        Sentinel s1;
        ReadSentinel(h.dev, s1);
        if (!SameSentinel(s0, s1))
        {
            if (statesKept)
                ReportSentinelDifferences(s0, s1);
            statesKept = false;
        }
        ReleaseSentinel(s0);
        ReleaseSentinel(s1);
        IDirect3DIndexBuffer9* boundIndices = nullptr;
        h.dev->GetIndices(&boundIndices);
        indicesKept = boundIndices == engineIndices && indicesKept;
        if (boundIndices)
            boundIndices->Release();
        after = Capture(h.dev);
    }
    h.dev->SetIndices(nullptr);
    if (engineIndices)
        engineIndices->Release();
    SavePng(outDir + L"\\overlay.png", after.w, after.h, after.bgra);
    Check(statesKept && indicesKept,
          "the overlay restores render, sampler, shader, constant, stream, index, viewport, scissor and target state");
    const double panelChange = OverlayProbeChange(before, after);
    std::printf("     mean luma change under the overlay window: %.4f\n", panelChange);
    Check(panelChange > 0.05, "the overlay draws its window over the frame");
    Check(MeanLumaChange(before, after, kBesideOverlayX - 200, kBesideOverlayY - 100, after.w, after.h) == 0.0,
          "the overlay leaves the rest of the frame untouched");

    PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawEngineFrameWithoutPresent(h, world);
    const Image hiddenBefore = Capture(h.dev);
    vf_test_draw_overlay();
    Check(MeanLumaChange(hiddenBefore, Capture(h.dev), 0, 0, hiddenBefore.w, hiddenBefore.h) == 0.0,
          "the hidden overlay draws nothing");
    PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawOverlayFrames(1);
}

struct PanelRect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    bool known = false;

    int GripX() const { return static_cast<int>(x + w) - kGripInset; }
    int GripY() const { return static_cast<int>(y + h) - kGripInset; }
};

PanelRect SettingsWindow()
{
    float rect[4] = {};
    const bool known = vf_test_settings_window(rect) != 0;
    return {rect[0], rect[1], rect[2], rect[3], known};
}

bool NearPixels(float a, float b)
{
    return std::fabs(a - b) <= kPanelSizeTolerance;
}

PanelRect DragGrip(Harness& h, const PanelRect& from, int dx, int dy)
{
    DragAcross(h.window, ClientPointOfBackBufferPixel(h, from.GripX(), from.GripY()),
               ClientPointOfBackBufferPixel(h, from.GripX() + dx, from.GripY() + dy));
    DrawOverlayFrames(kOverlaySettleFrames);
    return SettingsWindow();
}

void ShowOverlay(Harness& h, bool visible)
{
    if ((vf_test_overlay_visible() != 0) != visible)
        PressHotkey(h.window, kDefaultOverlayHotkey);
    DrawOverlayFrames(kOverlaySettleFrames);
}

UINT PanelEdge(float edge, UINT limit)
{
    return static_cast<UINT>(std::clamp(edge, 0.0f, static_cast<float>(limit)));
}

double ChangeBesideShrunkPanel(const Image& under, const Image& drawn, const PanelRect& shrunk, const PanelRect& grown)
{
    const UINT left = PanelEdge(grown.x, drawn.w);
    const UINT top = PanelEdge(grown.y, drawn.h);
    const UINT right = PanelEdge(grown.x + grown.w, drawn.w);
    const UINT bottom = PanelEdge(grown.y + grown.h, drawn.h);
    const UINT shrunkRight = PanelEdge(shrunk.x + shrunk.w + kBesideShrunkPanelGap, right);
    const UINT shrunkBottom = PanelEdge(shrunk.y + shrunk.h + kBesideShrunkPanelGap, bottom);
    return MeanLumaChange(under, drawn, shrunkRight, top, right, bottom) +
           MeanLumaChange(under, drawn, left, shrunkBottom, shrunkRight, bottom);
}

PanelRect CheckSettingsWindowResizes(Harness& h, const D3DVIEWPORT9& world, const std::wstring& outDir)
{
    const bool wasVisible = vf_test_overlay_visible() != 0;
    ShowOverlay(h, true);
    const PanelRect first = SettingsWindow();
    g_clientInput = {};
    const PanelRect grown = DragGrip(h, first, kGripDragX, kGripDragY);
    const ClientInput seen = g_clientInput;
    const PanelRect smallest = DragGrip(h, grown, kGripShrink, kGripShrink);
    const D3DVIEWPORT9 wholeBackBuffer = {0, 0, h.pp.BackBufferWidth, h.pp.BackBufferHeight, 0.0f, 1.0f};
    h.dev->SetViewport(&wholeBackBuffer);
    DrawEngineFrameWithoutPresent(h, world);
    const Image underSmallest = Capture(h.dev);
    vf_test_draw_overlay();
    const Image smallestImage = Capture(h.dev);
    SavePng(outDir + L"\\overlay-smallest.png", smallestImage.w, smallestImage.h, smallestImage.bgra);
    const double besideSmallest = ChangeBesideShrunkPanel(underSmallest, smallestImage, smallest, grown);
    const PanelRect restored =
        DragGrip(h, smallest, static_cast<int>(grown.w - smallest.w), static_cast<int>(grown.h - smallest.h));
    std::printf("     settings window at %.0f, %.0f: %.0f x %.0f, after dragging its grip by %d, %d: %.0f x %.0f, "
                "shrunk as far as it goes: %.0f x %.0f (mean luma change beside it %.4f); the game saw %d presses and "
                "%d releases\n",
                first.x, first.y, first.w, first.h, kGripDragX, kGripDragY, grown.w, grown.h, smallest.w, smallest.h,
                besideSmallest, seen.mouseDowns, seen.mouseUps);
    Check(first.known && NearPixels(first.x, kPanelMargin) && NearPixels(first.y, kPanelMargin) &&
              NearPixels(first.w, kPanelFirstWidth) && first.h > kPanelMinHeight,
          "the settings window opens 30 lines wide and as tall as the screen allows, 2 lines from the corner");
    Check(NearPixels(grown.w, first.w + kGripDragX) && NearPixels(grown.h, first.h + kGripDragY) &&
              NearPixels(grown.x, first.x) && NearPixels(grown.y, first.y),
          "dragging the settings window's resize grip through the window procedure resizes it");
    Check(seen.mouseDowns == 0 && seen.mouseUps == 0, "the resize grip keeps its press and release from the game");
    Check(NearPixels(smallest.w, kPanelMinWidth) && NearPixels(smallest.h, kPanelMinHeight) &&
              NearPixels(restored.w, grown.w) && NearPixels(restored.h, grown.h),
          "the settings window shrinks no further than 20 x 16 lines and grows back");
    Check(besideSmallest == 0.0,
          "the shrunk settings window draws nothing where the larger one was (overlay-smallest.png, drawn on a fresh "
          "frame)");
    ShowOverlay(h, wasVisible);
    return restored;
}

void CheckSettingsWindowKeepsItsSize(Harness& h, const PanelRect& expected, const char* what)
{
    const bool wasVisible = vf_test_overlay_visible() != 0;
    ShowOverlay(h, true);
    const PanelRect now = SettingsWindow();
    ShowOverlay(h, wasVisible);
    std::printf("     settings window %.0f x %.0f at %.0f, %.0f (expected %.0f x %.0f)\n", now.w, now.h, now.x, now.y,
                expected.w, expected.h);
    Check(now.known && NearPixels(now.w, expected.w) && NearPixels(now.h, expected.h) &&
              NearPixels(now.x, expected.x) && NearPixels(now.y, expected.y),
          what);
}

#include "authored_fog_checks.h"
#include "fog_integration_checks.h"
#include "local_lights_checks.h"
#include "local_light_stabiliser_checks.h"
#include "spell_light_checks.h"
#include "client_light_stay_checks.h"
#include "camera_focus_checks.h"
#include "client_game_clock_checks.h"
#include "noise_variation_checks.h"
#include "classic_phase_checks.h"
#include "derived_fog_hue_checks.h"
#include "level_ray_fog_checks.h"
#include "march_layer_checks.h"
#include "local_light_gpu_checks.h"
#include "silhouette_quality_checks.h"
#include "authored_noise_checks.h"
#include "grazing_upsample_checks.h"
#include "glow_upsample_checks.h"
#include "god_ray_quality_checks.h"
#include "god_ray_blur_checks.h"
#include "temporal_quality_checks.h"
#include "runtime_quality_checks.h"
#include "god_ray_shaft_checks.h"
#include "lighting_history_checks.h"
#include "runtime_cost_checks.h"
#include "resource_retry_checks.h"
#include "water_data_checks.h"
#include "water_settings_checks.h"
#include "water_fft_checks.h"
#include "water_mask_packing_checks.h"
#include "terrain_shadow_reference.h"
#include "terrain_shadow_checks.h"
#include "shadow_light_checks.h"
#include "water_contact_checks.h"
#include "nearby_unit_checks.h"
#include "water_ripple_checks.h"
#include "water_checks.h"
#include "water_ripple_pass_checks.h"
#include "water_wake_checks.h"
#include "ripple_scene.h"
#include "client_sprite_checks.h"
#include "display_dither_checks.h"
#include "forever_look_checks.h"
#include "transparent_fog_checks.h"
#include "early_composite_look_checks.h"
#include "multisampling_checks.h"
#include "device_creation_checks.h"

constexpr unsigned long kSecondClientProcessId = 4242;
constexpr int kLastLogSlot = 8;

std::string WrittenAndClosed(LogFile& log, const char* line)
{
    if (!log.file)
        return std::string();
    std::fputs(line, log.file);
    std::fclose(log.file);
    log.file = nullptr;
    return line;
}

std::string WrittenAndKeptOpen(LogFile& log, const char* line)
{
    if (!log.file)
        return std::string();
    std::fputs(line, log.file);
    std::fflush(log.file);
    return line;
}

void CloseLogs(std::vector<LogFile>& logs)
{
    for (LogFile& log : logs)
        if (log.file)
            std::fclose(log.file);
    logs.clear();
}

std::vector<std::wstring> LogFilesIn(const std::wstring& folder)
{
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW found = {};
    const HANDLE search = FindFirstFileW((folder + L"\\*.log").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE)
        return names;
    do
        names.push_back(found.cFileName);
    while (FindNextFileW(search, &found));
    FindClose(search);
    std::sort(names.begin(), names.end());
    return names;
}

std::wstring EmptyLogFolder(const std::wstring& outDir, const wchar_t* name)
{
    const std::wstring folder = FullPath(outDir + L"\\" + name);
    CreateDirectoryW(folder.c_str(), nullptr);
    for (const std::wstring& log : LogFilesIn(folder))
        DeleteFileW((folder + L"\\" + log).c_str());
    return folder;
}

std::wstring SlotLog(const std::wstring& folder, int slot)
{
    return folder + L"\\CoAGraphics-" + std::to_wstring(slot) + L".log";
}

void CheckSecondClientWritesTheSecondSlot(const std::wstring& outDir)
{
    const std::wstring folder = EmptyLogFolder(outDir, L"two-clients");
    const std::wstring shared = folder + L"\\CoAGraphics.log";
    const std::wstring own = SlotLog(folder, 2);
    LogFile first = OpenLogFile(NarrowPath(shared));
    LogFile second = OpenLogFile(NarrowPath(shared));
    const std::string firstLine = WrittenAndClosed(first, "first client");
    const std::string secondLine = WrittenAndClosed(second, "second client");
    Check(first.path == NarrowPath(shared) && second.path == NarrowPath(own) && !firstLine.empty() &&
              ReadText(shared) == firstLine && !secondLine.empty() && ReadText(own) == secondLine,
          "a second client started from the same folder writes CoAGraphics-2.log and leaves the first client's "
          "CoAGraphics.log intact");
}

void CheckHeldSlotIsSkippedUntouched(const std::wstring& outDir)
{
    const std::wstring folder = EmptyLogFolder(outDir, L"three-clients");
    const std::string shared = NarrowPath(folder + L"\\CoAGraphics.log");
    std::vector<LogFile> clients = {OpenLogFile(shared), OpenLogFile(shared)};
    const std::string secondLine = WrittenAndKeptOpen(clients[1], "second client");
    LogFile third = OpenLogFile(shared);
    const std::string thirdLine = WrittenAndClosed(third, "third client");
    CloseLogs(clients);
    Check(!secondLine.empty() && ReadText(SlotLog(folder, 2)) == secondLine &&
              third.path == NarrowPath(SlotLog(folder, 3)) && !thirdLine.empty() &&
              ReadText(SlotLog(folder, 3)) == thirdLine,
          "a third client skips the second client's held CoAGraphics-2.log without truncating it and writes "
          "CoAGraphics-3.log");
}

void CheckFreedSlotIsReused(const std::wstring& outDir)
{
    const std::wstring folder = EmptyLogFolder(outDir, L"reused-slot");
    const std::string shared = NarrowPath(folder + L"\\CoAGraphics.log");
    std::vector<LogFile> running = {OpenLogFile(shared)};
    LogFile exited = OpenLogFile(shared);
    WrittenAndClosed(exited, "exited client");
    LogFile next = OpenLogFile(shared);
    const std::string nextLine = WrittenAndClosed(next, "next client");
    CloseLogs(running);
    const std::vector<std::wstring> expected = {L"CoAGraphics-2.log", L"CoAGraphics.log"};
    Check(exited.path == NarrowPath(SlotLog(folder, 2)) && next.path == exited.path && !nextLine.empty() &&
              ReadText(SlotLog(folder, 2)) == nextLine && LogFilesIn(folder) == expected,
          "a client started after another has exited reuses and truncates its CoAGraphics-2.log, so no new log "
          "file appears");
}

void CheckNoLogWhenEverySlotIsHeld(const std::wstring& outDir)
{
    const std::wstring folder = EmptyLogFolder(outDir, L"every-slot-held");
    const std::string shared = NarrowPath(folder + L"\\CoAGraphics.log");
    std::vector<LogFile> holders;
    for (int client = 1; client <= kLastLogSlot; ++client)
        holders.push_back(OpenLogFile(shared));
    const bool everyHolderLogs =
        std::all_of(holders.begin(), holders.end(), [](const LogFile& log) { return log.file != nullptr; });
    LogFile ninth = OpenLogFile(shared);
    const bool ninthLogs = ninth.file != nullptr;
    WrittenAndClosed(ninth, "ninth client");
    CloseLogs(holders);
    const size_t files = LogFilesIn(folder).size();
    std::printf("     %zu log files with CoAGraphics.log and every slot held\n", files);
    Check(everyHolderLogs && !ninthLogs && files == static_cast<size_t>(kLastLogSlot),
          "with CoAGraphics.log and CoAGraphics-2.log to CoAGraphics-8.log held, a ninth client writes no log and "
          "adds no file");
}

void CheckHeldLogNoticeNamesTheSlotAndProcess()
{
    const std::string notice =
        HeldLogNotice("C:\\WoW\\CoAGraphics.log", "C:\\WoW\\CoAGraphics-2.log", kSecondClientProcessId);
    std::printf("     %s\n", notice.c_str());
    Check(notice.find("C:\\WoW\\CoAGraphics.log") == 0 &&
              notice.find("C:\\WoW\\CoAGraphics-2.log") != std::string::npos &&
              notice.find("process " + std::to_string(kSecondClientProcessId)) != std::string::npos,
          "the first line of a slot log names the held CoAGraphics.log, the slot file and the client's process id");
}

void CheckLogRefusesASecondWriter()
{
    FILE* second = _wfsopen(runtime_cost::FogLogBesideTheFogDll().c_str(), L"r+", _SH_DENYNO);
    if (second)
        std::fclose(second);
    Check(!second, "the DLL's log cannot be opened for writing by a second client while the DLL writes it");
}

#include "texture_filtering_checks.h"
#include "tree_wind_checks.h"
#include "wind_model_checks.h"
#include "grass_wind_checks.h"
#include "grass_doodad_checks.h"
#include "grass_density_checks.h"
#include "snow_checks.h"
#include "image_quality_checks.h"
#include "surface_light_area_checks.h"
#include "surface_light_checks.h"
#include "target_arrow_checks.h"

void CheckFirstColumnIsFogged(const Image& transmittance, const D3DVIEWPORT9& world)
{
    int largest = 0;
    for (UINT y = world.Y; y < world.Y + world.Height / kSkyRowsFraction; ++y)
        for (int c = 0; c < 3; ++c)
            largest = std::max(largest, std::abs(static_cast<int>(transmittance.At(world.X, y)[c]) -
                                                 transmittance.At(world.X + 1, y)[c]));
    std::printf("     transmittance view over the sky: largest difference between the viewport's first two columns "
                "%d/255\n",
                largest);
    Check(largest <= kMaxAdjacentSkyColumnDifference,
          "the full-screen fog passes cover the viewport's first column (their triangle's left edge lies outside it)");
}

void CheckUpsamplePathView(const Image& paths, const D3DVIEWPORT9& world, UINT pixelsPerLowResTexel)
{
    size_t matchingTaps = 0;
    size_t betweenTaps = 0;
    size_t marchedAgain = 0;
    size_t ownTapsOnAnotherPath = 0;
    for (UINT y = world.Y; y < world.Y + world.Height; ++y)
    {
        for (UINT x = world.X; x < world.X + world.Width; ++x)
        {
            const unsigned char* bgr = paths.At(x, y);
            const bool matching = bgr[0] == 255 && bgr[1] == 0 && bgr[2] == 0;
            matchingTaps += matching ? 1 : 0;
            betweenTaps += bgr[0] == 0 && bgr[1] == 255 && bgr[2] == 0 ? 1 : 0;
            marchedAgain += bgr[0] == 0 && bgr[1] == 0 && bgr[2] == 255 ? 1 : 0;
            const bool ownTap = (x - world.X) % pixelsPerLowResTexel == pixelsPerLowResTexel / 2 &&
                                (y - world.Y) % pixelsPerLowResTexel == pixelsPerLowResTexel / 2;
            ownTapsOnAnotherPath += ownTap && !matching ? 1 : 0;
        }
    }
    const size_t pixels = static_cast<size_t>(world.Width) * world.Height;
    std::printf("     upsample path view: %zu matching taps, %zu between taps, %zu marched again of %zu pixels; %zu "
                "pixels a low-resolution texel samples took another path\n",
                matchingTaps, betweenTaps, marchedAgain, pixels, ownTapsOnAnotherPath);
    Check(matchingTaps + betweenTaps + marchedAgain == pixels,
          "DebugView=4 paints every world pixel as one upsample path: blue matching taps, green between taps or red "
          "marched again");
    Check(ownTapsOnAnotherPath == 0, "a pixel a low-resolution texel samples takes that texel's fog");
    Check(marchedAgain > 0 && marchedAgain < pixels / 4,
          "the scene's silhouettes march again and most of its pixels do not");
}

struct CompositeDrawn
{
    Image image;
    unsigned passes = 0;
};

void CheckUnmatchedPixelsDrawnApart(const CompositeDrawn& apart, const CompositeDrawn& together,
                                    const Image& upsamplePaths, const D3DVIEWPORT9& world)
{
    size_t differing = 0;
    size_t marchedAgain = 0;
    for (UINT y = world.Y; y < world.Y + world.Height; ++y)
    {
        for (UINT x = world.X; x < world.X + world.Width; ++x)
        {
            differing += std::memcmp(apart.image.At(x, y), together.image.At(x, y), 3) != 0 ? 1 : 0;
            marchedAgain += upsamplePaths.At(x, y)[2] == 255 ? 1 : 0;
        }
    }
    std::printf("     composite passes: %u by default, %u when held to one; %zu pixels differ, %zu march again\n",
                apart.passes, together.passes, differing, marchedAgain);
    Check(apart.passes == 2 && together.passes == 1,
          "on a 1x device the composite draws the pixels without matching low-resolution fog in a second pass");
    Check(differing == 0 && marchedAgain > 0,
          "the composite that marches its unmatched pixels in a pass of their own draws the one-pass image");
}

void CheckDisabledTemporalIsStable(Harness& h, const Config& cfg, Vec3 eye, Vec3 at,
                                   const float* proj, const D3DVIEWPORT9& world)
{
    Config stable = cfg;
    stable.temporal = 0.0f;
    stable.godRays = 0.0f;
    stable.debugView = 2;
    stable.dataMode = 0;
    stable.quality = 1;
    vf_test_set_config(&stable);
    float view[16];
    CameraRelativeLookAt(eye, at, view);
    FrameInputs in = MakeInputs(view, proj, eye, at, world);
    Image captures[2];
    bool rendered = true;
    for (int frame = 0; frame < 2; ++frame)
    {
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        const char* skip = "";
        rendered = vf_test_render(&in, &skip) != 0 && rendered;
        captures[frame] = Capture(h.dev);
        h.dev->EndScene();
    }
    Check(rendered && captures[0].bgra == captures[1].bgra,
          "disabling temporal accumulation keeps stationary fog identical between frames");
    vf_test_set_config(&cfg);
}

void CheckBypassesAreWarnedOnce(Harness& h)
{
    const size_t start = water_settings_checks::DllLogSize();
    for (int call = 0; call < 2; ++call)
    {
        IDirect3DSwapChain9* chain = nullptr;
        if (SUCCEEDED(h.dev->GetSwapChain(0, &chain)) && chain)
            chain->Release();
        void* other = nullptr;
        if (SUCCEEDED(h.dev->QueryInterface(__uuidof(IDirect3DDevice9Ex), &other)) && other)
            static_cast<IUnknown*>(other)->Release();
    }
    const std::string text = runtime_cost::LogWrittenSince(start);
    Check(water_settings_checks::CountOf(text, "GetSwapChain: presenting through the swap chain bypasses") == 1 &&
              water_settings_checks::CountOf(text, "QueryInterface for an interface the wrapper does not implement") ==
                  1,
          "a swap chain taken from the device and a foreign interface query each log one warning that the wrapper "
          "is bypassed");
}

void CheckVoidBelowTheHorizonMirrorsTheSky(Harness& h, const Config& cfg, Vec3 eye, const float* proj,
                                           const D3DVIEWPORT9& world)
{
    Config transmittanceView = cfg;
    transmittanceView.temporal = 0.0f;
    transmittanceView.godRays = 0.0f;
    transmittanceView.debugView = 2;
    transmittanceView.dataMode = 0;
    transmittanceView.noiseAmount = 0.0f;
    vf_test_set_config(&transmittanceView);
    const Vec3 level = Add(eye, {100.0f, 0.0f, 0.0f});
    float view[16];
    CameraRelativeLookAt(eye, level, view);
    FrameInputs in = MakeInputs(view, proj, eye, level, world);
    h.BeginFrame();
    const char* skip = "";
    const bool rendered = vf_test_render(&in, &skip) != 0;
    const Image t = Capture(h.dev);
    h.dev->EndScene();
    vf_test_set_config(&cfg);
    const UINT horizonRow = world.Y + world.Height / 2;
    int worst = 0;
    for (UINT rows : {kVoidProbeRows, 2 * kVoidProbeRows})
        worst = std::max(worst, std::abs(static_cast<int>(t.At(world.Width / 2, horizonRow - rows)[2]) -
                                         t.At(world.Width / 2, horizonRow + rows)[2]));
    std::printf("     no geometry in view: sky transmittance %d/255 %u rows above the horizon, %d/255 below; largest "
                "mirror difference %d/255\n",
                t.At(world.Width / 2, horizonRow - kVoidProbeRows)[2], kVoidProbeRows,
                t.At(world.Width / 2, horizonRow + kVoidProbeRows)[2], worst);
    Check(rendered && worst <= kMaxVoidMirrorDifference,
          "sky below the horizon, such as the void under Netherstorm's edges, thins the fog as the sky above does "
          "instead of a level ray through the whole fog range");
}

int Run(const std::wstring& outDir, const std::string& dataPath, const std::wstring& iniPath,
        const std::string& waterDataPath, bool cpuOnly = false)
{
    Check(vf_test_water_data_loaded() == 0,
          "outside the supported client (no water hooks) the DLL does not load waterdata.bin");
    FogData classic;
    Check(classic.Load(dataPath), "Classic fog data loads");
    RUN_GROUP(CheckClassicData(classic));
    RUN_GROUP(CheckOutlandAndNorthrendHaveTheirOwnLights(classic));
    RUN_GROUP(CheckRegionalFogSettings());
    RUN_GROUP(CheckOutlandDensityThinsTheConvertedFog(classic));
    RUN_GROUP(CheckOutlandSkyColourTintsTheConvertedFog(classic));
    RUN_GROUP(CheckLayerSlidersScaleTheClassicLayers(classic));
    RUN_GROUP(CheckConvertedGroundFogFollowsTheHeightOfItsLight(classic));
    RUN_GROUP(CheckConvertedLightsCarryNoClassicNoise(classic));
    RUN_GROUP(authored_noise::CheckClassicNoiseOffByDefault(FullPath(iniPath)));
    RUN_GROUP(CheckSunlessSkiesScatterNoSunOrMoon(classic));
    RUN_GROUP(CheckGrizzlyHillsSholazarAndBoreanTundraKeepTheSun(classic));
    RUN_GROUP(CheckHellfireKeepsOnlyAThinGroundFogUnderAClearSky(classic));
    RUN_GROUP(CheckMoonScattersOnItsOwn(classic));
    RUN_GROUP(CheckLocalLightsGlowByRegion());
    RUN_GROUP(CheckNegligibleLightsLeaveTheLightLogAlone());
    RUN_GROUP(CheckStormBlendsLayersByClassicIndex(classic));
    RUN_GROUP(CheckScreenEffectLightSlot(classic));
    RUN_GROUP(CheckZoneLights(classic));
    RUN_GROUP(CheckAuthoredFogFollowsClientDirectLight(classic));
    RUN_GROUP(CheckDenseClassicFogAtHarbourSunset(classic));
    RUN_GROUP(CheckThinClassicFogAtHyjalMidnight(classic));
    RUN_GROUP(CheckFogThinsIntoFoglessClassicLight(classic));
    RUN_GROUP(authored_fog::CheckAuthoredFogExtras(classic));
    RUN_GROUP(authored_noise::CheckAuthoredNoise(classic));
    RUN_GROUP(classic_phase::CheckClassicPhase());
    RUN_GROUP(derived_fog_hue::CheckDerivedFogHue());
    RUN_GROUP(level_ray_fog::CheckLevelRayFog());
    RUN_GROUP(transparent_fog_checks::CheckStockFogFit(classic));
    RUN_GROUP(terrain_shadow_checks::CheckTerrainShadows());
    RUN_GROUP(shadow_light_checks::CheckShadowDirection());
    RUN_GROUP(shadow_light_checks::CheckShadowMapWeightFades());
    RUN_GROUP(shadow_light_checks::CheckSunShadowPlan());
    RUN_GROUP(resource_retry::CheckRetryGate());

    CreateDirectoryW(outDir.c_str(), nullptr);
    g_harnessLog = FullPath(outDir + L"\\harness.log");
    LogOpen(NarrowPath(g_harnessLog).c_str());
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    RUN_GROUP(CheckOverlayKeyNames());
    RUN_GROUP(CheckFrameCopySummary());
    RUN_GROUP(CheckIniSectionMatchesProfileApi(outDir, FullPath(iniPath)));
    RUN_GROUP(CheckShippedIniMatchesSettingsTable(FullPath(iniPath)));
    RUN_GROUP(CheckRegionLocalLightsSetting(outDir, FullPath(iniPath)));
    RUN_GROUP(CheckSettingsSaveKeepsTheIni(outDir, FullPath(iniPath)));
    RUN_GROUP(CheckFogDataBounds(outDir, dataPath));
    RUN_GROUP(forever_look_checks::CheckForeverGlow());
    RUN_GROUP(forever_look_checks::CheckLookSettings(outDir, FullPath(iniPath)));
    RUN_GROUP(water_data_checks::CheckWaterData(outDir, waterDataPath));
    RUN_GROUP(water_settings_checks::CheckWaterSettings(outDir, FullPath(iniPath)));
    RUN_GROUP(transparent_fog_checks::CheckTransparentFogSetting(outDir, FullPath(iniPath)));
    RUN_GROUP(texture_filtering_checks::CheckForcedSamplerFilter());
    RUN_GROUP(texture_filtering_checks::CheckShippedSetting(FullPath(iniPath)));
    RUN_GROUP(image_quality_checks::CheckShippedSetting(FullPath(iniPath)));
    RUN_GROUP(surface_light_checks::CheckShippedSetting(FullPath(iniPath)));
    RUN_GROUP(tree_wind_checks::CheckShippedSettings(FullPath(iniPath)));
    RUN_GROUP(grass_wind_checks::CheckShippedSettings(FullPath(iniPath)));

    if (cpuOnly)
    {
        RUN_GROUP(CheckLocalLightInputs());
        RUN_GROUP(CheckLocalLightReachFollowsRegion());
        RUN_GROUP(CheckPointLightTableCapture());
        RUN_GROUP(CheckLightsFollowTheirObjects());
        RUN_GROUP(CheckInteriorInputsSurviveAnIncompleteWalk());
        RUN_GROUP(CheckHdrLocalLightColours());
        RUN_GROUP(local_light_stabiliser_checks::CheckLocalLightStabiliser());
        RUN_GROUP(spell_light_checks::CheckSpellLights());
        RUN_GROUP(target_arrow_checks::CheckTargetArrow());
        RUN_GROUP(client_light_stay_checks::CheckClientLightStay());
        RUN_GROUP(camera_focus_checks::CheckTheFocusFollowsTheClientStores());
        RUN_GROUP(camera_focus_checks::CheckTheCharacterBoxFollowsTheFocus());
        RUN_GROUP(CheckLightProjectsOntoTheViewport());
        RUN_GROUP(surface_light_area_checks::CheckLampSpheresProjectOntoTheView());
        RUN_GROUP(surface_light_area_checks::CheckNoLitPointLiesOutsideTheArea());
        RUN_GROUP(client_game_clock_checks::CheckGameClock());
        RUN_GROUP(tree_wind_checks::CheckTreeWind());
        RUN_GROUP(wind_model_checks::CheckWindModel());
        RUN_GROUP(nearby_unit_checks::CheckNearbyUnits());
        RUN_GROUP(grass_wind_checks::CheckGrassWind());
        RUN_GROUP(grass_doodad_checks::CheckGrassDoodads());
        RUN_GROUP(grass_density_checks::CheckGrassDensity(outDir, FullPath(iniPath)));
        RUN_GROUP(snow_checks::CheckSnow());
        RUN_GROUP(CheckInteriorFogInputs());
        RUN_GROUP(water_contact_checks::CheckWaterContacts());
        CoUninitialize();
        std::printf("CPU checks %s (%d failures); D3D9 graphics checks were not run\n",
                    g_failures ? "FAILED" : "OK", g_failures);
        return g_failures ? 1 : 0;
    }

    WNDCLASSW wc = {};
    wc.lpfnWndProc = ClientWindowProc;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"vfog_harness";
    RegisterClassW(&wc);
    Harness h;
    h.window = CreateWindowW(L"vfog_harness", L"vfog", WS_OVERLAPPEDWINDOW, 0, 0, 1280, 720, nullptr, nullptr,
                             wc.hInstance, nullptr);

    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    auto realCreate = reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(d3d9, "Direct3DCreate9"));
    h.d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    Check(h.d3d != nullptr, "wrapped Direct3DCreate9");
    if (!h.d3d)
        return 1;

    Config withoutMultisampling = {};
    withoutMultisampling.multisampling = false;
    vf_test_set_config(&withoutMultisampling);
    D3DMULTISAMPLE_TYPE ms = D3DMULTISAMPLE_4_SAMPLES;
    Check(h.d3d->CheckDeviceMultiSampleType(0, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, TRUE, ms, nullptr) ==
              D3DERR_NOTAVAILABLE,
          "with Multisampling=0 multisampling is reported unavailable while fog is enabled");

    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = 1280;
    h.pp.BackBufferHeight = 720;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kClientDepthFormat;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    DWORD engineFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    Check(engineFlags == 0x52, "harness uses the client's device flags");
    Config startup = {};
    vf_test_set_config(&startup);
    const bool loadedInDllMain = vf_test_fog_data_loaded() != 0;
    HRESULT hr = h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, engineFlags, &h.pp, &h.dev);
    Check(SUCCEEDED(hr) && h.dev, "CreateDevice through the wrapper");
    Check(!loadedInDllMain && vf_test_fog_data_loaded() != 0,
          "the fog data is read at the first CreateDevice, not under the loader lock in DllMain");
    if (!h.dev)
        return 1;
    RUN_GROUP(CheckBypassesAreWarnedOnce(h));

    D3DDEVICE_CREATION_PARAMETERS cp;
    h.dev->GetCreationParameters(&cp);
    Check((cp.BehaviorFlags & D3DCREATE_PUREDEVICE) == 0, "pure-device flag removed");
    D3DADAPTER_IDENTIFIER9 adapter = {};
    D3DCAPS9 caps = {};
    h.d3d->GetAdapterIdentifier(0, 0, &adapter);
    h.dev->GetDeviceCaps(&caps);
    std::printf("     adapter %s, PS3 slots %lu, executed instructions %lu\n", adapter.Description,
                caps.MaxPixelShader30InstructionSlots, caps.MaxPShaderInstructionsExecuted);
    Check(h.pp.EnableAutoDepthStencil == TRUE && h.pp.AutoDepthStencilFormat == kClientDepthFormat,
          "engine-visible depth parameters preserved");
    IDirect3DSurface9* depth = nullptr;
    h.dev->GetDepthStencilSurface(&depth);
    D3DSURFACE_DESC depthDesc = {};
    if (depth)
        depth->GetDesc(&depthDesc);
    Check(depth && depthDesc.Format == kIntz && depthDesc.Width == 1280, "INTZ depth bound as the device depth");
    if (depth)
        depth->Release();
    IDirect3D9* parent = nullptr;
    h.dev->GetDirect3D(&parent);
    Check(parent == h.d3d, "GetDirect3D returns the wrapper");
    if (parent)
        parent->Release();

    h.CreateEngineObjects();
    RUN_GROUP(texture_filtering_checks::CheckForcedAnisotropyOnTheDevice(h.dev));
    RUN_GROUP(CheckFogIntegration(h.dev));
    RUN_GROUP(CheckLocalLightInputs());
    RUN_GROUP(CheckLocalLightReachFollowsRegion());
    RUN_GROUP(CheckPointLightTableCapture());
    RUN_GROUP(CheckLightsFollowTheirObjects());
    RUN_GROUP(CheckInteriorInputsSurviveAnIncompleteWalk());
    RUN_GROUP(CheckHdrLocalLightColours());
    RUN_GROUP(local_light_stabiliser_checks::CheckLocalLightStabiliser());
    RUN_GROUP(spell_light_checks::CheckSpellLights());
    RUN_GROUP(spell_light_checks::CheckTextureTintOnTheDevice(h.dev));
    RUN_GROUP(target_arrow_checks::CheckTargetArrow());
    RUN_GROUP(target_arrow_checks::CheckTargetArrowOnTheDevice(h.dev));
    RUN_GROUP(client_light_stay_checks::CheckClientLightStay());
    RUN_GROUP(camera_focus_checks::CheckTheFocusFollowsTheClientStores());
    RUN_GROUP(camera_focus_checks::CheckTheCharacterBoxFollowsTheFocus());
    RUN_GROUP(CheckLightProjectsOntoTheViewport());
    RUN_GROUP(surface_light_area_checks::CheckLampSpheresProjectOntoTheView());
    RUN_GROUP(surface_light_area_checks::CheckNoLitPointLiesOutsideTheArea());
    RUN_GROUP(client_game_clock_checks::CheckGameClock());
    RUN_GROUP(tree_wind_checks::CheckTreeWind());
    RUN_GROUP(wind_model_checks::CheckWindModel());
    RUN_GROUP(nearby_unit_checks::CheckNearbyUnits());
    RUN_GROUP(grass_wind_checks::CheckGrassWind());
    RUN_GROUP(grass_doodad_checks::CheckGrassDoodads());
    RUN_GROUP(grass_density_checks::CheckGrassDensity(outDir, FullPath(iniPath)));
    RUN_GROUP(grass_wind_checks::CheckShadersOnTheDevice(h.dev));
    RUN_GROUP(grass_wind_checks::CheckMotionOnTheDevice(h.dev));
    RUN_GROUP(grass_wind_checks::CheckBendOnTheDevice(h.dev));
    RUN_GROUP(grass_wind_checks::CheckCopiesOnTheDevice(h.dev));
    RUN_GROUP(grass_wind_checks::CheckSinkOnTheDevice(h.dev));
    RUN_GROUP(grass_wind_checks::CheckAlphaOnTheDevice(h.dev));
    RUN_GROUP(snow_checks::CheckSnow());
    RUN_GROUP(snow_checks::CheckLiftOnTheDevice(h.dev));
    RUN_GROUP(snow_checks::CheckTrenchShadingOnTheDevice(h.dev));
    RUN_GROUP(snow_checks::CheckTrenchDepthOnTheDevice(h.dev));
    RUN_GROUP(snow_checks::CheckSnowStaysBoundOnTheDevice(h.dev));
    RUN_GROUP(CheckInteriorFogInputs());
    RUN_GROUP(CheckNoiseVariation(h.dev));
    RUN_GROUP(authored_noise::CheckAuthoredNoiseOnTheGpu(h.dev));
    RUN_GROUP(classic_phase::CheckIsotropicSlabSaturation(h.dev));
    RUN_GROUP(march_layers::CheckUnrolledMarchMatchesLoopedMarch(h.dev));
    RUN_GROUP(march_layers::CheckNoisyMarchMatchesLitNoisyMarch(h.dev, authored_noise::kNoisyMarches,
                                                                authored_noise::kNoisyMarches + 3));
    RUN_GROUP(march_layers::CheckStoppedMarchMatchesUnstoppedMarch(h.dev));
    RUN_GROUP(local_light_gpu::CheckLocalLightIntegration(h.dev));
    RUN_GROUP(silhouette_quality::CheckSilhouettes(h.dev));
    RUN_GROUP(grazing_upsample::CheckGrazingGroundUpsample(h.dev));
    RUN_GROUP(glow_upsample::CheckGlowBetweenGrazingTaps(h.dev));
    RUN_GROUP(god_ray_quality::CheckGodRays(h.dev));
    RUN_GROUP(god_ray_blur_checks::CheckGodRayBlur(h.dev));
    RUN_GROUP(CheckTemporalQuality(h.dev));
    RUN_GROUP(CheckTemporalFallbackNoise(h.dev));
    RUN_GROUP(CheckLightDisappearanceHistory(h));
    RUN_GROUP(CheckSunOcclusion(h));
    RUN_GROUP(god_ray_shafts::CheckGodRaysAddOnlyShafts(h, outDir));
    RUN_GROUP(god_ray_shafts::CheckTheOwnCharacterCastsNoShafts(h, outDir));
    RUN_GROUP(god_ray_shafts::CheckTrunksBesideTheOwnCharacterKeepTheirShafts(h, outDir));
    RUN_GROUP(display_dither::CheckDisplayDither(h.dev));
    RUN_GROUP(classic_phase::CheckSunsetHaloKeepsItsHue(h, FullPath(iniPath)));
    RUN_GROUP(water_fft_checks::CheckWaterFft(h.dev));
    RUN_GROUP(water_mask_packing_checks::CheckWaveFoamMaskPacking(h.dev));
    RUN_GROUP(water_contact_checks::CheckWaterContacts());
    RUN_GROUP(water_ripple_checks::CheckWaterRipples(h.dev));
    const float aspect = 1280.0f / 688.0f;
    const D3DVIEWPORT9 world = {0, 0, 1280, 688, 0.0f, 1.0f};
    float proj[16];
    EngineProjection(aspect, proj);
    Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    Vec3 at = Add({100, 2, 4}, kGameLikeWorldOffset);
    float view[16];

    Config cfg = {};
    cfg.noiseAmount = 0.0f;
    cfg.maxDistance = 5000.0f;
    Config withGodRays = cfg;
    withGodRays.godRays = 0.2f;
    vf_test_set_config(&withGodRays);

    Image before;
    Image after;
    bool allRendered = true;
    bool statesKept = true;
    const char* skip = "";
    for (int frame = 0; frame < 12; ++frame)
    {
        Vec3 e = {eye.x + frame * 0.6f, eye.y, eye.z};
        CameraRelativeLookAt(e, at, view);
        h.BeginFrame();
        h.DrawScene(e, view, proj, world);
        if (frame == 11)
            before = Capture(h.dev);
        h.SetEngineState(world);
        Sentinel s0;
        ReadSentinel(h.dev, s0);
        FrameInputs in = MakeInputs(view, proj, e, at, world);
        allRendered = vf_test_render(&in, &skip) != 0 && allRendered;
        Sentinel s1;
        ReadSentinel(h.dev, s1);
        if (!SameSentinel(s0, s1))
        {
            if (statesKept)
                ReportSentinelDifferences(s0, s1);
            statesKept = false;
        }
        ReleaseSentinel(s0);
        ReleaseSentinel(s1);
        if (frame == 11)
            after = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    }
    Check(allRendered, (std::string("fog rendered on every frame ") + skip).c_str());
    Check(statesKept, "render, sampler, shader, constant, stream, viewport, scissor and target state restored");

    bool outsideKept = true;
    for (UINT y = 688; y < 720 && outsideKept; ++y)
        for (UINT x = 0; x < 1280; ++x)
            if (std::memcmp(before.At(x, y), after.At(x, y), 3) != 0)
            {
                outsideKept = false;
                break;
            }
    Check(outsideKept, "pixels outside the world viewport untouched");

    double diff = 0;
    for (UINT y = 0; y < 688; ++y)
        for (UINT x = 0; x < 1280; ++x)
            diff += std::fabs(after.Luma(x, y) - before.Luma(x, y));
    diff /= 1280.0 * 688.0;
    std::printf("     mean luma change inside the world viewport: %.4f\n", diff);
    Check(diff > 0.01 && diff < 0.5, "fog visibly changes the world viewport");

    float sunDir[3];
    {
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        const float* v = view;
        float s[3] = {in.toLight[0] * v[0] + in.toLight[1] * v[4] + in.toLight[2] * v[8],
                      in.toLight[0] * v[1] + in.toLight[1] * v[5] + in.toLight[2] * v[9],
                      in.toLight[0] * v[2] + in.toLight[1] * v[6] + in.toLight[2] * v[10]};
        std::memcpy(sunDir, s, sizeof(s));
    }
    UINT sunX = static_cast<UINT>((sunDir[0] / sunDir[2] * proj[0] * 0.5f + 0.5f) * 1280.0f);
    UINT sunY = static_cast<UINT>((0.5f - sunDir[1] / sunDir[2] * proj[5] * 0.5f) * 688.0f);
    UINT awayX = sunX > 640 ? 60 : 1220;
    float haloGain = after.Luma(sunX, sunY - 40) - before.Luma(sunX, sunY - 40);
    float awayGain = after.Luma(awayX, sunY - 40) - before.Luma(awayX, sunY - 40);
    std::printf("     sky luma change near the sun %.3f, away from the sun %.3f (sun at %u,%u)\n", haloGain, awayGain,
                sunX, sunY);
    Check(haloGain > awayGain, "forward scattering brightens the sky around the sun");

    SavePng(outDir + L"\\before.png", before.w, before.h, before.bgra);
    SavePng(outDir + L"\\after.png", after.w, after.h, after.bgra);

    RUN_GROUP(CheckLinearComposite(h, cfg, eye, at, proj, world));
    RUN_GROUP(CheckDisabledTemporalIsStable(h, cfg, eye, at, proj, world));
    RUN_GROUP(CheckVoidBelowTheHorizonMirrorsTheSky(h, cfg, eye, proj, world));
    RUN_GROUP(forever_look_checks::CheckColourGrading(h, world, classic));
    RUN_GROUP(image_quality_checks::CheckSharpening(h, world));
    RUN_GROUP(image_quality_checks::CheckAntialiasing(h, world));
    RUN_GROUP(image_quality_checks::CheckBloom(h, world));
    RUN_GROUP(image_quality_checks::CheckImageFaultRestoresTheClient(h, world));
    RUN_GROUP(forever_look_checks::CheckFogCompensatesTheDeliveredGlow(h, cfg, world));
    vf_test_set_config(&cfg);

    auto renderDebugIn = [&](int mode, float maxDist, const D3DVIEWPORT9& vp, float wdlPatchRawDepth) {
        Config c = cfg;
        c.debugView = mode;
        c.maxDistance = maxDist;
        c.temporal = 0.0f;
        vf_test_set_config(&c);
        CameraRelativeLookAt(eye, at, view);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, vp);
        if (wdlPatchRawDepth > kNoWdlPatch)
            h.DrawDistantTerrainPatch(wdlPatchRawDepth);
        FrameInputs in = MakeInputs(view, proj, eye, at, vp);
        in.farClip = std::fmin(in.farClip, maxDist);
        vf_test_render(&in, &skip);
        Image img = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
        return img;
    };
    auto renderDebug = [&](int mode, float maxDist) { return renderDebugIn(mode, maxDist, world, kNoWdlPatch); };

    Image radiance = renderDebug(1, 5000.0f);
    SavePng(outDir + L"\\debug-radiance.png", radiance.w, radiance.h, radiance.bgra);
    Image transmittance = renderDebug(2, 5000.0f);
    SavePng(outDir + L"\\debug-transmittance.png", transmittance.w, transmittance.h, transmittance.bgra);
    RUN_GROUP(CheckFirstColumnIsFogged(transmittance, world));
    Image upsamplePaths = renderDebug(4, 5000.0f);
    SavePng(outDir + L"\\debug-upsample-paths.png", upsamplePaths.w, upsamplePaths.h, upsamplePaths.bgra);
    RUN_GROUP(CheckUpsamplePathView(upsamplePaths, world, cfg.quality == 1 ? 4u : 2u));
    {
        const CompositeDrawn apart = {renderDebug(0, 5000.0f), vf_test_composite_passes()};
        vf_test_composite_in_one_pass(1);
        const CompositeDrawn together = {renderDebug(0, 5000.0f), vf_test_composite_passes()};
        vf_test_composite_in_one_pass(0);
        RUN_GROUP(CheckUnmatchedPixelsDrawnApart(apart, together, upsamplePaths, world));
    }

    {
        Config c = cfg;
        vf_test_set_config(&c);
        Config saved = cfg;
        cfg = c;
        Image t = renderDebug(2, 5000.0f);
        cfg = saved;
        CameraRelativeLookAt(eye, at, view);
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        const UINT samples[3][2] = {{101, 101}, {1181, 101}, {101, 201}};
        bool match = true;
        for (const auto& s : samples)
        {
            float reference = ReferenceUnshadowedSkyTransmittance(in, c, nullptr, s[0] + 0.5f, s[1] + 0.5f);
            float got = t.At(s[0], s[1])[2] / 255.0f;
            std::printf("     sky transmittance at %u,%u: shader %.3f, reference %.3f\n", s[0], s[1], got, reference);
            match = match && std::fabs(got - reference) < 0.02f;
        }
        Check(match, "sky transmittance matches the CPU reference (world-space reconstruction)");

        UINT farRow = 0;
        float farZ = 0.0f;
        for (UINT py = 0; py < 688 && !farRow; ++py)
        {
            float ndcY = 1.0f - (py + 0.5f) / 688.0f * 2.0f;
            Vec3 ray = {0.0f, ndcY / proj[5], 1.0f};
            float dz = ray.y * view[9] + ray.z * view[10];
            float z = dz < 0.0f ? -EyeHeightAboveGround(eye) / dz : 1e9f;
            if (z < 0.9f * in.fogEnd)
            {
                farRow = py;
                farZ = z;
            }
        }
        float farT = t.At(640, farRow)[2] / 255.0f;
        std::printf("     ground at %.0f yd (row %u, stock fog end %.0f): transmittance %.3f\n", farZ, farRow,
                    in.fogEnd, farT);
        Check(farRow && farT < 0.15f, "distance fog hides terrain where the stock fog it replaces turns opaque");
        float highSky = t.At(640, 20)[2] / 255.0f;
        std::printf("     sky near the top of the view: transmittance %.3f\n", highSky);
        Check(highSky > 0.5f, "distance fog leaves the upper sky visible");
    }

    {
        Config c = cfg;
        c.temporal = 0.0f;
        c.godRays = 0.0f;
        vf_test_set_config(&c);
        CameraRelativeLookAt(eye, at, view);
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        vf_test_render(&in, &skip);
        Image single = Capture(h.dev);
        h.dev->EndScene();
        c.temporal = 0.85f;
        vf_test_set_config(&c);
        Image accumulated;
        for (int frame = 0; frame < 16; ++frame)
        {
            h.BeginFrame();
            h.DrawScene(eye, view, proj, world);
            vf_test_render(&in, &skip);
            if (frame == 15)
                accumulated = Capture(h.dev);
            h.dev->EndScene();
        }
        double diff = 0.0;
        for (UINT y = 0; y < 688; ++y)
            for (UINT x = 0; x < 1280; ++x)
                diff += std::fabs(accumulated.Luma(x, y) - single.Luma(x, y));
        diff /= 1280.0 * 688.0;
        std::printf("     static camera: accumulated vs single-frame mean luma difference %.4f\n", diff);
        Check(diff < 0.01, "temporal accumulation converges on a static camera (reprojection)");
        vf_test_set_config(&cfg);
    }

    {
        Config c = cfg;
        c.godRays = 0.0f;
        c.temporal = 0.0f;
        c.dataMode = 1;
        CameraRelativeLookAt(eye, at, view);
        FrameInputs in = MakeInputs(view, proj, eye, at, world);
        in.mapId = kEasternKingdoms;
        AuthoredFog authored = {};
        bool resolved = classic.Resolve(kEasternKingdoms, in.camPos, in.dayFraction, kClearWeather, authored);
        c.debugView = 2;
        vf_test_set_config(&c);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        vf_test_render(&in, &skip);
        Image t = Capture(h.dev);
        h.dev->EndScene();
        c.debugView = 0;
        vf_test_set_config(&c);
        h.BeginFrame();
        h.DrawScene(eye, view, proj, world);
        vf_test_render(&in, &skip);
        Image classicImage = Capture(h.dev);
        h.dev->EndScene();
        SavePng(outDir + L"\\after-classic.png", classicImage.w, classicImage.h, classicImage.bgra);
        SaveClassicSunJustAboveViewCaptures(h, cfg, eye, in, proj, world, outDir);

        bool match = resolved;
        const UINT samples[3][2] = {{101, 101}, {1181, 101}, {101, 201}};
        for (const auto& s : samples)
        {
            float reference = ReferenceUnshadowedSkyTransmittance(in, c, resolved ? &authored : nullptr,
                                                                  s[0] + 0.5f, s[1] + 0.5f);
            float got = t.At(s[0], s[1])[2] / 255.0f;
            std::printf("     Classic sky transmittance at %u,%u: shader %.3f, reference %.3f\n", s[0], s[1], got,
                        reference);
            match = match && std::fabs(got - reference) < 0.02f;
        }
        Check(match, "Classic-layer sky transmittance matches the CPU reference");
        vf_test_set_config(&cfg);
    }

    Image depthView = renderDebug(3, 255.0f);
    SavePng(outDir + L"\\debug-depth.png", depthView.w, depthView.h, depthView.bgra);
    {
        const UINT centreColumn = 640;
        const UINT groundRowNearBottom = 660;
        float ndcY = 1.0f - (groundRowNearBottom + 0.5f) / 688.0f * 2.0f;
        float ndcX = (centreColumn + 0.5f) / 1280.0f * 2.0f - 1.0f;
        Vec3 ray = {ndcX / proj[0], ndcY / proj[5], 1.0f};
        CameraRelativeLookAt(eye, at, view);
        Vec3 dirW = {ray.x * view[0] + ray.y * view[1] + ray.z * view[2],
                     ray.x * view[4] + ray.y * view[5] + ray.z * view[6],
                     ray.x * view[8] + ray.y * view[9] + ray.z * view[10]};
        float groundViewZ = -EyeHeightAboveGround(eye) / dirW.z;
        float expected = std::fmin(groundViewZ / 255.0f, 1.0f);
        float got = depthView.At(centreColumn, groundRowNearBottom)[2] / 255.0f;
        std::printf("     ground view depth: expected %.2f yd, shader %.2f yd\n", groundViewZ, got * 255.0f);
        Check(std::fabs(got - expected) < 2.5f / 255.0f, "depth linearisation matches the scene geometry");

        D3DVIEWPORT9 clientDepthRange = world;
        clientDepthRange.MaxZ = kClientWorldMaxZ;
        Image clientDepth = renderDebugIn(3, 255.0f, clientDepthRange, kClientWdlRawDepth);
        float clientGot = clientDepth.At(centreColumn, groundRowNearBottom)[2] / 255.0f;
        float wdl = clientDepth.At(100, 50)[2] / 255.0f;
        std::printf("     client depth range: ground %.2f yd (expected %.2f), distant terrain %.2f of max distance\n",
                    clientGot * 255.0f, groundViewZ, wdl);
        Check(std::fabs(clientGot - expected) < 2.5f / 255.0f,
              "depth linearisation follows the world viewport's MaxZ 0.94");
        Check(wdl > 0.99f, "depth beyond the world range (distant terrain) counts as beyond the far clip");
        Image clientSkyT = renderDebugIn(2, 5000.0f, clientDepthRange, kClientWdlRawDepth);
        float distantTerrainT = clientSkyT.At(100, 50)[2] / 255.0f;
        float skyBesideTerrainT = clientSkyT.At(300, 50)[2] / 255.0f;
        std::printf("     transmittance: distant terrain %.3f, sky beside it %.3f\n", distantTerrainT,
                    skyBesideTerrainT);
        Check(distantTerrainT < skyBesideTerrainT - 0.1f, "distant terrain is fogged as terrain, not as sky");

        UINT midRow = 0;
        float midZ = 0.0f;
        for (UINT row = 687; row > 0 && !midRow; --row)
        {
            float rowNdc = 1.0f - (row + 0.5f) / 688.0f * 2.0f;
            float dz = rowNdc / proj[5] * view[9] + view[10];
            float z = dz < 0.0f ? -EyeHeightAboveGround(eye) / dz : 1e9f;
            if (z > 150.0f)
            {
                midRow = row;
                midZ = z;
            }
        }
        Image idealT = renderDebugIn(2, 5000.0f, world, kNoWdlPatch);
        Image clientT = renderDebugIn(2, 5000.0f, clientDepthRange, kNoWdlPatch);
        float ti = idealT.At(640, midRow)[2] / 255.0f;
        float tc = clientT.At(640, midRow)[2] / 255.0f;
        std::printf("     ground at %.0f yd: transmittance %.3f with the full depth range, %.3f with the client's\n",
                    midZ, ti, tc);
        Check(ti < 0.97f && std::fabs(ti - tc) < 2.5f / 255.0f,
              "fog on geometry is the same with the client's depth range");
    }

    {
        DWORD before = 0;
        DWORD forced = 0;
        DWORD during = 0;
        DWORD after = 0;
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &before);
        vf_test_force_depth_write(1);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &forced);
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &during);
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        h.dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        vf_test_force_depth_write(0);
        h.dev->GetRenderState(D3DRS_ZWRITEENABLE, &after);
        Check(before == FALSE && forced == TRUE && during == TRUE && after == FALSE,
              "liquid-pass depth writes stay on while forced and the client's last request is restored");
    }

    RUN_GROUP(CheckDepthWriteStateBlockRestore(h.dev));
    RUN_GROUP(CheckWorldTextDepthIsolation(h));
    RUN_GROUP(transparent_fog_checks::CheckTransparentFog(h));
    RUN_GROUP(early_composite_look::CheckLookThroughTheEarlyComposite(h));
    Config restored = cfg;
    vf_test_set_config(&restored);
    RUN_GROUP(water_checks::CheckWaterRipplePass(h, outDir));
    vf_test_set_config(&restored);
    RUN_GROUP(water_wake_checks::CheckWakes(h));
    vf_test_set_config(&restored);
    RUN_GROUP(client_sprite_checks::CheckClientSprites(h));
    vf_test_set_config(&restored);
    RUN_GROUP(water_checks::CheckWaterPass(h, outDir, waterDataPath));
    vf_test_set_config(&restored);
    RUN_GROUP(ripple_scene::CheckWakesShowInShadedWater(h, outDir, waterDataPath));
    vf_test_set_config(&restored);

    RUN_GROUP(CheckOverlayInput(h));
    RUN_GROUP(CheckHotkeyCharactersSwallowedOnlyWhileTheHotkeyIsTyped(h));
    const size_t logBeforeWidgets = water_settings_checks::DllLogSize();
    RUN_GROUP(CheckOverlayWidgets(h));
    RUN_GROUP(water_settings_checks::CheckSliderDragLoggedOnce(logBeforeWidgets));
    RUN_GROUP(CheckOverlayDraw(h, world, outDir));
    PanelRect panel = CheckSettingsWindowResizes(h, world, outDir);

    const void* densityBeforeReset = nullptr;
    const void* authoredBeforeReset = nullptr;
    vf_test_managed_noise(&densityBeforeReset, &authoredBeforeReset);
    h.ReleaseEngineObjects();
    const unsigned curveUploadsBeforeReset = forever_look_checks::CurveUploads();
    h.pp.BackBufferWidth = 1024;
    h.pp.BackBufferHeight = 600;
    hr = h.dev->Reset(&h.pp);
    Check(SUCCEEDED(hr), "Reset through the wrapper");
    h.dev->GetDepthStencilSurface(&depth);
    std::memset(&depthDesc, 0, sizeof(depthDesc));
    if (depth)
    {
        depth->GetDesc(&depthDesc);
        depth->Release();
    }
    Check(depthDesc.Format == kIntz && depthDesc.Width == 1024, "INTZ depth recreated at the new size after Reset");
    h.CreateEngineObjects();
    const D3DVIEWPORT9 resized = {0, 0, 1024, 600, 0.0f, 1.0f};
    EngineProjection(1024.0f / 600.0f, proj);
    CameraRelativeLookAt(eye, at, view);
    RUN_GROUP(CheckFogReturnsAfterRefusedDepth(h, resized, eye, at, view, proj));
    h.BeginFrame();
    h.DrawScene(eye, view, proj, resized);
    FrameInputs in = MakeInputs(view, proj, eye, at, resized);
    Check(vf_test_render(&in, &skip) != 0, (std::string("fog renders after Reset ") + skip).c_str());
    const void* densityAfterReset = nullptr;
    const void* authoredAfterReset = nullptr;
    vf_test_managed_noise(&densityAfterReset, &authoredAfterReset);
    Check(densityBeforeReset && densityAfterReset == densityBeforeReset && authoredAfterReset == authoredBeforeReset,
          "a Reset keeps the managed noise volumes instead of regenerating them");
    h.dev->EndScene();
    const Image beforeOverlay = Capture(h.dev);
    DrawOverlayFrames(kOverlaySettleFrames);
    Check(OverlayProbeChange(beforeOverlay, Capture(h.dev)) > 0.05, "the overlay draws again after Reset");
    PressHotkey(h.window, kDefaultOverlayHotkey);
    panel.h = std::min(panel.h, static_cast<float>(h.pp.BackBufferHeight) - kPanelMargin);
    RUN_GROUP(CheckSettingsWindowKeepsItsSize(h, panel,
                                              "after a Reset to a smaller back buffer the settings window keeps its "
                                              "place and width and fits its height to the screen"));
    RUN_GROUP(forever_look_checks::CheckColourGradingAfterReset(h, resized, curveUploadsBeforeReset));
    vf_test_set_config(&restored);

    RUN_GROUP(runtime_cost::CheckDisabledTemporalSkipsHistoryPasses(h, eye, at, proj, resized));
    RUN_GROUP(terrain_shadow_checks::CheckShadowMapUpdates(h.dev));
    RUN_GROUP(CheckRendererSwitchesLitShaders(h));
    RUN_GROUP(surface_light_checks::CheckLightsOnSurfaces(h));
    RUN_GROUP(surface_light_checks::CheckSpellLightsOnSurfaces(h));
    RUN_GROUP(surface_light_checks::CheckLightsFillClientShadows(h));
    RUN_GROUP(surface_light_checks::CheckSurfacesBrighterThanTheirLightingAreNotLitAgain(h));
    RUN_GROUP(surface_light_checks::CheckSurfaceLightsFadeOutInDaylight(h));
    RUN_GROUP(surface_light_checks::CheckSurfaceLightsAreDrawnOnlyWhereTheyReach(h));
    RUN_GROUP(surface_light_checks::CheckGodRaysTakeTheirSceneFromTheCompositeCopy(h));
    RUN_GROUP(authored_noise::CheckRendererDrawsStormNoise(h));
    RUN_GROUP(authored_noise::CheckRendererDrawsLitStormNoise(h));
    RUN_GROUP(runtime_cost::CheckFrameSummaryLogsClassicExtras(h));
    RUN_GROUP(runtime_cost::CheckLocalLightLogLines(h));
    RUN_GROUP(runtime_cost::CheckDepthProbeAndGpuTimeLog(h, eye, at));
    RUN_GROUP(runtime_cost::CheckGpuTimerRetriesTransientCreationFailures(h.dev));
    RUN_GROUP(runtime_cost::CheckGpuTimerReportsUnsupportedBeforeFirstSummary(h.dev));
    RUN_GROUP(runtime_cost::CheckPassGpuTimesListEachSupportedPass(h.dev));

    vf_test_set_config(&restored);
    h.ReleaseEngineObjects();
    client_sprite_checks::HoldGateForDeviceRelease();
    ULONG devRefs = h.dev->Release();
    RUN_GROUP(client_sprite_checks::CheckDeviceReleaseRestoresTheGate());
    h.dev = nullptr;
    hr = h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, engineFlags, &h.pp, &h.dev);
    Check(SUCCEEDED(hr) && h.dev, "a device is created again on the harness window");
    if (h.dev)
    {
        RUN_GROUP(CheckSettingsWindowKeepsItsSize(h, panel,
                                                  "a device created again for the same session opens the settings "
                                                  "window at the place and size it had"));
        h.CreateEngineObjects();
        RUN_GROUP(resource_retry::CheckFogResourceRetries(h, engineFlags, eye, at, proj, resized, devRefs));
        h.ReleaseEngineObjects();
        devRefs += h.dev->Release();
    }
    ULONG d3dRefs = h.d3d->Release();
    Check(devRefs == 0 && d3dRefs == 0, "wrapper reference counts reach zero");
    RUN_GROUP(multisampling_checks::CheckNvapiLoadsWithoutKb2533623());
    RUN_GROUP(CheckSecondClientWritesTheSecondSlot(outDir));
    RUN_GROUP(CheckHeldSlotIsSkippedUntouched(outDir));
    RUN_GROUP(CheckFreedSlotIsReused(outDir));
    RUN_GROUP(CheckNoLogWhenEverySlotIsHeld(outDir));
    RUN_GROUP(CheckHeldLogNoticeNamesTheSlotAndProcess());
    RUN_GROUP(CheckLogRefusesASecondWriter());
    RUN_GROUP(multisampling_checks::CheckMultisampling(realCreate, h.window, outDir, FullPath(iniPath)));
    RUN_GROUP(device_creation_checks::CheckDeviceCreation(realCreate, h.window));
    vf_test_set_config(&restored);
    DestroyWindow(h.window);
    CoUninitialize();
    std::printf("%s (%d failure%s%s)\n", g_failures ? "FAILED" : "OK", g_failures, g_failures == 1 ? "" : "s",
                g_groupFilters.empty() ? "" : ", filtered: checks outside the groups run may fail");
    return g_failures ? 1 : 0;
}

constexpr UINT kHarbourWidth = 2560;
constexpr UINT kHarbourHeight = 1440;
constexpr int kHarbourSettleFrames = 24;
constexpr D3DCOLOR kHarbourSea = 0xFF1A2430;

D3DCOLOR Shade(D3DCOLOR c, float s)
{
    auto ch = [&](int shift) {
        return static_cast<D3DCOLOR>(std::fmin(((c >> shift) & 0xFF) * s, 255.0f)) << shift;
    };
    return 0xFF000000u | ch(16) | ch(8) | ch(0);
}

constexpr float kSeaLevel = 0.0f;

struct HarbourObject
{
    float degreesLeftOfView;
    float nearFaceDistance;
    float depth;
    float halfWidth;
    float topHeight;
    D3DCOLOR color;
};

constexpr HarbourObject kShip = {18.0f, 80.0f, 30.0f, 12.0f, 75.0f, 0xFF808080};
constexpr HarbourObject kLighthouse = {-6.0f, 350.0f, 20.0f, 10.0f, 140.0f, 0xFFA8A8B0};
constexpr HarbourObject kRidge = {-25.0f, 650.0f, 80.0f, 150.0f, 190.0f, 0xFF4A5A38};
const HarbourObject kHarbourObjects[] = {kShip, kLighthouse, kRidge};

void AddBoxStandingOnSea(std::vector<SceneVertex>& v, float azimuthDeg, const HarbourObject& o)
{
    Vec3 forward = Dir(azimuthDeg, 0.0f);
    Vec3 right = {forward.y, -forward.x, 0.0f};
    Vec3 nearFace = {kHarbourEye.x + forward.x * o.nearFaceDistance, kHarbourEye.y + forward.y * o.nearFaceDistance,
                     kSeaLevel};
    auto corner = [&](float along, float side, float z) {
        return Vec3{nearFace.x + forward.x * along + right.x * side, nearFace.y + forward.y * along + right.y * side,
                    z};
    };
    const float w = o.halfWidth;
    const float top = o.topHeight;
    Vec3 p[8] = {corner(0, -w, kSeaLevel), corner(0, w, kSeaLevel), corner(o.depth, w, kSeaLevel),
                 corner(o.depth, -w, kSeaLevel), corner(0, -w, top), corner(0, w, top), corner(o.depth, w, top),
                 corner(o.depth, -w, top)};
    const Vec3 absolute = {0, 0, 0};
    AddQuad(v, p[0], p[1], p[5], p[4], o.color, absolute);
    AddQuad(v, p[1], p[2], p[6], p[5], Shade(o.color, 0.75f), absolute);
    AddQuad(v, p[2], p[3], p[7], p[6], Shade(o.color, 0.6f), absolute);
    AddQuad(v, p[3], p[0], p[4], p[7], Shade(o.color, 0.75f), absolute);
    AddQuad(v, p[4], p[5], p[6], p[7], Shade(o.color, 1.15f), absolute);
}

std::vector<SceneVertex> BuildHarbourSceneFacing(const float* viewAzimuths, int viewCount)
{
    std::vector<SceneVertex> v;
    const float e = 3000.0f;
    const Vec3 absolute = {0, 0, 0};
    const Vec3 o = {kHarbourEye.x, kHarbourEye.y, kSeaLevel};
    AddQuad(v, Add(o, {-e, -e, 0}), Add(o, {e, -e, 0}), Add(o, {e, e, 0}), Add(o, {-e, e, 0}), kHarbourSea, absolute);
    for (int i = 0; i < viewCount; ++i)
        for (const HarbourObject& obj : kHarbourObjects)
            AddBoxStandingOnSea(v, viewAzimuths[i] + obj.degreesLeftOfView, obj);
    return v;
}

enum class ProbeKind
{
    World,
    Sky,
    SunAzimuth,
};

struct HarbourProbe
{
    const char* name;
    ProbeKind kind;
    float degreesLeftOfView;
    float horizontalDistance;
    float height;
    float elevationDeg;
    float elevationFromSunDeg;
};

HarbourProbe WorldProbe(const char* name, float degreesLeftOfView, float horizontalDistance, float height)
{
    return {name, ProbeKind::World, degreesLeftOfView, horizontalDistance, height, 0.0f, 0.0f};
}

HarbourProbe SkyProbe(const char* name, float degreesLeftOfView, float elevationDeg)
{
    return {name, ProbeKind::Sky, degreesLeftOfView, 0.0f, 0.0f, elevationDeg, 0.0f};
}

HarbourProbe SunAzimuthProbe(const char* name, float elevationFromSunDeg)
{
    return {name, ProbeKind::SunAzimuth, 0.0f, 0.0f, 0.0f, 0.0f, elevationFromSunDeg};
}

const HarbourProbe kHarbourProbes[] = {
    WorldProbe("ship (80 yd, z 55)", 18.0f, 80.0f, 55.0f),
    WorldProbe("ship hull (80 yd, z 10)", 18.0f, 80.0f, 10.0f),
    WorldProbe("lighthouse (350 yd, z 60)", -6.0f, 350.0f, 60.0f),
    WorldProbe("lighthouse top (350 yd, z 125)", -6.0f, 350.0f, 125.0f),
    WorldProbe("ridge (650 yd, z 60)", -25.0f, 650.0f, 60.0f),
    WorldProbe("ridge top (650 yd, z 170)", -25.0f, 650.0f, 170.0f),
    WorldProbe("sea 100 yd", 3.0f, 100.0f, kSeaLevel),
    WorldProbe("sea 160 yd", 3.0f, 160.0f, kSeaLevel),
    WorldProbe("sea 400 yd", 3.0f, 400.0f, kSeaLevel),
    WorldProbe("sea 700 yd", 3.0f, 700.0f, kSeaLevel),
    SkyProbe("sky past the far clip (-4 deg)", 3.0f, -4.0f),
    SkyProbe("sky at the horizon (+1 deg)", 30.0f, 1.0f),
    SkyProbe("sky 10 deg", 30.0f, 10.0f),
    SkyProbe("sky 40 deg", 30.0f, 40.0f),
    SunAzimuthProbe("near the sun (6 deg below)", -6.0f),
    SunAzimuthProbe("near the sun (15 deg below)", -15.0f),
};

Vec3 ProbeOffsetFromEye(const HarbourProbe& p, float viewAzimuth, float sunAzimuth, float sunElevation)
{
    if (p.kind == ProbeKind::World)
    {
        Vec3 q = Dir(viewAzimuth + p.degreesLeftOfView, 0.0f);
        return {q.x * p.horizontalDistance, q.y * p.horizontalDistance, p.height - kHarbourEye.z};
    }
    if (p.kind == ProbeKind::Sky)
        return Dir(viewAzimuth + p.degreesLeftOfView, p.elevationDeg);
    return Dir(sunAzimuth, sunElevation + p.elevationFromSunDeg);
}

struct HarbourView
{
    const wchar_t* file;
    const char* name;
    float baseAzimuth;
    float pitch;
    float worldViewportMaxZ;
};

constexpr float kMaxDistanceBelowHarbourFarClip = 200.0f;
constexpr float kProbeDepthToleranceYards = 6.0f;

float DepthViewYards(const Image& depth, int x, int y)
{
    return depth.At(x, y)[2] / 255.0f * kHarbourFar;
}

bool DepthViewIsSky(const Image& depth, int x, int y)
{
    return depth.At(x, y)[2] >= 254;
}

void ReferenceUnshadowedLayerOpticalDepths(const FogParams& fog, float camZ, Vec3 dirW, float viewZ, float rayLen,
                                           bool sky, float* tau)
{
    float z = sky ? fog.maxDistance : viewZ;
    float horizon = sky ? 1.0f : SmoothStep(fog.horizonStart, fog.farClip, z);
    z = z + (fog.maxDistance - z) * horizon;
    float tMax = std::fmin(z * rayLen, fog.maxDistance);
    float up = sky ? std::fabs(dirW.z) : std::fmax(dirW.z, 0.0f);
    float dirZ = dirW.z + (up - dirW.z) * horizon;
    const float dt = tMax / kReferenceIntegrationSteps;
    for (int j = 0; j < kFogLayers; ++j)
        tau[j] = 0.0f;
    for (int i = 0; i < kReferenceIntegrationSteps; ++i)
    {
        float t = (i + 0.5f) * dt;
        float h = camZ + dirZ * t;
        for (int j = 0; j < kFogLayers; ++j)
        {
            const FogLayer& l = fog.layers[j];
            if (t < l.start || t > l.endDistance)
                continue;
            float scale = sky ? std::exp(-up * l.skyFalloff) : 1.0f;
            float curve = 1.0f + l.strength * std::pow(std::fmin(std::fmax(t - l.start, 0.0f) / fog.maxDistance, 1.0f) +
                                                           1e-6f,
                                                       l.exponent);
            float heightF = std::fmin(std::exp((l.upperHeight - h) * l.upperFalloff), 1.0f) *
                            std::fmin(std::exp((h - l.lowerHeight) * l.lowerFalloff), 1.0f);
            tau[j] += l.density * scale * curve * heightF * dt;
        }
    }
}

void PrintLayers(const FogParams& fog)
{
    for (int i = 0; i < kFogLayers; ++i)
    {
        const FogLayer& l = fog.layers[i];
        std::printf("  layer %d (%s): start %.0f density %.6f g %.2f diffuse %.2f %.2f %.2f emissive %.2f %.2f %.2f "
                    "upper %.1f/%.4f lower %.1f/%.4f shadowed %.0f limit %.0f\n",
                    i, fog.authored && i < kSceneLayers ? "classic" : "derived", l.start, l.density, l.g, l.diffuse[0],
                    l.diffuse[1], l.diffuse[2], l.emissive[0], l.emissive[1], l.emissive[2], l.upperHeight,
                    l.upperFalloff, l.lowerHeight, l.lowerFalloff, l.shadowed, std::fmin(l.endDistance, 99999.0f));
        std::printf("           curve strength %.2f exponent %.2f, sky falloff %.2f, shadow density %.2f, "
                    "shadow emissive %.2f %.2f %.2f, isotropic %.2f\n",
                    l.strength, l.exponent, l.skyFalloff, l.shadowDensity, l.shadowEmissive[0], l.shadowEmissive[1],
                    l.shadowEmissive[2], l.isotropic);
    }
}

struct HarbourOptions
{
    int classicPhase = Config().classicPhase;
    float stormBlend = 0.0f;
};

int RunHarbour(const std::wstring& outDir, const std::string& dataPath, const HarbourOptions& options)
{
    FogData classic;
    if (!classic.Load(dataPath))
    {
        std::printf("Classic fog data %s did not load\n", dataPath.c_str());
        return 1;
    }
    CreateDirectoryW(outDir.c_str(), nullptr);
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    const Vec3 toLight = Norm(kHarbourToLight);
    const float sunAz = std::atan2(toLight.y, toLight.x) / kDegree;
    const float sunEl = std::asin(toLight.z) / kDegree;
    const float pitch = sunEl - 20.0f;
    const HarbourView views[] = {
        {L"sun", "facing the sun, sun 20 deg above the view centre", sunAz, pitch, kIdealWorldMaxZ},
        {L"right", "90 deg right of the sun, same pitch", sunAz - 90.0f, pitch, kIdealWorldMaxZ},
        {L"sun-up", "facing the sun, pitched up 25 deg (high sky)", sunAz, 25.0f, kIdealWorldMaxZ},
        {L"right-up", "90 deg right of the sun, pitched up 25 deg (high sky)", sunAz - 90.0f, 25.0f, kIdealWorldMaxZ},
        {L"sun-down", "facing the sun, pitched down 40 deg (ship hull, near sea)", sunAz, -40.0f, kIdealWorldMaxZ},
        {L"sun-client-depth-range", "facing the sun, world drawn with the client's viewport MaxZ 0.94", sunAz, pitch,
         kClientWorldMaxZ},
        {L"right-client-depth-range", "90 deg right, world drawn with the client's viewport MaxZ 0.94",
         sunAz - 90.0f, pitch, kClientWorldMaxZ},
    };
    const float baseAzimuths[2] = {sunAz, sunAz - 90.0f};

    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"vfog_harbour";
    RegisterClassW(&wc);
    Harness h;
    h.window = CreateWindowW(L"vfog_harbour", L"vfog harbour", WS_OVERLAPPEDWINDOW, 0, 0, 1280, 720, nullptr, nullptr,
                             wc.hInstance, nullptr);
    HMODULE d3d9 = LoadLibraryA("d3d9.dll");
    auto realCreate = reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(d3d9, "Direct3DCreate9"));
    h.d3d = vf_test_wrap_direct3d9(realCreate, D3D_SDK_VERSION);
    if (!h.d3d)
        return 1;
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = kHarbourWidth;
    h.pp.BackBufferHeight = kHarbourHeight;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kClientDepthFormat;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    DWORD engineFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    if (FAILED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, engineFlags, &h.pp, &h.dev)) || !h.dev)
    {
        std::printf("CreateDevice through the wrapper failed\n");
        h.d3d->Release();
        return 1;
    }
    h.scene = BuildHarbourSceneFacing(baseAzimuths, 2);
    h.clearColor = kHarbourSky;

    const D3DVIEWPORT9 vp = {0, 0, kHarbourWidth, kHarbourHeight, 0.0f, 1.0f};
    const float aspect = static_cast<float>(kHarbourWidth) / kHarbourHeight;
    float proj[16];
    EngineGlDepthProjection(kHarbourLoggedP11, aspect, kHarbourNear, kHarbourFar, proj);
    Config shippedCfg = {};
    shippedCfg.classicPhase = options.classicPhase;

    auto inputsFor = [&](const float* view, Vec3 at) {
        return HarbourSunsetInputs(view, proj, at, vp, options.stormBlend);
    };

    {
        float view[16];
        Vec3 at = Add(kHarbourEye, Dir(views[0].baseAzimuth, views[0].pitch));
        CameraRelativeLookAt(kHarbourEye, at, view);
        FrameInputs in = inputsFor(view, at);
        AuthoredFog authored = {};
        bool resolved = classic.Resolve(kEasternKingdoms, in.camPos, in.dayFraction, in.lightParams, authored);
        std::printf("harbour frame: ClassicPhase %d, storm weight %.2f\n", shippedCfg.classicPhase,
                    in.lightParams.stormBlend);
        std::printf("harbour frame: camera (%.1f %.1f %.1f), day %.4f, toLight (%.3f %.3f %.3f) = azimuth %.1f "
                    "elevation %.2f deg\n",
                    in.camPos[0], in.camPos[1], in.camPos[2], in.dayFraction, toLight.x, toLight.y, toLight.z, sunAz,
                    sunEl);
        std::printf("  projection P00 %.4f P11 %.4f near %.2f far %.1f, %ux%u\n", proj[0], proj[5], kHarbourNear,
                    kHarbourFar, kHarbourWidth, kHarbourHeight);
        std::printf("  Classic lights:");
        for (int i = 0; i < authored.lightCount; ++i)
            std::printf(" %u:%.2f", authored.lightIds[i], authored.lightWeights[i]);
        std::printf(" (%s, %d layers)\n", resolved ? "resolved" : "NOT resolved", authored.layerCount);
        for (int i = 0; i < authored.layerCount; ++i)
        {
            const AuthoredLayer& a = authored.layers[i];
            std::printf("  authored %d: start %.1f density %.4f g %.3f intensity %.2f strength %.3f exponent %.3f "
                        "diffuse %.3f %.3f %.3f emissive %.3f %.3f %.3f flags %u noise %.2f\n",
                        i, a.start, a.density, a.g, a.intensity, a.strength, a.exponent, a.diffuse[0], a.diffuse[1],
                        a.diffuse[2], a.emissive[0], a.emissive[1], a.emissive[2], a.flags, a.noise.presence);
        }
        FogParams fog = BuildFogParams(in, shippedCfg, resolved ? &authored : nullptr);
        std::printf("  fog params (as the DLL logs them): refZ %.1f maxDistance %.0f horizonStart %.1f farLimit %.1f "
                    "exposure %.2f\n",
                    fog.referenceZ, fog.maxDistance, fog.horizonStart, fog.farLimit,
                    fog.authored ? shippedCfg.classicExposure : shippedCfg.exposure);
        PrintLayers(fog);
    }

    const char* skip = "";
    auto frame = [&](const float* view, const FrameInputs& in, const Config& c, bool capture, Image* before) {
        vf_test_set_config(&c);
        h.BeginFrame();
        h.DrawScene(kHarbourEye, view, proj, in.viewport);
        if (before)
            *before = Capture(h.dev);
        if (!vf_test_render(&in, &skip))
            std::printf("     fog skipped: %s\n", skip);
        Image img;
        if (capture)
            img = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
        return img;
    };
    auto restartTemporalHistory = [&](const float* view, const FrameInputs& in) {
        Config withoutHistory = shippedCfg;
        withoutHistory.temporal = 0.0f;
        frame(view, in, withoutHistory, false, nullptr);
    };

    for (const HarbourView& hv : views)
    {
        float view[16];
        Vec3 at = Add(kHarbourEye, Dir(hv.baseAzimuth, hv.pitch));
        CameraRelativeLookAt(kHarbourEye, at, view);
        FrameInputs in = inputsFor(view, at);
        in.viewport.MaxZ = hv.worldViewportMaxZ;
        const bool linearDepthComparable = hv.worldViewportMaxZ >= kIdealWorldMaxZ;

        Config depthCfg = shippedCfg;
        depthCfg.debugView = 3;
        depthCfg.maxDistance = kMaxDistanceBelowHarbourFarClip;
        depthCfg.temporal = 0.0f;
        Image depth = frame(view, in, depthCfg, true, nullptr);

        restartTemporalHistory(view, in);
        Config c = shippedCfg;
        Image before;
        Image after;
        for (int i = 1; i < kHarbourSettleFrames; ++i)
        {
            bool last = i == kHarbourSettleFrames - 1;
            Image img = frame(view, in, c, last, last ? &before : nullptr);
            if (last)
                after = img;
        }
        Image radiance;
        Image transmittance;
        c.debugView = 1;
        for (int i = 0; i < 4; ++i)
            radiance = frame(view, in, c, i == 3, nullptr);
        c.debugView = 2;
        for (int i = 0; i < 4; ++i)
            transmittance = frame(view, in, c, i == 3, nullptr);

        std::wstring stem = outDir + L"\\harbour-" + hv.file;
        SavePng(stem + L"-before.png", before.w, before.h, before.bgra);
        SavePng(stem + L"-after.png", after.w, after.h, after.bgra);
        SavePng(stem + L"-radiance.png", radiance.w, radiance.h, radiance.bgra);
        SavePng(stem + L"-transmittance.png", transmittance.w, transmittance.h, transmittance.bgra);
        SavePng(stem + L"-depth.png", depth.w, depth.h, depth.bgra);

        AuthoredFog authored = {};
        bool resolved = classic.Resolve(kEasternKingdoms, in.camPos, in.dayFraction, in.lightParams, authored);
        const FogParams drawn = BuildFogParams(in, shippedCfg, resolved ? &authored : nullptr);
        const FogParams fog = WithMeanNoise(drawn);
        float sunV[3];
        TransformDirection(in.toLight, view, sunV);
        std::printf("\nview %ls: %s (azimuth %.1f, pitch %.1f; sun view-space %.3f %.3f %.3f, refZ %.1f, "
                    "viewport MaxZ %.2f)\n",
                    hv.file, hv.name, hv.baseAzimuth, hv.pitch, sunV[0], sunV[1], sunV[2], fog.referenceZ,
                    hv.worldViewportMaxZ);
        std::printf("  %-31s %6s %6s %6s %13s %6s %15s %15s %15s %15s %6s  %s\n", "probe", "px", "py", "dist",
                    "viewZ/shader", "alpha", "fog rgb", "fog rgb / a", "before", "after", "cpu a",
                    "cpu tau L0 L1 L2 L3");
        if (AnyLayerNoise(drawn))
            std::printf("  cpu a and tau take the authored noise at its mean density; the GPU samples the noise, so "
                        "probes differ where it is patchy\n");
        for (const HarbourProbe& p : kHarbourProbes)
        {
            Vec3 d = ProbeOffsetFromEye(p, hv.baseAzimuth, sunAz, sunEl);
            float vx = d.x * view[0] + d.y * view[4] + d.z * view[8];
            float vy = d.x * view[1] + d.y * view[5] + d.z * view[9];
            float vz = d.x * view[2] + d.y * view[6] + d.z * view[10];
            if (vz <= 0.01f)
                continue;
            float px = (vx / vz * proj[0] + proj[8]) * 0.5f + 0.5f;
            float py = 0.5f - (vy / vz * proj[5] + proj[9]) * 0.5f;
            int ix = static_cast<int>(px * kHarbourWidth);
            int iy = static_cast<int>(py * kHarbourHeight);
            if (ix < 4 || iy < 4 || ix >= static_cast<int>(kHarbourWidth) - 4 ||
                iy >= static_cast<int>(kHarbourHeight) - 4)
                continue;
            const bool world = p.kind == ProbeKind::World;
            float len = std::sqrt(Dot(d, d));
            float zShader = DepthViewYards(depth, ix, iy);
            bool shaderSky = DepthViewIsSky(depth, ix, iy);
            bool depthAgrees = !linearDepthComparable || std::fabs(zShader - vz) < kProbeDepthToleranceYards;
            bool matches = world ? depthAgrees && !shaderSky : shaderSky;
            if (!matches)
            {
                std::printf("  %-31s %6d %6d  hidden (depth %.1f yd%s)\n", p.name, ix, iy, zShader,
                            shaderSky ? ", sky" : "");
                continue;
            }
            const int radius = 2;
            Rgb fogRgb = SampleRgb(radiance, ix, iy, radius);
            Rgb t = SampleRgb(transmittance, ix, iy, radius);
            Rgb b = SampleRgb(before, ix, iy, radius);
            Rgb a = SampleRgb(after, ix, iy, radius);
            float alpha = 1.0f - t.r / 255.0f;
            float inv = alpha > 0.02f ? 1.0f / alpha : 0.0f;
            float tau[kFogLayers];
            Vec3 dirW = {d.x / len, d.y / len, d.z / len};
            ReferenceUnshadowedLayerOpticalDepths(fog, kHarbourEye.z, dirW, world ? vz : 0.0f, world ? len / vz : 1.0f,
                                                  !world, tau);
            float tauSum = tau[0] + tau[1] + tau[2] + tau[3];
            char distText[16];
            char zText[24];
            if (world)
            {
                std::snprintf(distText, sizeof(distText), "%.0f", len);
                std::snprintf(zText, sizeof(zText), "%.0f/%.0f", vz, zShader);
            }
            else
            {
                std::snprintf(distText, sizeof(distText), "sky");
                std::snprintf(zText, sizeof(zText), "sky/sky");
            }
            std::printf("  %-31s %6d %6d %6s %13s %6.3f %4.0f %4.0f %4.0f  %4.0f %4.0f %4.0f  %4.0f %4.0f %4.0f  "
                        "%4.0f %4.0f %4.0f %6.3f  %.3f %.3f %.3f %.3f\n",
                        p.name, ix, iy, distText, zText, alpha, fogRgb.r, fogRgb.g, fogRgb.b,
                        std::fmin(fogRgb.r * inv, 999.0f), std::fmin(fogRgb.g * inv, 999.0f),
                        std::fmin(fogRgb.b * inv, 999.0f), b.r, b.g, b.b, a.r, a.g, a.b, 1.0f - std::exp(-tauSum),
                        tau[0], tau[1], tau[2], tau[3]);
        }
    }

    vf_test_set_config(&shippedCfg);
    ULONG devRefs = h.dev->Release();
    ULONG d3dRefs = h.d3d->Release();
    DestroyWindow(h.window);
    CoUninitialize();
    std::printf("\nharbour scene written to %ls (device refs %lu, d3d refs %lu)\n", outDir.c_str(), devRefs, d3dRefs);
    return 0;
}
}

namespace
{
#include "performance_scene.h"
#include "copy_cost_scene.h"
#include "water_performance_scene.h"
}

int wmain(int argc, wchar_t** argv)
{
    std::wstring out = L"harness-out";
    std::string data = "fogdata.bin";
    std::string waterData = "waterdata.bin";
    std::wstring ini = L"CoAGraphics.ini";
    std::wstring scene;
    D3DMULTISAMPLE_TYPE samples = D3DMULTISAMPLE_NONE;
    HarbourOptions harbour;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::wcscmp(argv[i], L"--samples") == 0 && _wtoi(argv[i + 1]) > 1)
            samples = static_cast<D3DMULTISAMPLE_TYPE>(_wtoi(argv[i + 1]));
        if (std::wcscmp(argv[i], L"--filter") == 0)
        {
            std::string list;
            for (const wchar_t* c = argv[i + 1]; *c; ++c)
                list.push_back(static_cast<char>(std::towlower(*c)));
            for (size_t from = 0; from <= list.size();)
            {
                const size_t to = std::min(list.find(',', from), list.size());
                if (to > from)
                    g_groupFilters.push_back(list.substr(from, to - from));
                from = to + 1;
            }
        }
        if (std::wcscmp(argv[i], L"--classic-phase") == 0)
            harbour.classicPhase = _wtoi(argv[i + 1]);
        if (std::wcscmp(argv[i], L"--storm") == 0)
            harbour.stormBlend = static_cast<float>(_wtof(argv[i + 1]));
        if (std::wcscmp(argv[i], L"--out") == 0)
            out = argv[i + 1];
        if (std::wcscmp(argv[i], L"--data") == 0)
        {
            char path[MAX_PATH] = {};
            WideCharToMultiByte(CP_ACP, 0, argv[i + 1], -1, path, MAX_PATH, nullptr, nullptr);
            data = path;
        }
        if (std::wcscmp(argv[i], L"--ini") == 0)
            ini = argv[i + 1];
        if (std::wcscmp(argv[i], L"--water-data") == 0)
        {
            char path[MAX_PATH] = {};
            WideCharToMultiByte(CP_ACP, 0, argv[i + 1], -1, path, MAX_PATH, nullptr, nullptr);
            waterData = path;
        }
        if (std::wcscmp(argv[i], L"--scene") == 0)
        {
            scene = argv[i + 1];
            if (i + 2 < argc && argv[i + 2][0] != L'-')
                out = argv[i + 2];
        }
    }
    if (scene == L"interior")
    {
        CheckInteriorFogInputs();
        ConfigStore settings;
        char path[MAX_PATH] = {};
        WideCharToMultiByte(CP_ACP, 0, ini.c_str(), -1, path, MAX_PATH, nullptr, nullptr);
        settings.Load(path);
        Config edited = settings.Get();
        edited.interiorFarFog = 0.8f;
        settings.Apply(edited);
        Check(settings.Save(), "InteriorFarFog saves through the setting registry");
        ConfigStore reloaded;
        reloaded.Load(path);
        Check(reloaded.Get().interiorFarFog == 0.8f, "InteriorFarFog reloads from the ini");
        return g_failures ? 1 : 0;
    }
    if (scene == L"harbour")
        return RunHarbour(out, data, harbour);
    if (scene == L"performance")
        return RunPerformance(samples, performance_scene::MeasureCases);
    if (scene == L"upsample-paths")
        return RunPerformance(samples, [&](Harness& h) { return performance_scene::ShowUpsamplePaths(h, out); });
    if (scene == L"ripples")
        return ripple_scene::RunRippleScene(out, waterData);
    if (scene == L"water-performance")
        return water_performance::RunWaterPerformance(waterData);
    if (scene == L"copy-cost")
        return copy_cost_scene::RunCopyCost();
    if (scene == L"cpu")
        return Run(out, data, ini, waterData, true);
    if (!scene.empty())
    {
        std::printf("unknown scene %ls (known: copy-cost, cpu, harbour, performance, ripples, "
                    "upsample-paths, water-performance)\n",
                    scene.c_str());
        return 2;
    }
    return Run(out, data, ini, waterData);
}
