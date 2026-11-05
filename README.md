# CoAGraphics

Graphics enhancements (volumetric fog and modern water) for the Ascension/CoA 3.3.5a (build 12340) Direct3D 9 client.

The DLL is a loader, a Direct3D 9 wrapper that makes the depth buffer readable, hooks into the world renderer, a
ray-marched fog renderer and a water renderer. Fog layers come from the Classic client's own
`LightDataGlobalVolumeFog` data wherever its lights cover the map; Outland, Northrend and the other maps Classic never
had get lights converted from the 3.3.5 client's own light tables, and the remaining maps use layers derived from the
3.3.5 day/night lighting. Water is shaded like WoW Forever's water, using its liquid presets, wave
patterns and foam textures.

## About this project

CoAGraphics grew out of CoAVolFog, a fork ([Raz0r1337/coa-vfog](https://github.com/Raz0r1337/coa-vfog)) of
[jealous-sound/coa-vfog](https://github.com/jealous-sound/coa-vfog). Its changes are summarised here; the sections
after it say how each works and what is checked.

**Fixes to flickering and vanishing fog lights**

- **LightStay** (`LightStay=1`, on by default) keeps the client from switching off a light whose model was not drawn.
  It is the same byte as patch No. 70 "lightstay" of St0ny's AIO WoW EXE Patcher.
- **Steady light slots**: the eight slots are held with hysteresis instead of being re-ranked every frame, and a
  jittering table read no longer drops the whole capture.
- **Lights in view**: a light stays on at any distance while its sphere meets the view, not within a fixed distance.
- **Smooth swaps**: lights fade in and out over about a second when they take or give up a slot. A light entering from
  the edge of the view shines at once.
- **Fog in the rain**: where the Forever kit has no storm fog for a light, the clear fog stays when it rains.
- **Storm fog over water**: no bright lines on grazing water in noisy fog near point lights.
- **Cheaper fog lights**: shorter light spheres and no second density sampling in the lit march.

**New lights**

- **Spell lights** (`SpellLights`): glowing spell effects light the fog around them in their particles' colour.
- **Lights on surfaces** (`SurfaceLights`): the point and spell lights also light the ground, walls and models around
  them, without shadows.
- **Lights follow their objects** (`LightsFollowObjects`): a lamp or fire the game stops drawing (beyond its fade
  distance, or hidden) takes its light with it.

**Shadows**

- **Light shafts are back** (`LightShafts`, `WorldShadows`): upstream removed them in a cost cut, and the sun or moon
  halo then shone through cliffs and terrain. They return in their own shaders, using a quarter to half of their former
  trace work.
- **Terrain shadows** (part of `LightShafts`): hills, cliffs and mountains block the sun or moon in the fog even outside
  the view, using the terrain the client has loaded around the camera.
- **Shafts without stipple**: the temporal filter averages the current jittered sun-shadow samples where it drops
  history, so the shafts do not sparkle when sun light dominates.

**Water and shores**

- **Shores and reflections**: the waterline washes in and out (`WaterShoreWaves`); shore foam has its own strength
  (`WaterShoreFoam`) and looks the same from any camera angle; sky reflections are fogged front to back, so night
  reflections are no brighter than the sky.

**Image quality** (all in the world viewport)

- **Anisotropic filtering** (`AnisotropicFiltering`, 16x by default): the game's Texture Filtering option never reaches
  the ground, buildings and grass, so their mipmapped textures are filtered anisotropically here.
- **Antialiasing, sharpening and bloom** (`Antialiasing`, `Sharpening`, `Bloom`): SMAA, a contrast-adaptive sharpen and a
  bloom halo, all off by default.

**Interface**

- **Target facing arrow** (`TargetArrow`): a small arrow on the rim of the target's ground circle points the way the
  target faces, as in the modern game.

## What it draws

- **Distance haze** thickening toward the horizon and fading with altitude, tinted by the zone's stock fog colour.
- **Ground mist** hugging the terrain around the player; zones with short stock fog get more.
- **Distance fog** replacing the stock linear fog on maps without Classic data. It turns opaque where the stock fog did,
  is lit by the direct light (warm at sunset), and fades into a horizon band on the sky. With Classic layers it returns
  only across the edge of Classic coverage, and where those layers are too thin to hide the far clip. Sky seen below
  the horizon, such as the void under the edges of Netherstorm, takes the fog of the sky mirrored above it; levelled
  to the horizon, its rays crossed the whole fog range and showed a white wall.
- **Sun scatter colour of the derived layers and the distance fog**: the zone's fog colour at the direct light's
  brightness, with a quarter of the direct light's own colour mixed in. A frame logged in Elwynn at noon (fog
  `FF4C788E`, direct light `FFFE8E40`, day 0.4921, map 0 without Classic data) scattered the raw direct colour, 1.61 0.45
  0.08 after the gamma decode, and turned the far fog salmon under a blue sky.
- **Forward scattering** (glow brighter toward the light) around the visible sun or moon sprite, using the
  Henyey–Greenstein phase function.
- **Light shafts**: terrain, buildings, trees and characters block the sun or moon in the fog, so the halo shines only
  where light reaches (`LightShafts`). Terrain blocks it from the client's loaded heights, in view or not. The client's
  world shadow maps include casters outside the view (`WorldShadows`); a trace through the image's depth covers the rest.
- **Local lights**: up to eight native point lights in view scatter into the fog with the client's constant, linear and
  quadratic attenuation. Light/ray intersections keep small light volumes between march samples. All point lights share
  one phase (`LocalLightPhase`), and a light brighter than 1 keeps its intensity.
- **Spell lights**: the additive particles and mesh batches (shields, auras) of each drawn spell model become one light in
  their colour times their texture's mean colour, sharing the eight slots (`SpellLights`, `SpellLightIntensity`).
- **Lights on surfaces** (`SurfaceLights`): the eight lights also light the opaque world around them, before the fog
  composite, from the depth buffer and without shadows.
- **Interior transitions**: the camera's native WMO blend reduces outdoor layers and sunlight, keeping the interior's
  own fog colour and range.
- **Authored noise** (optional, off by default): drifting fog banks where the Classic layers carry the modern
  client's noise, mostly in storms (`ClassicNoise`).
- **Density variation**: a two-octave noise field anchored in world space modulates the scene layers around their
  authored mean and drifts along world +X. Distance fog stays smooth so it cannot uncover the far clip. This is an
  artistic control, not a reconstruction of Classic's authored noise; layers with authored noise use that instead.
- **God rays** (optional): shafts of light through gaps in trees, buildings and terrain in front of the sun, from a
  radial blur of the bright sky; an open sky, its clouds included, gets none.
- **Modern water** on lakes, rivers, the sea and indoor pools: waves from a GPU FFT (fast Fourier transform) using
  Forever's wave tiles, refraction, depth absorption and in-scattering, foam on crests, along shores and in shallows,
  small waves washing in and out at the waterline, a sun or moon glint, and reflections of the scene with the sky as a
  fallback. The zone's own water colours can tint it (`WaterZoneColors`). Magma, slime, custom non-water liquids and water
  seen from below keep the client's look. Players, creatures, pets and mounts that wade or swim drag a V-shaped wake and
  stir the water as they start and stop (`WaterRipples`), in place of the client's flat splash and wake sprites
  (`WaterClientSplashes`).
- **Snow cover** (`Snow`): ground the zone paints with snow is raised by a layer of snow, so characters sink in and
  grass is buried, with ragged edges spreading past the painted ground.
- **Multisampling**: the game's Multisampling option is kept where the DLL can copy the multisampled depth
  (`Multisampling`); fog and water edges are resolved per sample.
- **Anisotropic filtering** (`AnisotropicFiltering`): the device wrapper turns the game's linear minification of every
  mipmapped texture into anisotropic filtering, so the ground, buildings and grass stay sharp at grazing angles. Textures
  with one level (render targets, shadow maps) and point-filtered samplers keep the game's filter. A higher level the game
  asks for is kept, and the level is capped at the device's `MaxAnisotropy`. The DLL's own passes are not affected.
- **SMAA antialiasing** (`Antialiasing`), **sharpening** (`Sharpening`) and **bloom** (`Bloom`), all off by default, applied
  to the world viewport after colour grading, so names drawn in the world are included, and before the later name and
  icon pass and the interface.
- **See-through effects in the fog** (`TransparentFog=1`, on by default): the fog is drawn before particles, spell effects
  and other see-through models, which take a linear fog fitted to it by their own distance, as in the stock client,
  instead of the fog of the scene behind them; lamp glows fade with distance (see Transparent fog).
- **Opt-in** (all off by default): Forever's per-light glow amount (`ForeverGlow`) and colour curve (`ColorGrading`) where
  Classic lights cover the camera, and an energy-normalised phase for the Classic layers' sun and moon scattering
  (`ClassicPhase`).

## How it renders

The march runs at quarter or half resolution (`Quality`). Each step integrates only its overlap with a layer's start and
end distances, so thin layers and partial boundary steps stay consistent across quality levels. Shader variants carry
light code only while a point light is uploaded, and carry noise sampling only while a Classic layer has noise.

**Temporal filter.** With `Temporal` above 0, samples vary between frames and a filter accumulates them. It rejects
history from a different surface depth or depth class (world, distant terrain, sky), and lowers its weight where animated
lighting changes the radiance, judged on the mean of the current 3×3 low-resolution neighbourhood so the per-texel
step jitter does not read as a lighting change. Where it drops history, it shows the average of the current samples on the pixel's surface
in the 3×3 low-resolution neighbourhood. So the jittered sun-shadow samples of the light shafts do not show as a stipple
when sun light dominates. Settings, map, screen-effect slot, projection and large camera changes discard history. With
`Temporal=0`, samples stay at fixed midpoints so a still frame does not shimmer, and the temporal passes are skipped.
Where no history applies (a newly exposed surface, the screen edge while turning, the frame after a discard), the filter
returns the same 3×3 average of the current samples.

**Upsampling.** The full-resolution composite upsamples the march through depth-checked taps. Ground seen at a grazing
angle is interpolated from the current march where the taps lie on the pixel's plane, the march is linear across them,
and no light's sphere meets the ray. Thin silhouettes that every tap misses are marched at full resolution. Under authored
noise, the lit composite interpolates the filtered fog wherever the taps lie on the pixel's plane (lights and curvature
aside), because its own march takes the noise at its mean (see Authored noise).

**Light shafts.** With `LightShafts=1` the march and composite use shadowed variants, which compile in the sun shadow
code and march the layers in a loop rather than unrolled. A step's sun or moon visibility scales only the direct light of
each layer. Emissive light and density are unchanged, so fog in shadow keeps its ambient colour. Visibility is taken once
per group of steps (every 4th, 3rd and 2nd step at `Quality` 1, 2 and 3), at a point jittered across the group and held
for it. It is skipped once the ray's transmittance is below 1/512. The lit noisy march evaluates it at every step, since
holding it across a group exceeds the 32 temporary registers of ps_3_0 there.

The marches also stop once `stepStart` passes the last layer's end, since no layer contributes beyond it and local
lights need a lit layer. The lit noisy shadowed march keeps marching to the far clip, because the stop's extra state exceeds
the 32 temporary registers of ps_3_0 there.

A point's visibility comes first from the client's world shadow maps (see Native world shadows): the near, middle and far
environment maps in that order. Each has one comparison tap (hardware PCF for `D24X8`) and a coverage that fades over the
outer 30% of the map, so the first map that covers the point decides it. Where no map covers it, a screen-space trace
projects the ray toward the light up to the far clip, clips it to the viewport, and checks 12 perspective-correct depth
intervals, stopping at the first occluder. For rays ending on opaque geometry with the light behind it, depth silhouettes
conservatively cover the hidden space behind them. So a nearer tree or character cannot reveal sunlight through a
mountain; sky rays keep a finite occluder thickness. The character map then darkens what it covers. Without the client's
maps (`WorldShadows=0`, its shadows off, or an unrecognised image) the trace covers everything.

The trace, the native maps and the terrain shadows share one direction: the client's shadow light at `0x00D43180`, the
direction its own shadow maps are cast along. It is used whenever that vector is finite and of length 0.7 to 1.2, whether
or not the maps are acquired. Before, the trace used it only with the maps and the sun or moon body otherwise, so the
shadows switched direction when the maps came and went. The owner's log from 2026-10-19 shows the switch: at 22:31:38 the
maps appeared with the light `(0.405 0.405 0.820)` while the moon's `toLight` was `(0.70 0.70 0.13)`, about 50° apart. The
fog's glow and direct light still come from the sun or moon in the sky.

That shared direction holds only while the sun or moon is within 25° of the client's shadow light (`PlanSunShadows`,
`shadow_light.h`). From 25° to 35° the native maps fade out: their weight (`cSunShadow.z` and `.w`, which the shader
multiplies into each map's coverage) falls along a smoothstep from 1 to 0, and the trace and terrain direction turns
from the client's shadow light to the sun or moon in step with it. Beyond 35°, as at a low sun, the maps are not used
and the trace and the terrain shadows are cast along the sun or moon itself. The depth of the image and the terrain
heights still block the light there, so a wall or a rock between the camera and a low sun does not let the sun's glow
shine through. Trees, characters and casters outside the view cast no shaft shadow while the sun is that low, and a sun
outside the view cannot be traced against what is on screen, so the shafts a map would draw fade out over those 10°
rather than ending at once. The fade is checked by the harness (`CheckShadowMapWeightFades`), not seen in game.

An earlier build switched the shafts off beyond 25° instead, which left the sun's in-scattered light unshadowed: on
2026-10-22 the owner saw the Stormwind sunset glow (sun about 10° up, the Classic layer 0 lobe of `g 0.94`) through a
wall in that state, and the shafts vanished within one tick of the time-of-day slider (17.88 to 17.80). The harness check
"with the client's shadow light more than 25 degrees from the sun" fails with the shafts off there (the fog keeps all
its sunlight behind a cliff) and passes now (0.04 kept).

**Terrain shadows.** With `LightShafts=1` the terrain blocks the sun or moon from its heights, not from the image. So a hill
or cliff outside the view or behind the camera still shades the fog. The renderer tracks the loaded terrain chunks (see
Terrain heights) in a window of 64×64 chunks, 512×512 outer vertices 4.17 yd apart, about 2133 yd across, centred on the
camera's chunk. Each frame it checks every eighth chunk, so each chunk is checked once in eight frames, and re-reads a
chunk's nine rows of nine outer vertices when its address changes. Unloaded chunks hold no terrain. The window moves once
the camera is more than 8 chunks from its centre. It keeps the chunks that stay inside the new window and reads only those
that enter it.

From the heights and the direction to the visible sun or moon, it computes each vertex's shadow height: the lowest a point
above it can be and still see the light past the terrain. This is one sweep from the light's side:
`S = max(h, S' − d·tan e)`, with `S'` interpolated on the previous line one vertex closer to the light along the dominant
axis, `d` the horizontal length of that step and `e` the light's elevation. Every line of a sweep takes the same fractional
offset, so each line is a lerp of two shifted copies of the previous one. The map is recomputed at once when the window
moves or the light turns by more than about 0.1°. It is also recomputed when terrain changes: a quarter second after the
last change (a map tile's 256 chunks appear together, checked over eight frames), or a second after the first change under
constant streaming. The window is uploaded as 512×512 `R32F`; until then the previous map stays in use.

A march sample takes the bilinear shadow height `S` at its world position and keeps `saturate(0.5 + (z + 1 − S) / 2)` of
the light (z in yards). A sample outside the window keeps the light. This visibility multiplies that of the other
occluders, and the image trace is skipped where the terrain already blocks the light. Only the outer vertices are kept, so
a peak at a cell's centre vertex counts at the height its corners give. Holes cut into the terrain for caves still block.

Every full-screen pass draws one triangle whose edges lie outside the viewport (NDC −1.5 and 4.5), so it covers every 4x
sample of the edge columns. Direct3D 9 maps NDC −1 onto the first column's centres.

**Local lights.** The captured diffuse includes the M2 light's animated intensity (see World lights), so a light brighter
than 1 carries it in its colour. In linear light, a colour whose brightest channel is above 1 decodes only its
chromaticity, `(c/peak)^2.2 · peak`, since the modern client scales light colours linearly. This is a choice; the kit
lacks its CPU packing. `DecodeLightColours=0` skips the decode for every colour. The logged torch colour, about (1, 0.27, 0.03) after the decode,
is what an authored (1, 0.55, 0.20) becomes under it. Every point light scatters with one Henyey–Greenstein phase, normalised to 1 toward the light, with
g = `LocalLightPhase`. The modern fog-light kernel 6227851 reads one g for all local lights. The lights scatter with a
gain of the region's intensity (`AzerothLocalLights`, `OutlandLocalLights`, `NorthrendLocalLights`) over its density
(`AzerothDensity`, `OutlandDensity`, `NorthrendDensity`, at least 0.05), here and in the fitted stock fog of see-through
models, so a region thinned to 0.15 keeps the glow of its lamps. Outland and Northrend start at 3: their converted
layers are far thinner than the old world's (Borean Tundra at night 0.00013/yd of ground fog against Tirisfal's 0.0045
in a tester's log), and the tester needed three to four times the intensity there. The region's intensity also scales
each lamp's cutoff radius, as for a lamp that much brighter, so the glow reaches further with the slider (a tester found
Outland's lamps at 24 barely stronger than at 8 before); Azeroth at 1 keeps its radius. The region values range to 128 in
the INI, and the settings window's sliders are logarithmic up to 96.

The eight uploaded lights are chosen by a stabiliser rather than re-ranked from scratch each frame:

- **Candidates.** Each capture ranks the lights whose sphere meets the view frustum, at any distance, by their irradiance at
  the camera, into a pool of up to 24 candidates. Equal irradiance ranks the nearer light first. The view test comes before
  the ranking, so a light whose sphere lies wholly outside the view takes no slot. A light behind or beside the camera
  still competes while its sphere reaches into the view, which a sphere of up to 200 yd does from far away. A candidate is
  the nearest light already known with the same node address within 2 yd plus 40 yd for each second since the last
  capture that was not held (see Gaps and frames), so a colour or attenuation that animates between captures stays one
  light, and so does a light that moves at up to 40 yd/s at any frame rate, also across held captures that span up to 1 s,
  while an address the client reuses 10 yd away within one frame at 60 fps is a new light.
- **Taking a slot.** A free slot goes first to a challenger that evicted its holder, otherwise to the visible candidate with
  the highest smoothed priority (0.3 s). An occupied slot changes hands only when a candidate's smoothed priority exceeds
  1.5 times the weakest slot's, for three captures in a row.
- **Fades.** A light that takes a slot fades in over 1 s along a smoothstep curve, so neither end of the fade jumps. It
  shines at once in the first capture after a reset (map change, start, a gap of more than 1 s), and when it enters the
  candidates with its centre outside the view. Its sphere then reaches in from the edge, as when the camera turns back
  toward it. A light that leaves the client's table, is evicted, dims to nothing or is crowded out of the candidate pool
  fades out over 1 s along the same curve instead of vanishing.
- **Swaps.** The evicted light fades out before its challenger takes the slot and fades in. Swaps run side by side while
  every fading-out slot has a challenger waiting, so a light that left holds back further swaps until its slot is free. An
  eviction that no challenger waits for any more, because the challenger left or took a slot freed elsewhere first, passes
  to the strongest other visible candidate whose smoothed priority exceeds 1.5 times the evicted light's, such as the
  challenger back after missing a capture; with no such candidate for three captures in a row it is called off, and the
  evicted light ramps back up instead of fading out and in again. A slotted light that is not evicted keeps its slot
  through two missed captures in a row even at nothing, so a light that has just taken a slot and drops out of one
  capture does not hand it back to the light it replaced. A
  light that has faded keeps its smoothed priority while it is still captured inside the view, and fades back in if it wins
  a slot again. The slot it lost goes to its challenger, so the challenger is not undercut by its momentary rank.
- **Forgetting.** A slotted light whose last captured sphere lies more than 5 yd outside the view is forgotten at once, freeing
  its slot in the same step, and re-enters later as a new light. One that only jitters across the view's edge, or moves out
  of the view while its last captured position still meets it, dims over 1 s and ramps back if it returns within that time.
  So a sphere on the edge does not hand its slot back and forth.
- **Gaps and frames.** A capture that walked nothing (damaged table, table changed, read fault) keeps the previous set for up
  to eight captures; they do not advance the clock of the identity radius and the priority smoothing, which counts the
  time since the last capture that was not held (at most 1 s), while the fades keep the frame time. The stabiliser
  steps once per captured world frame. When the frame inputs are read a second time in
  the same frame (the early composite and the whole-frame pass can both read them), the stabilised set is replayed. So
  hysteresis and holds count frames.
- **Range.** There is no camera-distance bound: a light stays while its sphere meets the view and the client keeps it in
  its table, which the client's own doodad loading bounds. The march evaluates a far light's scattering only on the steps
  where the ray crosses its sphere. `LightStay` keeps the client from disabling a light whose model is not drawn (see World
  lights). Otherwise that would also end its fog glow the moment the source leaves the frame.

Fog renders once per frame, before screen effects and the UI: after the world, including its late geometry and the native
sun/moon glare, or, with `TransparentFog=1` (the default) above water, at the end of the liquid pass, after the glare and
before the see-through models (see Transparent fog). While it draws, the stock fog is pushed out of range for the world
render and restored afterwards. Frames without volumetric fog keep the stock fog. An unexpected draw failure restores it
on the next frame. The client's glow
(`screen + g·blur²`, with `g` from the day/night light) runs afterwards and would bleach bright fog to white, so fogged
pixels are pre-compensated with the live glow amount while the fog blends in linear light over a scene copy
(`ColorSpace=1`, the log line `fog blend: linear over a scene copy`). The gamma blends (`ColorSpace=0`) and the
fixed-function blend without a scene copy leave the fog uncompensated; the god rays, which need a scene copy, are
compensated whenever they are drawn. God rays use the display highlight range left after the glow, and
need a scene copy and the low-resolution copy of the world viewport their mask is drawn from; without either they are
omitted, and a failed low-resolution copy is logged once. When the late god ray pass after the early composite fails,
the fog's temporal history stays as the early composite left it, since that pass never writes the fog or history
targets.

**Lights on surfaces.** The client lights the ground and buildings only with the sun or moon and its ambient colour; its
point lights reach models alone. With `SurfaceLights` above 0 and a light uploaded, one full-resolution pass after the march
and before any scene copy or the composite copies the world viewport and redraws the pixels a light reaches. It rebuilds
each pixel's view position from the depth copy. Its normal is the cross product of the smaller of the two depth differences
to its horizontal and vertical neighbours, so silhouettes do not bend it. Each light adds
`colour · falloff · fade · facing`, where the falloff is the march's `1/max(a0 + a1·d + a2·d², 1)`, `fade` is its sphere-edge
fade, and `facing` is wrapped by 0.25. The light falls on an albedo estimated from the scene in linear light (display gamma
2.2), divided by the client's own illuminant there: ambient plus direct colour times the facing to the sun or moon (at least
0.02, with the albedo at most 0.5). The pixel becomes `scene + albedo·light·SurfaceLights`.

A pixel whose estimated albedo in any channel exceeds 1 is brighter than ambient plus sun could make any surface, so the
client lit it with its own point lights or it glows (a torch, a banner next to a lamp, a character beside a fire). Its
albedo is faded to nothing between 1 and 2, so it is not lit a second time; before this the estimate was capped at 0.5
there, and such models took the light on top of the client's own and shifted in brightness with the camera angle (the
normal comes from the depth differences). Between 0.5 and 1 the cap stays.

The whole pass is scaled by how visible the lights are: the brightest uploaded light colour divided by the daylight
(linear luma of the ambient colour plus half the direct colour, at least 0.02) is the share a light adds to a surface right
at it. Below 3% the pass and its scene copy are skipped, above 10% it runs at full strength, with a smoothstep between.
That follows the client's own light colours, so a dark zone keeps it on at noon and a bright one turns it off sooner.

So ground lit dimly at night takes a point light at nearly its full colour, while in daylight the same light adds to a much
brighter scene and shows less. The sky, pixels beyond every light sphere, and pixels a light adds under 1/1024 to are
discarded and keep their colour. Translucent effects drawn after the pass are not lit. There are no shadows. With multisampling, a lit pixel writes one colour to all
its samples.

When the composite blends in the shader (linear blending or god rays), the pass writes the whole lit scene into a second
texture, unlit pixels copied as they are, and the composite reads that texture, so the composite's own copy of the target
is not made (one full-resolution colour copy fewer in lit frames). The frame is bit-identical to lighting the target and
copying it again (harness check). Measured with `--scene performance` at 1080p on the Radeon 780M, native: no change
(within 0.1 ms of 11.4 ms for eight flood lights); under the installed DXVK: about 0.3-0.7 ms less on a 6-15 ms frame
(four runs each, 5-10% run to run).

The god rays' low-resolution copy of the scene is taken from the same texture (the lit scene, or the composite's copy of
the target) instead of a second resolve of the target; the frame is bit-identical (harness check, single-sampled and
under DXVK at 4x). It measured no change at 4x and 8x at 1080p under DXVK (within the 5-10% run to run); the most it can
save is the difference between a multisampled and a single-sampled copy, 0.1-0.15 ms at 8x by `--scene copy-cost`.

**God ray mask.** The rays are the bright sky around the sun, masked at a quarter of the viewport. A mask texel covers
4x4 pixels, and one depth tap with one colour from a 4:1 copy let a thin branch switch the texel on and off as it slid
across, so the rays trembled while the camera turned. The mask now averages 16 depth taps per texel (the sky share of
its footprint) and reads four texels of a scene copy at half the viewport, each through the brightness threshold and
weighted by the sky share of its own four taps; the harness checks that a one-pixel occluder moving across the
footprint keeps the mask at three quarters, that one beside the footprint leaves it open, and that the four copy
texels are thresholded separately. The copy has four times the pixels (half the viewport's width and height instead of a
quarter) and the mask pass takes 16 depth reads instead of one; neither was timed here. Upstream's change (LuxVelum
`51a748b`); seen in the harness only.

**God-ray shafts.** A tester found that the God rays slider brightened the whole sky around the sun like Sun scatter
(upstream, 2026-10-27): the radial blur of an open sky is a round glow, and the rays only showed as the edges of the
occluders cut into it. The mask now also writes each texel's sky share into its alpha, which the radial blur carries
along with the colour, and a third pass after the blur keeps only the shafts. For each ray texel it reads the blurred
sky share along an arc around the sun, twelve taps 2.5 degrees apart on each side, takes the smallest on each side and
keeps the part of the texel's rays that its sky share holds above the larger of the two. An open sky, clear or brighter
toward the horizon, and the one-sided edge of a horizon or a mountain keep nothing; a gap between trunks or leaves, with
shadow on both sides, keeps its shaft. The arc compares the sky share and not the rays' colour, because a sky that
brightens toward the horizon, as at dusk, peaks along the arc above the skyline: the colour-based first version of the
pass brought back 1.8/255 on average there, up to 23/255, in the harness. Clouds and the skybox's models write no depth,
so they count as open sky: gaps between clouds give no shafts. So God rays never darken and leave the sun's glow to Sun
scatter and the client; within 30 degrees of a gap that spans the whole arc, next to the sun, they add nothing either.
The pass reads 25 bilinear taps at a sixteenth of the screen's pixels and writes into the blur's first target, which
both composites read. The harness renders the sun over a clear sky, over a dusk sky brightening toward the skyline below
it, behind thirteen trunks and behind a canopy of leaves at God rays 0, 1 and 4 and saves the images
(`god-rays-*.png`): at 4 the clear sky changed by 40.5/255 on average before (up to 91/255) and both skies by 0/255 now,
while the shafts between the trunks reach 0.36 in luma and nothing darkens (DXVK on the Radeon 780M). Upstream's change
(LuxVelum `8550566` and `4358ace`); seen in the harness only.

**The own character.** A tester found rays on and around the own character that moved with it, and none from the sun
over the open plains of the Barrens and Durotar (upstream, 2026-10-28). The character stands in front of the sun more
often than any tree: the mask counted it as an occluder like any other, and the shaft pass kept the open sky on both
sides of it as a gap, so rays fanned out around the character wherever it walked. The DLL now reads the position of the
unit the camera follows at `0xCD7778` (the camera position when the code that stores it there differs, see the
evidence below) and treats an upright cylinder around it as the character: 3 yards in radius, from half a yard below
the focus to 5 yards above. Each test rebuilds the world position of a depth sample from its pixel and depth and checks
it against the cylinder, after a screen rectangle around the projected box that holds the cylinder; when a corner of
that box comes within a quarter yard of the camera's plane, the rectangle is the whole viewport. The mask writes a
second target, the sky share with the character's samples counted as open, which the radial blur carries along with
the first; the shaft pass takes the minima along its arcs from it and the texel's own share from the first, so the
character no longer makes a gap, and trunks behind and beside it keep their shafts. Both composites draw no rays on the
character's pixels, so those shafts do not shine through it. A camera zoomed in closer than 3 yards beside the focus,
within the cylinder's height, narrows the cylinder to stay a quarter yard clear of it; below three quarters of a yard,
as in first person, beyond 100 yards and for an unreadable focus nothing is excluded. The second share is a second
render target in the ray format; on a device without two simultaneous render targets the character casts shafts again,
which the log says, and only the rays on its own pixels stay off. The harness renders a character 8 yards ahead, just
below the sun, 2.5 yards beside it over the ground, in front of the thirteen trunks and between trunks 4 yards to either
side at its own distance, and 2 yards ahead, at God rays 4 (`god-rays-character-*.png`): without the focus the sky
around it changed by up to 42/255 and 92/255, the character brightened by up to 114/255, and at 2 yards by up to 96/255;
with it all four stay at 0/255, the far trunks' shafts still reach 0.36 in luma, and the near trunks' shafts reach 0.052
with or without the character (DXVK on the Radeon 780M). The mask grows to about 729 instruction slots at a sixteenth of
the screen's pixels, the blur to 72, the late composite to 100 and the medium composite to 1018 (none past the ps_3_0
limits). Mounts and vehicles larger than the cylinder, other players and creatures still cast shafts, ground and leaves
inside the cylinder count as open sky, and the composites test one depth per pixel, so under multisampling the rays'
edge along the character is not antialiased. An open plain with nothing in front of the sun has no shafts by design; the
glow around the sun there is Sun scatter's and the client's. Upstream's change (LuxVelum `e7bda6b`); seen in the
harness only.

**God ray blur and banding.** The rays are then
blurred toward the sun in two passes of eight taps: the first steps 0.075 of the way to the sun per tap, the second an
eighth of that. The two passes only form one smooth falloff if the second decays by `0.9^(1/8)` per tap, so that the
product of the weights is `0.9^(distance / 0.075)`; with `0.9` per tap in both, the weight rose by 88% at every eighth
tap, which showed as a fan of stripes behind every branch (`god_ray_blur.h` holds the parameters, the harness checks the
kernel never rises and that the shader follows it). The ray targets are fp16 where the device filters them (the scene
copy they start from stays 8 bit), so faint streaks no longer step in 1/255 levels that the ray strength multiplies. The
composite that blends over a scene copy (and the late god ray composite) rounds only the difference it adds to the scene
(the fog and the rays) to whole display levels with a per-pixel noise (`InterleavedGradientNoise`, fixed in time), so
smooth fog gradients do not contour in the 8-bit target; pixels the fog does not touch keep the scene's own value exactly.
The fixed-function blend path (no scene copy) and the colour grading pass add a static noise of half a code either
way to what they write instead (`DitheredForDisplay` in `vf_pixel_noise.hlsli`, the same noise): the 8-bit codes then
average to the continuous value over a gradient, and a pixel the fog or the grade does not change keeps its exact code,
since the noise stays below half a code. The grading rereads the 8-bit frame through a curve that steepens by up to 1.22
codes per code, which would round the dithered fog into steps again. In the harness, a fog ramp from code 30 to 38
through the fixed-function blend is off by up to 0.52 codes per column without the dither and 0.06 with it, and the
Stormwind noon curve over code stripes by up to 0.58 codes per stripe without it and 0.13 with it (DXVK on the Radeon
780M); the curve texture's filtering adds up to 0.05 codes to the largest single-pixel error. Upstream's change
(LuxVelum `8fa1168`, the dither part; its scene-copy composites keep the rounding described above).

**Glow and colour grading.** With `ForeverGlow=1`, the DLL rewrites the glow byte the client hands the glow composite (see
Glow feed) once per frame, before the fog composite, to `lerp(client byte/255, Forever glow, t)`, where
`t = saturate((coverage − 0.5)/0.5)` from the Classic lights' share of the blend. It restores the byte at the frame end. It
never writes the day/night glow, and writes nothing under the ghost effect or an unexpected effect graph. `GlowCompensation`
uses the byte the composite receives, so a glow above 1, which wraps, is compensated as drawn.

`ColorGrading` passes a copy of the world viewport through the blended 32-entry curve per channel, at the end of the world
render, after the world-name text (`0x4F9218`) and the glow and before the later name and icon pass (`0x4FB042`) and
the interface. It writes `lerp(scene, curve(scene), ColorGrading·t)`. The ghost view, the view
under water, and a view without a grading key are left alone.

**Antialiasing and sharpening.** With `Antialiasing=1` or `Sharpening` above 0, the world viewport captured at the world-done
hook is copied at the frame end. SMAA 1x (Jimenez et al., the `SMAA_PRESET_HIGH` settings of the reference `SMAA.hlsl`,
fetched at build time) finds luma edges into one texture. It computes blending weights into a second, from its area and
search tables (`D3DFMT_A8L8` and `D3DFMT_L8`), and blends the neighbourhood back into the world viewport. Reads and writes
happen in linear light where the device supports sRGB reads and writes of the back buffer format. It runs with or without
the game's multisampling, and smooths the alpha-tested edges multisampling misses.

Sharpening then redraws the smoothed image (or the copy) through a contrast-adaptive filter after AMD FidelityFX CAS. From
the minimum and maximum of the 3×3 neighbourhood, `w = −sqrt(saturate(min(lo, 2 − hi)/hi))/lerp(8, 5, Sharpening)`, with
`lo` and `hi` the cross plus box minima and maxima. The pixel becomes `(e + w·(b + d + f + h))/(1 + 4w)`. Flat areas stay as
they are, and edges that already have contrast are sharpened less than soft ones. A frame without a world render is left
alone. With all three off, nothing is drawn and the textures are released.

**Bloom.** With `Bloom` above 0, the copy is also reduced to five levels at 1/2 to 1/32 of the viewport, in
`D3DFMT_A16B16G16R16F` where the device filters and blends it (`D3DFMT_A8R8G8B8` otherwise). The first level keeps the part
of each pixel's linear colour (display gamma 2.2) above a soft threshold: `max(soft, m − 0.7)/m` of the colour, with `m` its
brightest channel and `soft` a quadratic knee from 0.5 to 0.9. It is averaged over four bilinear taps weighted by
`1/(1 + luma)`, so single bright pixels do not flicker. Each further level averages a centre tap and four diagonal ones, and
each level is added to the one above it through a 3×3 tent. After antialiasing and sharpening, the first level is divided
by the level count, scaled by `Bloom`, returned to display gamma, and blended onto the world viewport as a screen
(`src·(1 − dst) + dst`), so it never clips. Colours below the threshold, the interface, and the client's own full-screen glow
(which runs earlier in the frame) are left as they are. The halo adds to that glow.

**Transparent fog.** Fog drawn once after the world gives every see-through effect the fog of the surface behind it, where
the stock client fogs it by its own distance. With `TransparentFog=1`, above water, the glare and the fog are drawn at the
end of the liquid pass instead, before the see-through models on the camera's side. Every later M2 batch fog call then gets
a linear fog with exponent 1. That is a least-squares line in planar view depth (the depth the client's M2 shaders fog by),
fitted through the volumetric transmittance along 15 rays within 100 yd. The fitted in-scatter, point lights included, is the
colour where the batch uses its lighting colour. Additive, modulate and modulate-2x batches keep black, white and grey. When
those 100 yd keep more than 90% of the light, the fitted range grows, smoothly with how clear they are, toward the depth
where the transmittance along the view axis halves, reached at 99.5%, and never past the far clip; when even the far clip
keeps 99.5%, no line is fitted and the effects stay unfogged. God rays are added at the end of the world render. Batches the client draws unfogged (a material with the unfogged flag, a
lighting without fog), among them lamp glows, wisps and other glowing effects, take the same line with the colour of
their blend mode (blend modes 0 to 6, the client's fog table; a batch of any other mode stays unfogged): drawn after the
composite they had no fog at all and shone through any fog at any distance, where the fog drawn after the world dims
them by the fog behind them (a tester's screenshots of lamps in Duskwood in rain and a lantern in fog, upstream,
2026-10-25). It is on by default (`TransparentFog=1`) since a tester played upstream's TEST Build-2-2 with it in
Duskwood, Darkshore and Orgrimmar (2026-10-27) and reported only improvements: rain, the Darkshore wisps and the
Orgrimmar fire pits stayed visible in the fog, and lamp glows faded with distance; it has not been played in game
here. `TransparentFog=0` remains the fallback if distant effects look wrongly fogged. Under water, with `StockFog=0`, a
debug view or the sun marker, or when the early composite cannot run, the fog is drawn after the world.

**Water.** The client draws every water and ocean surface, and nothing else, inside one call of its liquid renderer. Before
that call the DLL copies the scene colour and its linear depth, clears the stencil, and turns on stencil writes of 0 and
depth writes for the whole call. Each water draw is classified from its liquid type (lake, river, ocean, indoor) and tags its
pixels with that class in the stencil. After the call, the DLL simulates the waves of the classes drawn this frame, copies
the water surface's depth, and draws one stencil-tested full-screen pass per class that replaces the client's water colour.
This runs before weather, alpha models and the fog, so the fog is integrated against the water surface. Frames without
queued water skip the pass entirely. The client queues water it cannot see: it culls liquids only against the view
frustum and a coarse terrain horizon, never against trees, models or most buildings (in Duskwood most of the zone
reported `Drawing (lake)` with no water on screen), so an occlusion query counts the pixels of the client's water draws in every pass. While the newest finished count is 0 the
pass is not armed and the client draws its own (hidden) water; the first count above 0 arms it again, so water coming
into sight shows the client's look for the one or two frames the count takes to arrive. A frame without queued water
forgets the count, so water entering the view is shaded at once.

The waves are a GPU FFT (Tessendorf/Phillips spectra with Forever's tile parameters, 256² or 128² per tile, normalised to
256² so the wave height does not depend on the resolution). It yields slopes, slope variance and a persistent foam state per
tile. The shading ports Forever's deferred water pass: Beer–Lambert absorption, Henyey–Greenstein in-scattering, the EnvBRDF
reflection weight, and foam compositing. Its unknown sun lookup table is replaced by a GGX lobe, and its reflection probe by
a screen-space march against the pre-water copies.

Since the fog is drawn after the water, each reflection carries the fog along its reflected path. The fog layers are
integrated analytically over eight segments spaced like the march's steps, and composited front to back. So dark near fog
hides bright distant fog in a reflection, as it does on the sky. (One layer-weighted average over the whole path made night
reflections several times brighter than the sky in Classic zones whose layers differ in colour.)

**Ripples.** Units in the water drive Forever's local displacement recurrence,
`next = 0.97·edge·(0.5·(L + D + R + U) − C.g)`, with output `(next, C.r)` (R the height, G the previous one, edge
`saturate(16·min(u, 1 − u, v, 1 − v))`). It runs on a G16R16F ping-pong map that follows the camera target by whole texels.
Every step holds the footprint of each unit whose world entity reports the liquid surface crossing its body (see Water
contacts), along its path: `R = lerp(R, level, k)`, then `G = lerp(G, R, k)`. `k` fades over the footprint, and the level is
raised with immersion and speed. So a standing unit leaves the water still, a unit faster than the ripples drags a V, and
starting or stopping each release one ring. Wading makes splashes once.

The shading adds Forever's one-map slope (prepass 7552035) of `lerp(G, R, w)`, where `w` is the elapsed fraction of the next
step. With `WaterClientSplashes=0`, while ripples can run on shaded water, the DLL holds the client's `waterRipples` value at
0, so its `splash.blp` and `wake.blp` sprites stop. It restores the value when the hold ends.

**Antialiasing.** D3D9 textures cannot be multisampled, so with the game's Multisampling option the depth the passes read is
a copy. `CheckDeviceMultiSampleType` offers the game's sample counts only while `Multisampling=1` and a copy method exists:
`NvAPI_D3D9_StretchRectEx` on native NVIDIA D3D9, or the driver's `RESZ` resolve where `CheckDeviceFormat` reports it (AMD,
Intel). The multisampled depth is created as D24S8, since the game's D24X8 has no stencil for the water tags and the
silhouette split. A self-test copies two known depths before the game sees the device. If it fails, the device stays
single-sampled.

The depth is copied before the water pass, after the water surfaces and before the fog, one sample per pixel. Scene copies
become resolves, and water is tagged and shaded per sample. Where a pixel's 3×3 copied depths are not planar, a pass marks its
near and far samples in the stencil with an `oDepth` halfway between them (clamped to the viewport, so the split uses MinZ 0
and MaxZ 1). The composite fogs each side at its own depth, so a pixel half tree and half sky blends both fogs by coverage.

**View distance.** Ascension's Extensions.dll detours the far-clip clamp (`0x780770`) and caps maps 0, 1, 530 and 571 at
791.66 yd. The engine allows 1583.33 yd, and instances use it. With `FarClipMax` set, the DLL's calls to the clamp lift that
cap. Terrain loading, the chunk pool, the WDL horizon and the fog follow the far clip. Placed objects keep their own
size-class culling (`environmentDetail`), and creatures keep the server's visibility distance.

## Classic fog data

`tools/convert_classic_fog.py` converts the WoW Forever fog and lighting kit (build 1.60.1.70009, a folder or its zip: the
decoded `LightData`, `LightDataGlobalVolumeFog`, `LightParams` and `ZoneLightPoint` tables and the colour-grading LUTs,
checked against the kit's `SHA256SUMS`) into `data/fogdata.bin`. The kit has no decrypted `Light` and `ZoneLight`, so
lights and zone lights are carried byte for byte from a previous `fogdata.bin` (the shipped file keeps those of `9bf495a`,
from 69876), or read from the Classic client's `Light.csv` and `ZoneLight.csv` exports. It checks that the fog table's
layout hash is still `24290E20`, which keeps the column numbers valid:

```powershell
python tools/convert_classic_fog.py <kit folder or zip> --placements data/fogdata.bin data/fogdata.bin
```

The converter keeps the fog rows the Classic client selects (flag bit 3) at their layer index (0–2). At run time the DLL
blends the Classic lights around the camera:

- spheres at full weight inside the falloff start, falling linearly to the falloff end;
- otherwise the zone light whose outline holds the camera (fading in over 100 yd, innermost outline on top);
- otherwise the map's global light.

Each light uses the condition slot the client uses for its stock lighting: the slot a screen effect forces (the ghost
effect forces slot 4, death), otherwise clear weather (slot 0) blended toward storm (slot 2) by the client's storm weight.
The two time keys around the current time are interpolated, and layers are paired by their Classic layer index. Storm light params
without fog keys (31 lights, among them light 42 in the Eastern Plaguelands) keep the clear layers through the storm
instead of fading the fog out with the rain.

Then the Classic transforms apply: density ×0.01; heights relative to the player when flag bit 1 is set; shadow colour and
density for flag bit 0 while the light is below the horizon; `1 + strength·((d − start)/range)^exponent` over the
`MaxDistance` fog range; and scatter intensities up to 50 in linear light. The sun scattering is scaled by the client's
direct-light luminance over Classic's, capped at 1. That keeps the fog consistent with the older client's darker
lighting. It is a compatibility calibration, not a reproduction of the modern renderer.

Classic data applies on maps where any Classic light has fog, wherever Classic lights hold at least half of the blend
weight. A light without fog in the active slot counts with zero density, so the fog thins smoothly into it and the
distance fog hides the far clip there. Other maps use the derived layers.

Classic never had Outland (map 530), Northrend (map 571) or the Burning Crusade and Wrath dungeons, so the kit holds
no fog for them. `tools/convert_client_lights.py` (ported from Raz0r1337/coa-vfog `st0ny-dev`) adds them to
`fogdata.bin` from the 3.3.5 client's `Light`, `LightIntBand`, `LightFloatBand`, `LightParams` and `LightSkybox`
tables (a `DBFilesClient` folder or zip):

```powershell
python tools/convert_client_lights.py data/fogdata.bin <DBFilesClient folder or zip> data/fogdata.bin
```

Every client light on a map without Classic lights becomes a light of its own (id + 1000000), placed as the client
places it: world X = 17066.666 − z/36, Y = 17066.666 − x/36, Z = y/36, falloff /36, a light at the origin without
falloff stays map-wide. 257 Classic lights have a twin with the same id and position in the client table (light 77 at
the Stormwind harbour: (593586, 0, 931236)/36 → (−8801.0, 578.2), as in the Classic placement). Those with fog in
their clear slot are the templates: each new light takes the layers of the template whose 3.3.5 lighting is most
alike, compared every hour over the fog, sky top, sky middle, direct and ambient bands (linear) and the log of the
fog distance. Per condition slot and template key the layers keep their shape and grading curve; absolute heights move
by the new light's height above the template light, in 25 yd steps, so the ground fog of Hellfire Peninsula (its lights
170 to 250 yd below their template) stays below the ground as the template's does instead of filling the valley.
Map-wide lights and lights at Z 0 keep the template's heights, as Z 0 marks a light placed without a height (234
Classic lights, light 77 at the harbour among them). The Classic noise is left out: it showed nowhere in Outland or
Northrend and made Netherstorm look worse. The emissive, shadow and noise colours take the hue of the new light's fog
band (band 7) at the template's luminance scaled by the ratio of the two lights' fog-band luminance (0.5–2), the sun
scattering and the direct light likewise from the direct band (band 0), and the density follows the ratio of the fog
distances (float band 0, 0.5–2, median 0.87 over Outland and Northrend). The glow is the Forever glow of the template's params in the same slot: the 3.3.5
`LightParams` glow drives the old client's bloom, is far higher (1.0 in Ashenvale where Forever has 0) and washed the
colours out when a test build used it on every map. Recipes shared by several lights are written once; the run first drops the lights, params and zone lights it generated
(ids from 1000000 up), so it is repeatable on its own output and leaves the Classic lights and zone lights byte for
byte. Run it again after `convert_classic_fog.py`, which keeps only the kit's light params and the zone lights the kit
has points for. 445 lights on 47 maps come from 194 templates, and one more light is a copy for Northrend's zone
outlines (below; 3.2 MB in all).

The converted layers carry flag 0x10 where the client draws its sky without sun and moon: the light params' skybox
(`LightParams` column 2) lacks `LightSkybox` flag 0x2, which adds the procedural sky to the skybox model, or the sun
colour (`LightIntBand` band 9) is black all day. That holds for Hellfire Peninsula, Netherstorm, Shadowmoon Valley,
Zangarmarsh (`shadowmoon_sky_forest01.mdx` in the Reforged HD client), Blade's Edge, the Bone Wastes, Icecrown, the
Storm Peaks, Wintergrasp, Zul'Drak, Dragonblight, the Howling Fjord, Crystalsong Forest and Northrend's global light;
Shattrath City, Terokkar Forest, Eversong Woods and the Netherstorm eco-domes keep the sun, and Grizzly Hills,
Sholazar Basin and Borean Tundra keep it by the exceptions below. There the scene layers and the distance fog scatter
neither sun nor moon and no god rays are drawn. The 17 lights with Hellfire Peninsula's skybox (`hellfireskybox.mdx`)
keep only the ground fog, at 0.3 of the template's density, and carry flag 0x20: testers found the template's haze
and far wall too strong there and its fog on the sky out of place. Under that clear sky the distance fog is off and
the scene layers fall off on sky pixels by `exp(−upward·falloff)` with `falloff=−ln(1−clear)/0.005` (1842 fully clear,
so the ground fog is gone from the sky a tenth of a degree above the horizon; at a light's edge the share fades with
the blend).

Northrend's global light is 1406. Four client lights on map 571 sit at the origin, which wowdev.wiki's `Light` page
calls the fallback where no other light applies, but 752 among them has a falloff (32.6 to 46.1 client units). The
converter and the DLL take only an origin light without falloff for map-wide, so 752 becomes a sphere of about 1 yd at
(17066.7, 17066.7), the corner of the map grid, that reaches no reachable position, and the DLL's background is the
first map-wide light in file order, 1406 (1407 and 1408 repeat it; which origin light the client itself shows is not
known). 1406 borrows the Howling Fjord's sky, `fiord.mdx`, whose sun band is black, and is the only light over 90% of
Grizzly Hills, 63% of Sholazar Basin and 88% of Borean Tundra, so the rule above took the sun from those zones, which
testers missed. Two exceptions on map 571 give it back. The skyboxes `shalozar.mdx`, `shalozar(red).mdx`,
`ashrand_sky01.mdx`, `8riv_stormsongsky01.mdx` and `10drg_sky01.mdx` (Coldarra, part of Borean Tundra) keep the sun in
every slot that uses them, ahead of both tests; the 32 lights with them have their centres in those three zones
(`10drg_sky01.mdx` also serves the Oculus, map 578, which keeps the rule), but Borean Tundra's light 1386
(`ashrand_sky01.mdx`, falloff 247 to 347 yd) reaches about 240 yd into Wintergrasp and Dragonblight and causes most of
their change below. And two zone outlines, Borean Tundra with Sholazar Basin as one, so that no sunless seam opens at
their shared border, and Grizzly Hills, hold light 2001406, a copy of 1406 whose clear and storm slots keep the sun:
the same layers, colours, densities, glow and grading, only flag 0x10 dropped, while its other slots, the ghost's
spirit sky among them, are 1406's own. The copy sits at the client origin's world position without falloff, so it is
not map-wide and its sphere reaches no reachable position; it acts only through the outlines (zone lights 1000000 and
1000001, heights −64000 to 64000 as the Classic outlines have, vertices traced from the 3.3.5 `WorldMapOverlay`
subzone boxes and simplified to 150 yd, good to about 150–300 yd at the inner borders). `fiord.mdx` stays sunless, as
the Howling Fjord's own sky, and so does light 1411, the Howling Fjord's 611 yd highland light over Camp Winterhoof.
Sampled every 50 yd at an estimated ground height (+5 yd), at 18:00 in clear weather, the DLL's blend now leaves
Grizzly Hills 12.6% sunless (99.0% before: 1411 on the southern rim, the 100 yd fade inside the outline and the
Zul'Drak cliff lights), Sholazar Basin 7.5% (100%) and Borean Tundra 4.7% (99.3%), while Icecrown (96.4%, from 96.6%),
the Storm Peaks (99.8%), Dragonblight (99.7%, from 100%), the Howling Fjord (96.2%, from 96.4%) and Wintergrasp
(98.7%, from 100%) stay sunless and Outland is unchanged. These shares are an offline estimate over zones labelled by
the overlay boxes, not a measurement in the game.

The converted data, its counts and the shares above are the upstream converter's own, as LuxVelum's README reports
them for the `fogdata.bin` it ships; this repository ships that file, whose Classic lights, params, keys, layers, zone
lights and grading curves are identical to the previous `fogdata.bin`, and the harness checks its sunless, clear-sky
and sun-keeping lights. They were not re-measured against the client tables here, which are not on this machine. A
`fogdata.bin` built before the two flags carries neither and draws as it did.

`OutlandDensity` and `NorthrendDensity` start at 0.15, as the fog read as too strong there in play, and
`OutlandClassicFog=0` or `NorthrendClassicFog=0` brings back the derived layers with the client's glow.
`AzerothSkyColor`, `OutlandSkyColor` and `NorthrendSkyColor` (0 by default) mix the scene layers' emissive and shadow
colours at run time toward the hue of the client's current stock fog colour, which blends the scene into its sky and
follows the weather, at each colour's own luminance: the stock colour itself is far darker than the Classic emissive
(0x282A2A against about 0.5 at dusk in Hellfire Peninsula), so taking it whole would blacken the fog.

The phase of the Classic sun and moon scattering peaks at 1 toward the light. The authored intensities rise as g falls, to
about 4π for evenly scattering rows, which suggests that the modern lookup is energy-normalised (inferred; it is not in
the kit). `ClassicPhase=1` multiplies the scattering by `k(g) = (1+g)/(4π(1−g)²)`, capped at g = 0.95.

**Authored noise.** Layers with flag bit 2 modulate their density as the modern global fog kernel does (shader 6674335,
variant 002). Two octaves sample a tileable 3D noise volume at `frac((p − offset_i)·inverseTile_i)`. Their average passes
through a fixed S-curve of contrast 20 about 0.5, and `f = lerp(1, curve, alpha)` scales the density, while the emission
moves toward the fade colour by `1 − f`. So storm fog becomes patchier and about half as dense. It is off by default
(`ClassicNoise=0`), as a tester judged the storms without it right (upstream, 2026-10-25). Alpha is the share of a
layer's blend weight that carries noise, and each octave scrolls as a phase in tiles, so a blend that changes the tile
size scales the pattern about the camera. The modern `t_perlinNoise3D` is not in the kit; the volume here is a 64³
tileable Perlin noise built at load.

The water's reflection fog, the far-clip check and the lit composites' full-resolution march take a noisy layer at its
mean. A lit composite that samples the noise would exceed the 32 temporary registers of ps_3_0. A march at the mean is more
opaque than the noisy march on average: each long far step takes the noise of one point, the curve is nearly binary, and
transmittance averaged over dense and clear patches lets more light through than a uniform half density. On grazing water
the rows between the low-resolution taps match no tap. Where the lit composite marched them again, they showed as bright
horizontal lines one low-resolution texel apart (inferred from the code and a tester's screenshot of grazing water in
rain; not yet confirmed in game). So under noise it interpolates the filtered fog of the taps on the pixel's plane instead.
It takes the filtered fog rather than the current march, whose samples jitter each frame, so the noise cannot turn the
jitter into shimmer on every other row (inferred).

`fogdata.bin` format 4 holds the lights, the light params they reference (with `LightParams.Glow`), the fog keys (with a
grading curve index), the layers, the zone lights and their outlines, and the grading curves. The loader rejects any other
format. The layer noise columns are inferred (`FogData::UnpackNoise`): c4 the fade colour, c16–18 and c19–21 the octaves'
scroll directions, c27[i] octave i's tile in hundreds of yards, c28[i] its drift in yd/s. c24 is carried raw and unused.
`LightData.ColorGradingFileDataID` names a 32³ LUT. Every LUT that placed lights reach applies one curve alike to R, G and
B, so the file keeps 32 codes per LUT, and the converter fails otherwise (`DarkerColorGradingFileDataID` is left out). The
curve is interpolated between the nearest keys that set one, wrapping past midnight (inferred). Glow and grading blend
like the fog, on every map with a placed light.

## Forever water data

`tools/convert_forever_water.py` converts the WoW Forever water kit (a folder or its zip: decoded `LiquidType`,
`LiquidTypeXTexture`, `LiquidTypeXFFTTile` and `FFTTile` tables and the foam BLP textures, checked against the kit's
SHA-256 manifest) into `data/waterdata.bin`:

```powershell
python tools/convert_forever_water.py <kit folder or zip> data/waterdata.bin
```

It holds four presets (Forever liquids 1240 generic lake, 1288 generic river, 1250 generic ocean, 1290 WMO interior), their
seven FFT tiles, and six foam masks (1024², full mip chain, alpha as 8-bit coverage with a fitted linear colour).
Forever's CPU packing of the table fields into its GPU structure is not in the kit, so the mapping is inferred from value
ranges and shader use:

- absorption from `-ln(Color[0])` scaled by `Float[0]`;
- crest and trough scattering colours from `Color[1]`/`Color[2]` with `Float[2]`/`Float[1]`, and scattering weights
  `Float[3..6]`;
- depth, shore and wave foam `Float[7..19]`;
- sun and reflection roughness `Float[27]`/`Float[29]`, and reflection strength `Float[30]`;
- tile size, amplitude, wind alignment and multiplier from `FFTTile` fields 0–3, and the foam and oxygen rates from fields
  4–9. The loader rejects a tile whose size is not a whole number of yards of at least 1, because the spectrum, the
  loop clock and the slope stencil simulate a tile over floor(size) yards while the shading maps world yards onto it
  with 1/size, so a tile of 37.5 yd would be simulated over 37 and sampled over 37.5, stretching its slopes and foam.

The water is tuned by eye against this mapping, not matched to Forever captures.

## How it attaches

- **Loader.** A `version.dll` proxy. All 17 exports forward lazily to the system copy, and its static import loads
  `CoAGraphics.dll` before the client starts.
- **D3D9.** The client resolves `Direct3DCreate9` through the delay-loaded `GetProcAddress` slot `[0xB2ED98]`. The DLL
  points that slot at a filter that returns a wrapped `IDirect3D9`. No d3d9 code is patched, so DXVK or other `d3d9.dll`
  builds keep working underneath.
- **Depth.** The wrapper creates the device without auto depth and binds an `INTZ` texture as the depth-stencil, which the
  client caches as its world depth. Multisampling is kept only as described below, and `D3DCREATE_PUREDEVICE` is removed.
  The shaders read depth through the captured world viewport's range and treat anything deeper as beyond the far clip.
  When the game asks for multisampling and a depth copy passes its self-test (see Antialiasing), the device keeps the
  game's multisampled back buffer and automatic depth (D24S8 for D24X8) instead, and `INTZ` becomes a copy.
  The `INTZ` and D24S8 support checks pass the desktop's display mode as the adapter format for a windowed device and
  the back buffer's display format for a fullscreen one: X8R8G8B8 for A8R8G8B8 and X1R5G5B5 for A1R5G5B5, since D3D9
  displays no alpha and DXVK refuses the others (`IsSupportedAdapterFormat` in DXVK's `d3d9_monitor.cpp` accepts only
  A2R10G10B10, X8R8G8B8, X1R5G5B5 and R5G6B5, and `IsSupportedBackBufferFormat` pairs a fullscreen A8R8G8B8 back buffer
  with the X8R8G8B8 adapter format). The client's present-parameter builder takes a windowed back buffer format from the
  desktop's mode and a fullscreen one from `s_GxFormatToD3dFormat` = {R5G6B5, X8R8G8B8, A8R8G8B8, A2R10G10B10, ...}
  (`CGxDeviceD3d.cpp:2211`, `:2218`), so a fullscreen A8R8G8B8 back buffer is possible; which colour format the
  32-bit setting picks is not confirmed. When the `INTZ` texture cannot be created or bound, the wrapper binds a plain
  depth surface of the game's format and turns the fog off for that device; when that surface cannot be bound either,
  `CreateDevice` releases the fog device and creates the device again with the game's own parameters and flags, and
  `Reset` resets the device again with the game's parameters, so the game keeps its automatic depth (the next `Reset`
  tries the fog depth again).
- **Hooks.** Five 5-byte call displacements:
  - the world render call (`0x4FB03D`): stock-fog override and restore;
  - after the opaque M2 pass (`0x4F911D`): captures camera inputs;
  - the liquid surface pass (`0x4F9170`): forces depth writes;
  - world-name text (`0x7E5818`): draws the fog unless it already composited this frame, then suppresses depth writes;
  - before the frame effects (`0x4F9281`): draws the fog over the completed world when the world text did not.

  The original bytes are checked first; on any mismatch nothing is patched. Two more retarget the far-clip clamp calls
  (`0x780810`, `0x781444`) when `FarClipMax` is set at start-up, independently of the fog hooks.
- **Transparent fog hooks.** The M2 batch fog call (`0x81FD15`) and the glare pass call (`0x4F9213`, which then draws the fog
  before the world text unless it already composited this frame), installed after the
  fog hooks whatever `TransparentFog` says, and neither when a byte run checked under M2 batch fog, M2 fog setter or
  Native glare differs. The M2 thunk forwards to the client while nothing is armed. Otherwise it hands the cdecl argument
  block to the DLL (`pushad; lea eax, [esp+0x24]`). An exception in it turns transparent fog off. The unfogged batch's
  fog upload call (`0x81FD29`) is retargeted after them, on its own: when a byte run checked under M2 unfogged batch
  differs it stays unpatched and unfogged effects stay unfogged. Its thunk hands the batch (`esi`) and the upload's
  argument to the DLL, which calls the setter with the fitted line and turns the upload's 0 into 1.
- **Water hooks.** Installed after the fog hooks and only when those installed (`g_waterHooks = engineHooks &&
  InstallWaterHooks()` in `dllmain.cpp`), so a log without a water line after a fog hook mismatch is expected: the water
  pass call (`0x790AA2`) and the `Render` slots of the two water material vtables (`0xA5954C`, `0xA59580`, read-only data patched under
  `VirtualProtect`). The call site, both slots and the layout bytes the classification reads are checked first. On any
  mismatch none are patched and the fog is unaffected. `waterdata.bin` is loaded only once they are installed. A water
  exception restores the device state, turns water off for the session and leaves the fog running.
- **Glow and grading.** No patch. The glow override runs before the fog composite, and the grading in the world render's
  frame-end hook, each only while its guarded bytes match. An exception turns that one off for the session, or both when
  it comes from their shared inputs, and `GlowCompensation` then uses the clamped day/night glow.
- **State.** Every state the passes touch is captured with a recorded state block and restored, plus render targets, depth
  and stream 0 (whose offset state blocks drop). The client's shader-constant cache stays valid. The fog, grading and
  image passes restore them in a termination handler, so an exception inside a pass, which the hook's guard catches, still
  hands the client back its own targets and state, and releases the references that would otherwise block the next
  `Reset`.
- **Overlay.** When a fog device is created, the device window's procedure is chained so the settings window sees input
  first, and the wrapper's `Present` draws the window over the finished frame.

## Tree and bush wind

The client has no ambient wind. CoAGraphics tilts placed trees and bushes with two travelling sine waves, a constant
lean and a slow gust, on the CPU: no shaders and no Direct3D state are involved. The port of WarcraftXL's Treewind
runs inside this DLL, so `WarcraftXL.dll` and its Treewind extension are not needed, and must not be loaded at the same
time, since both would tilt the same placements.

- **Where it hooks.** The client builds a model's bone palette in `CM2Model::AnimateMT` (`0x0082F0F0`) and
  `CM2Model::AnimateMTSimple` (`0x0082E140`), both thiscall with five stack arguments and `ret 0x14`. The 14 `E8` call
  sites below are retargeted to a thunk that tilts the instance's placement matrix and then jumps to the original with
  the registers and stack untouched. A scan of the 12340 image finds no other reference to either function, so no
  caller escapes. Full: `0x81CEFA`, `0x821B4E`, `0x821BD8`, `0x82E6FD`, `0x830D8A`, `0x830E4C`, `0x830F27`. Simple:
  `0x81CECF`, `0x821B21`, `0x821BAD`, `0x82E680`, `0x82E6E2`, `0x830E1F`, `0x830F20`. Each site is checked to be a
  call to its function before any is patched, and the entry and `ret 0x14` bytes of both functions are guarded.
- **Disassembly evidence** (clean 12340 image, timestamp `0x4C2452FE`): `AnimateMT` begins
  `55 8B EC 81 EC 9C 01 00 00 56 8B F1 8B 46 10 A8` and returns at `0x830968` with `C2 14 00`; `AnimateMTSimple` begins
  `55 8B EC 83 EC 50 56 8B F1 8B 46 10 A8 01 0F 84` and returns at `0x82E545` with `C2 14 00`. Both read the model at
  `[this+0x2C]` and the placement at `[this+0xB4]`.
- **Model fields**, from the wowdecomp layouts: `CM2Model` flags at `+0x04` (bit `0x20` is set by
  `CMapDoodadDef::CreateModel`, so it marks a map-placed doodad), shared model at `+0x2C`, placement at `+0xB4` (three
  basis rows, translation in row 3); `CM2Shared` file name buffer at `+0x3C`, the pointer to the name without its
  directory at `+0x140` (read only when it points into that buffer), loaded data at `+0x150`, bounding box at `+0x154`.
- **What sways.** A map-placed doodad whose name contains a live-tree word (tree, pine, oak, palm ...) and not a dead
  one (dead, stump, log, charred ...), optionally also tall and narrow, not taller than `TreeMaxHeight` and within
  `TreeMaxDistance` of the camera. Bushes, ferns and hedges match by name alone and have their own wind.
- **Pristine placements.** The engine never rewrites a doodad's placement, so the unswayed matrix is remembered per
  instance (a fixed table of 8192 entries, probed 32 deep) and the tilt is rebuilt from it each frame. An entry not
  touched for two seconds belongs to a retired instance and is reused. The table is cleared when the map changes.
- **Settings.** `TreeWind*`, `Tree*`, `BushWind*` and `Bush*` in `CoAGraphics.ini` and the Tree wind and Bush wind
  sections of the settings window; changes apply on the next frame.
- **Checks.** The harness checks the matrix at zero strength, translation and scale after the tilt, each gate, the cache
  and its eviction, the guards, the shipped defaults and the thunks. That does not establish in-game behaviour.

## Grass wind and parting

The client's grass (the ground-effect "detail doodads") has no motion. CoAGraphics reassembles the grass vertex shader
with a block that displaces each blade in the world plane by the wind at its place and a per-blade flutter, and that
leans blades away from the player and every unit near it. The shader rewrite and the call-site thunks port WarcraftXL's
Grasswind; the wind model, the wind grid, the unit parting and the motion block port the Living Azeroth module
([Morfium-G/seyris-living-azeroth](https://github.com/Morfium-G/seyris-living-azeroth)). `WarcraftXL.dll` and its
Grasswind extension are not needed and must not be loaded at the same time.

- **The shaders.** The grass vertex shaders are `Shaders\Vertex\vs_2_0\DetailDoodad.bls` and
  `Shaders\Vertex\vs_3_0\DetailDoodad.bls` in the client's `patch.MPQ` (`HSXG`, version `0x10003`, six permutations
  each, read by `CGxDevice::IShaderLoad`). Their bytecode lengths are 552, 644, 788, 1428, 1520, 1664 bytes (vs_2_0) and
  548, 676, 844, 1328, 1456, 1624 bytes (vs_3_0). The permutation is `3 * index + shadow`; the only call of
  `CDetailDoodad::SetupVertexShader` passes index 0, so only the first three of each are ever drawn. The last three read
  c14 to c22, the point lights, which the motion block also needs; they are left stock. The first three read c0 to c13
  and, with shadows, up to c34 (the shadow matrices; checked by the harness on the extracted shaders), so c35 and up
  belong to the motion block.
- **Where it hooks.** The device wrapper's `CreateVertexShader` measures the bytecode to its end token and, for a
  version token of `0xFFFE0200` or `0xFFFE0300` with one of the six first lengths, disassembles it with
  `D3DDisassemble` from the system's `d3dcompiler_47.dll`, defines `c36` to `c38` and the loop counter `i0` after the
  version line, inserts the motion block after `add r0, r0, c3` (the view transform), reassembles it with `D3DAssemble`
  and passes the result on. Nothing in the client is patched for this, so it also works on DXVK. A shader that cannot
  be rewritten stays stock and is logged once with its length. The result is remembered per shader contents, not per
  length: another vertex shader of a grass length, created first, was once remembered as "not grass" under that length
  and kept the real grass shader stock for the session (seen in game on 2026-10-22, 844 bytes). The block uses `sincos`
  with one source, `mova`, a `loop` and relative constant addressing, which vs_3_0 assembles and vs_2_0 does not: on a
  client that runs the vs_2_0 grass shaders the grass stays still and the log says so. The harness assembles the block
  into all three stock vs_3_0 shaders.
- **The motion block.** The vertex's world position is its view position rotated back by `c0` to `c2` (the chunks are
  unrotated) plus the camera. Its bend weight is how far its texture row lies from its doodad's root row toward its tip
  row (see Per-doodad bend; `1 - v` when nothing is known of the doodad), with the lowest `GrassWindAnchor` share held
  still and the rest squared, as in Grasswind. The wind at the blade comes from an 8×8 grid of wind values,
  8 yd apart and centred on the player, sampled bilinearly. The tip moves by `wind × (amplitude + flutter × sin(phase) ×
  (gust + 0.3))`, the phase running with time and with the blade's world position. Each unit pushes blades away within
  its radius and only within 3 yd of its own height; a unit's push falls off linearly with distance. Grass more than
  45 yd from the player skips the unit loop.
- **The constants.** `CDetailDoodad::InitializeShaderConstants` (`0x007B15D0`, called only from `0x007B2E25`) clears the
  23-register structure at `0x00D1C518` and fills c0 to c13; `CDetailDoodad::SetupVertexShader` uploads all 23 registers
  for each chunk (`6A 17 68 18 C5 D1 00 6A 00 6A 00 FF D0` at `0x007B1194`). The call at `0x007B2E25` is retargeted to
  a thunk that runs the original and writes c14 to c18 of that structure: the camera and the flutter phase, the anchor,
  the amplitude and the flutter, the grid scale and origin, the blade phase per yard and the push strength, and the
  centre of the unit range. It also builds the frame: the wind model advances, the grid cells ease toward their new wind
  over 0.35 s, and the units are packed. `SetupVertexShader` (`0x007B10E0`, `(world matrix, shader index)`, cdecl;
  decomp `DetailDoodad.cpp:142-157`) has one caller, `CWorldScene::RenderDetailDoodads` at `0x007985EE`
  (`57 51 E8 ED 8A 01 00 83 C4 08` at `0x007985EC`: push the index, push the matrix, call, clean
  two arguments), once for each chunk
  with grass. That call is retargeted to a thunk that runs the original with a copy of its two arguments and then, for
  index 0 only, uploads 80 registers to the device from c40: the wind grid in c40 to c103 and the 16 units in c104 to
  c119 (position and push radius, unused slots far away with radius 0). It is done for every chunk, after the client's
  own upload of c0 to c22, so nothing drawn in between can replace it. Index 1 selects the point light shaders, which
  stay stock, and gets nothing. Their guards are the entries `55 8B EC 81 EC 5C 01 00 00 56 6A 00 68 70 01 00` and
  `55 8B EC 81 EC 80 00 00 00 E8 02 2F 0C 00 83 F8`, the clear `6A 00 68 70 01 00 00 68 18 C5 D1 00` at `0x007B15DA`
  and the upload above.
- **Per-doodad bend.** The client packs grass, flowers and pebbles into shared atlas textures, so a vertex's texture row
  says little about where on its own doodad it sits. Each vertex therefore carries its doodad's table entry, and the
  entry holds that doodad's root row, one over its span from root to tip, and a wind and a push scale.
  `CDetailDoodadInstBatch::FillVertices` (`0x007B1B50`, thiscall with the batch in `ecx`) builds a batch's vertices
  and runs only when the batch's vertex buffer has no data or is not valid (the two bytes at `+0x1C` and `+0x1D`
  of the buffer, tested at `0x007B33E6` to `0x007B33F0`); `CDetailDoodadInstBatch::Draw` (`0x007B3390`) calls it, and
  `DrawBatches` (`0x007B36B0`) calls `Draw` for the four batches of a chunk. Each has one caller: `8B CE E8 57 E7 FF FF
  8B 0F` at `0x007B33F2` (the call is `0x007B33F4`) and `8B CE E8 C4 FC FF FF` at `0x007B36C5` (the call is `0x007B36C7`). The batch keeps its instances at `+0x1C` with their
  count at `+0x18` (`8B 7B 1C` ... `8B 43 18` at `0x007B1B7D`), `0x2C` bytes each (`83 C7 2C` at `0x007B257E`), the
  doodad id at `+0x04` and the colour at `+0x28`, which the build copies into every vertex (`0x007B1CE0` to
  `0x007B1D38`). The client's build also waits for each model to load (`test byte [model + 0x10], 1`, else
  `0x00823ED0`), so a model is readable once the fill returns. The fill call is retargeted to a thunk that writes each
  instance's entry index into its colour before the original runs (the low three bits into green, the high two into
  red and blue, so a red and blue swap for the GPU format changes nothing; a slot with more than 31 kinds sends the
  extra ones to entry 31) and gives the colours back after. The shader reads the index back with float arithmetic on
  the 8-bit channels (the harness checks every index against that arithmetic for several other colour bits, swapped
  or not); a white vertex decodes to entry 31, the fallback. The fallback is the old rule, root at row 1 and tip at
  row 0, with the full wind and push, and every entry a slot does not use holds it, so the shader never reads zeros.
  The thunk also records which doodads each slot holds. The draw call is retargeted to a thunk that uploads the
  slot's table (32 registers from c120) before the original runs, and the fill thunk uploads it again right after a
  fill, which happens inside the draw. `GrassModelBend=0` (read live, with c19 holding the choice) makes the shader
  use entry 31 whatever the colours say, stops the tagging and uploads fallback tables.
- **Root and tip.** The table for a doodad id comes from its model: the doodad list at `0x00D1C4FC` (entry pointers by
  id, count at `0x00D1C4F8`) gives an entry whose `+0x04` is the model, `+0x2C` of the model its shared data, whose
  `+0x150` is the `M2Data` (vertex count at `+0x3C`, vertex pointer at `+0x40`, 48-byte vertices with the height at `+8`
  and the texture row at `+0x24`) and whose `+0x170` is the skin (vertex lookup count at `+0x04`, a pointer to 16-bit
  indices at `+0x08`; guards at `0x007B1D04`, `0x007B1D29`, `0x007B2330`, `0x007B233D` and `0x007B2391`). The 12340
  client keeps absolute pointers in these arrays where the decomp keeps offsets (the exe's own loop reads them that way). The
  root row is the mean texture row of the vertices in the lowest 12 percent of the model's height, the tip row that of
  the highest 12 percent; a model under 0.15 yd high is flat (pebbles, shells) and gets no wind and no push; one
  whose two rows agree is left on the fallback. A model is read once, when its first slot is filled, and at most 8192
  vertices; the reads are guarded and an unreadable model keeps the fallback and is retried at the next fill.
- **Overrides.** `GroundEffectDoodadWind.cdbc` beside the DLL (the module's table, 37 rows shipped, tuned by its author
  in the game) overrides single doodads: `ID`, `Flags` (1 no wind, 2 no push, 0x10 root and tip swapped),
  `Stiffness` (0 flexible to 1 rigid, which scales both motions by one minus it), `RootV` and `TipV` (a row of
  your own, -1 for the model's). A doodad whose model could not be read is usable when its override gives both
  rows. Without the file every doodad bends by its model alone. The module's settings-window editor for these rows is
  not ported.
- **Putting the constants back.** The client keeps its own copy of the vertex shader constants, `CGxDevice::s_shadowConstants`
  (decomp `CGxDevice.cpp:84`; the vertex array is at `0x00C5EFE8`, 16 bytes a register, as the flush at `0x006A9FE0` reads
  it with `shl ecx, 4; add eax, 0xC5EFE8`, guarded by `C1 E0 04 05 E8 EF C5 00 50 51 8B 8F 78 01 00 00` at
  `0x006AA003`), and `IShaderConstantsFlush` sends the device only the registers whose values it changed since its
  last flush. The wind grid, the units and the table overwrite c40 to c151 on the device behind its back, and models
  drawn afterwards write bone matrices there (`CM2SceneRender.cpp:430` locks the array at register 31), so a model
  whose bones happen to equal the client's last copy would be skipped by the flush and drawn with grass data. The call
  to `CWorldScene::RenderOccluders` (`0x00797280`, cdecl, no arguments, one caller at `0x00798781`) that ends
  `RenderDetailDoodads` is retargeted to a thunk that, when the pass uploaded anything, copies the client's own c40 to
  c151 back to the device before it runs the original.
- **The wind.** The wind comes from `WindProfile.cdbc` beside the DLL, a plain WDBC table (the format of the Living
  Azeroth module; fifteen sample rows ship in `data/`): `ID`, `ScopeType` (0 global, 1 map, 2 area), `ScopeID`, then
  `GroundMin`, `GroundMax`, `AloftMin`, `AloftMax`, `AloftHeight`, `WeatherInfluence`, `GustStrength`, `GustFrequency`,
  `TerrainWeight`, `LockedDirection`, `LockStrength`, `Flags` and `Comment` (a string). A row is resolved global, then
  map, then the areas of the player's area chain from the zone to the sub-area, the most specific last, and a negative
  field takes the value of the next less specific row; crossing a border blends over about 1.5 s. The steady strength
  wanders between `GroundMin` and `GroundMax` and is pushed toward `GroundMax` by the weather in proportion to
  `WeatherInfluence`; the bearing wanders by up to 60° around a base of its own for each map unless `LockedDirection`
  and `LockStrength` pin it; gusts are a noise field stretched along the wind and carried downwind, scaled by
  `GustStrength`. `WindStrength` scales the whole wind. The aloft and terrain columns are kept for the table's layout
  and not read. Without the file one built-in global profile (the file's global row) applies everywhere; a file with no
  global row is a calm world. Shelter under roofs and the lee behind walls, which the Living Azeroth wind has, are not
  ported.
- **Weather and area.** The weather is `World::s_weather` (`0x00CD7544`; the client loads it at `0x004F8645`, `A1 44
  75 CD 00 D9 40 28`, the density blend at `+0x28`) and its `m_intensity` at `+0x08`, stored by
  `Weather::SetStormIntensity` (`0x00784850`) at `0x007848BA` (`D9 56 08`). The client eases it toward the value the
  server's weather packets set and itself reads it as `(intensity - 0.25) × 4/3`, no less than 0 (decomp
  `MapWeather.cpp:385-389`); the wind uses the same dead zone, so a server module that shapes the weather over time
  shapes the wind with it. The area is the one of the terrain chunk under the player: `CMap::QueryAreaIdTerrain`
  (`0x007A0490`) returns `chunk->m_areaId` at `+0xB0` (`8B 80 B0 00 00 00` at `0x007A0526`), and the chunk is found with
  the area table `0x00CE48D0` that the terrain shadows already read. The parents come from the AreaTable storage, read
  by the client's own outdoor test at `0x0077FC87` to `0x0077FCA4`: the largest id `0x00AD3140`, the smallest
  `0x00AD3144` and the row pointers `0x00AD3154`, with `ParentAreaID` at row `+0x08` (decomp `AreaTableRec.h`; the
  storage object is `g_areaTableDB` at `0x00AD3134`). The chain stops at a cycle or after eight areas.
- **Units.** The walk of the client's visible object list that finds units wading through water (the object manager,
  `0x00C79CE0 + 0x2ED0`) also gathers the 16 units nearest to the player within 40 yd, with their position (the world
  entity's on a transport), their collision radius (`CMovement_C::m_collisionRadius` at unit `+0x850`, set to
  `width × scale × 0.5` by `SetCollisionBox`, decomp `Movement.cpp:1662`) and the client's own mounted test
  (`CGUnit_C::IsMounted` `0x0051A230`: `[unit + 0x9C0] > 0` and no `0x10000000` in `[unit + 0xA30]`, which the guard
  `83 B9 C0 09 00 00 00 7E 12 F7 81 30 0A 00 00 00 00 00 10` holds). A unit's push radius is its collision radius times
  `GrassPartingRadiusScale`, times `GrassPartingMountedScale` while mounted, and at least `GrassPartingMinRadius`. The
  collision radius does not grow with a mount (the mounted box keeps the rider's width), hence the mounted factor.
- **The player.** `CGWorldFrame::UpdateDayNight` `0x4F8410` copies the active player's `GetPosition()` into
  `DNInfo.m_playerPos` `[0xD38B0C]` every frame the player exists (`8B 08 89 4E 0C 8B 50 04 89 56 10 8B 40 08 89 46 14`
  at `0x4F8555`, `ESI` being the `GetInfo` result `0x7ECEF0`, `B8 00 8B D3 00`). `CGUnit_C::GetPosition` `0x6E6EF0` is
  the movement's `CPassenger::GetPosition`, the world position also on a transport, so the grass reads that float
  triple instead of walking the visible object list. `DayNight::LoadMap` zeroes it and it is not cleared when the
  player leaves, so out of the world the grass is parted around a stale point nobody sees. The harness cannot read
  client memory, so this read has no check; the first in-game look is a transport passenger.
- **Density copies.** The client chooses how much grass a chunk gets with `groundEffectDensity`, which its callback
  (`0x0078DAB0`, `8D 48 F0 81 F9 F0 00 00 00 77` in the 12340 exe: the value minus 16 against `0xF0`) accepts from 16 to
  256. `CMapChunk::CreateDetailDoodads` (decomp `MapChunk.cpp`) picks that many random cells of the
  chunk's 8 by 8, a cell being picked twice or more adding more, and each pick places the ground effect's own plant
  count (8 when the record says 0). `CDetailDoodadInst::AddDoodad` (`0x007B31E0`, decomp `DetailDoodad.cpp:661`) puts a
  plant into one of the chunk's four batches, one per texture, and returns without adding it when none has room: a
  batch holds `s_vtxBufItems` vertices, which `CDetailDoodad::UpdatePools` (`0x007B2A80`) sets to the density times 64
  and never above 4096. So the client builds all of a chunk's plants on the CPU into buffers of a fixed size, and its
  setting stops at 256. That a chunk of the game reaches that limit has not been seen; it is read from the decomp.
  `GrassDensity` (1 to 4) draws each plant more than once instead. The thunk on `CDetailDoodad::SetupVertexShader`
  (see Wind grid) now also reads the chunk's world matrix, the `world` argument that `CWorldScene::RenderDetailDoodads`
  (`0x007984A0`, decomp `World.cpp:3560`) builds as the identity translated by the chunk's top left corner minus the
  camera: the translation is the matrix's floats 12 and 13 (decomp `C44Matrix.h`, `d0` at `+0x30` and `d1` at `+0x34`).
  The plants of a chunk lie at local coordinates from the corner back to minus 33.33 yd, 8 cells of
  `SMOLTILE_SIZE` 4.1666665 (the same function: `position.x = -py`, then `position.x -= cellY`), so the chunk is the square from that
  corner back by 33.33 yd, and its distance is the horizontal distance from the camera to the nearest edge of that
  square. A chunk is drawn once for the plants and once for each copy `j` (1 to `ceil(GrassDensity - 1)`, at most
  3) whose outer distance `R - (R / 2) (j - 1) / (GrassDensity - 1)` (`R` being `GrassDensityRadius`) lies beyond the
  chunk's distance, so the chunks beyond the radius cost nothing. Which plants show the copy is decided in the
  shader, see Fading in. The draw thunk (call `0x007B36C7`) runs the client's
  `CDetailDoodadInstBatch::Draw` (`0x007B3390`), which binds the batch's vertex and index buffers and texture and
  issues the draw (it builds vertices only when the buffer has no data), once, and then once more for each copy, with
  the copy's offset in c20 for that draw. The offset is fixed per copy number: the direction advances by the golden
  angle and the length lies between half of `GrassDensitySpread` and all of it. The shader then turns it by an
  angle of the plant and shortens it to between 0.6 and 1 of its length, so that no direction repeats from plant
  to plant. The upload for a copy is two registers, c20 (the offset, the outer distance and the width of the
  copy's band) and c21 (the inverse of the share of plants that ever show it). c20 is the position of the third point
  light in the client's constant block (c14 to c22 hold three lights, `0xE0` and 48 bytes each in the structure at
  `0x00D1C518`), which the client zeroes and only the point light variants of the shader (index 1, which the client
  never draws) read; the disassembly of the stock blobs shows the three variants of index 0 reading c0 to c13 and, with
  shadows, c23 to c34 (the harness checks that none reads c20). The client's own copy of the constants holds zero in c20, so the thunk writes the offset straight to the
  device for each copy, writes zeros back after the last one, and writes zeros again at the next chunk's setup and at
  the end of the pass if one was left. The rewritten shader runs first in the motion block: the vertex normal is the
  ground triangle's normal in the same chunk coordinates as the position (`FillVertices`, decomp `DetailDoodad.cpp`,
  `vertex->m_normal = add.m_normal`), so the plant moves by (dx, dy) and by `-(nx dx + ny dy) / nz` in z, which
  keeps it on its triangle's plane, through the same world matrix columns as the position. The wind, the parting and
  the shadow coordinates then use the moved position.
  The shader has nothing per plant but the vertex colour, the normal and the position, so the fill thunk (which
  already writes the entry index into the colour) also writes six bits taken from the plant into the low six bits
  of the alpha byte of its colour: a hash of the plant's position (the instance record's floats at `+0x08` and
  `+0x0C`, `CDetailDoodadInstAdd` in the decomp, whose `+0x04` and `+0x28` were found to be the doodad id and the
  colour in the exe) is stable while the plant exists, so it does not change when the client rebuilds a buffer.
  The client writes that alpha as 255, or 0 for a plant in the shadow map (`CMapChunk::CreateDetailDoodads`, decomp
  `MapChunk.cpp`), and all six stock shaders pass it on with `mov o1.w, v2.w`; the rewrite changes that line to
  `sge o1.w, v2.w, 0.5`, which gives the pixel shader exactly the 1 or 0 it had before whatever the six bits are,
  so nothing about the plant's lighting changes. The bits are written only when the shaders were rewritten without
  failures, and the colour is given back after the fill like the tag. The shader turns the offset by
  `(bits + 0.5) / 64` of a full turn and scales it by `0.6 + 0.4 * frac(5.3 * (bits + 0.5) / 64)`.
  Fading in: a copy must not appear in whole chunks at a time, nor all at one distance. The same six bits give
  each plant a second number `t = frac(39.554175 * (bits + 0.5) / 64)` (64 times the golden ratio's fraction, so the
  64 values lie evenly in 0 to 1), and the copy of a plant shows while the plant is nearer to the camera than
  `outer - t * width`: copy `j` of a multiplier `m` has `outer = R - (R / 2) (j - 1) / (m - 1)` and
  `width = (R / 2) min(1, m - j) / (m - 1)`, so the share of plants showing it falls evenly from all of them at the
  inner end of its band to none at `outer`, and the copies thin out one after the other, the last first. For a
  fractional multiplier the last copy only has a share `m - j` of the plants (`t` divided by that share, and a
  plant whose result reaches 1 never shows it). The plant does not vanish at that distance: the shader measures the
  horizontal distance from the camera (the world offset from the view-space position, before the camera is added)
  and sinks the plant along the ground normal, which it transforms like the stock lighting does, by 2 yd times
  `clamp(1 - (hide distance - distance) / GrassDensityFade, 0, 1)`, so a plant grows out of the ground over
  `GrassDensityFade` yd of approach (32 by default; it is a distance, so it passes faster the faster you move) and a
  plant that is entirely beyond its hide distance is under the terrain, which hides it with the depth test (the
  grass is about a yard tall). A vertex of one plant is within a yard of the others, so they sink almost together;
  nothing is collapsed to a point. Without a copy window (c20 width under 0.001, which is every draw of the client's
  own) nothing sinks. Limits: a
  copy is not kept inside its terrain triangle, so with a large
  `GrassDensitySpread` grass reaches over a path or a cliff edge and its height is only right to the first order on bumpy
  ground; each copy costs one more draw per batch and the same vertices again on the GPU. The copies need the doodad
  hooks (the draw call) and the shader rewrite (`GrassWind`, `GrassParting` or `GrassDensity` above 1 at start). The
  status line in the log counts the chunks that got copies.
- **Settings.** `WindStrength` and the `GrassWind*` and `GrassParting*` keys in `CoAGraphics.ini` and the Grass section
  of the settings window. The shader rewrite is made when the game creates its shaders, so it needs `GrassWind=1`,
  `GrassParting=1` or `GrassDensity` above 1 at start; with it in place the keys apply within a second and 0 stops the motion. The keys of the
  two-wave model that Grasswind had (`GrassWindDirection`, `Speed`, `Wavelength`, the cross swell, `Lean`, `Variance`,
  `DistanceFade`, and the cone keys of the parting) are gone, and an INI that still names them is read without them.
- **Checks.** The harness reads the client's twelve grass shaders from `tests/shaders` (extracted from `patch.MPQ`), and
  checks their end tokens, that the stock shaders read nothing from c35 up, the rewrite of the three vs_3_0 shaders, that
  the light variants, other shaders, a missing compiler and a disabled rewrite pass through, the frame constants, the
  wind grid against the wind model, its easing and its cell table, the unit packing, both thunks (the original runs
  with its arguments, the upload is 80 registers from c40 for index 0 only), the guards, the shipped defaults and, on
  the device, that the wrapper hands the driver a longer shader. The wind model has its own checks (the WDBC reader,
  the shipped table, the resolution order, the weather and lock, the gusts, the area chain), as has the unit selection
  and the walk over a synthetic object list. The per-doodad bend has its own checks: the tags and their float decode,
  the slot entries, the root and tip bands, the entries with and without overrides, the shipped override table, tagging
  and untagging a synthetic slot, reading a synthetic model (and refusing broken ones), the guards, the three thunks and,
  on the device, points drawn with tagged colours that move by the weights, scales and fallback the table says. The
  density copies have their own checks: the chunk distance, the copies by distance, the offsets, the three settings,
  the draw thunk (a chunk near the camera is drawn once and once per copy with each offset in c20 at its draw and c20
  zero after, a far chunk, a multiplier of 1 and the point light variant once, no copies inherited from the chunk
  before), the copy bands and counts, the share of plants that still show a copy (it falls with distance, is all
  near the camera and none past the outer distance, is half in the middle of the band, and is the share the
  multiplier says for the last copy), the plant bits (their spread, their stability, tagging and untagging a
  synthetic slot with them) and, on the device, a point moved by a copy offset on level ground, on a slope and on a ridge for six alphas, and the
  alpha the pixel shader receives (exactly 1 or 0 for all 64 bit patterns), and a point at six distances for five
  patterns sunk by the amount the C++ mirror of the shader gives. That does
  not establish in-game behaviour: the area and weather reads, the unit walk, the doodad hooks and the density copies
  have not been seen in the game yet.

## Snow

Ground that the zone paints with snow gets a layer of snow on top of it: the client's own terrain is raised by the
snow's depth, so characters sink in up to their ankles, grass is buried and steep ground stays bare, and the snow
spreads past the painted ground with ragged edges. The snow rules (painted snow ground, a slope limit, drifts, ragged
edges) follow the Living Azeroth module's surface cover
([Morfium-G/seyris-living-azeroth](https://github.com/Morfium-G/seyris-living-azeroth)); its separate clipmap mesh, its
cover tables, fallen snow and melting are not ported. The terrain keeps drawing itself, so its lighting, shadows and
fog stay the client's.

- **Where snow lies.** A terrain layer is snow when its ground effect's `TerrainType` is 3 (Snow), the client's own
  ground type query `CMap::QueryGroundTypeTerrain` (`0x7A0530`, `Map.cpp:5404`; the 3.3.5 `GroundEffectTexture.dbc`
  has 347 such rows), or when its texture's path contains `snow`. The query reads the chunk's layers at chunk `+0x12C`
  (`8B 80 2C 01 00 00` at `0x7A0638`), the layer's ground effect at `+0x0C` (`8B 4C 02 0C 51 B9 00 3B AD 00` at
  `0x7A064C`: the effect id, then `this` = `0xAD3B00`, the `IDatabase` part of `g_groundEffectTextureDB` at
  `0xAD3AE8`), and the record's `+0x28` (`8B 50 28` at `0x7A0667`; the decomp calls the field `m_sound`, the dbc
  column is `TerrainType`). `GetRecord` (`0x65C290`, `55 8B EC 8B 51 F8 8B 45 08 3B C2 7C 11 3B 41 F4`) bounds the id
  by the minimum at `this - 8` (`0xAD3AF8`) and the maximum at `this - 0xC` (`0xAD3AF4`) and reads `records[id - min]`
  from `this + 8` (`0xAD3B08`). A layer's texture is the area's texture list entry its `+0x00` names:
  `CMapArea::LoadTextures` (`0x7D6D20`, `MapArea.cpp:278`) keeps the list at area `+0x58` (`83 C7 58` at `0x7D6D47`),
  its count at `+0x5C` and entries at `+0x60`, eight bytes each with the name first (`8B 47 04 8B 4F 08 8B 55 08 8D 0C
  C1 83 C0 01 89` at `0x7D6DAB`).
- **How much.** A layer's painted strength is its alpha map: `CMapRenderChunk::CollectLayerAlphaPtrs` (`0x7B7860`,
  `MapRenderChunk.cpp:1025`) takes it at the chunk's `+0x130` plus the layer's `+0x08` when the layer's flags
  (`+0x04`) have `0x100` (`8B 96 2C 01 00 00 F7 42 04 00 01 00 00 74 0C 8B` at `0x7B7876`, `8B BE 30 01 00 00 03 7A 08`
  at `0x7B7885`; the decomp names `+0x130` `m_additionalShadowmap`, but `ProcessIffChunks` `0x7C3A10` stores the
  `MCAL` data there). Flags `0x200` mean run-length compressed; otherwise the map is 8-bit when the map header's flags
  (`0xCF08D0`) have `0x4` (`F6 05 D0 08 CF 00 04` at `0x7B7A74`, `CMapRenderChunk::AllocTexture`) and 4-bit, low
  nibble first, when not, its last row and column repeating the ones before unless the chunk's `MCNK` flags have
  `0x8000`. The decode is bounded by the `MCAL` size in the chunk header (`+0x28`, which counts its 8-byte chunk
  header; the layer count is at `+0x0C`, the `MCNK` layout). The base layer has what the others leave, as the client's
  blend does. The terrain heights for the slope are the chunk's `MCVT` (`+0x11C`, the same read as the terrain
  shadows).
- **The snow map.** A 512 x 512 texture, one texel per 1.04 yd (32 per chunk), covers the 16 x 16 chunks around the
  camera, about 533 yd across. It wraps: world texel `floor(x / 1.04)` sits at `that mod 512`, so the window moves by
  filling the chunks it gains in place, nearest first, eight chunks a frame. A chunk is read again when its address,
  its layer pointer or its alpha pointer change. Each texel holds the share of snow layers (green; four alpha texels
  averaged), the depth over 2 yd (red: `SnowDepth` x `1 + SnowDrift x (2 noise - 1)`, the noise smooth over 8 texels,
  x the lowest, within 2 texels each way, of a smoothstep of the share from 0.5 to 0.9 times the slope factor), the
  slope factor (alpha: 1 below `SnowMaxSlope - SnowSlopeFade` falling to 0 at `SnowMaxSlope`, from the gradient of each
  terrain cell, blended between cells) and a fixed per-texel noise (blue). The texture has mipmaps where
  `D3DUSAGE_AUTOGENMIPMAP` is available. The client paints rock and snow blended on the same terrain, and the raised
  ground carries every layer with it, so rock was lifted with the snow and a character on it stood inside the rock (seen
  in Dun Morogh, 2026-11-02): ground at most half snow is not raised, and the lowest-of-the-neighbours step keeps the
  snow down for 2 yd around any such patch, so the vertices on and beside the rock stay at the client's height. The
  depth is recomputed for a chunk and the 2 texels around it whenever the chunk is read or cleared.
- **The terrain shaders.** `CMap::Initialize` (`0x79E7C0`, `Map.cpp:686`) loads the terrain shader sets into tables
  of `CGxShader*`: 128 vertex shaders at `0xCE0008` (`68 08 00 CE 00 FF D2` at `0x79E9A2`), `Terrain0` at `0xCE0488`
  (`0x79E9C5`), `Terrain0_env` at `0xCE0004` (`0x79E9E8`), `Terrain1` at `0xCE0408` (`0x79EA36`), and from
  `CMap::InitializePCFShaders` (`0x79E4F0`) `Terrain2` at `0xCE0388` (`0x79E574`) and `Terrain3` at `0xCE0208`
  (`0x79E5A0`). The client creates each variant when it is first bound: `IShaderCreateVertex` (`0x6AA0D0`) and
  `IShaderCreatePixel` (`0x6AA070`) hand `CreateVertexShader` and `CreatePixelShader` the shader's `m_code.m_data`, its
  `+0x50` (`83 7E 4C 00 C7 46 30 01 00 00 00 74 2C 8B 56 50` at `0x6AA0E7` and `0x6AA087`). So the device wrapper knows
  a terrain shader by its bytecode pointer: it is the `+0x50` of an entry of those tables. Nothing in the client is
  patched for this. The vertex shaders the client loads are `Shaders\Vertex\vs_3_0\Terrain.bls` from `patch-E.mpq`
  (a custom build with six more point lights in c236 to c251); the pixel shaders are the stock `patch.MPQ` ones.
- **The lift.** Every vertex shader takes `v0` as the world position (`mul r0, c1, v0.y` ... `add r0, r0, c3` is the
  view transform). The rewrite puts a block first that reads the snow map at `v0.xy / 533.3` with `texldl` from vertex
  sampler 0, scales red by 2 yd and by a fade over the last 16 yd of the window, and adds that to `z` in `r12`; every
  later read of `v0` reads `r12`. The lift is a function of the world position alone, so vertices shared by two chunks
  stay together. The lifted world position leaves through `texcoord7`: a new output where the shader has fewer than 11,
  and in the 32 variants that use all 11 the unread view position output (`dcl_texcoord3 oN.xyz`, `mov oN.xyz, r0`)
  is relabelled; no terrain pixel shader declares `texcoord3` with three components (checked on the extracted
  shaders; the Living Azeroth module relies on the same output). The constants are c200 to c202, which no client
  vertex shader reads (checked on all 86 vs_3_0 and ps_3_0 shader files of the client's MPQs). Because
  `IShaderConstantsFlush` (`0x6A9FE0`, `A1 E8 FF C5 00 56 8B F1 8B 0D EC FF C5 00`) uploads the whole span between the
  lowest and highest register changed since the last flush, and a terrain draw changes c0 and c251, the frame's values
  are also written into the client's own copy (`CGxDevice::s_shadowConstants`, the vertex array at `0xC5EFE8`, its
  `dirtyMax` at `0xC5FFE8` and `dirtyMin` at `0xC5FFEC`) and marked changed, so the flush sends them rather than stale
  ones.
- **The edges.** The pixel shaders with the stock fog ending (`add rN.xyz, rN, -c2`, `mad oC0.xyz, vM.x, rN, c2`; all
  but two of `Terrain0` and `Terrain0_env`) get a block before it. It reads the snow map at the pixel's world position
  (`v9`, sampler s9) and again at 4.7 times the frequency, turns the coverage plus the two noises times `SnowBreakup`
  into an amount with a sharp ramp around 0.5, and whitens only the part that exceeds the coverage the client already
  paints, times the slope factor, the window fade and `SnowEdges`. The white is the snow's albedo (0.9) lit by the
  vertex light (`2 v0`, as the stock shaders light their layers) and dimmed toward the pixel's own brightness in shade,
  so it follows the sun and the shadows. Fully painted snow keeps the client's own texture. The pixel constants are
  c220 to c223; client pixel shaders read nothing above c26. A pixel shader is only fed a world position when every
  terrain vertex shader took the lift; otherwise `SnowEdges` acts as 0.
- **Footprints.** Your character and the 16 nearest units within 30 yd leave footprints in the snow as they walk,
  and the prints fill back in over `SnowFootprintSeconds` (30). The first version followed the Living Azeroth
  module's trenches (round stamps along the whole path, 1.3 times the collision radius wide); in the game one
  body-wide trench per unit read as a sledge track, so the owner asked for separate feet. A unit stamps only while it
  stands on the terrain: its height within 0.6 yd of the terrain under it, from the chunk's `MCVT` over the four
  triangles of each cell meeting in its centre vertex (the vertex grid `0x7C3C60`), so units on bridges, in
  buildings, flying or swimming leave none. Each unit walks its own path: the distance it covers is cut into steps
  of `size x (0.45 + 0.11 speed)` yd (about 0.8 yd walking at 2.5 yd/s, 1.2 yd running at 7), where `size` is the
  collision radius over a human's 0.306 (0.5 to 3), and each step plants the next foot at the exact point along the
  path where the step ends, so the spacing does not depend on the frame rate. People plant left and right feet in
  turn, 0.11 size yd either side of the path: oval prints 0.34 by 0.16 size yd, turned to the unit's facing, so a
  unit walking backwards or sideways leaves them turned the way it faces. Mounts (at least size 1.6) and four-legged
  creatures plant diagonal pairs, the front foot 0.45 size yd ahead and 0.13 size yd to the side and the hind foot
  as far behind on the other side, 0.8 as wide, as round hoof prints 0.075 size yd across the radius. A new unit, one
  landing and one that moved more than 3 yd in a frame stands on all its feet at once. Each foot drags a shallow scuff
  from its last print to its next one, 0.8 of the print's half width wide and 0.4 of the print's depth at a walk,
  falling to 0.15 at a run; the snow pushed up around a print is the rim. The footprint map is 1024 x 1024 texels of
  0.0625 yd (a human print spans five texels by three), 64 yd around the player, wrapped like the snow map: red is
  how far the snow is pressed (0.9 of its depth on a flat floor out to 0.35 of the print, then a smoothstep up to the
  surface at its edge), green the rim (a sine bump from 0.8 to 1.7 of the print, kept below what the press leaves),
  blue the print's own random byte for the fade. Only the rows a stamp, the relaxing or a moved window changed are
  uploaded, in runs of whole rows (`LockRect` with that rectangle; DXVK's managed textures track the dirty box and
  upload only it, `d3d9_device.cpp` `AddDirtyBox`), and the relaxing steps every 0.25 s.
- **Which units walk on four legs.** A mounted unit always does. Otherwise the creature type decides, read the way
  `CGUnit_C::GetCreatureType` (`0x71F300`, checked in pieces at `0x71F313`, `0x71F324`, `0x71F337` and
  `0x71F34A`) does: the shapeshift form (the byte at `+0x1D3` of the unit fields at
  `+0xD0`, unless the byte at `+0x9F4` suppresses it) looked up in `SpellShapeshiftForm` (minimum id `0xAD4A04`,
  maximum `0xAD4A00`, records `0xAD4A14`, creature type at `+0x10`), else the creature cache entry at `+0x964` (type
  at `+0x10`; `CGUnit_C::GetCreatureFamily` `0x7153E0` reads the family at `+0x14`). Beasts and critters walk on four
  legs except the two-legged families (carrion birds 7, raptors 11, tallstriders 12, birds of prey 26, dragonhawks 30
  and devilsaurs 39), so a druid in cat or bear form and a shaman's ghost wolf leave four tracks. The facing is the
  movement's at `+0x20` (`CGUnit_C::GetRawFacing` `0x6E6F60`: `mov eax, [ecx+0xD8]` then `fld [eax+0x20]`).
- **Drawing the footprints.** They are not geometry: the rewritten terrain pixel shader, inside the footprint window
  and where the snow has depth, marches the view ray from the camera through the height field
  `depth x (rim x SnowFootprintRim - press)` below the raised surface in 13 probes (`texldl` on sampler s10, so it may
  run inside the branch), takes the first point under the field and moves it back to where the ray crosses the field
  between that probe and the one before (one secant step), and lights the ground there by the field's normal from
  central differences over two texels: the colour the client drew times
  `(ambient + (1 - ambient) N.L) / (ambient + (1 - ambient) L.z)`, at most 2, and darkened by 0.35 of the press.
  A first version with nine probes, no secant step, a one-texel forward difference and a `sqrt(1 - (d/r)^2)` wall
  drew stepped contour lines across the walls and looked blocky in the game. `L` is the direction to the sun or moon
  from the sky's positions (`0xD38B18`, `0xD38E28`, `0xD38E48` by the day window `0xA41CA4` and `0xA41CA0`) and the
  ambient share the ambient colour's (`0xD38BD4`) part of the ambient plus direct (`0xD38BD8`), the reads the fog's
  frame inputs already use. So a print keeps the client's own snow texture, light and shadow. The prints fade out
  the way the grass copies thin out: each pixel takes its print's random byte `t` and is drawn at
  `clamp((reach - t x SnowFootprintFade) / 1.5 yd)` of its depth, where `reach` is the nearer of `SnowFootprintRadius`
  (28) less the distance from the camera and the distance to the edge of the footprint window (which recentres every
  8 yd, so its edge is 24 to 32 yd from the player). So prints go one by one over the last `SnowFootprintFade` (8) yd
  instead of at a line. The pixel constants are c204 to c207 (switch, rim share, window, camera, light) and c211
  (fade distance, fade width, the 1.5 yd softness and the two-texel normal spacing); the shader defines c208 and
  c209.
- **Trench depth.** Without it the depth buffer holds the raised surface, so a character standing in a trench was cut
  off at the old snow line. The rewritten vertex shader also hands the pixel shader its view depth in texcoord7's `w`
  (the `z` row of the world-view transform in c0 to c3, which all 128 variants start with `mul r0, c1, v0.y / mad r0,
  v0.x, c0, r0 / mad r0, v0.z, c2, r0 / add r0, r0, c3`, the projection `o0` from c4 to c7). Each rewritten terrain
  pixel shader is assembled twice, and the wrapper creates the second, depth-writing copy beside the one it hands the
  client. At the first indexed draw after the client binds a terrain pixel shader (and once a frame), while the
  footprint constants are on and the footprint map has texels in use, it binds that copy and sets c210 to the depth
  mapping the fog uses to read depth back (`depth = a + b / viewDepth`, `a` and `b` from the client's current projection
  `_33` and `_43` and the viewport's depth range with the client's pending range, the same reads as the opaque state
  capture); otherwise the client's copy stays bound. The depth copy writes `oDepth` for every pixel: the surface's view
  depth, or at the trench point the view depth scaled by how far that point lies behind the surface along the view ray,
  pushed back by a factor 1.00001 so that a decal drawn later with the same geometry still passes its depth test.
  Writing `oDepth` turns off early depth rejection for those draws, which is why the copy is used only near trenches.
- **Binding.** When the client binds a rewritten terrain vertex shader, the wrapper binds the snow map to vertex
  sampler 0 and to sampler s9 (wrap, linear), the trench map to s10, and sets the constants. The client's terrain
  uses samplers s0 to s8. In the game, with depth above 0, neighbouring terrain chunks were sometimes raised on one
  side of their shared edge and not on the other, opening a gap the sky showed through, and which side was raised
  changed as the camera turned. The lift itself cannot do that: with shader vertices the client builds every chunk's
  vertices in world coordinates (`CMapChunk::CreateVertices` `0x7C54C0` calls `CreateVerticesWorld` `0x7C3F30`
  while `enableTerrainShaderVertex` is set), so the two chunks' edge vertices read the same texels of the same map.
  It can only happen when a chunk is drawn without the snow constants or the snow map. The client sets its terrain
  constants per chunk from c0 (`CMapRenderChunk::SetVertexShader` `0x7D0050`), and the wrapper binds ours only when
  the client changes the vertex shader. So before every indexed draw with a rewritten terrain vertex shader, the
  wrapper now reads c200 to c202 and the texture on vertex sampler 0 back from the device, and binds them again if
  anything replaced them. The status line counts those draws and the ones that had to be rebound. In the game, in
  Dun Morogh, about 5% of the terrain draws had to be rebound (50,648 of 969,781 after four minutes), and the gaps were
  gone.
- **Ground circles.** Selection circles, the target circle, area spell targeting and the other projected textures are
  drawn over copies of the terrain's triangles: `ProjectTexRender2d` (`0x7E3E80`) and `ProjectTex2dDrawShaderEffect`
  (`0x7E3AA0`) query them (`World::GetTris`, `CMap::QueryTris`), and for each batch build a vertex buffer from the
  batch's heights (`ProjectTexBuildVertexBuffer` `0x7E2FD0` with normals, the variant `0x7E32F0` with a colour). For
  terrain the batch (`World::TriData::Batch`) holds the chunk's own heights at `+0x0C` (`batch->m_heightList =
  chunk->m_height` in `CMap::GetTriSubchunk` `0x7A6260`), the shared 145-vertex grid `CMapChunk::vertexList` at `+0x04`
  and a translation to the chunk corner at `+0x00`; buildings have no heights. Both builders read the heights through
  the batch when they fill the buffer (`cmp [esi+0x0C], ebx` at `0x7E3006` and `0x7E3327`, `mov edi, [esi+0x0C]` / `fld
  [edi+edx*4]` at `0x7E3046`, the index and vertex reads at `0x7E3020`, `0x7E3030`, `0x7E3340` and `0x7E3350`, the
  colour one's height read `mov edx, [esi+0x0C]` at `0x7E3366`), with the batch in `esi` (the colour one takes it in
  `eax`, `mov eax, esi` before the call). With any snow depth these circles lay under the raised snow. The four calls
  (`0x7E3CAC` and `0x7E3CC4` in the shader effect render, `0x7E41CB` and `0x7E41E3` in the 2D render, bytes checked from
  `0x7E3CA2`, `0x7E3CBC`, `0x7E41C1` and `0x7E41DB`) are retargeted to thunks that, while the terrain shaders take the
  lift, point the batch at a copy of its 145 heights each raised by the snow under that vertex, call the client's
  builder with the same arguments and put the chunk's heights back. The snow under a vertex is read on the CPU the way
  the rewritten vertex shader reads it: the snow map's red bilinear between texel centres, times the 1.2 yd range and
  the window fade. The circles follow the snow surface, not the footprints.
- **Limits.** The terrain vertices are 2 to 4 yd apart, so the raised ground ramps up over a few yards at a snow edge;
  the sharp outline comes from the edges. Collision stays the client's, which is why characters sink in. Spell
  effects that are models placed on the ground, not projected textures, and the client's own footprint decals stay
  at the old ground. Doodads and buildings stand in the snow as placed. Snow beyond about 260 yd is not raised.
- **Settings.** `Snow`, `SnowDepth`, `SnowMaxSlope`, `SnowSlopeFade`, `SnowDrift`, `SnowEdges`, `SnowBreakup`,
  `SnowFootprints`, `SnowFootprintSeconds`, `SnowFootprintRim`, `SnowFootprintRadius` and `SnowFootprintFade` in
  `CoAGraphics.ini` and the Snow section of the settings window. The shader rewrite is made as the client creates its
  shaders, so it needs `Snow=1` at start; the keys then apply within a second, and `Snow=0` lowers the ground again.
  The log line `snow: map ...` once a minute counts the chunks read, those with snow, the shaders rewritten, the
  footprint texels in use, the prints since the line before, the footprint rows uploaded and the ground circle
  batches lifted since the line before.
- **Checks.** The harness reads the client's terrain shaders from `tests/shaders` (the vs_3_0 vertex shaders of
  `patch-E.mpq` and the `Terrain0` to `Terrain3` ps_3_0 pixel shaders of `patch.MPQ`) and checks that all 128 vertex
  shaders and the 161 pixel shaders with the fog ending take the rewrite and reassemble, that the 32 full variants
  hand over the view position output, and that a rewritten shader is refused a second time. It checks the layer rule,
  the three alpha map formats (and refuses cut-short ones), the depth, slope and drift rules, the texel packing, and the
  snow map with a synthetic terrain: chunks nearest first and a few a frame, the window box, an unchanged window read
  no more, the alpha rows along world x, refills on a changed identity, unloaded chunks, a moved window, steep ground
  and a new map. On the device it draws a point with the rewritten vertex shader and the uploaded map: raised by the
  map's depth, not raised outside the window and half raised in the fade; the CPU reads the same lift at the
  camera and in the fade. For the footprints it checks the print shape and rim, a stamped segment, a shallow one, an
  unchanged restamp and that a stamp marks only its own rows, relaxing (only the rows with prints), the moved window
  (no stale print at its wrapped place), the gaits (two oval feet, four round ones for mounts and beasts, steps
  lengthening with speed, less drag at a run, the four-legged families), a standing unit's two prints and none
  between them, a walk planting left and right feet one step apart with a shallow scuff between a foot's prints, a
  jump, a standing mount's four hooves and none under its belly, the constants and the ambient share. For the
  ground circles it lifts a stand-in terrain batch by a known lift and puts its heights back, and leaves a batch
  without heights alone. On the device it draws a trench under a light from one side with a stand-in for the
  client's pixel shader rewritten the same way: the floor darker, the wall toward the light brighter than the one
  away, untouched snow, a switched-off map and a trench beyond the footprint distance as the client drew them. It
  checks that every pixel shader also reassembles with the depth write, the depth row, and on the device draws the
  trench with the depth-writing copy into a depth buffer and then a plane 0.05 yd under the snow: the plane shows in
  the trench and stays hidden behind untouched snow. In the game the first trenches (about 3,000 to 6,000 unit
  stamps a minute in Dun Morogh) looked blocky with the character hidden below the old snow line; the secant step,
  the rounder profile and the depth write fixed that (owner, 2026-11-02, at `SnowDepth` 0.15). The footprints, the
  fade and the lifted ground circles have not been seen in the game yet.

## Target facing arrow

The client draws the target's ground circle as a projected texture and turns it toward the camera, so it never shows
which way the target faces. CoAGraphics draws a second projected texture after the circle, with an arrow at its rim
turned to the target's facing (`TargetArrow`).

- **How the circle is drawn** (wowdecomp `Unit_C.cpp`, `Object_C.cpp`, `shadow.cpp`, `ProjectTex.cpp`).
  `CGWorldFrame::ClearTrackedObjects` (`0x004F6F90`) calls the locked target's `RenderTargetSelection` through vtable
  slot `+0x6C` (`mov eax, [edx+6Ch]; call eax` at `0x004F6FC9`); `CGObject_C::PreAnimate` (`0x00743EC0`) only tracks a target that is
  not the active player and only while `ObjectSelectionCircle` is set. `CGUnit_C::RenderTargetSelection`
  (`0x00725980`) keeps `this` in `esi` from `0x00725990` (`56 8B F1`), builds the box as the model position ± the
  selection radius (`+0xB0C`), the colour from `GetSelectionHighlightColor` and the camera-facing rotation from
  `CGObject_C::BuildSelectionRotMatrix` (`0x00744150`), sets the circle's states with
  `CGObject_C::SetCircleRenderStates` (`0x00744EB0`: `Textures\UnitSelectTexture.blp` on stage 0, the add fade
  texture on stage 1, additive blending), then calls `ProjectTex::Render2d` (`0x007E4370`, cdecl, seven arguments:
  box, colour, rotation, fade 0.5 from `0x009E2EC4`, `0x200122`, 1, polygon offset 0.4 from `0x009F98D8`) at
  `0x00725A8A` (`E8 E1 E8 0B 00`) and pops them with `add esp, 1Ch` at `0x00725A8F`. The combat flash circle later in
  the function calls `Render2d` from another site and is left alone, as is the pet target circle
  (`RenderPetTargetSelection`, `0x00725BF0`).
- **Where it hooks.** The call at `0x00725A8A` goes to a thunk that passes the seven arguments and `esi` (the unit) to
  the DLL. It calls `Render2d` with the client's arguments, then, when `TargetArrow=1`, reads the unit's facing with
  `CGUnit_C::GetRenderFacing` (`0x007156A0`, thiscall, `float` in `st0`: `m_renderFacing` at `+0xA94` through
  `CPassenger::GetFacing` at `0x004F42A0`, which adds the transport's facing for a passenger) and calls `Render2d` a
  second time with the same colour, fade, flags and polygon offset, a box widened to 1.35 times the circle's half size
  in x and y (z unchanged, so the depth fade is the circle's), and the rotation `RotationAroundZ(-facing)`, the form
  `UnitMissileTrajectory_C` uses for its facing-dependent impact texture. The bytes guarded before patching are the
  unit register, the render-state call, the argument pushes and the cleanup of the circle call, and `GetRenderFacing`
  with its call into `CPassenger::GetFacing` (`TargetFacingGuards`, bytes in `0x00725A56`–`0x00725A91` and
  `0x007156A0`–`0x007156B5`).
- **Why the arrow lands in the facing.** `ProjectTex2dMakeMatrices` builds the texture matrix as
  `T(camera - centre) * S(1/size) * Rz(-90°) * rotation` plus 0.5 on u and v; the texture coordinates are the vertex
  position minus the camera (the world transform is the batch matrix times a translation by `-camera`). With
  `C44Matrix::Rotation` and `RotationAroundZ` both `a0 = c, a1 = s, b0 = -s, b1 = c` (row vectors), a world direction
  `(cos f, sin f)` becomes `(sin f, -cos f)` after the spin and `(0, -1)` after `RotationAroundZ(-f)`: the top of the
  texture. The arrow texture has the arrowhead at the top, from 0.93 to 1.15 circle radii out (notch apex at 1.01,
  half width 0.16). The notch is cut by a triangle that reaches past the back edge (to 0.89, half width 0.24), so the
  arrow is two arms meeting at the tip and has no bar along its back.
- **The arrow texture.** The ring and colour are the client's; only the stage 0 texture changes. While the second
  `Render2d` runs, the device wrapper holds stage 0 on a 512 × 512 A8R8G8B8 managed texture with a full mip chain
  (white, alpha = coverage × 0.9 × a brightness that falls from 1 at the back edge to 0.45 at the tip, so the arrow
  glows where it meets the ring; coverage is smoothstepped across 5 texels at every edge, every level's edge texels clear) and clamps its addressing; a
  `SetTexture` or address-mode change the client makes on stage 0 meanwhile is kept and put back afterwards. Only
  standard `SetTexture` and `SetSamplerState` calls are involved, so no driver difference applies (DXVK not checked).

The harness checks, through a C++ copy of the client's texture matrices, that the arrow lies outside the circle in the
facing for seven facings and nowhere behind, beside or inside it; that every texture level has a clear border; the
guards; the stage 0 hold and restore on the wrapped device; and that the thunk draws the circle unchanged, then the
arrow with the unit's facing under the arrow texture, keeps `esi` and the stack, and draws only the circle with
`TargetArrow=0`. That does not establish in-game behaviour: the arrow has not been seen in the game yet.

## Engine inputs

All addresses are static in the 12340 image (base `0x400000`, timestamp `0x4C2452FE`, both checked at start-up);
Gx is the graphics device `[0xC5DF88]`.

| Input | Address |
|---|---|
| View, projection | Gx `+0x1B00 + 64·[+0x1AF8]`, `+0xF88`; copies at `0xADF5E8`, `0xADF628` |
| World viewport depth range (MinZ, MaxZ) | Gx `+0xF80` |
| Camera position / look-at target | `0xCD8F5C`, `0xCD8F68` |
| Camera focus: the unit the camera follows | `0xCD7778` (stored by `0x780860`), else the camera position |
| Current map (`DayNight::s_mapId`) | `0xD38AC4` |
| Day fraction | `0xD38B04` |
| Stock fog groups: colour, start, end (read by the render callbacks) | `0xD38B8C`–`0xD38B94`, `0xD38BA0`–`0xD38BA8` |
| Zone fog distance (light float band 0) | `0xD38C1C` |
| Light colours: ambient, direct, sun | `0xD38BD4`, `0xD38BD8`, `0xD38BF8` |
| Visible sun / moon sprite, sky centre | `0xD38E28`, `0xD38E48`, `0xD38B18` (day window `[0xA41CA4, 0xA41CA0]`) |
| Camera in liquid | `0xCD8794` |
| Storm weight (0..1) blending the clear and storm light params | `0xD38B88` |
| Light params slot forced by the current screen effect (−1 = none) | `0xD38B58` |
| Glow: `ffx` CVar, current effect, glow effect, glow amount | `0xD45774`, `0xD45780`, `0xB74364`, `0xD38C2C` |
| Ghost (death) screen effect | `0xB74368` |
| Glow byte: pass list count and array, pass [2], its colour's alpha | `+0x0C`/`+0x10`, `+0x20`/`+0x24`; `+8`; `+0x33` |
| Far clip | From the projection; `[[0xB7436C] + 0xB14]` as a fallback |
| World M2 scene (point lights) | `[0xCD754C]` |
| Camera WMO instance, group ids, group count, interior fog blend | `0xCD87A4`, `0xCDB0DC`, `0xCDB0D8`, `0xD38B9C` |
| Sky colours (bands 2–7: top, middle, band 1, band 2, smog, fog) | `0xD38BE0`–`0xD38BF4` |
| Water colours: ocean close/far, river close/far | `0xD38C0C`, `0xD38C10`, `0xD38C14`, `0xD38C18` |
| Liquid renderer, transparent (water) draws queued this frame | `[0xCD8610]`, `[[0xCD8610] + 0x14]` |
| Liquid settings bank: count, entries | `0xD43B18`, `0xD43B1C` |
| `LiquidType` rows: maximum id, minimum id, row pointers | `0xAD4070`, `0xAD4074`, `0xAD4084` |
| Object manager, visible list: link offset, first object | `[[0xC79CE0] + 0x2ED0]`, `+0xA4`, `+0xAC` |
| Object: descriptors (type mask `+8`), GUID, list link, world entity | `+0x08`, `+0x30`, `+0x38`, `+0xB8` |
| Unit: movement block (pointer, embedded block), transport GUID, raw position | `+0xD8`, `+0x788`, `+0x790`, `+0x798` |
| Unit movement flags (swimming), collision radius and height | `+0x7CC`, `+0x850`, `+0x854` |
| World entity: world position, liquid flags, liquid surface | `+0x6C`, `+0x7C`, `+0x80` |
| Client `waterRipples` value | `0xADF7F0` |

Engine notes behind the code:

- Call sites. `0x4FB03D` (`E8 5E DE FF FF`) calls the world render `0x4F8EA0`, a thiscall on the world frame
  with no stack arguments; the thunk keeps ECX across the frame-begin hook. `0x4F911D` (`E8 8E AB 32 00`) calls
  the opaque M2 pass `0x823CB0`, a thiscall with one stack argument (`ret 4`). `0x4F9170` (`E8 AB 5E 28 00`)
  calls the liquid surface pass `0x77F020` (no arguments, outside liquid only). `0x4F9281` (`E8 8A 7D 3C 00`)
  calls FFX end `0x8C1010` (no arguments); the thunk renders the fog, then tail-jumps to it.
- Loader slot. On disk `[0xB2ED98]` holds the delay-load thunk `0x41C654`, which pushes the slot (`0x41C65F`)
  before resolving it; `0x41C673` jumps through it. The hooks install only while the slot still holds the thunk. Any other value is another
  hook: the engine, transparent fog and water hooks are not installed, while `LightStay` and the far-clip calls still
  apply, and `CoAGraphics.log` names the slot's value.
- Matrices. `0x6821A4` reads the camera-relative view matrix at Gx `+0x1B00 + 64·[+0x1AF8]` and then the projection at
  `+0xF88`. Gx `+0x1AF8` is the level of the view `CGxMatrixStack` (`m_xforms[10]`, 0x118 bytes each from `+0x1008`),
  which holds four matrices, so the level is read only below 4 (wowdecomp `CGxMatrixStack.h`). The engine builds its
  projection with `0x6BF370` (OpenGL depth range, w = view z); the D3D backend
  converts it.
- Depth. The world viewport's MaxZ is `[0xADEEE4]` = 0.94 (loaded at `0x4F9019`, passed to GxXformSetViewport
  at `0x4F905A`) and uploaded as `D3DVIEWPORT9::MaxZ` by the D3D9 backend (`0x6A9ACC`). The Gx viewport is
  stored by `0x681890` and uploaded lazily on the next draw or clear (`0x6A99E0`), so after the opaque pass the
  device can still hold the sky's `[0.999, 1]`; the capture reads MinZ/MaxZ from the Gx viewport, which the sky
  and WDL passes restore (`0x7F0CB3`, `0x796466`). The distant WDL terrain draws into `[0.998, 0.999]` with its
  own viewport and projection (`0x7960EB`); the sky viewport is set at `0x7F0A79` and the sky writes no depth,
  so depth at or above max(deepest world depth, 0.99903) is sky.
- Multisampling. The Video options' multisample list (`0x54F1B0`) is built once per process (it returns while
  `[0xBEA744]` is set) by `0x68A170`, which drops each count of `0xAD8CE8` = {0, 2, 4, …, 16} that the wrapped
  `CheckDeviceMultiSampleType` refuses (`0x68A2B9`, `0x68A2DE`). `gxMultisample` (`0xCABCF8`) asks for `gxRestart`,
  not established to reset rather than recreate the device, so `CreateDevice` (`0x68F4DE`) and `Reset` both choose
  between the copied and the bound `INTZ` depth. The present-parameter builder `0x68E250` takes the depth format from
  `0xA2E4A8` = {…, D16, D24X8, D24S8, D32} at `[0xCABCE8]`, where `gxDepthBits` "24" stores D24X8 (`0x7692D0`).
- Depth copy. `NvAPI_D3D9_StretchRectEx` copies a multisampled D24S8 depth into a registered `INTZ` texture, one
  sample per pixel. The NVAPI functions come from `nvapi_QueryInterface` with the IDs of NVIDIA's public headers:
  `0x0150E828` Initialize, `0xA064BDFC` RegisterResource, `0xBB2B17AA` UnregisterResource, `0x22DE03AA` StretchRectEx.
  `nvapi.dll` is loaded by its full path in the system folder (`GetSystemDirectoryW`, which WOW64 redirects to
  SysWOW64 for the 32-bit client) with `LOAD_WITH_ALTERED_SEARCH_PATH`, so its imports are also searched there first;
  the `LOAD_LIBRARY_SEARCH_SYSTEM32` flag would need KB2533623 on Vista and Windows 7. A failed load is logged with its
  Windows error.
- Liquid depth. While writes are forced on, the wrapper records the client's own `D3DRS_ZWRITEENABLE` requests
  and re-applies the last one afterwards, so the client's render-state cache stays accurate.
- World-name text. `0x7E5818` (`E8 23 76 ED FF`) calls `0x6BCE40`, a cdecl wrapper that takes the font batch
  `[0xD380A8]` and tail-jumps to `0x6C53A0`. The batch is created by `0x6BF160(1, 1)` at `0x7E6511`; its bit 0
  at `+8` selects world rendering (`0x6C5564`), which requests depth testing (Gx state 13, `0x6C5591`) and depth
  writes (Gx state 15, `0x6C55BE`, dispatched as `D3DRS_ZWRITEENABLE` at `0x6A8FC7`). Suppressing the writes
  keeps glyph quads out of the depth the fog is integrated against, where they would end fog rays at the text,
  while keeping the draw and its depth test. The text is drawn without depth writes after lightning, missile arcs and
  the glare, so a fog composite after it fogs each glyph by the depth behind it: names in front of the sky took the
  sky's full fog and vanished, while the same name in front of a nearby tree stayed visible. The thunk therefore draws
  the fog first when it has not composited this frame, as the stock client leaves names unfogged, and the world-done
  hook then skips it. A name in front of a tree was still reported with the sky's fog beside the trunk with only this
  hook, so the glare thunk at `0x4F9213` draws the fog when the glare pass returns, just before the world text call
  (`0x4F9218`) that its byte guard checks; the text hook stays as the fallback while the glare thunk is not installed.
  The later name/icon path at `0x4FB042` is left in place.
- Registers across the two plain thunks. After the call at `0x4F9213` the next instructions are `call 0x7E5580` (which
  loads `eax` from `[0xAF4704]` itself) and, at `0x4F9235`, `mov ecx, [0xC5DF88]`; `esi`, `edi` and `ebp` are the only
  registers the caller keeps and a cdecl function preserves them. The glare entry `0x7F0870` already clobbers `ecx`.
  After `0x7E5818` the caller does `add esp, 0x14` and jumps to `0x7E5834`, which reloads `edx` and `eax`; `ebx`, `esi`
  and `edi` are used later and are preserved. The x87 stack is empty at both calls (`fld`/`fstp` pairs balance before
  `0x4F9213` and at `0x7E57FC..0x7E5809`), and both targets are cdecl, so the plain C thunks need no `pushad`.
- Native world shadows. The 12340 image registers the map-shadow callbacks at `0x7BD3A0`: matrix construction
  `0x7BAC10`, cascade setup `0x7BAFD0`, caster gathering `0x7BD200`, and rendering `0x7BBC50`. Exterior shadow quality
  is `[0xD43154]`; `[0xB1D51C]` means its resources need rebuilding. Qualities 1 and 2 supply the dynamic character
  map; qualities 3 and 4 add three environment caches, and quality 5 updates cascaded environment maps. The default
  character extent is 20 yd (`0x875E6E`); environment extents are 40, 160 and 640 yd (`0xB1D520`). Caster gathering
  traverses map chunks and tests their shadow frusta (`0x7BC490`, `0x7BCC00`, `0x7BC890`, `0x7BCF20`), so visibility
  includes geometry outside the camera image, within the client's loaded and culled shadow scene.
  `0x8750B0` constructs four view-space-to-shadow transforms, packs three float4 rows each at `0xD43348` (the first
  store is `D9 1D 48 33 D4 00` at `0x87526B`), and uploads all twelve rows at `0x87450A`. It inverts the current Gx view (`0x7BAE25`), builds the light view, and applies a Y
  flip and a depth scale of 1/4000 (`0x875197`, `0x7BC105`), which the client keeps in `CShadowCache::s_shadowScale`
  `[0xD431C8]` (`D9 05 C8 31 D4 00` at `0x8751A3`) and the hook checks before it reads the maps. Both final XY axes map to UV with `0.5*x + 0.5`; Z is
  normalized linear light-view depth. The constant receiver biases are 0.4 yd for the character and first environment
  map, 0.8 yd for the second and 1.6 yd for the third (`0x7BAF4E`–`0x7BAFBE`), with no surface-normal dependence.
  The native direction comes from `[[0xCE04A8] + 0x7C]`, has its Z multiplied by five and capped at -1.2, and is
  normalized before storage at `0xD43180` (`0x7BB570`–`0x7BB628`). It points from the light into the world; volumetric
  direct scattering reverses it. This direction follows the client's surface-shadow lighting, not the visible
  celestial sprite: `DayNight::SetDirection` `0x7EEA90` takes it from fixed tables, pointing along azimuth 225° with
  a polar angle of 127° at midnight and noon and 110° at 06:00 and 18:00. After the Z scale and cap the maps are
  therefore always cast from 52–56° above the horizon, from azimuth 45° where the sun and moon also stand
  (`0x7EECC0`), whatever the sun's or moon's elevation. The optional sprite-centred god rays remain a separate effect.
  Texture handles are `[0xD43250 + 4*hardwareComparison]` and
  `[0xD43290 + 0x3C*cascade + 4*[0xD432C8 + 0x3C*cascade]]`, with hardware comparison selected by `[0xD43014]`.
  A texture resource holds its Gx texture at `+0x44`, or its cache at `+0x5C` holds the Gx texture at `+0x18`
  (`0x4B6CB0`–`0x4B6D80`); Gx `+0x38` is the D3D texture passed to `SetTexture` (`0x6A492E`, `0x6A88D6`). Native
  format 11 is `R32F` (`0xA2F81C`), while format 12 explicitly creates `D24X8` depth textures (`0x6A2C62`,
  `0x6A2D01`). The shipped `ShadowMapSL.bls` pixel shader writes `max(lightViewZ/4000, 0)` to red. The shipped
  `Terrain2_pcf.bls` uses `texldl` with `(0.5*XY+0.5, Z, 0)` for hardware depth comparison; the native texture flags
  select linear minification and magnification with no mip filtering for that path (`0x875DC1`, `0x6A4944`).
  Capture checks the image and relevant instruction bytes, the live formats and matrix values, and texture device
  ownership. It retains D3D references only for the fog draw, releases them on every exit, and falls back when the
  native maps are unavailable. These static contracts and offline tests do not establish in-game visual alignment.
- Terrain heights. The terrain query of the shadow maps, `0x7BCF20`, reads the map's area table at `0xCE48D0`
  (`0x7BCF9B 8B 04 85 D0 48 CE 00`) at `64·(row >> 4) + (col >> 4)`, the chunk row counted along world −X and the
  column along −Y in 33.33 yd steps from 17066.67 yd. It skips an area whose asynchronous load at `+0x70` is set
  (`0x7BCFAA 83 78 70 00`) and takes the chunk at `+0xBC + 4·(16·(row & 15) + (col & 15))`
  (`0x7BCFC1 8B 84 B0 BC 00 00 00`). `CMapChunk::GetVertexPosition` `0x790520` places vertex `i` at the offsets of
  `0xD25498` from the chunk's top-left corner at `+0x7C` (`0x790534 D8 41 7C`), its height `[+0x11C][i]` plus the
  corner's Z (`0x79054C 8B 81 1C 01 00 00`). The vertex grid `0x7C3C60` lays out the 145 heights as nine rows of nine
  outer vertices 4.17 yd apart, each row but the last followed by eight centre vertices, so outer vertex `(r, c)` is
  index `17r + c` at `(−4.17r, −4.17c)` from the corner. The fog reads a chunk only when its corner lies at its row
  and column. The heights point into the tile's file buffer and need not be 4-byte aligned (in Dun Morogh, chunk
  674/531 on 2026-11-02 had them at `0x36ECCD2E`), so only their range is checked; an alignment test once left
  whole tiles out of the terrain shadows and the snow. The shadow map render `0x7BBC50` draws WMO groups and M2
  batches only; the terrain queries `0x7BCF20` and `0x7BCC00` walk the chunks for the doodads on them, so the client's shadow maps hold no terrain.
- Window messages. `EventInitialize` is called as `EventInitialize(1, 0)` (`0x406A0B`, `push 1` for the thread count),
  so no scheduler thread exists. `OsInputGet` (`PeekMessageA`/`GetMessageA`/`DispatchMessageA`, `Input.cpp:1237-1254` in
  the wowdecomp reconstruction) and `CGxDeviceD3d::ScenePresent` (`0x6A3450`) both run on the main thread, so
  `OverlayWindowProc` and the `Present` hook never run concurrently. The only other threads the reconstruction creates
  are asynchronous file reads, the model cache, URL downloads, sound and a Steelseries HID thread.
- Pure device. The client requests `D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE`
  (`CGxDeviceD3d.cpp:2609`) and calls only `GetBackBuffer`, `GetCreationParameters`, `GetDepthStencilSurface` and
  `GetRenderTarget` on its device, all valid on a pure device. The wrapper removes the flag because the DLL itself calls
  `GetRenderState`, `GetStreamSource`, `GetViewport`, `GetScissorRect`, `GetTexture`, `GetPixelShader`,
  `GetVertexShader`, `GetVertexDeclaration`, `GetFVF` and `GetIndices` on the real device in nine source files.
- Native glare. `0x4F9213` (`E8 58 76 2F 00`) calls the sun/moon glare pass `0x7F0870`, which is not patched.
  Its draw `0x9AC400` adds the `sunGlare`/`moonGlare` quads (`D3DBLEND_SRCALPHA`, `D3DBLEND_ONE`) at depth
  `[0.9990234375, 1]`, and its update `0x9AC3C0` runs a GPU occlusion query against the current world depth
  (`0x9ABE00`). World geometry `0x7984A0` also runs after the opaque hook (`0x4F9154` calls `0x77F010`, which
  jumps to it). Drawing the fog once at `0x4F9281` sees the completed depth and keeps the glare and its query
  order unchanged.
  With `TransparentFog=1` the liquid thunk calls the glare pass instead of its own call, after checking `0x4F9210`
  (`83 C4 14`), `0x4F9218` (`E8 63 C3 2E 00`, world text) and the pass after its entry, which Extensions.dll detours:
  `0x7F0877 74 3C D9 05 48 8B D3 00 51 B9 A8 8E D3 00`, the moon tail jump `0x7F08AB B9 58 8F D3 00 E9 4B BB 1B 00`
  and `ret` at `0x7F08B5`. The entry `0x7F0870 83 3D CC 8C D3 00 00` is only logged (see Extensions.dll). The call
  at `0x4F9213` itself is patched (see Transparent fog hooks).
- World render order. In `0x4F8EA0` the map render draws the sky, the WDL, terrain and the WMOs with their alpha
  batches, then M2 pass 0 (`0x4F911D`). Above water come M2 pass 2 (`0x4F9167 6A 02`), the liquid (`0x4F9170`),
  weather, the barrier effect and M2 pass 1 (`0x4F918C 6A 01`), under water pass 1, weather, the liquid (`0x4F91B0`)
  and pass 2; then lightning, missile arcs, the glare (`0x4F9213`), world text (`0x4F9218`) and FFX end (`0x4F9281`).
  `0x821A20` puts see-through batches (blend mode above 1 or alpha below 0.99999, `0x821F47`) in pass 1 on the
  camera's side of the model's liquid plane and in pass 2 beyond it. The liquid pass sets the Gx fog, draws the water
  (`0x790AA2`) and tail-jumps to the ripples `0x79D5E0`.
- M2 batch fog. `0x81FB10` picks the fog colour by blend mode through `0xA45390` = {1, 1, 1, 2, 2, 3, 4, 0} (eight dwords; the second half starts at `0xA453A0`)
  (`0x81FB7B 8B 04 8D 90 53 A4 00`) and the jump table `0x81FE7C` (`0x81FBA3 FF 24 85 7C FE 81 00` → `0x81FBAA`,
  `0x81FCAE`, `0x81FCC2`, `0x81FCD8`): mode 1 is the lighting fog colour with alpha 0xFF (`0x81FC5D C6 45 FB FF`),
  modes 2, 3 and 4 black, white and grey 0x80 with alpha 0 (`0x81FB20 33 DB`; `88 5D FB` at `0x81FCB7`, `0x81FCCD`
  and `0x81FCE3`), mode 0 unfogged. From `0x81FCEE` to `0x81FD14` it pushes the exponent (`[eax+0xB4]` of the
  lighting `[esi+0x70]`), the colour's address (`lea ecx, [ebp−4]`), the end (`+0xAC`) and the start (`+0xA8`),
  calls the setter at `0x81FD15` (`E8 F6 34 05 00` → `0x873210`), then `0x873390(1)`, and pops the arguments
  (`0x81FD1A 6A 01 E8 6F 36 05 00 83 C4 14`).
- M2 unfogged batch. The batch is `esi` throughout `0x81FB10` (`0x81FB18 8B F1`; wowdecomp `CM2SceneRender::SetupLighting`);
  seven calls reach it. It reads the material (`0x81FB58 8B 8E 98 00 00 00`), takes mode 0 when its flags have 0x2
  (`0x81FB61 F6 01 02`) or the lighting's fog scale is not above 0, and otherwise the table entry of the blend mode at
  material `+2` (`0x81FB77 0F B7 49 02`). Mode 0 jumps (`0x81FB86 3B C3 89 86 80 00 00 00 0F 84 92 01 00 00`) to
  `0x81FD26 53 DD D8 E8 62 36 05 00 83 C4 04`, the upload `0x873390(0)` (`CShaderEffect::SetFogEnabled`) with `ebx` = 0,
  and `0x81FD2E 83 C4 04` pops its argument.
  The upload (`55 8B EC 83 EC 10 83 7D 08 00 74 5D`) with 1 sets vertex c30 from the setter's stored constants at
  `0xD43068` (`0x8733C6 68 68 30 D4 00 6A 1E 6A 00 FF D2`, written at `0x87327F A3 68 30 D4 00`); with 0 it sets c30 = (0,
  1, 1, 0), fog 1 at every depth. The fog table `0xA45390` has no entry for blend mode 7 (the eighth dword is 0), so the
  DLL gives the unfogged batch a colour only for modes 0 to 6 (lighting, lighting, lighting, black, black, white, grey).
  Bytes read from the 12340 image (timestamp `0x4C2452FE`); the setter is called only from `0x7A84BA`, `0x7A9B35` and
  `0x81FD15`, as LuxVelum's reading of the same image reports, which was not repeated here.
- M2 fog setter. `0x873210` (`55 8B EC 83 EC 14 83 3D 20 30 D4 00 00`) writes the colour's bytes +2, +1 and +0 over
  255 (`0x873225 8B 75 14 0F B6 46 02`, `0x873242 0F B6 4E 01`, `0x873254 0F B6 16`) to pixel c2 and builds vertex
  `c30 = (−1/(end − start), end/(end − start), exponent, 0)` from the start and end (`0x873263 D9 45 0C D9 45 08 D8
  E9`) and the exponent (`0x87328C D9 45 10`). The client's M2 and WMO shaders fog by planar view depth, `oFog =
  min(pow(max(c30.x·z + c30.y, 0), c30.z), 1)`, blending `lerp(c2, colour, fog)`. c2.w is the alpha-test reference,
  so the DLL changes the colour only through the setter's argument; each WMO render resets its fog cache.
- Weather fog. `0x78AE20` draws weather either with its own grey fog and a vertex fade (`0x78A7C7`) or with the Gx
  fog set to 0x80808080 from 70 yd (`0x7893B5`) to 75 yd (`0x7893C6`); no weather draw reads the zone fog.
- Extensions.dll. Ascension's Extensions.dll (TimeDateStamp `0x6ABAD5C2`, read statically) detours the glare pass
  `0x7F0870` (7 bytes; it then draws a second moon), the LightParams getter `0x7EB180` (it multiplies the glow by its
  `ambientGlow` CVar, so the glow can exceed 1), the light blend `0x7F3230` and M2 and DayNight functions. None
  touches `0x4F9170`, `0x81FD15`, `0x873210`, `0x4F9213`, `0x4F9281`, `0x4F8770`, `0xD38C2C` or the FFX code, or
  overlaps a byte run checked before the M2 unfogged batch hook; at `LogLevel=2` the log says what `0x7F0870` begins
  with. The runs of that hook (`0x81FB18`-`0x81FD30`, `0x873390`, `0x8733C6`, `0x87327F`) and its call `0x81FD29` were
  not checked against it; when one differs the log says so and the hook stays off.
- Screen effects. FFX end runs the current effect `[0xD45780]` when the `ffx` CVar (`[0xD45774]`, int at `+0x30`)
  and the effect's own CVar (`+4`) are on. The glow effect `[0xB74364]` keeps `ffxGlow` there (`0x8BFEDB`);
  `0x4F8770` feeds it the DayNight glow (`0xD38C2C`) as the additive weight of `lerp(screen, blur, other) +
  g·blur²`, where `other` is the screen effect's own blend amount, which the fog leaves as is.
  The ghost effect is `[0xB74368]` (`FFXDeath`, vtable `0xA418D8`, stored at `0x7EA274` by its constructor).
- Glow feed. The world frame callback `0x4FAF90` rewrites `0xD38C2C` from the light blend every world frame before
  the world render. There `0x4F8F3D`, the only call of `0x4F8770`, feeds the glow effect while it is current: it
  turns the glow into a byte with the +512.0 trick (`0x4F878E`–`0x4F87AC`, floor(255·g) mod 256, so 1.1 gives 24) and
  passes {underwater, glow byte, blur byte} with id 3 to SetParam (`0x4F883C 8B 40 10 52 6A 03 FF D0`), slot
  `0xA941D8` of the glow vtable `0xA941C8` (stored at `0x8BFE98 C7 03 C8 41 A9 00`). SetParam `0x8BFDE0` (`55 8B EC 8B
  45 0C 8B 10 89 51 2C 8A 50 04 8A 40 08`) stores the D3DCOLOR {B = G = R = blur, A = glow} at `+0x30` of pass [2] in
  the clear-view list at `+0x08` (`0x8BFE14 8B 41 10 8B 48 08`, `0x8BFE21 89 51 30`) and the underwater list at
  `+0x1C` (`0x8BFDF2 8B 71 24 8B 76 08`, `0x8BFE08 89 7E 30`), and returns with `ret 8` (`0x8BFE26 C2 08 00`). A list
  is {capacity, count, array}; pass [2] is the composite (vtable `0xA94294`, stored at `0x8C2206 C7 06 94 42 A9 00`),
  which hands that colour to its draw (`0x8C28A1 83 C6 30`). `0x4F8F3D` precedes both liquid pass calls, so when the
  liquid pass returns the byte holds this frame's value. The ghost composite `0x7E87B0` reads `0xD38C2C` itself. The
  glow override requires these byte runs and `0x4F9286`, and each frame an effect graph with these vtables.
- FFXGlow. The shader loader `0x684970` accepts only BLS version `0x10003`, so Ascension's ps_3_0 `FFXGlow` falls
  back to ps_2_0, `lerp(screen, blur, v0.z) + blur²·v0.w` with the pass colour as `v0`. Every FFX pass end rebinds
  the default target (`0x8C15A7 8B 01 8B 50 5C 6A 00 6A 00 6A 00 FF D2`), so the composite draws to the back buffer.
- After the frame effects. After FFX end the world render only runs `0x747AE0` (`A1 68 13 CA 00`, clearing flag
  `0x1000` along `[0xCA1368]`) and returns (`0x4F9286 E8 55 E8 24 00 5F 5E 8B E5 5D C3`); names and icons follow
  (`0x4FB042 E8 F9 A0 2E 00`). Early returns skip FFX end and the world-done hook, and with them the grading, which
  requires `0x4F9286`, `0x747AE0`, `0x4FB042` and `0x8C15A7`. None of these guarded bytes is one the hooks patch.
- Light params slots. `0x7EB180` returns a light's `LightParams` for a slot (`Light` record `+0x1C + 4·slot`).
  For each light, `0x7EE510` takes slot 0 and, while the storm weight `[0xD38B88]` is above zero, blends in
  slot 2 by it (`0x7EC220`). The DayNight update sets that weight to `min(1, 4·[0xD38B4C])` (`0x7F3995`); the
  weather update writes `[0xD38B4C]` (`0x784A01`). When `[0xD38B58]` holds a slot, the light blend `0x7F3230`
  uses it instead (`0x7F346F`). `0x7ECEC0` stores it from the current `ScreenEffect` row (`+0x1C`, called at
  `0x4F712D`; values above 7 become −1) and `0x7ECEE0` clears it. In CoA's `ScreenEffect.dbc` the ghost effect
  (ID 1) and the other death-style effects use slot 4; a few event effects force slots 0, 1, 2, 3 or 5.
- Current map. `DayNight::s_mapId` `[0xD38AC4]` is stored by `DayNight::LoadMap` `0x7F2790` (`A3 C4 8A D3 00` at
  `0x7F27A1`, the argument of `CMap::LoadMap` `Map.cpp:3040`), the same map the light tables are loaded for, and
  is not cleared on unload. `World::QueryMapId` `0x77F080` returns the map's own copy `[0xADFBC4]`
  (`A1 C4 FB AD 00 C3`), which `Map.cpp:709` resets to −1 on unload. The earlier source `[0xAB63BC]` is the
  loading screen's `s_simpleMapID`: the world-transfer loading screen `0x40AD50` stores its map argument there
  (`0x40AD56`, `A3 BC 63 AB 00`) and `0x40AE0A` and `0x40AECF` set it to −1 for the ship and zeppelin screens, so
  it is not the world's map. Bytes checked in the 12340 image (timestamp `0x4C2452FE`). In-game logs showed map 0
  in Elwynn Forest with the old address; the value after a zeppelin ride is not yet confirmed in game.
- Camera focus. The world render takes the world position it hands to the world update from the object the active
  camera follows: `0x4FAAA0` (`8B 8F 20 7E 00 00 83 79 48 00 75 13 85 DB 74 0F`) takes the object's position when
  `[camera + 0x48]` is 0 and the object exists, else the camera position (wowdecomp `WorldFrame.cpp`:
  `position = (m_camera->m_model || !target) ? camPos : target->GetPosition()`), and calls `0x7831A0` with it
  (`0x4FAADF`). `0x7831A0` hands it to `World::PrepareAreaOfInterest` `0x780860` (`0x783209`), which first stores it as
  `World::s_currentWorldPos` at `0xCD7778`–`0xCD7780` (`0x780871`, `A3 78 77 CD 00 8B 4F 04 89 0D 7C 77 CD 00`;
  `0x78087F`, `8B 57 08 89 15 80 77 CD 00`; wowdecomp `World.cpp`, `// STATIC: 0x00CD7778`). Bytes checked in the 12340
  image (timestamp `0x4C2452FE`). The DLL reads the focus there while both stores match, and the camera position
  otherwise (the log says which). It differs from `DNInfo.m_playerPos` `[0xD38B0C]` on a mount or vehicle the camera
  follows instead of the player; not seen in game.
- Far clip. The clamp `0x780770` is cdecl `float(float farclip, int mapId)`, result in ST0, caller pops; it
  bounds the value to `[0xA3E708]` (183.33) .. `[0xA3E710]` (1583.33). Its calls at `0x780810` (in the
  `farclip` CVar setter `0x780800`, also reached from Extensions.dll on zone changes) and `0x781444` (map load
  `0x781430`) are rare, so the hook re-reads the INI on every call.
- Present. The D3D9 backend presents through the device at Gx `+0x397C` (`0x6A3584`, `0x6A7724`), which is the
  wrapper, so every frame passes through its `Present`.
- Input. `GxWindowClassD3d` and `GxWindowClassD3d9Ex` share the window procedure `0x6A0360`, which hands keyboard
  and mouse messages to the Gx input callback. The message pump's pre-filter `0x86CB00` acts only on the client's
  own dialog windows, and DirectInput only enumerates game controllers (`0x870725`), so keyboard and mouse
  reach the chained window procedure. The client draws its own D3D cursor (`0x6A009C`, `0x6A058E`), so ImGui
  leaves the cursor alone.
- World lights. `0x4F90EC` loads the world M2 scene `[0xCD754C]` and passes it to the opaque M2 pass. The
  point-light registration `0x834C70` allocates a 64-by-64 pointer table at scene `+0x24`, hashes the light's
  world X/Y in 20-yard cells and inserts it into a null-terminated list. A light holds its scene at `+0`, type at
  `+8` (1 = point), world position at `+0xC`, diffuse RGB at `+0x3C`, constant/linear/quadratic attenuation at
  `+0x54`, enabled state at `+0x60`, the address of its preceding link at `+0x64` and the next light at
  `+0x68`; removal `0x834AB0` and the setters `0x835690`/`0x8356F0` maintain the links. Animated M2 positions
  are already transformed to world space before the setter (`0x828AF4`). The diffuse already includes the
  animated intensity and model scale (`0x8305B9`–`0x8305FE`) and is uploaded unchanged (`0x835527`,
  `0x6A462F`). The attenuation is uploaded as `D3DLIGHT9::Attenuation0/1/2` (`0x835539`, `0x6A46AC`) with the
  fixed range 10000 (`[0xA2F95C]`), so the 200-yd cap on a fog light's sphere and the eight-light selection are
  the fog's own policy. No native spotlight cone has been established.
  Registration skips a disabled light (`0x834C79`, `83 7E 60 00`) and the enable setter `0x8356F0` re-registers or
  unlinks it (`0x835716`, `0x835720`–`0x835740`), so the table holds only enabled lights.
  Lights carry a frame stamp at `+4` that the client writes only when the light's model is drawn (`0x828B9C`), and
  the map light query `0x81E400` disables every light with a stale stamp through `0x8356F0`, so a torch that leaves
  the frustum is unlinked from the table the fog walks; the `je` at `0x81E548` is the branch that skips that
  disable call (St0ny's AIO WoW EXE Patcher No. 70 "lightstay" turns its opcode `74` into `EB` at file offset
  `0x41D948`, which its patch table records as holding `74` in the stock exe; with the image base `0x400000` and
  `.text` at RVA `0x1000` for raw offset `0x400`, that file offset is VA `0x81E548`). `LightStay=1` writes the same
  byte in memory at start-up, after `EngineHooks=1` passed the image check, when the opcode is `74` or `EB` and the
  block the `je` skips (rel8 5..64) holds a `call` to `0x8356F0`; the bytes around the branch have not been
  recorded from a disassembly, only this structure, so the guard is looser than the byte guards of the other sites
  and the patcher itself lists No. 70 as untested. Lights whose model is not drawn then keep their last position
  and colour; removal of the model still unregisters them (`0x824D30`).
  The stamp is the M2 scene's frame counter at `+0x14`, which the scene animation `0x821A20` advances first
  (`0x821A2D`, `83 46 14 01`). The world scene render `0x79A870`, reached from the world render at `0x4F909F`
  through `0x77EFF0`, animates the scene at `0x79AC13` before it draws, so by the opaque M2 pass `0x4F911D`, where
  the fog captures its lights, every model queued for drawing this frame has stamped its lights: `AnimateST`
  `0x828A00` writes `[[model+0x28]+0x14]` to each light's `+4` (`0x828B88`, `0x828B9C`, `89 46 6C`), and the map
  light query compares the same two words (`0x81E53F`, `8B 41 04 8B 55 FC 3B 42 14`). Doodads are queued only
  within their fade distance, by size: up to 1, 4, 15 and 100 yd across they fade out over the last 5, 10, 15 and
  20 yd before 30, 100, 200 and 750 yd, the middle three scaled by the environment detail (`0x78F570`,
  `[0xADF3A0]`), and larger ones at 1250 yd.
- Spell particles. `CParticleEmitter2::Render` `0x97EA60` is a thiscall with three stack arguments (frame of
  reference, vertex buffer, first-in-batch) that returns with `ret 0xC` (`0x97EAFF`). It resets the emitter's world
  bounds at `+0x218` (`0x97EA81`), returns early when the live list count `+0x50` is 0 and clears the rendered count
  `+0x1C` (`0x97EABA`); otherwise the vertex build `0x97E580` grows the bounds over the rendered particles, transforms
  them back to world space and stores the rendered count. The M2 renderer calls it from two sites. The single draw
  `0x8214E0` keeps the scene renderer in ESI and its current element `[esi+0x50]` holds the emitter at `+0x18`
  (`0x8214E7`, `8B F1 8B 46 50 57 8B 78 18`); the call is at `0x821633` (`E8 28 D4 15 00`, ECX = EDI), guarded from `0x821623` (`8B 56 60 8B 82 74 01 00 00 6A 01 6A
  00 50 8B CF`, its arguments and ECX). The batched
  draw `0x821100` reuses its third argument slot `[ebp+0x10]` as the cursor into the element index list and indexes
  the element array `[ebp+0xC]` by 0x44 (`0x821411`, `8B 55 10 8B 02 8B 55 0C 8B C8 C1 E1 04 03 C8`) for the emitter
  at `+0x18` (`0x821420`, `8B 74 8A 18`); the call is at `0x82143F` (`E8 1C D6 15 00`, ECX = ESI), guarded from `0x82143C` (`51 8B CE`). An element's model
  is at `+4` (`0x821169`, `8B 4F 04`), a model's scene at `+0x28` (`0x824141`, `8B 56 28`) and its shared data at
  `+0x2C`, whose file name starts at `+0x3C` (`0x8245BE`, `8B 46 2C 53 57 83 C0 3C`). The emitter's blend is at
  `+0xD0` and its material flags at `+0xD4` (`0x8214F0`–`0x8214F9`); the model loader `0x832EA0` maps the file's
  blend 4 (add) to 3 and 3 (no-alpha add) to 10. The colour track pointer is at `+0xD8` with the value count at
  `+8` and the values, RGB floats 0..255, at `[+0xC]` (`0x9795DA`); flag `0x10` at `+0x134` replaces them with the
  three colours at `+0xF4` (`0x979642`, `0x979652`). The alpha track, fixed16 values in the same layout, is at
  `+0xDC`, scaled by `+0x130` (`0x979F01`, `0x979F0F`, `0x9794F7`). `0x82143F` and `0x821633` are the only calls to
  `0x97EA60` in the image (a byte search). The hook replaces both calls with thunks that call the original, then read
  the emitter after its bounds were built, and installs only when every recorded byte above matches. Particles draw
  after the fog's light capture, so a frame's spell lights reach the fog on the next frame.
- Spell meshes and textures. The M2 draw `0x823130` dispatches on the element type through a jump table
  (`0x823A6C`–`0x823A81`); type 0 calls `CM2SceneRender::DrawBatch` `0x8203B0` at `0x823A83` (`E8 28 C9 FF FF`,
  ECX = ESI = the scene renderer), its only call in the image, and the batch draw returns with a bare `ret`
  (`0x820615`). The batch draw stores the element's batch `+0x28` and skin section `+0x2C` at renderer `+0x88` and
  `+0x90` (`0x8203D8`) and its material, the data's material array `[+0x74]` indexed by the batch's `+0xA`, at
  `+0x98` (`0x8203ED`); a material's blend is the word at `+2` (`0x82021F`), 3 no-alpha add and 4 add. The renderer
  holds the model's data at `+0x48`. The batch colour is the model's diffuse `+0x1A0` (`0x8201E9`) times, when the
  batch's colour index `+8` is below the data's colour count `+0x48`, the current value `+8` of the model's colour
  `[+0xA0]` at a stride of 0x20 (`0x82023F`, `0x820251`); the element's alpha is at `+0xC`. The first texture is the
  model's texture `[+0xA4]` at the data's texture combo `[+0x84]` indexed by the batch's `+0x10`, bounded by the
  texture count `+0x50` (`0x81F4E6`–`0x81F508`). A section's bone is the data's bone combo `[+0x7C]` at the
  section's `+0xE` (`0x820486`); the bone matrices `[model+0x98]` map model space to view space and the scene's
  inverse view is at `+0xC4` (`0x828AC2`, the light placement in `AnimateST`), and the section holds its bounds
  centre at `+0x20` and radius at `+0x2C` (the M2 skin layout). A `CTexture` holds its `CGxTex` at `+0x44`
  (`0x4B6D4B`), which holds the Direct3D texture the device binds at `+0x38` (`0x6A492E`); textures are created in
  the managed pool (`0x6A2C00`), and an emitter's texture is at `+0x128` (`0x821563`). The tint locks the smallest
  mip level of 4 to 16 texels a side read-only, once per texture, and only for an object whose interface table
  matches a texture the client created through the wrapper.
- Interiors. `0x795D40` stores the camera's WMO instance at `[0xCD87A4]`, its active group IDs at `[0xCDB0DC]`
  and their count at `[0xCDB0D8]`. The instance's root is at `+0xF4`; the root's loaded flag is `+0x1E0`, its
  group count `+0x1F4` (set at `0x7D7F18`, bounded at `0x7AE467`) and inline group pointers start at `+0x1F8`
  (`m_mapObjGroupArray[512]`, so the group count is accepted up to 512);
  group `+0x198` bit 0 means loaded (`0x7AEA80`). The client's camera-fog query at `0x7A11C7` accepts an interior
  group when its `+0x30` flags have neither bit in `0x48` set. Day/night fog update `0x7F17B5` writes the
  transition weight to `[0xD38B9C]` at `0x7F1931` and from `0x7F1955` blends the outdoor fog toward the WMO fog,
  so the stock fog inputs already hold the blended colour, start and end.
- Capture guards. Before reading the light and interior inputs, the module checks the image timestamp and base
  and the instruction bytes at `0x4F90EC`, `0x834C8D`, `0x834D3C`, `0x835539`, `0x7A11B0`, `0x7A11C7`,
  `0x7AEA83` and `0x7F1931`. Reads are SEH-guarded with pointer, span and count limits (512 nodes per bucket,
  links checked against their owner); no client references survive the capture. A disabled or non-point node is
  skipped and counted, a walk beyond 8192 nodes stops as truncated, and a damaged link, a table that changed during
  the walk or a read fault end the walk; in each case the lights read before it stay usable and the reason is
  logged once. The interior inputs are read after the walk whatever it returned and kept when their reads were
  consistent; a read fault leaves them exterior. An unsupported client or an out-of-range camera gives an empty
  result. `LocalLights=0` skips the point-light walk.
- Light grid. The 4096 buckets at the scene's `+0x24` are the 64 by 64 `CM2Scene::m_lightGrid`: a light's bucket is
  `x + 64·y` with `x`, `y` = `floor(position·0.05) & 63` (`M2Light.cpp:150`, `s_lightGridScale`; the client's own
  `CM2Scene::SelectLights` `0x81E400` visits the cells around a model's sphere with the same cell formula and wraps at
  64). The walk visits every cell in rings around the camera's cell, nearest ring first, so the 8192-node limit drops
  the lights farthest from the camera, not those in the later buckets. It still covers all 4096 cells, because a light's
  reach (up to 200 yd) lights surfaces far from the camera. Candidates are ranked by a priority computed once when the
  candidate is ranked, not at each comparison. `CM2Light::Link` `0x834C70` multiplies both coordinates by `[0xAF59D4]`
  (`0.05`, `CD CC 4C 3D`; `D8 0D D4 59 AF 00`), floors them, masks with 63 (`83 E7 3F C1 E7 06`, `83 E1 3F`) and adds
  `64·y` to `x` (`03 F9`).
- Water pass. `0x77F020` jumps to `0x790A80`, which calls the liquid renderer `0x8A2240` at `0x790AA2`
  (`E8 99 17 11 00`) with ECX = `[0xCD8610]` (`0x790A91`), the camera `0xCD8F5C` and pass 1 (`0x790A9B`, `6A 01`);
  it is a thiscall that returns with `ret 8` (`0x8A2376`). The same `0x77F020` runs from `0x4F9170` (camera above
  liquid) and `0x4F91B0` (below). Liquids are queued into one of two 16-byte buckets by their `LiquidMaterial`
  flag (`0x8A20D6`, settings `+0x360`): pass 1 holds every water and ocean material, pass 0 the magma and slime,
  drawn earlier in the map render (`0x79ACF1`). `0x8A2240` keeps the renderer in EBX (`0x8A224C`), indexes the
  bucket by `pass·16` (`0x8A229C`), loads its count from `+4` (`0x8A229F`), skips its draw loop when that is 0
  (`0x8A22B2`) and empties the bucket every frame (`0x8A2314`–`0x8A2319`). The water ripples `0x79D5E0` run after
  the call and are not part of the pass. Each instance is drawn by its material's vtable slot 2 through `call edx`
  (`0x8A22C7`–`0x8A22EA`), a thiscall with seven stack arguments, the last the `Liquid::CMaterialSettings*`:
  `CMaterialWater::Render` `0x8A5590` (vtable `0xA59544`, slot `0xA5954C`) and `CMaterialWaterNoSpec::Render`
  `0x8A5900` (vtable `0xA59578`, slot `0xA59580`), both `ret 0x1C` (`0x8A58FB`, `0x8A5C6B`). Stock water is
  alpha-blended without depth writes on D3D9 hardware, and the client never uses the stencil: its D3D9 backends set
  no stencil state and its clear (`0x6A74B0`) passes only the target and depth flags.
- Liquid culling (wowdecomp, not checked against the 12340 bytes). `CWorldScene::CullSortTable` `0x79A790` walks the
  64 distance rings of 33.3 yd from the camera outwards and, per ring, runs `CullLiquid` `0x7935A0` before
  `UpdateOcclusion` `0x793760` adds that ring's terrain to the clip buffer. `CullLiquid` queues a chunk's liquid
  (`Liquid::CRenderer::Draw` `0x8A20C0`) unless its box is outside the frustum, under the clip buffer
  (`CWorldOcclusion::QueryBuffer` `0x78FDC0`) or inside one of the hand-placed occlusion volumes
  (`QueryVolumes` `0x7CCE00`). The clip buffer is one horizon height per screen column (384 columns): each map chunk
  adds only its two far edges, nine vertices each, taking the lower end of every edge segment
  (`CMapChunk::ClipBufferUpdate` `0x7CFB10`, `UpdateBufferChunk` `0x78F6A0`), plus WMO occluder segments
  (`CWorldOccluder::ClipBufferUpdate` `0x7CC880`); terrain holes clear their columns. A box is hidden only if its top
  is under the horizon in every column it covers, and never when a corner is nearer than 50 yd or the camera pitch is
  steeper than `|camVec.z| > 0.9`, where the buffer is neither filled nor queried. Trees, doodads and fog hide
  nothing, so water behind a forest, at the far clip or below a ridge the coarse horizon misses still reaches the
  transparent bucket.
- Water classification. The settings bank `0x8A28F0` bounds a liquid type id by `[0xD43B18]` (`0x8A28F8`) and
  keeps one settings object per id in `[0xD43B1C]` (`0x8A2900`, `0x8A2952`), so the draw's settings map back to its
  id. The settings builder `0x8A27C0` copies `LiquidType.Texture[k]` (`+0x3C`, `0x8A280C`) into the settings at
  `+0x80·k` (`0x8A2809`, `0x8A282E`). `LiquidType` rows are looked up as at `0x793DF7`–`0x793E12`; the material id
  is `+0x38` (`0x8A1FE8`) and the sound bank `+0x0C` (0 water, 1 ocean; from the 45-field `LiquidType.dbc` layout).
  A draw is water when its material is 1 and its first texture lies in a `\river\` or `\ocean\` folder, which
  excludes CoA's liquid 181 (orange slime drawn with the water material); liquid 17 (WMO water, interior) is
  indoor, sound bank 1 ocean, liquid 9 (fast water) river, and the rest lake.
- Water contacts. `0x4D77A9` (`mov ecx, [0xC79CE0]; mov [ecx+0x2ED0], eax`) stores the object manager; `[0xC79CE0]`
  is null before login. The visible list starts at `[mgr + 0xAC]` (`0x4D4B44`, `add eax, 0xA8; mov eax, [eax+4]`) and
  steps to `[obj + [mgr + 0xA4] + 4]` (`0x4D4B80`, `add eax, 0xA4; mov eax, [eax]; add eax, ebx; mov ebx, [eax+4]`),
  link offset 0x38 (`0x4D6193 BA 38 00 00 00`, `0x4D6233 89 96 A4 00 00 00`); a set low bit ends it, and each link's
  `[obj + 0x38]` holds the previous link's address, which the walk checks. The GUID is at `+0x30` (`0x4D4B6D 8B 4B 34
  8B 43 30`), the type mask at `[[obj + 8] + 8]`, unit bit 0x8 (`0x4D4DF1`, `test [ecx+8], edx`). A unit's `+0xD8`
  points at its movement block `+0x788` (`0x73F67A`, `lea eax, [esi+0x788]; mov [esi+0xD8], eax`): raw position
  `+0x10` (`0x6E6F13`, `mov edx, [ecx+0x10]`), transport-local while the transport GUID `+0x8` is set (`0x6E6F70`,
  `mov eax, [ecx+0x790]`), movement flags `+0x44`, swimming 0x200000 (`0x730DA2 8B 40 44 A9 00 00 20 00`), collision
  radius and height `+0xC8`/`+0xCC` (`0x6E95AF D9 9E C8 00 00 00`, `0x6E95CE D9 9E CC 00 00 00`). The world entity is
  at `+0xB8` (`0x7438B0 89 86 B8 00 00 00`), its world position at `+0x6C` (`0x7803BD`, `mov [ecx+0x6C], edi`; the
  feet in world space, inferred, used only for passengers). The entity's liquid refresh `0x7A1BC0` sets 0x20 with the
  surface at `+0x80` (read at `0x77F1EA`, `test byte [eax+0x7C], 0x20; jz; fld [eax+0x80]`) and 0x100 when the surface
  lies above the feet in a liquid with `LiquidType` flag bit 0 (read at `0x77F230`, `mov ecx, [eax+0x7C]; …; shr ecx,
  8; and ecx, 1`). The client's ripple emitter `0x71CBA0` emits only while surface − z is below max(1, 2·height)
  (`0x71CC28`, `fld [esi+0x854]; fadd st, st; fld1`), at strength 1 at half that depth to 0.5 at it, and the swim
  update `0x730D10` splashes when the depth crosses 0.4·height (`0x730DED`) while not swimming. The walk calls no
  client code, is SEH-guarded with pointer and range checks, rejects a frame with a broken back link and stops at 4096
  objects.
- Client ripple sprites. `waterRipples` is a console command registered at `0x7813A4` (`68 90 F6 77 00 68 00 E8 A3
  00`); its handler, the only writer, reads its argument into `0xADF7F0` (`0x77F696 68 F0 F7 AD 00 68 9C 28 9E 00`).
  The only reader, `0x79D463` (`83 3D F0 F7 AD 00 00 74 61`), makes no new sprite while it is 0; it is reached only
  through `0x77F434` (`E8 27 E0 01 00`) from the unit emitter `0x71CF16` (`E8 E5 24 06 00`). The update `0x79D5E0`
  expires each sprite (`0x79D648 D9 41 28 A1 58 FB AD 00 D8 1D A4 76 CD 00`, at most 0.7·e s) without reading it.
  Extensions.dll holds neither the address nor the emitter's functions (a byte search).
- Water colours. The DayNight light record at `0xD38BD4` (`0x7F3574`) stores the sky bands 2–7 at `0xD38BE0`–
  `0xD38BF4` (`0x7EC03C`–`0x7EC09C`) and the ocean and river close/far colours at `0xD38C0C`–`0xD38C18`
  (`0x7EC11D`–`0x7EC152`); the client bakes the water colours into its per-frame depth ramps. The water is lit by
  the direct light (band 0, `0xD38BD8`), which `0x7EE756` copies to the world's direct light; band 9
  (`0xD38BF8`) only colours the sun and moon sprites (`0x7F36F6`).
- Texture filtering. Read from the wowdecomp reconstruction of the 12340 client, not yet checked against the
  binary's bytes: the `textureFilteringMode` CVar sets the filter mode and anisotropy (`0x4B61C0`, `0x4B6230`) that
  `TextureCreate` (`0x4B8C80`) gives a texture only when its caller passes `defaultFilter` = 1, as the M2 textures
  do. `CMap::LoadTexture` (`0x7D9990`), which loads the terrain layers (`CMapArea`), the WMO materials and the detail
  doodads, passes 0 with `GxTex_LinearMipLinear`, so those stay trilinear at any Texture Filtering setting. The
  override needs no client address: it acts on the `SetTexture` and `SetSamplerState` calls the game makes through
  the wrapper, and the client never reads a sampler state back or records a state block (`CGxDeviceD3d`).
- Game clock. Read from the `Wow-Base-RDP.exe` image (timestamp `0x4C2452FE`) and the wowdecomp reconstruction. The
  day fraction `0xD38B04` is `DayNight::g_dnInfo.m_dayProgression`; the world frame rewrites it every frame from
  `CGameTime::GameTimeGetDayProgression` (`0x76CFF0`) on `g_clientGameTime` (`0xD37F98`), whose `UpdateTime` (`0x7E2730`)
  loads it at `0x7E273F` (`B9 98 7F D3 00`). The progression is `(now - [clock+0x3C]) * 0.001 * [clock+0x30] +
  [clock+0x44]`, with the elapsed time 0 while `[clock+0x40]` is 0 (`0x76CFF7 80 7E 40 00`, `0x76D007 2B 46 3C`,
  `0x76D024 D8 4E 30 D8 46 44`). `CGameTime::GameTimeSetTime` (`0x76D810`, `55 8B EC 83 EC 20 8B 45 08 53 56 8B D9 83 7B
  24`; `this` in ecx, the time and a tick flag on the stack, `0x76D8FA C2 08 00`) stores the hour at `+4` and the minute
  at `+0`, steps one minute back and ticks, which sets the tick time, the running flag and the base minute. Pinning the
  clock writes the minute, hour, fractional minutes (`+0x34`), running flag and base minute each `Present`; resuming
  calls `GameTimeSetTime` once with the pinned time. The server's `SMSG_GAME_TIME_UPDATE` ticks the clock through
  `GameTimeSync`, so the pin is rewritten every frame. Maps with a time of day override and ghosts in the spirit realm
  ignore the clock (`CGWorldFrame` at `WorldFrame.cpp:1588-1609`).
- Not used. `0xD38B98` is fog group 0's exponent, which Extensions.dll overrides (group 0's end is `0xD38B94`), and
  `0xD38C9C` is a near-constant model lighting direction, not the visible sun, so the fog's light direction
  follows the sprite positions.


## In-game settings

`Ctrl+F7` (`OverlayKey`) shows and hides a Dear ImGui window over the game. On laptops whose F-keys send media keys by
default, hold Fn (the log names the key that arrived). The window edits every setting below except `Enable`,
`EngineHooks`, `Overlay` and `OverlayKey`, and the next frame uses the change.

- **Save** writes the settings changed in the window back to `CoAGraphics.ini`, keeping the comments and every other line.
  Settings not changed in the window keep what the file holds.
- **Revert** reloads the file. A hand edit of the file also replaces unsaved changes.
- **Time of day** freezes the game clock at an hour, or drags it to move the sun, moon, sky and light. It is not saved
  and is off after a restart; the server's own time updates cannot move it while frozen. Maps with a fixed time of
  day ignore it. **Back to live time** returns the clock to the time it would show now had it never been frozen: the
  live time at the first freeze plus the real seconds since, at the clock's minutes per second (`+0x30`, the 1 game
  minute per 60 s that the server sends), and sets it running again. It has not been tried in game; the next server time
  packet corrects any drift. Turning **Freeze time** off instead continues from the hour you set.
- The window shows whether the fog drew in the last frame, or why it did not, and whether the game's multisampling is
  kept, or why it is off (`Multisampling` applies later, see Settings). It can be moved and resized, and keeps its place
  and size for the session.

While the window is open, clicks and the wheel go to it only while the cursor is over it or a drag started on it, and
keys only while one of its text fields is active (Ctrl+click on a slider). The hotkey never reaches the client, and
neither does the character it types while it is held. The hotkey's character stops being swallowed once another key goes
down or the focus leaves (`WM_KILLFOCUS`, `WM_ACTIVATE` and `WM_ACTIVATEAPP` deactivation, also with `Overlay=0`),
so a hotkey released in another window cannot leave the chat box and edit boxes empty. Mouse moves and key-ups always
reach the client, so no game key sticks. The window is drawn in the wrapper's `Present` with the device state
saved and restored, and releases its resources before every `Reset`. An exception in the overlay turns it off for the
session and is logged. `Overlay=0` leaves the game window untouched from the next start.

## Build

Requirements: Visual Studio 2022 (C++ x86), the Windows 10/11 SDK (`fxc.exe`), and CMake 3.20+. The first configure
downloads Dear ImGui v1.92.9b and SMAA (commit `71c806a`, MIT licence) with FetchContent, pinned by SHA-256, into the
build directory.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Outputs in `build/Release`: `version.dll`, `CoAGraphics.dll`, `CoAGraphicsTest.dll` and `vfog_harness.exe`, with
`CoAGraphics.ini`, `fogdata.bin`, `waterdata.bin`, `WindProfile.cdbc` and `GroundEffectDoodadWind.cdbc` copied beside
them. Both DLLs link the same compiled objects
(`coa_core`). `CoAGraphics.dll` is the one to install and exports only `vf_loader_anchor`; `CoAGraphicsTest.dll` adds
the `vf_test_*` entry points (`src/test_exports.cpp`) that the harness calls.

## Test

```powershell
ctest --test-dir build -C Release --output-on-failure
```

This runs the comment, format and evidence checks (`tools/check_comments.py`, `tools/check_format.py`,
`tools/check_evidence.py`), the CPU-only `interior_fog` transition and INI persistence checks, and `vfog_harness`. The
harness creates a real D3D9 device through the wrapper with the client's flags (`0x52`, auto depth D24X8), renders a Z-up
test scene with the client's projection convention, and runs the fog passes through the same entry the hook uses. Its
checks in `tests/` cover:

- the device wrapper and state restoration;
- depth and sky handling;
- Classic light blending and slot selection;
- the march against CPU integrals at every quality, and temporal filtering and upsampling;
- point lights and interiors, and the text and liquid depth overrides;
- fog-data validation, the GPU timer and depth probe;
- the settings window, INI saving and `Reset`.

The water suites check the water data and its loader, the FFT against a double-precision reference, the liquid
classification, the water pass driven through the hook entry points (state restoration, stencil tagging, optics against a
CPU reference, fault recovery) and the water settings. The harness writes `before.png`, `after.png`, `overlay.png` and the
debug views to `build/harness-out`. These checks do not establish in-game appearance or performance.

Further suites check the authored noise and `ClassicPhase`, the transparent fog, glow and grading through the hooks, a 4x
device (`SKIP` without a copy method), and the ripples. The sun-shadow suite checks the light shafts behind a wall and
against the client's shadow maps. The terrain-shadow suite checks the terrain window (its chunks, centre and map changes),
the shadow height sweep against a wall, and the march's visibility from the terrain heights. They establish the offline
behaviour only, not the in-game look.

Scenes for looking at the output:

- `vfog_harness --scene harbour <dir> --data data/fogdata.bin` renders the logged in-game frame at the Stormwind harbour
  (sunset, far clip 791.6 yd) with ideal depth and with the client's depth range. It prints fog opacity and colour at
  probe points next to a CPU integration. `--classic-phase 1` renders it with `ClassicPhase=1`, and `--storm 1` in a full
  storm (noisy layers at their mean).
- `vfog_harness --scene ripples <dir> --water-data data/waterdata.bin` renders standing, running, walking, swimming and
  wading-in units at `WaterRipples=0.5`, each as the shaded frame, its normals and a calm reference.
- `vfog_harness --scene performance` times the fog passes at 1920×1080 at each quality on one street: derived layers with
  no point lights, eight flood lights or eight street lamps, Classic layers at the harbour with and without the lamps
  (skipped if `fogdata.bin` beside `CoAGraphics.dll` does not resolve the harbour), and the shipped `LogLevel=1`. Each case
  runs with light shafts, and three cases with `LightShafts=0`. It prints `quality,case,point_lights,median_ms,p95_ms` of
  GPU time. This is a controlled renderer cost, not a game frame-rate test. The harness has no world shadow maps, so the
  shafts are timed with the screen-space trace alone. On an AMD Radeon 780M at 1080p it added 0.6 ms at `Quality=1`,
  0.8–1.1 ms at 2 and 1.6–1.9 ms at 3, without point lights. It also times the harbour in a full storm and the colour
  grading pass. `--samples 4` creates a 4x device.
- `vfog_harness --scene water-performance --water-data data/waterdata.bin` times the water pass on the basin scene (1280×688
  world viewport) with the shipped settings and with one setting changed at a time, and prints `case,median_ms` of the
  water pass's own GPU time over 90 frames.
- `vfog_harness --scene copy-cost` times one full-screen colour copy (`StretchRect` from the back buffer into a texture, a
  resolve when multisampled) and one multisampled depth refresh, each right after a scene draw, at 1280×800 and
  1920×1080 with 1x, 4x and 8x devices, and prints the median GPU ms of 60 rounds. It loads `d3d9.dll` by name, so a
  DXVK `d3d9.dll` placed beside a copy of `vfog_harness.exe` and `CoAGraphicsTest.dll` times DXVK instead of the system
  runtime.

## Install

Close the client, then copy `version.dll`, `CoAGraphics.dll`, `CoAGraphics.ini`, `fogdata.bin`, `waterdata.bin`,
`WindProfile.cdbc` and `GroundEffectDoodadWind.cdbc` next to `Ascension.exe`. To uninstall, remove `version.dll` and `CoAGraphics.dll`; nothing else in the client is changed. The DLL
writes `CoAGraphics.log` next to itself, and holds it open for writing, so a second client started from the same folder
cannot truncate it. A client that finds it held writes the first free one of `CoAGraphics-2.log` to `CoAGraphics-8.log`
instead, and its first line names that file and the client's process id. A slot held by a running client is left as it
is; a free one is reused and overwritten, so the clients of one folder share at most eight log files. A ninth client
running beside eight others writes no log. Other programs can still read the log while the client runs.

## Steam Deck and Linux (Proton)

Install the files as above, then set the game's Steam launch options to:

```sh
WINEDLLOVERRIDES=version=n,b %command%
```

This lets the native `version.dll` proxy load CoAGraphics before Proton's builtin version library.

A Steam Deck test on 8 October 2026 (Proton with DXVK, AMD VanGogh, 1280x800) loaded the DLL and installed all hooks.
DXVK copied the multisampled depth through `RESZ` into `INTZ`, passing the depth-copy self-test, so the default
`Multisampling=1` worked on this device.

DXVK answers the `RESZ` format query and honours the `RESZ` trigger write only when its reported vendor ID is AMD
(`d3d9_adapter.cpp` `CheckDeviceFormat`, `d3d9_device.cpp` `SetRenderState`). On NVIDIA or Intel under DXVK the depth
copy therefore finds no method (NVAPI cannot reach DXVK's device) and multisampling is hidden; the log line names the
reason. Setting `d3d9.customVendorId = 1002` in `dxvk.conf` makes DXVK report AMD and enables its `RESZ` path, and its
other AMD workarounds with it. This has not been tested in game.

With `Quality=2` and the game's Multisampling set to 2x, the tested scene held 60 fps at 69% GPU utilisation, 914 MHz and
2.8 W. At 8x, median GPU costs were 6.47 ms per frame for fog and 2.81 ms for water; GPU utilisation was 97% at 1,600 MHz
and 8.3 W. These are effect GPU costs, excluding CPU work and the client's own rendering, and observations of one scene
rather than a frame-rate guarantee. Start with 2x on the Deck; if a scene still dips, lower water quality, then fog
quality. No defaults are changed by these notes. Other Linux hardware and Proton/DXVK versions have not been verified by
this test.

## Log

When the fog shaders are created the log says how long it took, for example `fog shaders: 42 pixel shaders created in
18.2 ms` (native Direct3D 9 on the Radeon 780M; the figure under DXVK, which translates each shader at that call, has not
been measured).

At `LogLevel=1` the log holds a frame summary on the first frame, whenever the viewport or far clip changes, and every 60 s
(`LogLevel=2` adds one every 600 frames). Its first line ends with the median GPU time of the fog passes since the
previous summary, for example
`frame 3600: viewport 0,0 1920x1080 ... maxdist 5000, fog gpu 2.31 ms (median of 3542 frames, 0 skipped)`.
The time covers the fog passes alone, god rays included, without the client's own rendering or any CPU work. Compare it
with the frame time to see the fog's share of a GPU-bound frame. `skipped` counts frames whose timing was lost, and
`fog gpu no samples` means none finished in the interval. Without timestamp queries the log says so once and the summaries
omit the time. At `LogLevel=0` no queries are issued. With Classic data the summary adds the resolved glow, three points of
the grading curve and each noisy layer.

Every 60 s the water adds a line such as `water gpu 1.24 ms (median of 3500 frames, 0 skipped), classes lake+ocean, waves
256 (7 tiles)`: its GPU time without the client's own water draws. `water:` lines name each liquid type the first time it is
classified. Idle states (no water in view, water hidden behind the scene, camera under water) are logged once, and
repeated only at `LogLevel=2`. Each settled change from the settings window, an INI reload or Revert is logged as one line,
for example
`settings: WaterFoam 1 -> 1.5, WaterWind 2 -> 4`. The water line goes on with the ripples (`ripples 512 at 0.125 yd, 30 Hz,
up to 3 contacts, 0 steps dropped`, or `ripples idle`/`off`). `water: the client's splash and wake sprites are hidden` or
`shown again` reports the hold.

Every 60 s a `frame copies over N frames, per frame:` line gives the full-screen colour copies per frame at each site
(water begin, surface lights, god-ray source, fog composite, late god rays, grading, image), then the depth refreshes
per frame at water begin, water end and fog. Each refresh is followed in brackets by how many of them came with no depth
write by the client since the previous refresh (a draw with depth writes on or a depth clear counts, whatever the depth
test; the DLL's own passes do not) and how many issued a multisampled depth copy. A multisampled device issues no copy
for a refresh that follows no client depth write, since the INTZ copy still holds the depth (harness check; the flag is
set again by any Reset or depth release). Single-sampled devices count the refreshes too, though they copy nothing.

The depth probe logs raw depth, distance and fog opacity at 25 points on frame 60, then every 60 s up to five times (every
30 s without limit at `LogLevel=2`). Its rows are read back on a later frame. The first reason a fog draw is skipped is
logged as `fog skipped: <reason>`, and a camera under water is logged once as `fog idle`. With `LightShafts=1`,
`terrain shadows: 4096 of 4096 chunks around the camera loaded on map 0` is logged when terrain appears around the camera,
and the same line with 0 chunks when the last of it unloads.

Each device creation and `Reset` logs the adapter, the sample count requested and used, the depth format bound, and the
depth copy method with its self-test result, or why multisampling is off. Guarded client bytes that differ are logged with
the feature they switch off.

## Settings

`CoAGraphics.ini` is re-read within a second while the game runs (except `Enable`, `EngineHooks` and `LightStay`). Its
comments describe every key. In the game, `Ctrl+F7` opens the same settings in a window (see In-game settings).

| Key | Default | Meaning |
|---|---|---|
| `Enable` | 1 | Master switch (restart) |
| `EngineHooks` | 1 | Install the hooks; 0 leaves the client unmodified (restart) |
| `LightStay` | 1 | Keep point lights on while their model is not drawn: exe patch No. 70, applied in memory (restart, hooks) |
| `Overlay` | 1 | In-game settings window; 0 hides it at once, and from the next start leaves the game window alone |
| `OverlayKey` | Ctrl+F7 | Key that shows and hides the window, with optional `Ctrl+`, `Shift+`, `Alt+` |
| `Quality` | 2 | 1 quarter resolution / 16 steps, 2 half / 24, 3 half / 32 |
| `Density`, `Haze`, `GroundFog`, `FarFog` | 1, 1, 0.6, 1 | Density multipliers; with Classic data `Haze`, `FarFog` and `GroundFog` relative to their defaults scale layers 0, 1 and 2 (haze, far wall, ground fog) |
| `NoiseAmount` | 0.15 | World-space density variation, 0..1; 0 disables it |
| `NoiseScale` | 0.025 | Inverse feature size of the variation |
| `NoiseWindSpeed` | 0.5 | Drift of the variation along world +X, in yd/s |
| `ClassicNoise` | 0 | The modern client's authored noise on the Classic layers that carry it (mostly storms); 0 = off |
| `StockFog` | 1 | 1 replaces the stock fog with the distance fog; 0 keeps it |
| `TransparentFog` | 1 | 1 fogs see-through effects at their own distance; 0 at the scene behind them (see Transparent fog) |
| `DataMode` | 1 | 1 Classic layers where available; 0 derived layers everywhere |
| `AzerothDensity`, `OutlandDensity`, `NorthrendDensity` | 1, 0.15, 0.15 | Density per region, on top of `Density` |
| `OutlandClassicFog`, `NorthrendClassicFog` | 1, 1 | 1 the converted client lights there, 0 derived layers |
| `AzerothSkyColor`, `OutlandSkyColor`, `NorthrendSkyColor` | 0, 0, 0 | Share of the client's fog hue in the layers |
| `ColorSpace` | 1 | 1 scatters and blends in linear light with a highlight roll-off; 0 uses gamma |
| `SunScatter`, `Ambient`, `Exposure` | 1, 1, 1 | Light in the fog |
| `MoonScatter` | 0.2 | Moon light in the fog at night, on its own (`SunScatter` is the sun's), Classic and derived |
| `AzerothLocalLights`, `OutlandLocalLights`, `NorthrendLocalLights` | 1, 3, 3 | Local light intensity per region, 0..128; it also lengthens each lamp's reach |
| `ClassicExposure` | 1 | Brightness of the Classic layers (1 = as authored) |
| `ClassicPhase` | 0 | Classic sun and moon scattering: 0 peaks at 1 toward the light; 1 is energy-normalised |
| `LocalLights`, `LocalLightIntensity` | 1, 1 | Scatter up to eight world point lights in view; intensity 0..8 |
| `LocalLightPhase` | 0.3 | Henyey–Greenstein g of every point light in the fog: how strongly its glow peaks toward the viewer, −0.9..0.9 (a calibration) |
| `SpellLights`, `SpellLightIntensity` | 1, 1 | Spell effects light the fog (needs `LocalLights=1`); intensity 0..8 |
| `SurfaceLights` | 1 | The point and spell lights also light surfaces, 0..2 (needs `LocalLights=1`); 0 = off |
| `LightsFollowObjects` | 1 | A light in view whose object the client stopped drawing fades out with it |
| `InteriorAware`, `InteriorDensity` | 1, 0.15 | Fade outdoor layers indoors, keeping this fraction of their density |
| `InteriorFarFog` | 1.0 | Native distance-fog density kept indoors (0..1); lower values clear dungeon haze |
| `LightShafts` | 1 | Terrain, buildings and trees block the sun or moon light in the fog |
| `WorldShadows` | 1 | Use the client's world shadow maps when available (needs `LightShafts`) |
| `GodRays` | 0 | Shafts of light through gaps in trees, buildings and terrain in front of the sun, 0 = off |
| `GlowCompensation` | 1 | Pre-compensate for the client's glow: the fog with `ColorSpace=1`, else only the god rays |
| `ForeverGlow` | 0 | 1 gives the client's glow Forever's per-light amount where Classic lights cover the camera |
| `ColorGrading` | 0 | Strength 0..1 of Forever's per-light colour curve over the 3D view; 0 = off |
| `AnisotropicFiltering` | 16 | Anisotropic filtering of the game's mipmapped textures: 0 = the game's own; 2–16 |
| `Antialiasing` | 0 | SMAA antialiasing of the 3D view |
| `Sharpening` | 0 | Contrast-adaptive sharpening of the 3D view, 0..1; 0 = off |
| `Bloom` | 0 | Halo around the brightest parts of the 3D view, 0..1; 0 = off |
| `TargetArrow` | 1 | Arrow on the target circle's rim in the target's facing; 0 = the plain circle |
| `FarClipMax` | 1583 | Continent view distance up to 1583 yd, within `farclip`; 0 keeps Ascension's 791 cap |
| `MaxDistance` | 5000 | Fog range: sky integration length and the Classic distance-curve scale |
| `Temporal` | 0.85 | History weight; 0 = off |
| `Underwater` | 0 | Keep the effect under water |
| `LiquidDepth` | 1 | Water surfaces write depth so fog uses their distance (always on while modern water is drawn) |
| `Multisampling` | 1 | Keep the game's Multisampling when its depth can be copied; 0 = multisampling off (see below) |
| `DebugView` | 0 | 1 radiance, 2 transmittance, 3 linear depth |
| `SunMarker` | 0 | Red dot where the light direction projects |
| `LogLevel` | 1 | 0 errors, 1 info (frame summary with the fog's GPU time, depth probe; see Log), 2 debug |
| `Water` | 1 | Modern water; 0 = the client's own water |
| `WaterQuality` | 2 | 1 128² waves, sky reflections only; 2 256², scene reflections; 3 256², finer reflections |
| `WaterWaveRate` | 20 | Wave updates per second, 0..120; 0 = every frame. Between updates the surface maps are rebuilt every frame from a blend of the previous and the latest displacement, so the waves show one update late and keep moving. On the basin benchmark an update costs about 2.6 ms and a blended frame about 0.5 ms over flat water, so 30 at 60 frames per second saves about 1.0 ms on average |
| `WaterWaves` | 1 | Wave height 0..2; 0 = flat water |
| `WaterWind` | 2 | Wind driving the waves, 0.5..10 |
| `WaterFoam`, `WaterReflections` | 1, 1 | Crest foam and reflection strength, 0..2 |
| `WaterShoreFoam` | 1 | Shore and shallow-water foam, 0..2 |
| `WaterShoreWaves` | 1 | Waves washing in and out at the waterline, 0..2; 0 = a still waterline |
| `WaterSpecular` | 1 | Sun or moon glint, 0..4 |
| `WaterClarity` | 1 | How far you see into the water, 0.25..4 |
| `WaterZoneColors` | 0.5 | Tint by the zone's own water colours, 0..1 |
| `WaterRipples` | 0.5 | Wakes of units in the water, 0..2; 0 = no unit reads and no ripple simulation |
| `WaterClientSplashes` | 0 | 1 keeps the client's splash and wake sprites; 0 hides them for all units while wakes run |
| `WaterDebugView` | 0 | 1 wave normals, 2 foam, 3 transmittance, 4 reflection, 5 liquid class, 6 ripple height |
| `TreeWind` | 1 | Placed trees sway; the other `TreeWind*` keys shape the wind: `Direction` 35 (degrees), `Speed` 3 (yd/s), `Amplitude` 0.6 and `CrossAmplitude` 0.22 (tilt, degrees), `Wavelength` 26 and `CrossWavelength` 9 (yd), `CrossAngle` 40, `Lean` 0.25, `Variance` 0.5, `Gust` 0.35, `DistanceFade` 0 |
| `TreeDoodadsOnly`, `TreeMatchNames`, `TreeMatchTallThin`, `TreeSkipDead` | 1, 1, 0, 1 | Which models count as trees; every enabled test must pass |
| `TreeMinHeight`, `TreeMinAspect`, `TreeMaxHeight`, `TreeMaxDistance` | 6, 1.8, 200, 0 | Tall-model test (yd, height over width), tallest tree that sways and farthest tree from the camera; 0 = no limit |
| `BushWind` | 1 | Placed bushes, ferns and hedges sway with their own `BushWind*` keys (same names as `TreeWind*`; defaults `Speed` 2, `Amplitude` 0.5, `Wavelength` 9, `CrossAmplitude` 0.25, `CrossWavelength` 5, `Lean` 0.2, `Variance` 0.6, `Gust` 0.4) |
| `BushDoodadsOnly`, `BushSkipDead`, `BushMaxDistance` | 1, 1, 0 | Map-placed models only, skip dead bushes, farthest bush from the camera; 0 = no limit |
| `WindStrength` | 1 | Scale of the whole wind (0..4) that `WindProfile.cdbc` describes; 0 = calm |
| `GrassWind` | 1 | Grass blades sway with the wind; the other `GrassWind*` keys shape the sway: `Amplitude` 0.35 (yd at wind strength 1), `Flutter` 0.4 (yd), `FlutterSpeed` 3 (rad/s), `Anchor` 0.3 (the share of the blade that never moves). Needs 1 here or in `GrassParting` at start to rewrite the shaders |
| `GrassModelBend` | 1 | Each kind of grass, flower and pebble bends from its own root to its own tip, measured from its model; 0 = every doodad bends by its texture row |
| `GrassDensity` | 2 | Plants drawn per plant the client places, near you (1 to 4): the client's own grass drawn again on the GPU, each copy moved sideways in a direction of its plant's own and kept on its ground slope; `GrassDensityRadius` 150 (yd from the camera, the copies thin out plant by plant from half of it to all of it, each plant sinking into the ground), `GrassDensitySpread` 1 (yd), `GrassDensityFade` 32 (yd of approach over which a plant grows out of the ground). Needs the doodad hooks; 1 here, in `GrassWind` and in `GrassParting` at start leaves the shaders alone |
| `GrassParting` | 1 | Grass leans away from your character and the 16 nearest units within 40 yd; `GrassPartingForce` 0.25 (yd), `GrassPartingRadiusScale` 4.5 (times the collision radius), `GrassPartingMinRadius` 1 (yd), `GrassPartingMountedScale` 2 |
| `Snow` | 1 | Ground painted with snow is raised by a layer of snow with ragged edges; `SnowDepth` 0.15 (yd, 0 to 0.6), `SnowMaxSlope` 40 and `SnowSlopeFade` 15 (degrees), `SnowDrift` 0.35 (share of the depth), `SnowEdges` 1 (0 = the client's own snow texture alone), `SnowBreakup` 0.5; `SnowFootprints` 1 (units leave left and right footprints, four tracks for mounts and four-legged beasts), `SnowFootprintSeconds` 30 (fill-in time), `SnowFootprintRim` 0.3 (share of the depth), `SnowFootprintRadius` 28 (yd from the camera), `SnowFootprintFade` 8 (yd over which the prints fade out one by one); selection and spell circles are laid on the snow. Needs 1 at start to rewrite the terrain shaders |

Turning `FarClipMax` on from 0 needs a restart. Other changes, including turning it off, apply at the next `farclip`
change, map load or zone change, where raising it shows a loading screen.

Turning `Multisampling` on from 0 also needs a restart: the game builds its Video options' Multisampling list once per
start (see Engine inputs), and with 0 that list holds only 1x. Turning it off applies the next time the game resets or
recreates its display, for example after changing Multisampling or the resolution.

## Status and limits

This is an atmospheric approximation, not a reproduction of WoW Forever's complete lighting renderer.
[Blizzard's official overview](https://news.blizzard.com/en-gb/article/24303862/world-of-warcraft-forever-whats-next-panel-recap)
describes mist over water and moonlight through trees. Matching those scenes needs matched camera, time, weather and
exposure captures. Surface lighting and bloom remain the client's own. The modern client's glow amounts and colour
grading can be applied with `ForeverGlow` and `ColorGrading`, which stay off until they are compared with Forever in the
game, so by default colours still differ from Classic.

**Testing**

- The fog has been tested in the client with native D3D9 on an RTX 2060 laptop. A first modern-water build ran there on
  Elwynn lakes and the Darkshore coast. The review fixes since (direct-light shading, narrower shore foam, fogged
  reflections, the dry-frame skip) are checked by the harness only. Changes to lighting and occlusion need an owner
  test. The offline harness does not establish in-game appearance or performance.
- Other GPU vendors and integrated GPUs are untested, except for the Proton/DXVK Steam Deck result above. Other Wine
  configurations are untested too.
- Multisampling is checked by the harness on the RTX 2060 (NVAPI), and was tested in game on the Steam Deck (`RESZ` through
  DXVK, see above). Alpha-tested foliage stays jagged.
- The owner reported on 2026-10-15 that the flicker was gone in game with the stabiliser, the tolerant table walk and
  `LightStay` together. No `CoAGraphics.log` from that test has been read, so which of the three ended it is not separated.
  The view-only selection without a camera-distance bound and the edge margin are checked by the harness only. The exe
  patch that `LightStay` mirrors is listed as untested by its patcher.

**Rendering limits**

- The composite and the lit march need more than the 512 instruction slots guaranteed by
  [baseline pixel shader 3.0](https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/dx9-graphics-reference-asm-ps-3-0),
  and lower quality does not reduce that. An unsupported required shader disables volumetric fog for that device and logs
  its name, HRESULT and the shader caps. Native fog remains the fallback.
- The silhouette split sees one copied depth sample per pixel, so sub-pixel geometry it misses takes the far fog.
- The screen-space trace cannot see blockers outside the view. Its conservative treatment of depth silhouettes can darken
  fog where geometry hidden behind them would let light through.
- Interior treatment follows the camera's transition weight, not rooms or portals along each ray, so views through
  doorways may differ.
- Transparent materials without depth writes, such as windows, are fogged at the depth behind them, and so are
  see-through effects with `TransparentFog=0`. With `TransparentFog=1`, the default since one tester's game test of
  upstream's build (see Transparent fog), M2 effects above water, those the client draws unfogged included, take one
  line fitted to the fog of the first 100 yd, or of a longer range where the fog in those is thin. That line cannot
  follow height fog and fully fogs effects past its end (500–900 yd in tests), so far glows vanish.
- The `gxApi d3d9ex` path is not wrapped, so fog and the settings window stay off there. The settings window also needs a
  fog device, so it is missing when INTZ depth is unsupported.

**Terrain, buildings and lights**

- Terrain blocks the visible sun or moon from the loaded heights, in view or not, out to the window's edge 800 to 1333 yd
  from the camera. This is checked by the harness only. Buildings, trees and characters outside the view block it only
  through the client's world shadow maps. Those maps are cast from 52–56° elevation whatever the sun's or moon's, so under
  a low sun or moon they shade far less fog than they should. With the client's shadows off they do not block it at all.
  Native shadow-map alignment in the game is unverified.
- Local lighting captures the current M2 scene's point-light table. It does not create spotlight cones or local shadows,
  and coverage of WMO-only lights is unverified.
- A light's sphere ends where its falloff leaves 1/64 of its peak (1/256 before; in the harness benchmark, eight 130 yd
  lights shrink to 65 yd and the frame changes by at most 1 level of 255). The sphere is capped at 200 yd, and the
  attenuation denominator is floored at 1. A light is a candidate at any camera distance while its sphere meets the view
  and the client keeps it in its light table (see World lights). A far light in view costs each march ray its chord test,
  and the scattering is evaluated only where the ray crosses its sphere.
- A march step's light scattering takes the density its layers were sampled at, rather than sampling the layers again at
  each quadrature point. Together with the sphere limits, this cuts the lit march's cost with eight large lights by about
  a third at Quality 2 (harness, Radeon 780M at 1080p). The in-game gain is not yet measured.
- With `LightsFollowObjects=1`, the capture skips a light whose stamp lags the scene's by more than two frames while its
  centre lies at least 2 yd inside the view. Its model is then beyond its fade distance, occluded or otherwise not drawn.
  The stabiliser fades it out over its usual second and back in when the model is drawn again, so the light arrives with
  its object rather than before it. A light whose centre is outside the view keeps shining whether its model is drawn or
  not, as `LightStay` intends. So a lamp hidden behind a wall, or inside a building, while its centre is in view now goes
  dark with its model. A far light no longer takes a slot or keeps the lit march running on its own. The stamp bytes are
  checked separately; when they differ, every light shines as before. The skip is checked by the harness on a fake light
  table. Its effect in game is not yet verified.
- `LightStay` is the only setting that changes an instruction of the client's own code. With `LightStay=1`, a source that
  moves while its model is not drawn (a carried lantern) keeps lighting its last drawn spot, in the fog and in the client's
  own model lighting, until it is drawn again. The client's own choice of four lights per model is unchanged. The hooks
  retarget call sites and vtable slots to the DLL.
- The zone lights' edge fade distance is chosen here: their `TransitionType` is 0 in every row, and the modern client's
  transition rule is not known.

**Spell lights**

- Spell lights come from the additive particles and additive mesh batches of models under `Spells\`, one light per drawn
  model at the centre of its glows' bounds. A particle glow's colour is the mean of the emitter's colour track, weighted by
  its mean alpha and rendered particle count (full at 8 particles). A mesh batch's colour is its batch colour at the
  element's alpha, around its skin section's bounding sphere.
- Each colour is multiplied by its first texture's mean colour, scaled to a peak of 1 (weighted by texel alpha for the
  alpha-added blend). So a white track on a blue texture lights the fog blue.
- The sphere reaches 3 yd plus three times the glows' half-diagonal, at most 20 yd. Textures in formats other than
  DXT1/3/5 and 16- and 32-bit RGB keep the track colour.
- Alpha-blended glows, ribbons, model particles and spell effects stored outside `Spells\` do not glow.
- Spell lights fade in and out over 0.15 s instead of a second, and compete for the eight slots with the world's lights by
  the same priority. So a dim spell far away loses to a nearby torch.
- The reading, the tint and the thunks are checked by the harness on fake client memory and a harness texture. The mesh
  batch glows and tints are not yet seen in game.

**Water and ripples**

- Modern water: water seen from below keeps the client's look. Rivers have no flow direction, since the client has no
  flow maps, so Forever's river foam is omitted. The surface stays flat, and only its shading moves. Reflections are
  screen-space, so anything off screen falls back to the sky colours. Interiors are detected by liquid type only.
- Shore waves (`WaterShoreWaves`) are an addition of this project with no Forever counterpart. They are checked by the harness
  only. Their periods, drawdowns and wavelengths per class, the waterline fade and the wet-shore shade are chosen by eye.
  They only draw the waterline back from the client's mesh. The shore distance is estimated from the water's depth (four
  yards per yard), so on steep banks the shore waves are narrow. The shore and shallow-water foam bands are measured by
  that depth (the shallow-water band at 1.5 yards of path per yard of depth), not by the view depth. So they no longer
  change with the camera's angle. Rivers take the lake's shore-foam strength, which Forever leaves at 0 for them.
- Ripples are checked by the harness only. Walkers (2.5 yd/s) push a mound rather than a V, and every unit is stamped
  round. The ripple step, texel, window, footprint, fades, splash and contact limits are choices, not recovered values.
- The sprite hold is global. Units beyond the 32 nearest, beyond the 24 yd window, or in modern water get no wake at all.
- The client's footstep spray (`0x723A50`) is not hooked, and game objects such as boats make no ripples.

**Fog and data**

- The distance fog (`FarFog`, maps without Classic data) has no modern counterpart. It stands in for the stock fog up to
  the 3.3.5 far clip, which is far shorter than the modern client's.
- `FarClipMax` raises memory use (about four times the loaded terrain in a 32-bit process). Ascension's reason for the
  continent cap is unknown. Without the key in the INI, it stays off.
- The noise column mapping, the noise volume and `ClassicPhase`'s normalisation are inferred, so `ClassicPhase` stays 0.
- `ForeverGlow` sets only the amount. The client's glow adds `g·blur²`, while Forever's adds a linear term.

## Credits

- **jealous-sound** wrote CoAVolFog, which this project builds on: the volumetric fog and light shafts, the Classic fog
  data, the modern water, the settings window and the original release build
  ([jealous-sound/coa-vfog](https://github.com/jealous-sound/coa-vfog)).
- **WarcraftXL** ([WarcraftXL/core](https://github.com/WarcraftXL/core)) and the authors of
  [wxl-treewind](https://github.com/manatactical/wxl-treewind) and
  [wxl-grasswind](https://github.com/WarcraftXL/wxl-grasswind), whose GPLv3 tree, bush and grass wind the Tree and bush
  wind and Grass wind and parting sections port.
- **Morfium-G** ([seyris-living-azeroth](https://github.com/Morfium-G/seyris-living-azeroth)), whose GPLv3 Living Azeroth
  module the wind model, the `WindProfile.cdbc` table, the wind grid, the parting around units, the grass motion
  block and the snow rules come from.
- **Luca Foscili** added the interior distance-fog control and the Steam Deck and Proton notes upstream
  (pull requests #9 and #10).
- **St0ny** and the AIO WoW EXE Patcher, whose patch No. 70 "lightstay" byte the `LightStay` setting reproduces.
- **The wowdecomp project** ([SatyPardus/wowdecomp](https://github.com/SatyPardus/wowdecomp)), by Saty and its
  contributors: a C++ decompilation of the 3.3.5a (build 12340) client, which the texture-filtering notes are read
  from.
- **Claude**, the AI model by Anthropic, which wrote the code, shaders, harness checks and documentation of this
  fork's additions and fixes.
- **Third-party code**: Dear ImGui (the settings window) and SMAA (Jimenez et al., the antialiasing), fetched at build
  time as described under Build; the sharpening follows AMD FidelityFX CAS.

## License

CoAGraphics is licensed under the GNU General Public License version 3 only (`GPL-3.0-only`); see [LICENSE](LICENSE).

As an additional permission under GPLv3 section 7, you may link or combine CoAGraphics, including modified versions,
with the World of Warcraft client, and distribute the resulting combination without providing the client's source
code. GPLv3 continues to apply to CoAGraphics.
