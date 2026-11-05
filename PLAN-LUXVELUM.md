# PLAN-LUXVELUM

Status (2026-11-05): in progress. Group 1 is done except 7337c37; the light stabiliser set is next. Update this file
with every port, in the same commit.

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
- Run the harness under DXVK (see `PLAN.md` 3.1 and the memory notes): the native Radeon 780M driver hangs in the
  forced-RESZ multisampling checks, and the device-creation checks here force RESZ too. Baseline of the full DXVK run
  after group 1: 1617 passes and 3 failures, all 4x multisampling checks.
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

## Held back, revisit when wanted

- 7337c37, noisy march for silhouettes in the lit composite. Our composite still marches silhouettes with the noise
  mean (`vf_composite.hlsli` around line 204, `WithMeanNoise` at `renderer.cpp` around line 1849), which shows as
  specks under noise and lamps. The fix adds noisy-march variants of the lit composite. Risks: our extra shadowed
  (`tracesSunShadows`) families multiply the shader permutations, the ps_3_0 limit of 32 temporaries is what stopped
  other ideas (`PLAN.md` 3.2), and it changes the look at silhouettes. Do it with a before and after harness image,
  and check that every variant compiles with `fxc` first.

## Next, group 2 (fixes that need an in-game look)

- Light stabiliser set, in this order: 34f620e (call off an eviction without a challenger), 7d6b0ba (identity radius
  grows with the time between captures), fe461d6 (a returning challenger adopts an orphaned eviction), 720f895
  (identity radius across held captures), dc49536 (a fresh slot survives a missed capture). Our stabiliser is
  LuxVelum's parent plus our spell-light fade (`FadeSeconds`).
- Outland and Northrend fog data, one batch, `fogdata.bin` regenerated once at the end: bd506b5 (converted layers
  shifted by light height, no Classic noise on converted lights, lamp scatter gain 1 / region density), 505c4d7
  (sunless-sky flag 0x10 and clear-sky flag 0x20 from LightParams and LightSkybox, own moon scatter, region lamp
  settings), the converter part of 8fa1168 (Grizzly, Sholazar and Borean keep the sun), 0615f93 (lamp reach times the
  region lamp setting, log-scale sliders). 505c4d7's sunless flag also zeroes `lightVisibility`; check how that meets
  `d7bb50f` (shafts along a low sun).
- 51a748b mask half: the god-ray mask takes 16 depth taps from a ray source copied at twice the ray size (ours is the
  one-tap mask and `m_raySource` is ray-sized). It is also what 4358ace needs.

## Group 3, the owner's call because the look or a default changes

- 8550566, 4358ace, e7bda6b: god rays reduced to the shafts (arc-minimum pass, sky share in alpha, the own character
  kept out). Changes what the God rays slider shows. e7bda6b reads camera focus `0xCD7778`; ours could use
  `ReadActivePlayerPosition`, which differs on vehicles.
- Defaults: 65005ee `TransparentFog=1` (only after 59b506f, now in), 5201796 `ClassicNoise=0`, region lamp default 8
  (8fa1168) and slider ranges.
- Dither for the fixed-function blend and the grade pass (8fa1168); ours dithers the scene-copy composite and the
  ray composite only.

## Small and optional

af774f3 (remove the unused `LightStayOutcomeName`), 089cfc7 (GlowCompensation wording, but check first that our gamma
over a scene copy really compensates), and the documentation fixes 1ac0630 (water hooks install after the fog hooks),
bc33d06 (transparent fog fit range beyond 100 yd), 5bab1b1 (world names are colour-graded; the same "before names"
wording for SMAA and bloom in `CoAGraphics.ini`, the settings panel and README needs the same fix), and the INI wording
of bbddbc7 (`TransparentFog`).

## Not taken, with the reason

The renames, the release workflow and the data and log folder moves (84b27ff, de62bce, 214b485, 9823b73 and similar),
the INI takeover (5271c81, db47c53, ea0d3d0), 8092ab3 and 080d8d6 (a loader that can load the DLL late; our proxy
imports it statically), ae9b6ef (a test for a shader family we do not have), halo occlusion (removed upstream in
51a748b), and 0615f93's map-id read (ours, `DayNight::s_mapId`, is better).
