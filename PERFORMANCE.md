# PERFORMANCE

Plan for making the fog, water and ripple effects cheaper without changing their intent or quality. Updated 2026-10-21
from a read of the code, the README's measurements, wowdecomp and the talks in `learning/`. Nothing here has been
implemented or measured; every cost below is either a README measurement or marked as unmeasured.

## Constraints

- Direct3D 9, ps_3_0: no compute shaders, no atomics, no renderable volume textures, 32 temporary registers. The
  register limit is the real design constraint: README says visibility holding and the past-layers stop do not fit in
  the lit, noisy, shadowed march.
- Must run on the client's D3D9 path and under DXVK/Proton (Steam Deck).
- A change that alters the look needs a before/after harness image and the owner's eyes, not just a lower timing.
- The harness does not establish in-game appearance or performance. Say so when reporting.

## Baseline

README measurements (the only numbers we have):

- Steam Deck, 8x MSAA, median per frame: fog 6.47 ms, water 2.81 ms.
- Radeon 780M, 1080p, no point lights: light shafts add 0.6 ms at `Quality=1`, 0.8-1.1 ms at 2, 1.6-1.9 ms at 3.
- Lit march with eight large lights: the sphere limits cut its cost by about a third at Quality 2 (harness).

Needed before any change is judged:

- `fog gpu` and `water gpu` lines from `CoAGraphics.log` in one or two typical scenes, with the client's Shadows
  setting noted.
- `vfog_harness --scene performance` output on the target GPU, saved as the baseline CSV.
- `water gpu` at `WaterQuality=1` against 2 (isolates the screen-space reflection), and the FFT, ripple and reflected
  fog shares of the water time.

## Where the time goes (by reading)

1. **Fog march** (`shaders/vf_integrate.hlsli`): 16/24/32 steps at Quality 1/2/3 per half-resolution pixel. With
   `LightShafts=1` (default) every march is the shadowed variant: a 12-tap screen-space trace every 4th/3rd/2nd step,
   up to 48/96/192 taps per pixel. Below client Shadows quality 3 the trace runs for every traced step
   (`src/engine_shadows.cpp:124`); above it most samples are one-tap map lookups.
2. **Copies and resolves.** With default settings one frame can do up to 7 full-screen colour copies (water scene,
   surface lights, linear fog blend, two for god rays, grading, image), 3 multisampled depth resolves
   (`water_renderer.cpp:1006,1523`, `renderer.cpp:1628`) and 2 full-resolution R32F linear-depth copies
   (`water_renderer.cpp:1012,1531`). Under 8x MSAA each colour copy is a full resolve; likely a large part of the Deck's
   9 ms. Water `Begin` copies before it knows whether any water draws (`AnyClassDrawn` is only checked in `End`).
3. **The composite shader carries the march.** `vf_composite.hlsli` includes `vf_integrate.hlsli` and runs it as the
   full-resolution fallback (`:243`), so every full-resolution pixel pays the march's register allocation even on the
   4-tap fast path. Fallback pixels march with jitter 0.5 and skip the temporal filter.
4. **Depth reads.** Full-resolution INTZ is sampled at strided points and re-linearised by the march, temporal (up to
   14 taps), composite (8 taps) and every traced shadow step (12 taps).
5. **Per-step recomputation** in `AccumulateLayer`: horizon shadow, shadow density scale and the shadow-emissive blend
   depend only on constants; phase (sqrt, reciprocal) and sky density (exp) only on the pixel.
6. **Density variation** is shared across layers only when `kUnrollsLayers`. The shipped variant never unrolls, so each
   layer with variation does its own 2 `tex3Dlod`: up to 8 per step instead of 2.
7. **Step placement.** Steps are quadratic over `marchLength`; the past-layers stop discards the steps beyond the last
   layer instead of moving them. Fog ending at 1500 yd on a 5000 yd ray gets about 13 of 24 steps.
8. **No opaque early exit.** Only the shadow trace stops below transmittance 1/512 (`:326`); layers, noise and lights
   keep accumulating.
9. **Water at `WaterQuality=2`:** up to 16 reflection steps plus 4 refinements per pixel
   (`shaders/vw_reflection.hlsli:88-109`), then fog along the reflected path in 8 segments over 4 layers (`:232-241`).
10. **Ripple step:** every texel loops all 48 disturbance slots (`vw_ripple_step.hlsl:77-80`), only while ripples run.
11. **MSAA split composite** reads a 3x3 depth neighbourhood before it may discard (opt-in).
12. **Temporal filter:** up to about 34 fetches per low-resolution pixel (default on).
13. **Local-light data** is a `D3DPOOL_MANAGED` texture locked every frame without `DISCARD`
    (`renderer.cpp:1665-1673`): a driver sync risk, especially under DXVK.

Checked and not hot: depth-probe readbacks (capped at five at `LogLevel=1`), the blocking query spin in the MSAA copy
test (setup only), GPU timer reads, spectrum uploads, `DrawPrimitiveUP` for the full-screen triangle, the 16-bit history
depth. CPU: nothing large; tree sway (`tree_wind.cpp:455-491`) and the contact walk of up to 4096 objects
(`engine_actors.cpp:26,224`) are the paths to measure if CPU time ever matters. Per-`SetTexture` cost in the wrapper
(`d3d9_wrap.cpp:360-379`) is small; cache the anisotropy level per frame.

## Plan

Order: cheap shader edits, then copies, then composite split, then shared depth, then the far-fog split. Each step is
timed with `--scene performance` and gets a harness check; steps marked "look" need the owner's eyes.

### 1. Cheap march edits (`vf_integrate.hlsli`)

- Break when `transmittance < 1/512` (item 8). No new registers. Check with `--storm 1`.
- Place the steps over `[first layer start, min(marchLength, layersEnd)]` (item 7). Local lights still need a lit
  layer, so `layersEnd` bounds them. Look: shifts step positions slightly.
- Sample density variation once per step and share it across layers (item 6). It differs only on steps where a layer
  boundary falls; compare in the harness.
- Fold constant-only layer terms (shadow density scale, pre-lerped emissive) into the six registers per layer on the
  CPU; hoist the four phases and sky scales into per-pixel values (item 5). This should also free registers for the
  lit, noisy, shadowed variant's past-layers stop and visibility hold.
- Extend `march_layers::CheckUnrolledMarchMatchesLoopedMarch` so every variant matches the unbroken march.
- `LocalLightScattering` past the last layer: confirm it never adds light there before relying on `layersEnd`.

### 2. Copies

- Fold surface lights into the fog composite (both read the scene copy at the same pixel).
- Let grading write straight into the image pass's scene copy when the image pass is on.
- Gate water `Begin`'s copies on the liquid renderer's chunk count or last frame's draws.
- Skip the fog's depth `Refresh` when the wrapper saw no depth-writing draws since water `End`.
- Make the local-light data texture DEFAULT + DYNAMIC with `D3DLOCK_DISCARD` (or a ring of 2-3).
- Look, optional: replace the linear blend's copy with `SRGBWRITEENABLE` blending on DX10-class hardware. Loses the 2.2
  curve and the highlight roll-off.
- Smaller: stencil-mask the SMAA weights pass from the edges pass; fold the bloom composite into sharpen.

### 3. Take the march out of the composite

Classify first with a cheap stencil-marking pass, then run the fast composite and the march composite under stencil.
The MSAA path already does this (`MarkSilhouetteSamples`, `renderer.cpp:820`). Also gives silhouette pixels the temporal
filter and the same jitter as their neighbours.

### 4. One low-resolution linear depth

Write an R32F (or G16R16) low-resolution linear-depth target before the march; the shadow trace, upsample and temporal
pass read it (the temporal surface match still needs the full-resolution depth class). Last frame's copy becomes the
history depth by swap, retiring the history-depth pass and its 8-bit packing.

### 5. Visibility atlas, then near/far split (instead of full froxels)

- **Visibility atlas (F4):** evaluate `SunVisibility()` into a small atlas (about 120x68x24) and have the march fetch
  it instead of tracing: about 196k evaluations against about 4.1M per-pixel traces at Quality 2, and the march's
  layers can unroll again. The client's shadow bands (40/160/640 yd, refreshed after 4/16/1024 yd of camera movement,
  wowdecomp `ShadowCache.cpp:21-24`) make cached visibility a good fit. Look: shafts coarser than the per-pixel trace;
  needs a harness comparison.
- **Near/far split:** march only `[0, D_near]` (300-640 yd, matching the far shadow band and the light limit) with
  noise, lights and shadows. Beyond it add a closed-form tail: height is linear along the ray, so each layer's density
  is piecewise exponential and integrates exactly (`e^a (e^(bL) - 1)/b`, at most two clamp breakpoints), with mean
  noise (`WithMeanNoise` exists) and far-map visibility or 1. Check against the CPU `LevelRayFogAt`. The same
  integral can replace the single jittered height sample per step, which aliases sharp falloffs at grazing angles;
  evaluate `DistanceCurve` at the step midpoint. Look: needs before/after images.
- **Full froxel volume (Wronski) is not recommended.** 160x90 cells are about 12 px at 1080p, which blurs the tree and
  character shafts the screen-space trace exists for; 64 slices over 5000 yd are 150+ yd thick at 1 km, where the
  authored height layers sit; ps_3_0 has no renderable volume, so the scan is a 6-pass ping-pong over an atlas of about
  0.9M fp16 texels plus a hand-blended two-slice lookup. Revisit only if the near/far split stalls.

### 6. Temporal filter quality (look)

Variance clipping (mean +/- 1 sigma, YCoCg or luminance) instead of the 3x3 min/max clamp; a 64x64 blue-noise tile with
an R2 per-frame offset instead of IGN plus the golden ratio; decorrelated jitter for the step offset, per-layer offsets
and shadow-trace offset (all share one value today).

### 7. Water and ripples

- Reflected fog (8 segments x 4 layers): smooth, so evaluate at lower resolution with a depth-aware upsample. Measure
  first.
- Ripples: scissor the disturbance pass to the disturbances' bounding rect and drop the 48-slot loop from the full-grid
  propagate pass. Saves nothing when contacts are spread across the grid.
- FFT: alternate tiles across frames once its share of the water time is known.

## Ruled out

- Pixel-projected reflections (Cichocki, ARTR 2017): our 16-step plus 4-refinement trace is already the low-quality
  class (0.95 ms at 4K on a GTX 1070, same as pixel projection), and projection needs `InterlockedMin` on a UAV and
  handles only planar, non-glossy surfaces, which does not fit FFT water.
- Hierarchical-depth SSR: improves long rays more than cost at 16 steps.
- Epipolar sampling and 1D min-max shadow mips: need rectified shadow maps the client provides only above shadow
  quality 2; the radial god-ray blur already covers the look.

## Open questions

- Which client Shadows setting do target players run? It decides how much the screen-space trace costs.
- What share of the water time is SSR, reflected fog, FFT and ripples?
- Does `LocalLightScattering` ever add light past the last fog layer?
- Is the default peak-normalised phase (`ClassicPhase=0`) intended? Raising `g` lowers total scattered light instead of
  redistributing it; only `ClassicPhase=1` is energy-normalised.

## Sources

- Wronski, "Volumetric fog" (SIGGRAPH 2014); Cichocki, "Optimized pixel-projected reflections" (ARTR 2017); Engelhardt
  and Dachsbacher, epipolar sampling (I3D 2010); Chen et al., 1D min-max mipmaps (I3D 2011).
