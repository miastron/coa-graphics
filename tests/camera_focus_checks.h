#pragma once

namespace camera_focus_checks
{
constexpr size_t kStoreCodeBytes = 0x20;
constexpr unsigned char kStoreFiller = 0xCC;
constexpr uintptr_t kStoreCodeBase = kCameraFocusStore - 5;
constexpr unsigned char kFocusGroundStore[] = {0xA3, 0x78, 0x77, 0xCD, 0x00, 0x8B, 0x4F,
                                               0x04, 0x89, 0x0D, 0x7C, 0x77, 0xCD, 0x00};
constexpr unsigned char kFocusHeightStore[] = {0x8B, 0x57, 0x08, 0x89, 0x15, 0x80, 0x77, 0xCD, 0x00};
constexpr D3DVIEWPORT9 kView = {0, 0, 1280, 720, 0, 1};
constexpr float kFocusAhead = 8.0f;
constexpr float kEyeAboveTheFeet = 1.6f;
constexpr float kBeyondTheFarthestFocus = 150.0f;
constexpr float kCloseZoom = 3.2f;
constexpr float kZoomedIn = 2.0f;
constexpr float kFocusTolerance = 0.01f;

struct StoreCode
{
    unsigned char bytes[kStoreCodeBytes] = {};

    StoreCode()
    {
        std::memset(bytes, kStoreFiller, sizeof(bytes));
        std::memcpy(bytes + (kCameraFocusStore - kStoreCodeBase), kFocusGroundStore, sizeof(kFocusGroundStore));
        std::memcpy(bytes + (kCameraFocusHeightStore - kStoreCodeBase), kFocusHeightStore,
                    sizeof(kFocusHeightStore));
    }
    ClientCodeView View() const { return {kStoreCodeBase, bytes, sizeof(bytes)}; }
};

void CheckTheFocusFollowsTheClientStores()
{
    const StoreCode stock;
    StoreCode otherGround;
    otherGround.bytes[kCameraFocusStore - kStoreCodeBase + 1] = 0x7C;
    StoreCode otherHeight;
    otherHeight.bytes[kCameraFocusHeightStore - kStoreCodeBase + 5] = 0x84;
    const ClientCodeView cut = {kStoreCodeBase, stock.bytes, kCameraFocusHeightStore - kStoreCodeBase + 4};
    Check(engine::CameraFocusReadable(stock.View()) && !engine::CameraFocusReadable(otherGround.View()) &&
              !engine::CameraFocusReadable(otherHeight.View()) && !engine::CameraFocusReadable(cut),
          "the camera focus is read from 0xCD7778 only while the client stores the followed unit's position there "
          "(0x780871 and 0x78087F); any other code leaves the focus on the camera, so nothing is left out of the "
          "god-ray shafts");
}

struct FocusView
{
    Vec3 eye = Add({0, 0, 3}, kGameLikeWorldOffset);
    float view[16] = {};
    float proj[16] = {};

    FocusView()
    {
        CameraRelativeLookAt(eye, Add(eye, {kFocusAhead, 0, -1}), view);
        EngineProjection(static_cast<float>(kView.Width) / kView.Height, proj);
    }

    CharacterOnScreen At(Vec3 focus) const
    {
        const float focusAt[3] = {focus.x, focus.y, focus.z};
        const float camera[3] = {eye.x, eye.y, eye.z};
        return ProjectCharacterOntoViewport(focusAt, camera, view, proj, kView);
    }
};

bool Covers(const CharacterOnScreen& character, float x, float y)
{
    return character.present && x >= character.pixelMin[0] && x <= character.pixelMax[0] &&
           y >= character.pixelMin[1] && y <= character.pixelMax[1];
}

bool FillsTheView(const CharacterOnScreen& character)
{
    return character.present && character.pixelMin[0] == static_cast<float>(kView.X) &&
           character.pixelMin[1] == static_cast<float>(kView.Y) &&
           character.pixelMax[0] == static_cast<float>(kView.X + kView.Width) &&
           character.pixelMax[1] == static_cast<float>(kView.Y + kView.Height);
}

bool ReachIs(const CharacterOnScreen& character, float expected)
{
    return character.present && std::fabs(character.reach - expected) <= kFocusTolerance;
}

bool FocusFromTheEyeIs(const CharacterOnScreen& character, Vec3 expected)
{
    return std::fabs(character.focusFromCamera[0] - expected.x) <= kFocusTolerance &&
           std::fabs(character.focusFromCamera[1] - expected.y) <= kFocusTolerance &&
           std::fabs(character.focusFromCamera[2] - expected.z) <= kFocusTolerance;
}

void CheckTheCharacterBoxFollowsTheFocus()
{
    const FocusView v;
    const Vec3 feetAhead = Add(v.eye, {kFocusAhead, 0, -1});
    const CharacterOnScreen ahead = v.At(feetAhead);
    const float centre[2] = {kView.Width * 0.5f, kView.Height * 0.5f};
    const CharacterOnScreen firstPerson = v.At(Add(v.eye, {0, 0, -kEyeAboveTheFeet}));
    const CharacterOnScreen faraway = v.At(Add(v.eye, {kBeyondTheFarthestFocus, 0, -1}));
    const CharacterOnScreen unreadable = v.At({NAN, NAN, NAN});
    const CharacterOnScreen closeZoom = v.At(Add(v.eye, {kCloseZoom, 0, -1}));
    const CharacterOnScreen zoomedIn = v.At(Add(v.eye, {kZoomedIn, 0, -1}));
    std::printf("     character box for feet %.0f yd ahead: (%.0f %.0f) to (%.0f %.0f) in a %ux%u view, reach %.2f yd; "
                "%.1f yd ahead reach %.2f yd, %.1f yd ahead %.2f yd\n",
                kFocusAhead, ahead.pixelMin[0], ahead.pixelMin[1], ahead.pixelMax[0], ahead.pixelMax[1], kView.Width,
                kView.Height, ahead.reach, kCloseZoom, closeZoom.reach, kZoomedIn, zoomedIn.reach);
    Check(Covers(ahead, centre[0], centre[1]) && ahead.pixelMax[0] - ahead.pixelMin[0] < kView.Width &&
              FocusFromTheEyeIs(ahead, {kFocusAhead, 0, -1}) && ReachIs(ahead, kCharacterReach) &&
              FillsTheView(closeZoom) && FocusFromTheEyeIs(closeZoom, {kCloseZoom, 0, -1}) &&
              ReachIs(closeZoom, kCloseZoom - kCameraClearance) && FocusFromTheEyeIs(zoomedIn, {kZoomedIn, 0, -1}) &&
              ReachIs(zoomedIn, kZoomedIn - kCameraClearance) && !firstPerson.present && !faraway.present &&
              !unreadable.present,
          "the own character's box on screen surrounds the unit the camera follows and fills the whole view once a "
          "corner comes within a quarter yard of the camera's plane, its reach stays a quarter yard clear of a "
          "camera zoomed in closer than 3 yd, and there is none in first person, beyond 100 yd or for an unreadable "
          "focus");
}
}
