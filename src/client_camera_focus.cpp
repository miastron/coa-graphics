#include "client_camera_focus.h"

namespace
{
const ClientCodeGuard kFocusGroundStore = {
    "camera focus store", kCameraFocusStore, 14,
    {0xA3, 0x78, 0x77, 0xCD, 0x00, 0x8B, 0x4F, 0x04, 0x89, 0x0D, 0x7C, 0x77, 0xCD, 0x00}};
const ClientCodeGuard kFocusHeightStore = {
    "camera focus height store", kCameraFocusHeightStore, 9, {0x8B, 0x57, 0x08, 0x89, 0x15, 0x80, 0x77, 0xCD, 0x00}};
}

namespace engine
{
bool CameraFocusReadable(const ClientCodeView& code)
{
    return ClientGuardMatches(code, kFocusGroundStore) && ClientGuardMatches(code, kFocusHeightStore);
}
}
