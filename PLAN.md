# PLAN

Roadmap for CoAGraphics, in the order agreed with the owner on 2026-11-10. Harness checks never establish in-game
behaviour; figures come from the Radeon 780M harness (native D3D9 or the owner's DXVK) or the owner's
`CoAGraphics.log` unless stated. Committed and installed: the 2026-11-10 performance session (`328a58c`, `8b956bc`;
the owner's verdict on its look is pending, Phase 1), the surface-light rectangle (`5cc13f1`) and the soft sum and
shadow lift under lamps (`d382061`, seen working in game 2026-11-11), the stencil composite (`13028ab`). Committed,
not installed: the Phase 3 transfer sheet (`24fcad9`, harness only). Phase 3 step 2, relative light strengths
(`5a745bb`, `a350256`), is installed and the owner saw it working in game on 2026-11-12.

## Rules for every item

- A meaningful behaviour change gets a harness check that fails before the fix and passes after it.
- Look-changing items need a before/after harness image and the owner's eyes.
- Ask before launching the client; install only when the owner says it is closed (AGENTS.md).
- Constraints: ps_3_0 only (no compute, no atomics, 32 temporaries; X4505 is the limit that blocks strided shadows in
  the lit noisy variants). Must run on native D3D9 and under DXVK/Proton (Steam Deck).
- Judge shader changes on the lit `flood8` rows of `vfog_harness --scene performance` (under 1% run to run); unlit rows
  move 20-25% with the APU clocks. Water: `--scene water-performance`. Copies: `--scene copy-cost`.
- **[R]** marks items from the review of 2026-11-10: unbuilt, read from code, costs are estimates. Checked: the sources
  and shaders here, DXVK's `d3d9_device.cpp` for alpha-to-coverage, wowdecomp for the client's use of it.

## Order

| Phase | What | Why there |
|---|---|---|
| 1 | One game session before anything new | Much has never been seen in game; more on top hides regressions |
| 2 | Measurement | Small; later decisions wait on the numbers |
| 3 | Settings that transfer | The owner's largest problem; every later look change is judged by its sheet |
| 4 | Cheap performance | Lights' screen projection done; the rest ends with a budget |
| 5 | Structure | Easier before more hooks arrive, on the baseline Phase 1 verified |
| 6 | Visuals, cheapest first | Each new cost lands under the governor of Phase 4; clouds (6.5) last |
| 7 | Big bets | Largest and riskiest; the froxel result decides two other items |

Performance programme steps (owner-approved 2026-11-10): 1 measure, 2 storm composite at 6 taps, 4 glow correction
done; 3 surface lights done (Phase 4); 5 froxel prototype and 6 grading lookup table in Phase 7; 7 water breakdown in
Phase 6. Pass timers (march, temporal, surface lights, composite) log as "fog pass gpu ms (median)" at `LogLevel=1`.

## Phase 1. One game session before anything new

Nothing new is built until the owner has played what is committed. Read `CoAGraphics.log` afterwards.

- **Glow correction (step 4):** the look of lamp and torch glow on the ground.
- **2026-11-10 lights (`328a58c`, `8b956bc`):** spell lights scaled apart from lamps, a flying spell's light at its
  head, lights filling the client's shadows on the ground.
- **LuxVelum ports (`PLAN-LUXVELUM.md`):** god rays steady while turning past branches; shafts and the own character,
  also on a mount or vehicle; the new defaults (`ClassicNoise=0`, `TransparentFog=1`, Outland and Northrend lamps at
  8); storms with lamps and noise; lamp-lit streets and spell lights of the stabiliser set.
- **Outland and Northrend region fog:** ported (`40da028`) and never seen there; may conflict with local config.
- **Waiting on the owner from earlier:** Back to live time, the multisampled depth skip, SurfaceLights near torches
  and its daylight fade, DXVK `customVendorId` (Engine inputs), the zeppelin map-ID log.
- **Surface-light rectangle and ground fix:** no dark patch under lamps at night, the pool under a lamp not too
  bright; at `LogLevel=1` the `scene copy`, `surface lights` and coverage lines; at `LogLevel=2` in a Reforged area at
  night against a stock area, the `local light N:` lines.
- **8 point lights with the patched exe** (Patch No. 67), if wanted: `LocalLights=1`, `SurfaceLights=0`,
  `FarClipMax=0`, a copy of the client folder, read the log for "far clip", "call site", "differs", "layout",
  "LightStay". Open: Reforged shader coverage, terrain lights 4-8 doubling, whether LightStay is patch No. 68 not 70.

## Phase 2. Measurement

1. **CPU timers beside the GPU ones [R].** None exist, and several dropped items wait "unless a CPU profile asks".
   Time the hooks (tree sway, unit and light walks, grass constants, grass copy draws) and log medians like the pass
   timers. That also says whether hardware instancing of the grass copies pays (`SetStreamSourceFreq` already passes
   through the wrapper).
2. **Pass timers in the harness (done).** `--scene performance` prints the pass timers the game logs. DXVK, 1920x1080,
   quality 2, street scene, ms: fog 3.4 (no lights) to 10.5 (flood8); march 1.2 to 7.1; temporal about 0.5; surface
   lights 0.58 with no light reaching, 0.6-0.75 with lights; composite 1.5-2.0. Owner's game, same night, 1920x1200:
   fog 11.8-13, march 2.8-3.6, temporal 0.9-1.0, surface lights 1.3-1.5, composite 6.6.
3. **The street benchmark understates the game; depth edges are the composite's cost.** Blades across the street
   (`thicket` rows) raise the composite 1.65 to 3.13 ms with lamps8 and 2.10 to 5.86 with flood8; march and surface
   lights do not move. In game, grass costs about 3.5 ms of composite (5.6 with grass down, 9.1 with it up); four open
   client shadow cascades raise nothing. Judge composite changes on the thicket rows.
4. **One minute of a game log is not a measurement.** The march and composite moved 30% for two minutes and back with
   nothing changed (owner confirmed): compare several minutes. The surface pass does not move with it.
5. **Power mode** does not explain the game's surface pass (rerun under Balanced: street within 10%). The mode is an
   overlay on Balanced, so `powercfg /getactivescheme` always says Balanced; read `ActiveOverlayAcPowerScheme` under
   `HKLM\SYSTEM\CurrentControlSet\Control\Power\User\PowerSchemes` (all zeros is Balanced, `ded574b5-...` Best
   performance) and record it beside every figure.
6. **Open: the surface pass at 1.3-1.4 ms in game against 0.6 here.** Not the shadow fill, power mode or grass. It
   points at the scene copy or the depth and scene reads at 1920x1200; read the `scene copy` column and the coverage
   line from the game, and `--scene copy-cost`.

## Phase 3. Settings that transfer (agreed with the owner, 2026-11-10)

The owner's problem: 145 keys, many competing or overlapping. Tuned in one place, they are wrong a few hundred yards
on, in other weather or at another hour. Players should install and play; few will learn what the sliders mean.

The cause: strength settings are absolute multipliers on effects whose visibility depends on the surroundings. A
lamp's glow is its intensity times the fog density around it (ground fog 35 times thinner in Borean Tundra at night
than in Tirisfal, per README); a light on the ground is added to a scene far brighter at noon than at night. The
owner's INI of 2026-11-10 differed from the defaults mostly in light strengths (`SurfaceLights=0.03`,
`LocalLightIntensity=0.59`, `AzerothLocalLights=0.25`, `SpellLightIntensity=0.41`, `DecodeLightColours=0`). The code
already patches this twice (lamps scatter with the region's intensity over its density; the surface pass scales by
daylight). Lumenveil's per-zone settings are the same patch by hand; not adopted, they hand the tuning to players.

### Step 1. Transfer sheet: done (2026-11-12, `24fcad9`)

`vfog_harness --scene transfer <folder> --data data/fogdata.bin --contexts tests/transfer_contexts.csv --ini
CoAGraphics.ini`: 64 contexts (Elwynn, Duskwood, Tirisfal, Tanaris, Hellfire, Nagrand, Borean Tundra, Storm Peaks; 07,
12, 19:30, 00; clear, storm), each rendering a lamp, a purple spell light of 8 yd, a character, trees and boards at
150 and 400 yd, shaded by that context's ambient and direct light. Under DXVK about 7 minutes; output kept in
`build/transfer-sheet` (open `transfer-sheet.html`). `--scene transfer-fog` is the CPU half alone (a second).
Sky light comes from the client's own tables, `tools/extract_client_tables.py` (the owner's 30 archives, re-extract in
a new session) and `tools/make_transfer_contexts.py` (blended as the decomp reads the client; matches the owner's
Elwynn log to a step; README has the evidence). `Data\patch-Y.mpq` on the owner's client is a saved web page, which the
client skips. Harness renders under shipped settings, not the game.

**As shipped, across the 64 contexts:**
- Fog optical depth at 150 yd spans 194 times (opacity 0.010 Borean Tundra by day to 0.87 Duskwood at midnight), at
  1000 yd 129 times, the fog's own light at 400 yd 2210 times.
- Lamp on the ground 3 yd away: 1 step of 255 (Hellfire noon) to 36 (Storm Peaks midnight storm), contrast 0.02 to 6.4,
  292 times; 2 steps or more in 58 contexts; noon clear 1-5, midnight 7-36.
- Lamp glow in the air: 0 to 29 steps, 2 or more in 39 contexts, under half a step in 12 (all eight Hellfire).
- **Spell light never reaches 2 steps** in any context (at most 1.5 in the air, 0.7 on the ground), matching the
  owner's screenshots.

**What each setting moves (doubled or flipped, per context):**
- `Exposure` moves nothing (all 64 use Classic layers, where `ClassicExposure` rules). `Ambient` is `ClassicExposure`
  to the power 2.2 on the fog's light: one knob, two names, two curves.
- `AzerothDensity`, `OutlandDensity`, `NorthrendDensity` equal `Density` wherever they act. `GroundFog` moves all 64
  contexts (median 2.00) and matches `Density` in 124 of 200 shared cases (58 of 60 in Outland). `Haze` moves 20
  contexts (at most 1.29), `FarFog` 4 near and 22 at the horizon, `StockFog=0` 6 at 1000 yd only (at most 26%),
  `ClassicNoise=1` 8 (halves). `MaxDistance` halved raises the near thickness in 55 contexts (up to 1.59): a range
  setting that changes density. `DataMode=0` moves all (0.13 to 4.9 times).
- Lamp glow, nine settings: `LocalLightIntensity` 2.00, region lamp values 2.0 in their region, `Density` 1.81,
  `GroundFog` 1.79, `ClassicExposure` 1.72, `LocalLightPhase` 0.3 to 0.6 gives 0.70, `DecodeLightColours=0` 1.52,
  `ColorSpace`.
- Lamp on the ground: `LocalLightIntensity` 1.92, `SurfaceLights` 1.63, `DecodeLightColours=0` 1.60; `Density`,
  `GroundFog`, `ClassicExposure` about 1.2 through the glow in front of it.
- Spell light: `SpellLightIntensity` doubled gives up to 3.2 in the air and 5.9 on the ground; `SpellSurfaceLights`
  moves 10 contexts. **`DecodeLightColours=0` raises the spell light on the ground 4 times at the median, up to 21:**
  the decode to linear light crushes a spell colour whose channels lie under 1. A lead for step 3.
- Fog light on the boards: `Ambient` doubled gives 4.6 at the median (up to 8.7), `ClassicExposure` 2.0, `Density`
  1.6, `GroundFog` 1.5. `SunScatter` moves 25-28 contexts, `MoonScatter` 2, `ClassicPhase` 20-28. `AzerothSkyColor`
  shifts hue in 23, `NorthrendSkyColor` in 14, `OutlandSkyColor` in none. `NoiseAmount`, `NoiseScale`, `LightShafts`
  move nothing a still image shows (nothing stands before the sun in the scene).
- Not on the sheet: `TransparentFog`, `GlowCompensation`, `ForeverGlow`, `ColorGrading`, `NoiseWindSpeed`, the interior
  settings (no client glow pass, no models after the world, no indoor context).

Step 2 takes its before from these files. Found on the way: Northrend's zone lights (Engine inputs below).

### Steps 2-6 (step 2 done)

2. **Relative strengths: done, installed, seen working in game (`5a745bb`, `a350256`, 2026-11-12).** 1.0 means "the
   same look in every fog at night, cancelled by daylight": the DLL scales each of the four light strengths every frame
   (lamps and spells in the fog, lamps and spells on surfaces). Glow gain of a lamp =
   `clamp((0.003 / e) * (0.045 / I)^0.5, 0.1, 128)`, of a spell the same with `^0.25`; `e` is the fog's mean extinction over the 40 yd around that light's
   distance from the camera (`LocalLightGlowGain`, from `LevelRayFogAt`'s optical depth), `I` = linear luma of ambient +
   half direct; surface gain = `clamp(0.42 * (0.045 / I)^0.5, 0.05, 2)`. `SpellSurfaceLights` no longer rides on
   `SpellLightIntensity`; the region lamp values set reach only (`fog_model.h`, README).
   Two corrections after the first install, both the owner's finding: (1) the first build raised the gain with
   brightness to hold a lamp's contrast constant, which made lamps 6 times as strong on the ground at Elwynn noon and 30
   times in the air; the owner's rule is that full sun cancels lamps while spells still show, so the gains now fall with
   `I`. (2) The glow gain was measured at the camera, so it swung while the client's fog start swept from 29 to 556 yd
   across the Orgrimmar gate (about 20 times the outdoor glow near interior blend 0.97, by hand from the owner's
   `LogLevel=2` log); it is now taken per light, at the light's distance. The owner confirmed after the second install:
   lamp lights look great, the gate flare is gone. An independent review found that opacity capped the optical depth, so
   a distant lamp in dense fog got the highest gain; fixed, with checks.
   Sheet (64 contexts, DXVK, not the game), lamp on ground contrast before step 2 / now: Elwynn noon 0.073 / 0.011, dusk
   0.81 / 0.32, midnight 1.78 / 0.91; lamp glow in the air at Elwynn noon 0.003 / 0.004 (first build 0.099).
   Open: Outland's night sky is bright, so its lamps are dim at night (Hellfire midnight on the ground 0.064 against
   0.355; an exponent of about -0.25 for lamps would soften it and cancel noon less: owner's call, not asked in game);
   Hellfire glow absent in the air (no fog within 40 yd: a fog-free lamp halo is a new feature, owner's call); a spell's
   reach still grows with `SpellLightIntensity` and spells stay under 1 step on the ground (step 3); indoors the thinned
   fog raises the glow gain up to the cap (the doorway check covers the sweep, not a room); nothing smooths the gain
   frame to frame. Full harness under DXVK: 3 failures, the two 4x water checks and the 4x gamma fixed-function
   silhouette check (the last also fails at `5a745bb`; the 4x early-composite depth check fails now and then); comments
   and format pass. The surface checks run at `Density=0` so the glow gain does not brighten the pixel they measure.
   The owner's installed INI keeps `LogLevel=2`; the repository default is 1.
3. **Spell lights on surfaces, to Lumenveil's look** (below): the same four strengths, done here.
4. **Overlapping chains removed.** `Density` x region densities x `Haze`, `GroundFog`, `FarFog`;
   `LocalLightIntensity` x `AzerothLocalLights` x `SurfaceLights`; `Exposure`, `ClassicExposure`, `Ambient`,
   `SunScatter`. The sheet already shows `Exposure`, the region densities and `Ambient`/`ClassicExposure` as
   duplicates or dead. An INI with retired keys still loads, and the log names them.
5. **Fewer settings in sight.** Players see the feature toggles, one look preset and one performance preset (the
   governor of Phase 4 item 5); the rest moves to an Advanced part of the window and the INI. Trees and bushes alone
   have 35 keys that can sit behind one strength each.
6. **Zone overrides as shipped data, not settings** (a file like `WindProfile.cdbc`) for the places the sheet still
   shows as outliers. It replaces the "settings per zone, hour and weather" of 6.4.

Every step changes the look everywhere: sheet before and after, then the owner's eyes. Taste stays a setting (hue,
grading, how strong the fog should be). The weather and clock preview of 6.4 would let the owner check hours and
weather at one spot.

### Spell lights on surfaces, to Lumenveil's look (owner's request, 2026-11-10)

The owner's two screenshots of one spot at night, a purple shield around the character: with Lumenveil the ground and
grass lie in a wide purple pool; with ours the ground stays dark green and no light shows (the screenshots are not in
the repository). The owner's log and INI of that session (`C:\Warcraft\WoW-Base`, map 0 at (-9953 264 47), 1920x1200):
8 lights uploaded, 3 to 4 from spells, the nearest 15.7 yd away, the surface pass running, `SpellLightIntensity=0.41`,
`SpellSurfaceLights=0.5`, `DecodeLightColours=0`; the installed DLL was `8b956bc`. So a spell light was most likely
there and too weak to see; the sheet confirms it is too weak in every context.

Why, from reading both sources (`..\coa-graphics-similar projects\Lumenveil\engine`: `vf_lamp_surfaces.hlsl`,
`vf_lamp_shadow.hlsli`, `vf_composite.hlsl`, `effect_lights.h`); not measured except where noted:
- **Albedo chroma.** Ours multiplies by the full-colour albedo estimate (at most 0.5), so purple on green has almost
  nothing to reflect. Lumenveil keeps half the chroma (`lerp(luminance, albedo, 0.5)`).
- **Falloff.** Ours is `1/(1 + a2 d^2)` with 1/64 left at its reach (3 yd plus three times the glow's half-diagonal, at
  most 20): half gone at an eighth of the reach. Lumenveil's `1/(0.7 s + 0.03 s^2)`, `s^2 = d^2 + near^2`, to a reach of
  6-24 yd with intensity up to 2.5: about 0.3 at 4 yd against 0.06 for ours at 8 yd reach.
- **Strength.** `SpellLightIntensity` scales the surface light as well as the glow, so 0.41 left the ground 0.2 of it.
- **Decode** (measured on the sheet): `DecodeLightColours=0` raises it 4 times at the median.
- **Grass.** Our normal comes from depth differences, noisy across blades; Lumenveil gives thin surfaces a facing of at
  least 0.6.
- **Shadows.** Lumenveil traces 16 depth steps toward the light; ours has none (6.2 item 2).
- **Night.** Lumenveil darkens night where no light reaches, which raises contrast; not proposed here.
- **Detection** differs (Lumenveil finds glows on screen, ours reads additive particles and mesh batches under
  `Spells\`); `LogLevel=2` lists each light.

Steps (each a look change: harness image before and after, then the owner's eyes at the same spot):
1. The sheet's spell-light measurements (purple light, 1/3/6 yd) are the "before"; a one-spot scene under the logged
   night light (ambient `FF122636`, direct `FF40546A`) at the owner's settings can be added if the sheet's is not close.
2. `SpellSurfaceLights` no longer rides on `SpellLightIntensity` (with step 2 of the phase).
3. A surface falloff of its own for spell lights, gentler than the fog's.
4. Albedo chroma kept in part, as Lumenveil does: spells first; whether lamps take it too is the owner's call.
5. A floor under the facing on grass and other thin surfaces.
6. Shadows from the depth trace (6.2 item 2).

## Phase 4. Cheap performance

1. **Cheaper composite at depth edges (done, `13028ab`).** A pixel whose depth no low-resolution tap matches runs
   `IntegrateFogAtDepth` at full resolution, and grass makes most such. `DebugView=4` (blue matching taps, green
   between, red marched again) and `--scene upsample-paths` showed few misses (street 0.05-0.96%, thicket 0.54-1.54%;
   owner's game 1.3% red in Elwynn grass, 3.7% in Duskwood canopies) but scattered, and a GPU group waits on its one
   marching pixel. Built: a stencil-marked second pass (1x devices only; multisampled devices keep one pass): the
   first pass discards pixels without a matching tap and marks the rest, the same shader draws the unmarked ones.
   Harness (DXVK, Balanced, Medium) thicket composite: flood8 5.90 to 2.98-3.15 ms, lamps8 3.18 to 2.11-2.13, unlit 2.41
   to 1.75-1.79; the image equals the one-pass image pixel for pixel (check). Played 2026-11-11: composite 2.6-2.9 ms
   against 5.3-5.5 (standing against moving, not controlled); the owner saw no difference. Open: 0.6-1.0 ms left above
   the no-march probe floor in the thicket; the same split on multisampled devices.
2. **Surface lights (done: `5cc13f1`, `d382061`; seen working in game 2026-11-11).**
   - `surface_light_area` projects each light's sphere (cut where the light alone falls under the shader's 1/1024) to a
     view rectangle; one rectangle around all is drawn, none means no pass and no copy. Pixel for pixel equal to the
     whole-view pass (harness check). The composite's scene copy is timed apart (`scene copy`) and a log line gives
     the rectangle's share of the view. A strength skip was dropped (the owner chose to keep faint lights exact).
   - Saves nothing while a lamp's sphere covers the view (Elwynn lamps: attenuation 0, 0.7, 0.03, sphere 46.1 yd),
     about 0.4 ms when no light reaches. It does not explain the game's 1.4 ms (Phase 2 item 6).
   - **Ground under lamps at night:** a dark patch where the client's own orange patch lay (read as albedo above 2,
     losing the whole light). Ground (normal Z 0.5-0.8) is now exempt from the already-lit fade and keeps the 0.5
     albedo cap.
   - **Soft sum (owner's decision):** `scene + albedo*(1 - shown)*(1 - exp(-light))`, `shown` the brightest channel of
     the client's illuminant or of `scene/0.5`, at most 1. Lumenveil's `PointLightGain`, read closely, would have
     brightened the patch (plain sum plus `2*sqrt(scene)*lamp`). Two equal lights 1.55 times one (2.03 before); lit
     ground takes 0.60 of the darker ground's lift. Expect lamps' ground light somewhat dimmer, most at dusk, on snow
     and cobbles; `SurfaceLights` is the lever. Checks write `surface-lights-night-*.png`.
   - **Shadow lift under lamps:** the client's terrain shader multiplies its own lamp patch by `0.7 + 0.3*lit`, so a
     tree shadow stayed dark under a lamp. Where a light reaches ground, the scene minus `0.5 x` shaded sky light is
     multiplied back by `1 / shade`, weighted `1 - exp(-light / 0.1)` (`ShadowLiftedScene`); the sky's share keeps its
     shadow. Ground only: shadows baked into the terrain's alpha map, and models or walls, are not covered. Harness
     `CheckLampLightUndoesTheClientsShadowOnGround`.
   - Full harness under DXVK at the last run: 1798 passes, 3 failures (the known 4x ones). Native not run. The owner's
     INI keeps `LocalLightIntensity=0.65` and `LogLevel=2` against the repository's 1.0 and 1. Lumenveil also darkens
     night where nothing lights a surface; ours does not (6.2 item 3).
   - **`lampdata.bin` (Lumenveil's lamp positions) is not taken:** positions, not lighting; it would add lamps the
     client's table lacks, needs its own MPQ extraction tool and a licence check. Revisit only if lamps are missing.
   - **The goal is Project Reforged.** If its models carry light records, our capture of the client's light table
     already sees them. One `LogLevel=2` night log in a changed area against a stock area settles it: count the
     `local light N:` lines, whether torches and braziers now have one, their attenuation. WMO lights may take another
     path (unchecked). Where the client's patch under a lamp comes from is not established; the client's light
     selection per terrain chunk (Phase 1, terrain lights 4-8) would have to be known before dividing its share out of
     the albedo, the principled alternative to the soft sum.
3. **Lit variants only where the lights project [R].** The march and composite could draw the unlit variant over the
   screen and the lit one inside the lights' rectangle. Pixels no light covers cost about 0.5 ms in the street, almost
   all in the march; near lamps can fill the screen. Wait for Phase 2 item 3.
4. **Skip fog that cannot show [R].** Every skip reason in `renderer.cpp` is a failure; none is "nothing visible".
   With a region at 0.15 density and no lights, a conservative CPU test of the level rays (`LevelRayFogAt`) can skip
   the march, temporal passes and composite.
5. **Frame-time governor [R].** The pass timers exist and nothing acts on them; the fog took 9.4-11.3 ms at Raven Hill.
   A budget setting lowers the march resolution, steps, wave rate or grass copies when the median exceeds it, with
   hysteresis (a quality switch discards the temporal history). Matters most on the Deck. Last in this phase, so
   Phase 6 has a budget.

## Phase 5. Structure

1. **`hooks.cpp` split** (about 3000 lines; needs the owner's go-ahead): move the `Test*` stand-ins first, then
   call-site patching behind `hook_patching.h`, then water, transparent fog, wind and spell lights, far clip; shared
   frame state into a `HookFrame` struct. One commit per step, `ctest` at baseline. A `FramePhase` state machine and
   splitting diagnostics out of `Renderer` and `WaterRenderer` come after.
2. **`DllMain` patches client code under the loader lock;** move to first `CreateDevice` (needs the game).
3. **As time allows:** share constant register numbers between HLSL and C++ (or check against the compiled constant
   table); one vertex shader per device. Harness: one `.cpp` per `*_checks.h`, a `TEST(name)` registry, WARP golden
   images, round-trip tests for `tools/convert_*.py` and the new extractor; `/analyze` and ASan presets (ASan component
   missing here).

Baseline: `ctest` shows 13 failures natively (the Multisampling=1 group or config-save, flaky); under DXVK 3
multisampling failures, and the 4x early composite check is occasionally flaky. Keep `CoAGraphics.pdb` of each install
for crash dumps.

## Phase 6. Visuals, cheapest first

### 6.1 Small ones

1. **Alpha-to-coverage under multisampling [R].** README limit: "Alpha-tested foliage stays jagged". The wrapper sees
   `SetRenderState`, so it can set the vendor toggle while the client's alpha test is on and the target is
   multisampled, and clear it for our passes. DXVK honours both (`UpdateAlphaToCoverangeAndAlphaTest`):
   `D3DRS_POINTSIZE = A2M1` on an AMD adapter, `D3DRS_ADAPTIVETESS_Y = ATOC` otherwise; `customVendorId = 1002` takes
   the AMD path. wowdecomp shows no client use of either. Risk: DXVK drops the alpha test while coverage is on, so thin
   leaves may thin out with distance; native drivers unchecked. Almost free; needs the owner's eyes.
2. **Grass root shading [R].** The grass vertex shader knows each doodad's root and tip and has the vertex colour
   (`dcl_color v2`); darken toward the root. No pixel cost.
3. **Caustics on shallow water beds [R].** The water pass already reads the refracted scene and has the FFT slope maps;
   brighten the bed by the slopes' convergence, faded by depth and absorption. One or two more taps.

### 6.2 Ambient occlusion and light occlusion

1. **Ambient occlusion [R].** The client and we have none. The surface-lights pass rebuilds view position and normal
   from depth, estimates albedo and runs before the composite: a half-resolution occlusion target applied there can
   darken the ambient share only. Guess: about 1 ms on the 780M; off by default at first. Sky and water masked.
2. **Occlusion for the surface lights [R].** They cast no shadows, so a lamp behind a wall lights the ground in front.
   A short depth trace toward the strongest one or two lights stops the leak, sharing the occlusion target. In view
   only. Lumenveil's is 16 steps per light; Phase 3 wants it for spell lights.
3. **Night darkness where nothing lights a surface [R].** Lumenveil darkens night where no light reaches
   (`NightDarkness() * Moonlit(albedoLuminance)`), so lamps stand out. A look change; judge after the soft sum is seen.

### 6.3 Water

1. **Step 7, water breakdown.** Shipped quality 2, basin scene 4.7 ms: waves 2.5-2.8 (FFT runs for any non-zero
   setting), scene reflection about 1.0, foam and specular under 0.1, ripples 0.1. In game: lake 4.56 ms, ocean 7.39.
   Reflected fog and the water-depth pass unmeasured. `WaterWaveRate=20` is the default (30 Hz possible after the
   owner's look). Left: water-depth pass (INTZ sampled while bound for stencil differs by driver, about 0.3 ms),
   alternate FFT tiles or fewer mips, reflected fog at lower resolution, atlas hitches (`EnsureAtlas`,
   `EnsureMasks`). Ripple optimisations dropped.
2. **Terrain in reflections that miss the screen [R].** They fall back to sky colours. `terrain_shadows.cpp` keeps the
   512x512 height window on the CPU; upload the heights too and march a missed reflection ray through the heightfield
   (8 to 12 steps) at the fog's colour. Cost only where the screen-space march misses.

### 6.4 From the Lumenveil review (`..\Lumenveil`, read 2026-10-23; unbuilt, mostly from reading code)

It matches us on cost and lacks our MSAA depth, image passes, FFT water, wind and real sun shadows. Both mods hook the
same five sites, so they cannot run together. Record decomp evidence in README before relying on its addresses.

1. **Tooling:** master compare key; per-section profiler; one-frame draw log to CSV; spots recorder for A/B work.
2. **Features, cheapest first:** corpse lights (`EnumVisibleObjects` `0x4D4B30`, dynamic flags `+0x13C` bit 0, health
   `+0x60`, SEH around the walk); rain rings on water; cloud shadows; glow into fog; weather and clock preview (use
   `GetType` `0x783B60`, declare `GameTimeSetMinutesPerSecond` as returning a float in ST0); effect lights by scene
   differencing (detection only first, about 1,000-1,400 lines, under 0.5 ms at 1080p; traps: early composite order,
   extra depth copy under MSAA, weather false lights). Settings per zone, hour and weather are replaced by Phase 3.
3. **Bigger:** baked town lamps out to 800 yd, wet surfaces and puddles, valley fog banks, sky filter.
4. **Robustness and packaging:** hash-span site guards, per-feature install with fault latching, grass wind without
   `d3dcompiler_47.dll` under Wine (binary token splicing), release script and install docs.
5. **Not adopting:** shaft memory, lamp tiles, sum-of-sines water (except as low quality), forcing MSAA off.

Ground relief from the same review is in Phase 7.

### 6.5 Volumetric clouds (from the friends-graphics review, 2026-11-11; unbuilt, read from code and docs)

We have no clouds. `..\coa-graphics-similar projects\friends-graphics` (a fork of `modern-wow-renderer`) ray-marches a
cloud deck the owner finds very good. Nothing was measured.

- **Licence first.** Neither repository has a LICENSE file, and this project is GPLv3. No code is copied: either a
  clean-room implementation from the published design (Nubis-style layered noise, dual-lobe Henyey-Greenstein,
  multiple-scatter octaves, powder term) or the author's written permission (`github.com/kasperfriend/friends-graphics`).
- **Design, from their code (`src/Effects/VolumetricClouds.cpp`, `docs/volumetric-clouds.md`):**
  1. A march into a half-float target at 20-100% of the screen (default 75%), upsampled with depth-aware weights; no
     temporal pass, interleaved-gradient jitter averaged by the upsample.
  2. A flat slab between two world heights, clipped by scene depth. 3D textures (shape 64^3, detail 32^3, a 512^2
     weather map); detail read only after a coarse probe hits; step length grows with distance.
  3. A light march of at most 8 `[loop]` iterations (`[unroll]` overflows 32 temporaries), lit by the client's sun or
     moon colour.
  4. Composited as `scene * transmittance + scatter` over the native sky, before the fog composite, so fog, halo and
     god rays see the clouds; the sun and moon sprites must show through.
  5. Weather: native rain, snow or sand pushes coverage toward overcast over about 18 s, clears over about 35 s. One
     global look first; per-zone climates only if needed.
  6. Second stage: ground shadows and cloud-filtered god rays from a 512^2 top-down shadow map sampled 8 times along
     each fog ray (the "cloud shadows" of 6.4.2).
- **Already here:** INTZ depth including the multisampled copy, the camera and sun or moon vector, `wind_model`, the
  weather read, `noise_volume`, the full-screen triangle and the scene copy at world done; their disc tracker and
  light-state workarounds do not apply.
- **Their pitfalls:** shadow flicker (snap the map origin to texels), grey wash (unitless amount with a contrast curve),
  light blinking when the sun left the frustum (we use world-space light), banding (dither), sky classified from raw
  depth (`rawDepth >= (MaxZ + 1) / 2`), coverage stuck overcast after a teleport, the Death Knight start zone gives
  clouds no depth (expect a per-map off switch for custom Ascension maps).
- **Conflicts to check:** order against the early and late composites; the temporal history seeing moving cloud pixels;
  sun glow and god-ray masks assuming a bright open sky; SMAA and bloom on a noisy half-resolution march; keep it a
  separate pass (32 temporaries).
- **Cost and size:** unknown; add `clouds` to the pass timers and a `--scene` row first. About 1,500-2,500 lines, 2-4
  sessions. Off by default; look-changing. Last in the phase, under the governor of Phase 4 item 5.

## Phase 7. Big bets

1. **Step 5, froxel prototype.** First here, since it decides items 2 and 3. Prototype only, behind a setting, default
   off: ps_3_0 needs a ping-pong atlas, so compare cost and look against the march before anything ships. Related
   older ideas (look changes, unbuilt): a `SunVisibility()` atlas (about 120x68x24 against 4.1M per-pixel traces at
   Quality 2; shafts are 0.9 ms of 3.5 unlit, the lights loop is the larger cost: flood8 11.8 ms against 3.5) and a
   near/far split with a closed-form tail checked against CPU `LevelRayFogAt`.
2. **Mist over water [R],** if froxels stay. A Forever look we lack. A camera-centred liquid map built like the snow
   map feeds a low layer above the water at night and dawn. The 32 temporaries make it hard in the current march.
3. **Checkerboarded march [R],** only if froxels are dropped. March half the texels per frame and let the temporal
   filter fill the rest; could save about half of the 2.7-2.9 ms march; ghosting is the risk.
4. **Tree bend by vertex shader rewrite [R].** `PLAN-TREEWIND.md` lists it as the alternative to the bone route; a
   smaller job now the grass and snow rewrites exist; the free registers and per-draw tree flag must be read from the
   client's M2 vertex shaders first.
5. **Ground relief (parallax occlusion; Lumenveil review):** splice a ps_3_0 fragment into the client's terrain and
   model shaders at `CreatePixelShader`, swap twins per draw, fake `GetPixelShader`; heights built at run time on a
   worker thread; models first. Needs wrapper plumbing we lack; order: draw log and profiler, splicer with a harness
   check, model heights, then terrain. About 40 height taps per pixel within 40 yd.
6. **Step 6, grading lookup table and final-pass merge.** Last, or dropped: it saves about 0.3 ms under DXVK at Deck
   resolution, and both passes are off by default.

## Engine inputs, open and unplaced

Take one when a phase touches its code, or when the owner asks.

- MSAA under DXVK on NVIDIA/Intel: DXVK answers RESZ only for AMD vendor IDs, so `ProbeDriver` finds nothing; the log
  and README name `d3d9.customVendorId = 1002`, untested in game (Phase 1). A DXVK patch would be the real fix.
- **Northrend zone lights (found 2026-11-11, unbuilt).** The client picks a zone's light from eleven outlines in the exe
  (`s_zones`, lights 914, 825, 959, 862, 1847, 1703, 1796, 1777, 1792, 1589, 1740); `fogdata.bin` holds those as 2 to
  23 yd spheres, so every Northrend zone's fog is the map-wide light's. A fix puts the outlines into
  `tools/convert_client_lights.py` as zone lights and regenerates from the owner's tables. A look change across
  Northrend; unseen in game either way. The sheet's two Northrend contexts already use the outlines.
- Weather: read `DNInfo.m_weatherIntensity` `0xD38B4C` (continuous 0-1) instead of the storm blend (look change).
- Liquid: keep the camera's liquid type and underwater depth (`SceneCamLiquidStatus` `0x780620`); classification by
  type id 9/17 and path substrings may not match CoA's DBCs.
- Glow through `EffectGlow::SetParameters` (`0x8BFDE0`) instead of raw byte writes; a time-of-day freeze touching only
  visuals (hook the end of `UpdateDayNight` `0x4F8410`); unit ripple events at `0x71CF16`.
- The client picks four lights per model and unlinks a model's light when it is not drawn (`M2Scene.cpp:876`); invisible
  GM models write no depth, so fog treats them as background.

## Record: measured, decided, done

**Glow correction (step 4, `vf_composite.hlsli`, `328a58c`).** For shadowed, authored-noise-free composites, `fog = sum
w (tap - G(tap ray)) + G(pixel ray)`; G integrates the light chord with four graded pieces of two Gauss points
(`kInnerGlowPieceFraction` 0.3). Harness `CheckGlowBetweenGrazingTaps`: error 0.0012 against 0.0202 for plain
interpolation. Benchmark before to after (ms): flood8 mid 11.40 to 10.40, high 14.60 to 12.64; lamps8 mid 5.11 to 4.94.
Unshadowed composites regressed (Q1 2.78 to 3.43, unconfirmed), so it applies only to shadowed ones. Owner log, Raven
Hill, 2026-11-10: composite 4.5-6.0 ms (6.0-9.3 before; not a controlled comparison), fog total 9.4-11.3.

**Performance**
- Folded layer terms in the march (`d9c04e9`): derived-flood8 14.29 to 11.85 ms. Dropped: hoisting phases per pixel and
  shared density variation (register limit), a single `exp` in `HeightProfile`. Not done: stepping over `[first layer,
  layers end]` (moves the look).
- Frame pipeline: one depth refresh per frame skipped when the client wrote no depth; surface lights write a second
  texture the composite reads; god ray copy from the lit scene. 780M under DXVK: colour copy 0.30 (1280x800) to 0.83 ms
  (1080p 8x); depth refresh 0.36-0.90 ms. Not seen under MSAA in game. Wrapper state shadowing and
  `D3DCREATE_PUREDEVICE` dropped unless a CPU profile asks (Phase 2).
- Not worth it: one low-resolution linear depth (under 0.1 ms), temporal variance clipping and blue noise (look change,
  no noise metric), anisotropy cache, overlay state block, tree sway mutex, lazy shader variants (18 ms for 42).
- Ruled out: pixel-projected and hierarchical-depth reflections, epipolar sampling, 1D min-max shadow mips.
- Open, unplaced: a single-tap temporal option (drops the history-depth pass, ghosting risk); noise volume mips (128^3,
  LOD from step length); late god rays merged with the composite copy (`GodRays=0` by default).

**Done and committed:** map ID from `DayNight::s_mapId` `0xD38AC4`, tree sway placement cache, bounds, player position
and ring-ordered light walk, grass shader cache by token hash, low-sun shadows, config parsed once, `/GL /LTCG`; test
seams, shared core and D3D helpers, `--filter`, evidence and format checks, `/W4 /WX`, release PDB; the LuxVelum
st0ny-dev port (`PLAN-LUXVELUM.md`) with the Outland and Northrend region fog (both wait for Phase 1).

## Open questions

- Does the owner give the go-ahead for the `hooks.cpp` split before the visual work (Phase 5)?
- Spell lights on surfaces (Phase 3): should lamps take the new albedo chroma and falloff too, or spells only?
- Phase 3 step 4: may `Exposure`, the region density keys and `Ambient` be retired now, given the sheet shows them dead
  or duplicate? Is "fog effects at their own distance" the owner's name for `TransparentFog`?
- Which settings does the owner re-tune most, if not the light strengths?
- Outland lamps are dim at night under the new gains (bright Outland night sky): soften with a lamp exponent of about
  -0.25, or leave it? Should a fog-free lamp halo give Hellfire lamps a glow in the air?
- Which client Shadows setting do target players run? It decides the cost of the screen-space trace.
- Is the default peak-normalised phase (`ClassicPhase=0`) intended? Raising `g` lowers total scattered light.
- Check `jealous-sound/coa-vfog` and Lumenveil's changes to shared files for newer commits.
- Which Lumenveil features first? Suggested: tooling, corpse lights, rain rings, effect lights step 1, model relief.
- Do shadow bands and `s_needsRebuild` matter for visibility caching?

## Sources

Wronski, "Volumetric fog" (SIGGRAPH 2014); Cichocki, "Optimized pixel-projected reflections" (ARTR 2017); Engelhardt and
Dachsbacher, epipolar sampling (I3D 2010); Chen et al., 1D min-max mipmaps (I3D 2011).
