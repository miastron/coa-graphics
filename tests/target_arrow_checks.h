#pragma once

#include "target_facing.h"

namespace target_arrow_checks
{
constexpr float kCircleRadius = 2.0f;
constexpr float kCircleCentre[3] = {-8930.0f, 540.0f, 94.0f};
constexpr float kChevronMiddle = 1.08f;
constexpr float kOnTheRing = 0.85f;
constexpr float kQuarterTurn = 1.5707964f;
constexpr float kHalfTurn = 3.1415927f;
constexpr float kFacings[] = {0.0f, 0.7f, kQuarterTurn, 2.5f, kHalfTurn, 4.4f, 5.9f};
constexpr float kTestFacing = 2.2f;
constexpr uintptr_t kFakeUnit = 0x12345678;
constexpr uint32_t kHostileRed = 0xFFFF2020;
constexpr float kClientFade = 0.5f;
constexpr uint32_t kClientTriangleFlags = 0x200122;
constexpr uint32_t kClientFlags = 1;
constexpr float kClientPolygonOffset = 0.4f;
constexpr int kMaxRecordedRenders = 4;
constexpr float kMatrixTolerance = 1e-5f;
constexpr DWORD kOpaqueGreyTexel = 0xFF808080;
constexpr UINT kPlainEdge = 4;

void CircleBox(float out[kAabbFloats])
{
    for (int axis = 0; axis < 3; ++axis)
    {
        out[axis] = kCircleCentre[axis] - kCircleRadius;
        out[axis + 3] = kCircleCentre[axis] + kCircleRadius;
    }
}

void ClientTextureCoordinates(const float box[kAabbFloats], const float rotation[kMatrixFloats], float x, float y,
                              float& u, float& v)
{
    const float boxX = ((x - (box[0] + box[3]) * 0.5f) / (box[3] - box[0]));
    const float boxY = ((y - (box[1] + box[4]) * 0.5f) / (box[4] - box[1]));
    const float spunX = boxY;
    const float spunY = -boxX;
    u = spunX * rotation[0] + spunY * rotation[4] + 0.5f;
    v = spunX * rotation[1] + spunY * rotation[5] + 0.5f;
}

float CoverageAt(float facing, float angle, float radii)
{
    float circle[kAabbFloats];
    float box[kAabbFloats];
    float rotation[kMatrixFloats];
    CircleBox(circle);
    TargetArrowBox(circle, box);
    TargetArrowRotation(facing, rotation);
    float u = 0.0f;
    float v = 0.0f;
    ClientTextureCoordinates(box, rotation, kCircleCentre[0] + std::cos(angle) * radii * kCircleRadius,
                             kCircleCentre[1] + std::sin(angle) * radii * kCircleRadius, u, v);
    return ChevronCoverage(u, v, 1.0f / kChevronTextureSize);
}

void CheckArrowPointsTheFacing()
{
    bool ahead = true;
    bool elsewhere = true;
    for (float facing : kFacings)
    {
        ahead &= CoverageAt(facing, facing, kChevronMiddle) > 0.99f;
        elsewhere &= CoverageAt(facing, facing + kHalfTurn, kChevronMiddle) == 0.0f &&
                     CoverageAt(facing, facing + kQuarterTurn, kChevronMiddle) == 0.0f &&
                     CoverageAt(facing, facing - kQuarterTurn, kChevronMiddle) == 0.0f &&
                     CoverageAt(facing, facing, kOnTheRing) == 0.0f && CoverageAt(facing, facing, 0.0f) == 0.0f;
    }
    Check(ahead && elsewhere,
          "through the client's projected texture matrices the arrow lies just outside the circle in the unit's "
          "facing, and nowhere behind, beside or inside the circle");
}

void CheckArrowIsOpenAtTheBack()
{
    bool open = true;
    for (float facing : kFacings)
        for (float radii = kChevronBack - 0.01f; radii <= kChevronBack + 0.03f; radii += 0.01f)
            open &= CoverageAt(facing, facing, radii) == 0.0f;
    Check(open, "the arrow has no bar along its back: its centre is clear from the back edge to the notch");
}

void CheckArrowFadesTowardsTheTip()
{
    const auto vAt = [](float radii) { return 0.5f - radii / (2.0f * kTargetArrowBoxScale); };
    const float atBack = ChevronBrightness(vAt(kChevronBack));
    const float middle = ChevronBrightness(vAt(kChevronMiddle));
    const float atTip = ChevronBrightness(vAt(kChevronTip));
    Check(std::fabs(atBack - 1.0f) < kMatrixTolerance && middle < atBack && atTip > 0.0f && atTip < middle,
          "the arrow is brightest at its back, next to the ring, and fades towards the tip without vanishing");
}

void CheckArrowTextureBorder()
{
    bool clear = true;
    float covered = 0.0f;
    for (int size = kChevronTextureSize; size >= 1; size /= 2)
    {
        std::vector<uint32_t> texels(static_cast<size_t>(size) * size);
        FillChevronLevel(texels.data(), size, size * sizeof(uint32_t));
        for (int i = 0; i < size; ++i)
            clear &= (texels[i] >> 24) == 0 && (texels[(size - 1) * size + i] >> 24) == 0 &&
                     (texels[i * size] >> 24) == 0 && (texels[i * size + size - 1] >> 24) == 0;
        if (size == kChevronTextureSize)
            for (uint32_t texel : texels)
                covered += (texel >> 24) / 255.0f;
    }
    const float share = covered / (kChevronTextureSize * kChevronTextureSize);
    std::printf("     arrow covers %.2f%% of its texture\n", share * 100.0f);
    Check(clear && share > 0.001f && share < 0.03f,
          "the arrow texture is clear along every edge of every level, so neither clamping nor wrapping repeats "
          "it, and the arrow is a small mark");
}

void CheckTargetArrowGuards()
{
    size_t count = 0;
    const ClientCodeGuard* guards = engine::TargetFacingGuards(count);
    uintptr_t low = UINTPTR_MAX;
    uintptr_t high = 0;
    for (size_t i = 0; i < count; ++i)
    {
        low = std::min(low, guards[i].address);
        high = std::max(high, guards[i].address + guards[i].size);
    }
    std::vector<unsigned char> bytes(high - low, 0xCC);
    for (size_t i = 0; i < count; ++i)
        std::memcpy(&bytes[guards[i].address - low], guards[i].bytes, guards[i].size);
    const ClientCodeView view = {low, bytes.data(), bytes.size()};
    const bool matches = engine::TargetFacingCodeMatches(view);
    bytes[guards[0].address - low] ^= 0xFF;
    const bool changed = engine::TargetFacingCodeMatches(view);
    Check(matches && !changed, "the target arrow hook needs every guarded byte of the client's target circle code");
}

void CheckTargetArrow()
{
    CheckArrowPointsTheFacing();
    CheckArrowIsOpenAtTheBack();
    CheckArrowFadesTowardsTheTip();
    CheckArrowTextureBorder();
    CheckTargetArrowGuards();
}

struct RecordedRender
{
    float box[kAabbFloats];
    float rotation[kMatrixFloats];
    const float* rotationPointer;
    uint32_t color;
    float fade;
    uint32_t triangleFlags;
    uint32_t flags;
    float polygonOffset;
    IDirect3DBaseTexture9* stageTexture;
};

IDirect3DDevice9* g_arrowDevice = nullptr;
RecordedRender g_renders[kMaxRecordedRenders] = {};
int g_renderCount = 0;
uintptr_t g_facingUnit = 0;
uintptr_t g_arrowEsiAfter = 0;
uintptr_t g_arrowEspAfter = 0;
uintptr_t g_arrowEspExpected = 0;

void __cdecl FakeProjectedTextureRender(const float* box, const uint32_t* color, const float* rotation, float fade,
                                        uint32_t triangleFlags, uint32_t flags, float polygonOffset)
{
    if (g_renderCount >= kMaxRecordedRenders)
        return;
    RecordedRender& r = g_renders[g_renderCount++];
    std::memcpy(r.box, box, sizeof(r.box));
    std::memcpy(r.rotation, rotation, sizeof(r.rotation));
    r.rotationPointer = rotation;
    r.color = *color;
    r.fade = fade;
    r.triangleFlags = triangleFlags;
    r.flags = flags;
    r.polygonOffset = polygonOffset;
    r.stageTexture = nullptr;
    if (SUCCEEDED(g_arrowDevice->GetTexture(0, &r.stageTexture)) && r.stageTexture)
        r.stageTexture->Release();
}

float __fastcall FakeRenderFacing(uintptr_t unit, uintptr_t)
{
    g_facingUnit = unit;
    return kTestFacing;
}

__declspec(naked) void __cdecl CallTargetCircleThunk(const void*, uintptr_t, const ProjectedTextureArgs*)
{
    __asm {
        push ebp
        mov ebp, esp
        push ebx
        push esi
        push edi
        mov esi, dword ptr [ebp + 12]
        mov eax, dword ptr [ebp + 16]
        push dword ptr [eax + 24]
        push dword ptr [eax + 20]
        push dword ptr [eax + 16]
        push dword ptr [eax + 12]
        push dword ptr [eax + 8]
        push dword ptr [eax + 4]
        push dword ptr [eax]
        call dword ptr [ebp + 8]
        add esp, 0x1C
        mov g_arrowEsiAfter, esi
        mov g_arrowEspAfter, esp
        lea eax, [ebp - 12]
        mov g_arrowEspExpected, eax
        lea esp, [ebp - 12]
        pop edi
        pop esi
        pop ebx
        pop ebp
        ret
    }
}

bool SameFloats(const float* a, const float* b, int count)
{
    for (int i = 0; i < count; ++i)
        if (std::fabs(a[i] - b[i]) > kMatrixTolerance)
            return false;
    return true;
}

bool PassedTheClientArguments(const RecordedRender& r)
{
    return r.color == kHostileRed && r.fade == kClientFade && r.triangleFlags == kClientTriangleFlags &&
           r.flags == kClientFlags && r.polygonOffset == kClientPolygonOffset;
}

IDirect3DTexture9* PlainTexture(IDirect3DDevice9* dev)
{
    IDirect3DTexture9* texture = nullptr;
    if (FAILED(dev->CreateTexture(kPlainEdge, kPlainEdge, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture,
                                  nullptr)))
        return nullptr;
    D3DLOCKED_RECT locked;
    if (SUCCEEDED(texture->LockRect(0, &locked, nullptr, 0)))
    {
        for (UINT y = 0; y < kPlainEdge; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch);
            for (UINT x = 0; x < kPlainEdge; ++x)
                row[x] = kOpaqueGreyTexel;
        }
        texture->UnlockRect(0);
    }
    return texture;
}

DWORD StageAddressU(IDirect3DDevice9* dev)
{
    DWORD value = 0;
    dev->GetSamplerState(0, D3DSAMP_ADDRESSU, &value);
    return value;
}

IDirect3DBaseTexture9* StageTexture(IDirect3DDevice9* dev)
{
    IDirect3DBaseTexture9* texture = nullptr;
    if (SUCCEEDED(dev->GetTexture(0, &texture)) && texture)
        texture->Release();
    return texture;
}

uint32_t ArrowAlpha(IDirect3DBaseTexture9* base, float radii)
{
    if (!base || base->GetType() != D3DRTYPE_TEXTURE)
        return 0;
    auto* texture = static_cast<IDirect3DTexture9*>(base);
    D3DLOCKED_RECT locked;
    if (FAILED(texture->LockRect(0, &locked, nullptr, D3DLOCK_READONLY)))
        return 0;
    const int y = static_cast<int>((0.5f - radii / (2.0f * kTargetArrowBoxScale)) * kChevronTextureSize);
    const auto* row = reinterpret_cast<const uint32_t*>(static_cast<const unsigned char*>(locked.pBits) +
                                                        y * locked.Pitch);
    const uint32_t alpha = row[kChevronTextureSize / 2] >> 24;
    texture->UnlockRect(0);
    return alpha;
}

void CheckStageTextureSwap(IDirect3DDevice9* dev, IDirect3DTexture9* plain, IDirect3DTexture9* other)
{
    dev->SetTexture(0, plain);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
    const bool begun = vf_test_begin_target_arrow(dev) != 0;
    IDirect3DBaseTexture9* arrow = StageTexture(dev);
    const DWORD addressDuring = StageAddressU(dev);
    dev->SetTexture(0, other);
    dev->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_MIRROR);
    const bool kept = StageTexture(dev) == arrow && StageAddressU(dev) == D3DTADDRESS_CLAMP;
    const uint32_t tipAlpha = ArrowAlpha(arrow, kChevronMiddle);
    const uint32_t behindAlpha = ArrowAlpha(arrow, -kChevronMiddle);
    vf_test_end_target_arrow(dev);
    const bool restored = StageTexture(dev) == other && StageAddressU(dev) == D3DTADDRESS_MIRROR;
    std::printf("     arrow texel alpha ahead %u, behind %u\n", tipAlpha, behindAlpha);
    Check(begun && arrow && arrow != plain && addressDuring == D3DTADDRESS_CLAMP && kept && restored,
          "while the arrow draws, stage 0 holds the clamped arrow texture whatever the client sets, and afterwards "
          "the client's last texture and addressing are back");
    Check(tipAlpha > 100 && behindAlpha == 0, "the arrow texture on the device has the arrow ahead and nothing behind");
}

void CheckTargetCircleThunk(IDirect3DDevice9* dev, IDirect3DTexture9* plain)
{
    g_arrowDevice = dev;
    const void* thunk = vf_test_target_circle_thunk(reinterpret_cast<uintptr_t>(&FakeProjectedTextureRender),
                                                    reinterpret_cast<uintptr_t>(&FakeRenderFacing));
    float circle[kAabbFloats];
    CircleBox(circle);
    float cameraRotation[kMatrixFloats];
    TargetArrowRotation(0.3f, cameraRotation);
    const uint32_t color = kHostileRed;
    const ProjectedTextureArgs args = {circle, &color, cameraRotation, kClientFade, kClientTriangleFlags,
                                       kClientFlags, kClientPolygonOffset};
    float expectedBox[kAabbFloats];
    float expectedRotation[kMatrixFloats];
    TargetArrowBox(circle, expectedBox);
    TargetArrowRotation(kTestFacing, expectedRotation);

    Config saved;
    vf_test_get_config(&saved);
    Config cfg = saved;
    cfg.enable = true;
    cfg.targetArrow = true;
    vf_test_set_config(&cfg);
    dev->SetTexture(0, plain);
    g_renderCount = 0;
    g_facingUnit = 0;
    CallTargetCircleThunk(thunk, kFakeUnit, &args);
    const bool stackKept = g_arrowEsiAfter == kFakeUnit && g_arrowEspAfter == g_arrowEspExpected;
    const RecordedRender& circleRender = g_renders[0];
    const RecordedRender& arrowRender = g_renders[1];
    const bool circleFirst = g_renderCount == 2 && SameFloats(circleRender.box, circle, kAabbFloats) &&
                             circleRender.rotationPointer == cameraRotation &&
                             PassedTheClientArguments(circleRender) && circleRender.stageTexture == plain;
    const bool arrowSecond = g_renderCount == 2 && g_facingUnit == kFakeUnit &&
                             SameFloats(arrowRender.box, expectedBox, kAabbFloats) &&
                             SameFloats(arrowRender.rotation, expectedRotation, kMatrixFloats) &&
                             PassedTheClientArguments(arrowRender) && arrowRender.stageTexture &&
                             arrowRender.stageTexture != plain;
    const bool plainAfter = StageTexture(dev) == plain;

    cfg.targetArrow = false;
    vf_test_set_config(&cfg);
    g_renderCount = 0;
    CallTargetCircleThunk(thunk, kFakeUnit, &args);
    const bool offDrawsOnlyTheCircle = g_renderCount == 1 && circleRender.rotationPointer == cameraRotation;
    vf_test_set_config(&saved);

    Check(stackKept && circleFirst && arrowSecond && plainAfter,
          "the target circle thunk draws the client's circle unchanged, then the arrow with the unit's render "
          "facing in a wider box under the arrow texture, and returns with the client's esi, stack and texture");
    Check(offDrawsOnlyTheCircle, "with TargetArrow=0 the thunk draws only the client's circle");
}

void CheckTargetArrowOnTheDevice(IDirect3DDevice9* dev)
{
    IDirect3DTexture9* plain = PlainTexture(dev);
    IDirect3DTexture9* other = PlainTexture(dev);
    Check(plain && other, "plain textures for the target arrow checks");
    if (!plain || !other)
        return;
    CheckStageTextureSwap(dev, plain, other);
    CheckTargetCircleThunk(dev, plain);
    dev->SetTexture(0, nullptr);
    plain->Release();
    other->Release();
}
}
