#pragma once

#include <cstdint>

struct FrameInputs;
struct WaterInputs;
struct M2BatchFogArgs;

namespace engine
{
struct StockFog;
}

bool InstallEngineHooks();
void EnableForeverLookOnHookedClient();
bool InstallTransparentFogHooks();
bool InstallSpellLightHooks();
bool InstallTreeWindHooks();

void InstallFarClipHooks();

struct FogFrameStatus
{
    bool drawn;
    const char* reason;
};

FogFrameStatus LastFogFrameStatus();

bool InstallWaterHooks();

struct WaterFrameStatus
{
    bool drawn;
    const char* reason;
};

WaterFrameStatus LastWaterFrameStatus();

extern "C" void __cdecl vf_on_frame_begin();
extern "C" void __cdecl vf_on_frame_end();
extern "C" void __cdecl vf_on_opaque_done();
extern "C" void __cdecl vf_on_liquid_end();
extern "C" void __cdecl vf_on_transparents_begin();
extern "C" void __cdecl vf_on_world_done();
extern "C" void __cdecl vf_on_m2_batch_fog(M2BatchFogArgs* args);
extern "C" void __cdecl vf_on_water_pass_begin(const void* liquidRenderer);
extern "C" void __cdecl vf_on_water_pass_end();
extern "C" void __cdecl vf_on_single_particle_render(uintptr_t emitter, uintptr_t sceneRender);
extern "C" void __cdecl vf_on_batched_particle_render(uintptr_t emitter, uintptr_t callerFrame);
extern "C" void __cdecl vf_on_mesh_batch_drawn(uintptr_t sceneRender);
extern "C" void __cdecl vf_on_model_animate(void* instance);

void RecordHookedFogFrame(bool rendered, bool cameraUnderLiquid, const char* skip);
void UseTestWorldClient(const FrameInputs& in, bool glowScreenEffectRuns);
void SimulateFogHookFailure(bool failed);
void UseTestFogClient(const FrameInputs& in);
engine::StockFog TestClientStockFog();
void SetTestClientStockFog(const engine::StockFog& fog);
void LogTransparentFogStatsAtFrameEnd();
void ClearTransparentFogFailure();
const void* RetargetM2BatchFogThunk(uintptr_t target);
const void* RetargetGlarePassThunk(uintptr_t target);
const void* RetargetWorldTextDrawThunk(uintptr_t target);
void UseTestWaterClient(const FrameInputs& in, const WaterInputs& water);
unsigned TestWaterContactReads();
void RefuseTestWaterContacts(bool refused);
const void* RetargetWaterPassThunk(uintptr_t target);
const void* RetargetMeshBatchDrawThunk(uintptr_t target);
void RetargetModelAnimateThunks(uintptr_t full, uintptr_t simple, const float* camera, const void*& fullThunk,
                                const void*& simpleThunk);
void RetargetParticleRenderThunks(uintptr_t target, uintptr_t worldSceneSlot, const void*& single,
                                  const void*& batched);
void ReuseWaterPassBeginArgumentSlot(bool reuse);
bool TagHookedWaterDraw(const void* liquidSettings);
void UntagHookedWaterDraw();
