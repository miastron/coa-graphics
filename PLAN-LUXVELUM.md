# PLAN-LUXVELUM

Status (2026-11-05): everything the owner approved is done and committed on main, group 1 to 3, the small items and
7337c37 included ("Do it all", after the stabiliser, fog data and mask ports). Nothing here has been seen in game, and
the full DXVK run gives 1667 passes and only the known 4x multisampling failures (and the intermittent 4x silhouette
check, which also fails on commits before these). The table of what was ported follows; the open items are at the end.

## Source and rules

Source: `..\coa-graphics-similar projects\LuxVelum`, branch `origin/st0ny-dev` (75 commits newer than `st0ny-main`;
its working tree is mid-merge, so read it with `git -C <path> show <hash>`, never edit it). LuxVelum is the same upstream
(CoAVolFog) renamed; `40da028` already took everything up to its `e09a654` (Outland and Northrend fog). Three
cheaper-model agents triaged the commits after that against our code; the verdicts below come from that reading and
from the ports themselves.

- One commit per LuxVelum commit (a few closely tied pairs are one), each with a harness check that fails without the
  change, the README and `ctest -R "comments|format|evidence"` kept green. The message ends with "Ported from LuxVelum
  st0ny-dev <hash>".
- Harness checks only; nothing here has been seen in game unless it says so.
- Run the harness under DXVK (see the baseline in `PLAN.md` Phase 5 and the memory notes): the native Radeon 780M
  driver hangs in the forced-RESZ multisampling checks, and the device-creation checks here force RESZ too. Baseline
  of the full DXVK run after group 1: 1617 passes and 3 failures, all 4x multisampling checks.
- `--filter` matches the call text of `RUN_GROUP(...)`; a broad substring pulls in unrelated groups, which then run out
  of order and can flake.
- Verify client addresses against the 12340 image and wowdecomp before relying on them, and name them in `README.md`
  (the `evidence` check enforces it).

## Done (committed on main 2026-11-05)

- `1955fe2` c53a82d: INTZ and D24S8 checks use the display format of a fullscreen back buffer (DXVK refuses
  A8R8G8B8 as an adapter format). Scripted `IDirect3D9` in `tests/device_creation_checks.h`.
- `b5359d0` afecd62: the game keeps its automatic depth when neither the INTZ nor the fallback depth can be bound
  (`CreateDevice` and `Reset` create the device again with the game's parameters).
- `34e305d` eee51de: water state block records 100 pixel constants (ripple step sets up to c96).
- `bc02439` 6d1fc68: new foam maps are cleared once per tile, also when a frame returned before the simulation.
- `cd9509a` b083c73 and c3fabf8: late god rays keep the fog history when they fail; a failed ray-source copy draws no
  rays.
- `a204259` 59b506f: unfogged M2 batches (lamp glows) take the fitted fog after the early composite. Hook at
  `0x81FD29`; bytes read from the 12340 image, semantics from wowdecomp. Deliberately differs from LuxVelum: blend
  mode 7 stays unfogged, because the client's fog table `0xA45390` has no entry for it (eighth dword is 0).
- `a51d633` e9aa633 (retried device logs its real flags), `25b8b23` 75f8b39 (NVAPI by full system path, no
  KB2533623 needed), `a5cefd8` 5f6de6a (FFT tile sizes must be whole yards), `6a44e54` 571a915 (overlay key held
  across a focus change), `2728a52` b208ef1 (grading copy released after failed copies).
- `300df0a` a52763b grading half, plus `79e8fa9` the same restore in a termination handler for our own
  `ImageRenderer`. The fog half of a52763b was already ours (`de8fa4a`).
- `cfa860f` 8eabc17 and 4c91000: the log is opened denying other writers; a second client takes
  `CoAGraphics-2.log` to `-8.log`.

## Held back, done later

Done (committed on main 2026-11-05, `fef93f9`), 7337c37, the noisy march for silhouettes in the lit composites. The
lit noisy composite and its split twin are LuxVelum's (SAMPLES_AUTHORED_NOISE=1, VARIES_DENSITY=0 to fit the 32
temporaries). LightShafts is on by default here, so the lit composite a storm draws is usually the shadowed one: it got
a shadowed noisy variant and split twin of its own (shadow trace at every step, no stop past the layers, as the
shadowed lit noisy march does; the quiet variants' stride overflows the temporaries). The composite constants now carry
the real noise for every lit family (`WithMeanNoise` there is gone). Evidence: thin silhouettes with a noise of 0.40
miss the noisy fog by 188/255 without the noise and by 0.53/255 with it; the shadowed noisy composite is checked only
as the shader the renderer draws.

## Group 2 (fixes that need an in-game look)

Done (committed on main 2026-11-05), the light stabiliser set, in LuxVelum's order. Our stabiliser was LuxVelum's
parent plus our spell-light fade (`FadeSeconds`), so the source and test patches applied as they were; the README text
was adapted to our bullets. Each check fails on the commit before it. The tests are CPU-only and run natively:
`vfog_harness --scene cpu --filter local_light_stabiliser`. Still to be seen in game: lamps that flicker in and out of
the client's table, a fast spell light, a lamp-lit street while turning the camera.
- `5c0518d` 34f620e: an eviction no challenger waits for any more is called off, so the lamp ramps back up.
- `203b45d` 7d6b0ba: the identity radius is 2 yd plus 40 yd per second since the last capture, and a late capture
  continues the nearest entry (the check for it fails if the first entry in range is taken).
- `10a95b4` fe461d6: a returning challenger adopts an orphaned eviction; calling off needs three captures in a row.
- `73ad963` 720f895: the radius and the priority smoothing run on the time since the last capture that was not held.
- `693c58b` dc49536: a slotted light that is not evicted keeps its slot through two missed captures.

Done (committed on main 2026-11-05), the Outland and Northrend fog batch, code and converter. The source patches did
not apply as they were here (our shader constants live in `vf_local_light_data.hlsli`, the settings table feeds the
panel's slider ranges, `LocalLightUpload` also carries spell lights), so they were ported by hand, patch by patch.
- `1cb03c8` bd506b5: lamps scatter with a gain of one over the region's density (march and fitted stock fog), and
  MoonScatter as a share of SunScatter (replaced by the next commit). Converter: heights shifted by the light's height
  above its template in 25 yd steps, no Classic noise on converted lights.
- `a20113d` 505c4d7: the loader reads layer flags 0x10 (sky without sun and moon: no sun or moon scatter, no god rays)
  and 0x20 (clear sky: no distance fog, scene layers fall off on sky pixels); MoonScatter is the moon's own scatter;
  AzerothLocalLights, OutlandLocalLights, NorthrendLocalLights (1, 3, 3). The harness checks cover the moon and the
  lamp gain by region.
- `071c630` 8fa1168, converter only: a sun-keeping copy of Northrend's background light inside generated zone
  outlines of Borean Tundra with Sholazar Basin and of Grizzly Hills.
- `ae25b2f` 0615f93, the lamp half: a lamp's cutoff radius follows its colour times the region's lamp intensity
  (spell lights share the bound), region values to 128 in the INI, logarithmic sliders to 96, `LocalLightUpload` takes
  the map. Its map read was not taken (ours is `DayNight::s_mapId`).

Done (merged into main 2026-11-05 on the owner's decision, merge `b2e5977`, data commit `4d859e3`): the data itself.
`data/fogdata.bin` is LuxVelum's finished file, taken as built, because it cannot be regenerated here: the client's
DBFilesClient tables (inside the MPQs, no extractor on this machine) and the Forever kit are not available, and
`40da028` took upstream's previous file the same way. Evidence at the time: our converter is identical to LuxVelum's at
its branch tip and no later commit changed the file; the 625 Classic lights, 508 Classic params with their keys and
layers, 18 zone lights and the grading curves are identical to the previous file record by record (only the generated
part differs, plus the copy light and two zone outlines); the eight data checks (ground fog height, no noise, sunless
skies, landmarks, sun-keeping copy, Hellfire's clear sky) failed with the previous file and pass with this one; the
full DXVK run gives 1644 passes and only the known 4x multisampling failures. The README says the converted counts and
shares are upstream's and were not re-measured here. To revisit: extract Light, LightIntBand, LightFloatBand,
LightParams and LightSkybox from the client's MPQs (a small MPQ reader in Python would do) and run
`tools/convert_client_lights.py` on the shipped file; that gives data for the owner's own client, which may differ
from upstream's. To undo: revert `b2e5977` (the previous `data/fogdata.bin` is in `ae25b2f`).

Left out of the batch: the halo occlusion part of 8fa1168 (removed upstream in 51a748b) and the Bone Wastes renames
in the tests; the dither and the region lamp defaults came later (see Group 3).

Done (committed on main 2026-11-05), the mask half of 51a748b: the god-ray mask averages 16 depth taps per texel and
reads four texels of a ray source copied at twice the ray size, each through the brightness threshold and weighted by
its own sky share. Shader and test patches applied as they were; `m_raySource` became twice the ray size. Three checks
in `tests/temporal_quality_checks.h` fail with the old shader. Not timed (four times the copy pixels, 16 depth reads);
still to be seen in game: rays steady while the camera turns past branches. It is also what 4358ace needs.

## Group 3, done on the owner's "do it all"

Each is a look or default change taken from upstream's testers; none was played here.
- `0fc3d0b` 8550566, `5f00160` 4358ace, `84801fb` e7bda6b: God rays keep only the shafts. A pass after the blur reads
  the sky share (kept in the mask's alpha and carried by the blur) along an arc around the sun and keeps what the
  texel's share holds above the arc's larger minimum, so an open sky, a dusk sky and a horizon edge add nothing. The
  own character is excluded: the mask writes a second share with an upright cylinder around the camera's focus counted
  as open, and both composites draw no rays on its pixels. The focus is `0xCD7778`, `World::s_currentWorldPos`, the
  position the world update gets (the followed unit's, else the camera's); bytes and callers checked in the 12340
  image and wowdecomp and recorded in `README.md`. It differs from `DNInfo.m_playerPos` (our `ReadActivePlayerPosition`)
  on a vehicle. Evidence: the open sky changed by 40.5/255 without the shaft pass and by 0/255 with it, the character
  by up to 114/255 without the exclusion and by 0/255 with it. The sun's projection moved into `light_on_screen` with
  its check. A second ray target needs two render targets; without them the character casts shafts and the log says so.
- `40491b6` 5201796 `ClassicNoise=0`; `82bbbf5` 65005ee `TransparentFog=1` (upstream's tester played Duskwood,
  Darkshore and Orgrimmar; the hooks were already installed whatever the setting); `14e54d8` 8fa1168 Outland and
  Northrend lamps at 8 (3 before), keeping our range to 128 and sliders to 96.
- `ad755ef` 8fa1168, the dither part: the fixed-function blend and the grade add a static noise of half a code
  (`vf_pixel_noise.hlsli`); our scene-copy composites keep their rounding of the added difference, which leaves
  untouched pixels exact. The display dither checks are upstream's, the grading checks compare stripe means (0.58 codes
  off without the dither, 0.13 with it) and allow 0.05 codes for the curve texture's filtering.

## Small and optional, done

- `36f22eb` af774f3 (the unused `LightStayOutcomeName`).
- `43a6639` 089cfc7 (GlowCompensation compensates the fog only with `ColorSpace=1`; our shader and `m_drawnGlowCompensation`
  confirm it), 1ac0630 (water hooks only after the engine hooks), bc33d06 (the fit's range beyond 100 yd, constants
  checked), 5bab1b1 (names drawn in the world are graded, bloomed and antialiased; our evidence already had 0x4F9218 and
  0x4FB042, and a stand-in world name pins it), bbddbc7 (loader slot, `0x2` flag, Extensions.dll note, the INI's
  TransparentFog text, which also gained the lamp-glow sentence of 59b506f).

## Open

Nothing from the approved list. To look at in game: rays steady while turning past branches (51a748b), the shafts and the
own character (8550566 to e7bda6b, also on a mount or vehicle), the three new defaults, storms with lamps and noise, and
the lamp-lit streets and spell lights of the stabiliser set. Not timed: the mask's 16 depth taps, the shaft pass, the second
ray targets, the larger composites (about 1400 to 1700 slots).

## Not taken, with the reason

The renames, the release workflow and the data and log folder moves (84b27ff, de62bce, 214b485, 9823b73 and similar),
the INI takeover (5271c81, db47c53, ea0d3d0), 8092ab3 and 080d8d6 (a loader that can load the DLL late; our proxy
imports it statically), ae9b6ef (a test for a shader family we do not have), halo occlusion (removed upstream in
51a748b), and 0615f93's map-id read (ours, `DayNight::s_mapId`, is better).
