# PLAN-TREEWIND

Status (2026-10-24): tried, not shipped, reverted. Nothing here ran successfully in the game. Everything below was
built and passed the harness; the one in-game run faulted (see "What happened").

## Goal

Trees sway as one rigid piece today: `TreeWind::Sway` tilts the instance's placement matrix about the model origin.
The owner wanted the trunk to stay still at the base and the crown to bend, without drastic motion, and without
rules based on trunk size (the game's trees follow none). Near trees would bend; far trees would keep the cheap rigid
tilt.

A placement matrix is affine, so it can only tilt, shear or scale; a curve needs per-bone or per-vertex movement.

## What the client does (wowdecomp, plus the 12340 image where noted)

- `CM2Model::AnimateMT` (`0x0082F0F0`) builds `m_boneMatrices` (`+0x98`) in view space. The root bone's parent is
  `m_matrixF4` (`+0xF4`, `m_worldTransform * *view`; `view` is the first stack argument). Every other bone is
  `local * parentMatrix` and parents come before children. `AnimateMTSimple` (`0x0082E140`) has one bone, so there is
  nothing to bend there.
- Both functions return early when `m_frameStamp (+0x3C) == m_scene (+0x28) -> m_frameStamp (+0x14)`, or when
  `m_fFlags (+0x10)` lacks bit 1. A post-processing step must skip those calls or it bends the same matrices twice.
- Bones are `M2CompBone`, `0x58` bytes: parent index (`u16`) at `+0x08`, pivot (3 floats, z last) at `+0x4C`. They sit
  in the `M2Array` at `M2Data+0x2C` (`CM2Shared+0x150` is the `M2Data` pointer, the bounding box is at `+0x154`, the
  file size at `+0x16C`).
- **`M2Array` differs from the decomp.** The decomp keeps a signed offset from the array's own address. The 12340
  client keeps an absolute pointer: `M2Array::operator[]` at `0x0081CD50` is `8B 45 08 8D 04 40 C1 E0 04 03 41 04`
  (`index * 48 + [this+4]`). Trusting the decomp here caused the in-game fault.
- `CM2Shared::Load` (`0x0083D410`) assigns `m_data` before the asynchronous read fills it, so a non-null `m_data` is
  not proof the model is loaded. The instance's `m_fFlags & 1` is what `AnimateMT` itself relies on.
- M2 drawing has two paths: with `CShaderEffect::s_enableShaders` the vertex shader skins on the GPU from the bone
  matrices in constants from c31 on; without it the CPU skins through the `CM2Model::s_transformVertices` table
  (`TransformVertices_cpp`, `0x0082A210`). Both read `m_boneMatrices`, which is why the bone route works on either.
  The grass wind working in game suggests the shader path is the one in use, but that was not confirmed.
- `CM2Model::SetBoneProceduralTransform` (`0x008272F0`) exists, but it is queued on a model call list and only applied
  to bones flagged `0x80`; it was not pursued.

## What was built (reverted)

- **Hook.** The full `AnimateMT` call-site thunk became a call instead of a jump, so code can run after the bone palette
  exists. It copies the five stack arguments into its own frame, with a 32-byte `TreeBendRequest` in a local:

  ```
  push ebp / mov ebp, esp / sub esp, 32 / push esi / mov esi, ecx
  lea eax, [ebp-32] / push eax / push esi / call vf_on_model_animate(instance, request) / add esp, 8
  push [ebp+0x18] .. push [ebp+0x08]        ; the five arguments, last first
  mov ecx, esi / call [g_modelAnimateFullTarget]
  push [ebp+0x08] / lea eax, [ebp-32] / push eax / push esi
  call vf_on_model_animated(instance, request, view) / add esp, 12
  pop esi / mov esp, ebp / pop ebp / ret 0x14
  ```

  The request lives on the stack because the animate runs on worker threads. The simple thunk passed a null request
  and kept its old jump.
- **Before the call (`Sway`).** If the model will build its bones this frame, the tree is a tree (not a bush) within
  range, and its bones qualify, the placement gets only the rigid share `1 - share` of the tilt and the request gets the
  rest (tilt vector times share, the model's base height, `1 / height`, `1 / highest pivot fraction`, the power).
- **After the call (`ApplyBend`).** Walk the bones in index order. Pivot fraction `f = clamp((pivotZ - baseZ) /
  height)`; weight `w = (f / fMax) ^ power`; `increment = w - parentWeight`. Build the world tilt rotation for
  `tilt * increment` (the existing `BuildTiltRowMatrix`), conjugate it into view space with the view's 3x3
  (`V^T * R * V`), take the bone's pivot through its own matrix as the centre, make `local` (rotation, and shift
  `centre - centre * R`), compose with the parent's accumulated transform (`rotation = local * parent`,
  `translation = shift * parentRotation + parentTranslation`), and multiply the bone matrix by it. Rotating each bone
  about its own pivot and stacking the parent's movement keeps every joint on its parent, so the trunk does not tear.
- **Gates.** Share = `TreeWindBend`, eased to 0 over the last quarter of `TreeWindBendDistance`. A model bends only with
  2 to 128 bones and a highest pivot at least 20% up its bounds; otherwise it tilts whole (without this rule a model
  whose bones all sit at its base would stop swaying). Bushes never bend. The bone table must lie inside the model file.
- **Settings.** `TreeWindBend` 0.8 (0..1), `TreeWindBendPower` 2 (1..4), `TreeWindBendDistance` 100 yd (0..500, 0 =
  all), in `CoAGraphics.ini`, `Config`, and the Tree wind section of the settings window. One log line per model:
  `tree wind: <model> has N bones, the highest pivot at P% of its height: bends|tilts whole`.
- **Checks (harness, `tests/tree_wind_checks.h`).** A fake model with a trunk of three bones (pivots at 0, 4 and 8 of
  12 yd): the base bone stays put, the crown moves more the higher the bone, a joint stays on its parent, the bend
  follows the wind's direction, an inactive request changes nothing, the placement keeps `1 - share` of the tilt, the
  outer quarter of the range bends less, beyond the range tilts whole, low-pivot and single-bone models tilt whole, a
  frame that is not rebuilt is not bent again, bushes never bend, a wild bone pointer is refused, and the thunk bends
  the bones after the original returns. Harness checks do not show a tree in game.

## What happened

The first in-game run logged `exception 0xC0000005 in the tree wind hook` within seconds, which by design switched all
tree and bush sway off for the session. The cause was the `M2Array` pointer semantics above (the decomp's offset was
read instead of the absolute pointer). The fix (read the absolute pointer, refuse a table outside the model file) was
built and passed the harness, but was not run in game before the work was reverted.

The only per-model log lines seen came through the wrong pointer and mean nothing (`kalidartree01.m2` 2 bones at 5%,
`kalidarstreetlamp02.m2` 4 bones at 3%).

## Open question that decides whether to revisit

How many of the game's tree models have bones spread up their height? A model whose bones all sit near its base cannot
bend by this route and tilts whole, exactly as today. If most trees are like that, the bone route gains little.
Quick way to find out: log, once per tree model, the bone count and highest pivot fraction (the log line above), using
the corrected absolute pointer.

## Alternatives

- **Vertex shader rewrite**, like the grass wind (`CreateVertexShader` in the device wrapper, `D3DDisassemble` /
  `D3DAssemble`): a smooth bend per vertex, the work on the GPU, the distance fade inside the shader, independent of
  bone counts. M2 vertex shaders are shared by every model, so it needs a per-draw "this is a tree" flag in a free
  vertex constant register, set from a hook where the model is known (reusing `TreeWind`'s classification), or a
  tree-only shader permutation selected through `m_vertexPermute`. The bone matrices are uploaded from c31, so the
  free registers and the insertion point must be read from the client's M2 vertex shaders in `patch.MPQ` first.
  Unproven; the larger job.
- **CPU hook on `s_transformVertices`:** per-vertex bend after the original skinning, but only the fixed-function
  fallback uses it, so it would not run on the shader path.
- **Wind from the shader without a flag:** not possible, a shader cannot tell a tree from a building.

## Restoring the work

The change touched `src/tree_wind.{h,cpp}`, `src/hooks.{h,cpp}`, `src/config.{h,cpp}`, `src/settings_panel.cpp`,
`CoAGraphics.ini`, `README.md` and `tests/tree_wind_checks.h`. It was never committed; this file holds the design
and the code outline above, which is enough to rebuild it.
