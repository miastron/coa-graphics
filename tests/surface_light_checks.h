#pragma once

namespace surface_light_checks
{
constexpr UINT kViewWidth = 256;
constexpr UINT kViewHeight = 192;
constexpr float kEyeHeight = 9.0f;
constexpr float kLookAhead = 40.0f;
constexpr float kLightAhead = 22.0f;
constexpr float kLightHeight = 2.0f;
constexpr float kLightColour[3] = {1.0f, 0.6f, 0.3f};
constexpr float kLightConstantAttenuation = 1.0f;
constexpr float kLightQuadraticAttenuation = 0.05f;
constexpr float kBeyondReach = 5.0f;
constexpr float kHalfStrength = 0.5f;
constexpr float kFullStrength = 1.0f;
constexpr int kLitAtLeast = 8;
constexpr float kClientShadowShare = 0.7f;
constexpr float kShareTolerance = 0.03f;
constexpr float kShadowLeftUnderALamp = 0.9f;
constexpr int kLitUnderASkyAtLeast = 4;
constexpr uint32_t kDarkAmbient = 0xFF060606;
constexpr uint32_t kBrightAmbient = 0xFFFFFFFF;
constexpr uint32_t kFullDirectLight = 0xFFFFFFFF;
constexpr float kFaintLight = 0.2f;
constexpr uint32_t kNoDirectLight = 0xFF000000;
constexpr UINT kShadowMapSize = 256;
constexpr float kShadowCasterDepth = 0.5f;
constexpr float kShadowedReceiverDepth = 0.75f;
constexpr float kLitReceiverDepth = 0.25f;
constexpr float kOffTheShadowMap = 5.0f;
constexpr int kOneShadowMap = 1;
constexpr int kNoShadowingMap = -1;
constexpr int kCharacterMap = 0;
constexpr int kNearMap = 1;
constexpr int kMiddleMap = 2;
constexpr int kFarMap = 3;
constexpr uint32_t kOpenGroundAmbient = 0xFFAFAFAF;
constexpr uint32_t kShadedGroundAmbient = 0xFFFAFAFA;
constexpr int kShadowFillAtLeast = 4;
constexpr float kSmallLight = 0.3f;
constexpr uint32_t kDuskAmbient = 0xFF707070;
constexpr float kLargestShareLitInPlace = 0.75f;
constexpr float kLightBehind = -60.0f;
constexpr Vec3 kWallLow = {26, -3, 0};
constexpr Vec3 kWallHigh = {28, 3, 8};
constexpr Vec3 kWallPoint = {26, 0, 3};
constexpr DWORD kWallColour = 0xFF6E5A48;
constexpr int kTwoLights = 2;
constexpr float kStrongLight = 4.0f;
constexpr float kLargestShareOfTwiceTheLight = 0.85f;
constexpr float kBesideTheLight = 2.5f;
constexpr float kLitPatchThickness = 0.05f;
constexpr Vec3 kLitPatchLow = {kLightAhead - 1.5f, 1, 0};
constexpr Vec3 kLitPatchHigh = {kLightAhead + 1.5f, 4, kLitPatchThickness};
constexpr Vec3 kLitPatchPoint = {kLightAhead, kBesideTheLight, kLitPatchThickness};
constexpr Vec3 kDarkGroundPoint = {kLightAhead, -kBesideTheLight, 0};
constexpr DWORD kLitPatchColour = 0xFF8C7A48;
constexpr float kLargestShareOnLitGround = 0.85f;
constexpr float kOverpoweringLight = 16.0f;
constexpr float kBrightestAssumedAlbedo = 0.5f;
constexpr float kLinearLightTolerance = 0.02f;
constexpr D3DVIEWPORT9 kPictureView = {0, 0, 3 * kViewWidth, 3 * kViewHeight, 0.0f, 1.0f};

struct LitFrame
{
    Image image;
    bool rendered = false;
    bool statesKept = false;
    unsigned draws = 0;
    unsigned colourCopies = 0;
    unsigned surfaceLightCopies = 0;
    unsigned compositeCopies = 0;
    unsigned godRayCopies = 0;
    float share = 0.0f;
};

struct Scene
{
    D3DVIEWPORT9 world = {0, 0, kViewWidth, kViewHeight, 0.0f, 1.0f};
    Vec3 eye = Add(kGameLikeWorldOffset, {0, 0, kEyeHeight});
    Vec3 at = Add(kGameLikeWorldOffset, {kLookAhead, 0, 0});
    float lightAhead = kLightAhead;
    int lights = 1;
    float view[16] = {};
    float projection[16] = {};
};

Config SurfaceConfig(const Config& saved, float strength, float godRays = 0.0f)
{
    Config config = saved;
    config.quality = 1;
    config.noiseAmount = 0;
    config.localLights = true;
    config.temporal = 0;
    config.godRays = godRays;
    config.maxDistance = 1000;
    config.dataMode = 0;
    config.surfaceLights = strength;
    return config;
}

FrameInputs SceneInputs(Scene& scene, const Config& config, float lightScale = 1.0f,
                        LocalLightSource source = LocalLightSource::World)
{
    CameraRelativeLookAt(scene.eye, scene.at, scene.view);
    EngineProjection(static_cast<float>(kViewWidth) / kViewHeight, scene.projection);
    FrameInputs input = MakeInputs(scene.view, scene.projection, scene.eye, scene.at, scene.world);
    LocalPointLight light;
    const Vec3 position = Add(kGameLikeWorldOffset, {scene.lightAhead, 0, kLightHeight});
    light.position[0] = position.x;
    light.position[1] = position.y;
    light.position[2] = position.z;
    for (int channel = 0; channel < 3; ++channel)
        light.color[channel] = kLightColour[channel] * lightScale;
    light.attenuation[0] = kLightConstantAttenuation;
    light.attenuation[2] = kLightQuadraticAttenuation;
    light.source = source;
    for (int i = 0; i < scene.lights; ++i)
        engine::SelectLocalPointLight(input.localLights, light, input.camPos, LocalLightUpload(config, input.mapId));
    return input;
}

LitFrame RenderLit(Harness& h, Scene& scene, const Config& saved, float strength, bool withLight,
                   uint32_t ambientColor = 0, uint32_t directColor = 0, float lightScale = 1.0f,
                   float godRays = 0.0f, LocalLightSource source = LocalLightSource::World)
{
    const Config config = SurfaceConfig(saved, strength, godRays);
    vf_test_set_config(&config);
    FrameInputs input = SceneInputs(scene, config, lightScale, source);
    if (ambientColor)
        input.ambientColor = ambientColor;
    if (directColor)
        input.directColor = directColor;
    if (!withLight)
        input.localLights.pointLightCount = 0;
    LitFrame frame;
    FrameCopyCounts counts;
    vf_test_take_frame_copies(&counts);
    const unsigned before = vf_test_surface_light_draws();
    h.BeginFrame();
    h.DrawScene(scene.eye, scene.view, scene.projection, scene.world);
    Sentinel s0;
    ReadSentinel(h.dev, s0);
    const char* skip = nullptr;
    frame.rendered = vf_test_render(&input, &skip) != 0;
    Sentinel s1;
    ReadSentinel(h.dev, s1);
    frame.statesKept = SameSentinel(s0, s1);
    if (!frame.statesKept)
        ReportSentinelDifferences(s0, s1);
    ReleaseSentinel(s0);
    ReleaseSentinel(s1);
    frame.image = Capture(h.dev);
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    frame.draws = vf_test_surface_light_draws() - before;
    frame.share = frame.draws ? vf_test_drawn_surface_light_share() : 0.0f;
    vf_test_take_frame_copies(&counts);
    for (unsigned copies : counts.colourCopies)
        frame.colourCopies += copies;
    frame.surfaceLightCopies = counts.colourCopies[static_cast<int>(ColourCopySite::SurfaceLights)];
    frame.compositeCopies = counts.colourCopies[static_cast<int>(ColourCopySite::FogComposite)];
    frame.godRayCopies = counts.colourCopies[static_cast<int>(ColourCopySite::GodRaySource)];
    return frame;
}

bool ScenePixel(const Scene& scene, Vec3 point, UINT& x, UINT& y)
{
    const Vec3 relative = Sub(Add(kGameLikeWorldOffset, point), scene.eye);
    const float world[3] = {relative.x, relative.y, relative.z};
    float view[3];
    TransformDirection(world, scene.view, view);
    if (view[2] <= 0.0f)
        return false;
    const float* p = scene.projection;
    const float ndcX = view[0] / view[2] * p[0] + p[8];
    const float ndcY = view[1] / view[2] * p[5] + p[9];
    const float px = scene.world.X + (ndcX * 0.5f + 0.5f) * scene.world.Width;
    const float py = scene.world.Y + (0.5f - ndcY * 0.5f) * scene.world.Height;
    if (px < 0.0f || py < 0.0f || px >= scene.world.Width || py >= scene.world.Height)
        return false;
    x = static_cast<UINT>(px);
    y = static_cast<UINT>(py);
    return true;
}

bool GroundPixel(const Scene& scene, float ahead, UINT& x, UINT& y)
{
    return ScenePixel(scene, {ahead, 0, 0}, x, y);
}

int GreenLift(const Image& lit, const Image& unlit, UINT x, UINT y)
{
    return static_cast<int>(lit.At(x, y)[1]) - static_cast<int>(unlit.At(x, y)[1]);
}

int RedLift(const Image& lit, const Image& unlit, UINT x, UINT y)
{
    return static_cast<int>(lit.At(x, y)[2]) - static_cast<int>(unlit.At(x, y)[2]);
}

void CheckLightsOnSurfaces(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    const FrameInputs probe = SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    const float reach = probe.localLights.pointLightCount ? probe.localLights.pointLights[0].cutoff : 0.0f;
    const LitFrame off = RenderLit(h, scene, saved, 0.0f, true);
    const LitFrame half = RenderLit(h, scene, saved, kHalfStrength, true);
    const LitFrame full = RenderLit(h, scene, saved, kFullStrength, true);
    const LitFrame noLight = RenderLit(h, scene, saved, kFullStrength, false);
    const LitFrame noLightOff = RenderLit(h, scene, saved, 0.0f, false);
    UINT underX = 0;
    UINT underY = 0;
    UINT farX = 0;
    UINT farY = 0;
    const bool placed = GroundPixel(scene, kLightAhead, underX, underY) &&
                        GroundPixel(scene, kLightAhead + reach + kBeyondReach, farX, farY);
    const int halfLift = placed ? GreenLift(half.image, off.image, underX, underY) : 0;
    const int fullLift = placed ? GreenLift(full.image, off.image, underX, underY) : 0;
    const int farLift = placed ? GreenLift(full.image, off.image, farX, farY) : -1;
    std::printf("     ground under a point light %.1f yd away (reach %.1f yd): green lift %d at 0.5, %d at 1; beyond "
                "its reach %d; surface passes %u %u %u, without lights %u\n",
                kLightAhead, reach, halfLift, fullLift, farLift, off.draws, half.draws, full.draws, noLight.draws);
    Check(placed && off.rendered && full.rendered && half.rendered,
          "the surface light scene renders with the light's ground point and a point beyond its reach in view");
    Check(off.draws == 0 && half.draws == 1 && full.draws == 1 && halfLift >= kLitAtLeast && fullLift > halfLift,
          "SurfaceLights lights the ground under a point light, more at 1 than at 0.5");
    Check(farLift == 0, "ground beyond the light's reach is left exactly as it was");
    Check(noLight.draws == 0 && image_quality_checks::SameImage(noLight.image, noLightOff.image),
          "without point lights in view the surface pass is not drawn and the frame is unchanged");
    vf_test_keep_surface_lights_in_target(1);
    const LitFrame kept = RenderLit(h, scene, saved, kFullStrength, true);
    vf_test_keep_surface_lights_in_target(0);
    const LitFrame handed = RenderLit(h, scene, saved, kFullStrength, true);
    std::printf("     colour copies in the frame: %u with the lit scene handed to the composite, %u with the lights "
                "written into the target\n",
                handed.colourCopies, kept.colourCopies);
    Check(handed.draws == 1 && kept.draws == 1 && handed.compositeCopies == 1 && kept.compositeCopies == 1 &&
              image_quality_checks::SameImage(handed.image, kept.image),
          "the fog composite reading the lit scene texture draws exactly the frame that lighting the target and "
          "copying it again draws");
    Check(handed.surfaceLightCopies == 0 && kept.surfaceLightCopies == 1 &&
              handed.colourCopies + 1 == kept.colourCopies,
          "lighting the composite's own scene copy over the whole view takes no colour copy beside the "
          "composite's, where lighting the target takes a second one");
    Check(half.statesKept && full.statesKept,
          "the surface light pass restores render, sampler, texture, shader, constant, stream, viewport, scissor, "
          "render target and depth state, from its first frame on");
    vf_test_set_config(&saved);
}

LitFrame RenderSpellOrLamp(Harness& h, Scene& scene, const Config& saved, LocalLightSource source)
{
    return RenderLit(h, scene, saved, saved.surfaceLights, true, 0, 0, 1.0f, 0.0f, source);
}

void CheckSpellLightsOnSurfaces(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    UINT x = 0;
    UINT y = 0;
    if (!GroundPixel(scene, kLightAhead, x, y))
    {
        Check(false, "the ground under the light is in view");
        return;
    }
    Config shipped = saved;
    shipped.surfaceLights = kFullStrength;
    shipped.spellSurfaceLights = kFullStrength;
    shipped.localLightIntensity = 1.0f;
    shipped.azerothLocalLights = 1.0f;
    Config dimLamps = shipped;
    dimLamps.surfaceLights = 0.0f;
    dimLamps.localLightIntensity = 0.25f;
    dimLamps.azerothLocalLights = 0.25f;
    Config noSpellSurfaces = shipped;
    noSpellSurfaces.spellSurfaceLights = 0.0f;
    const LitFrame unlit = RenderLit(h, scene, saved, 0.0f, false);
    const LitFrame spellShipped = RenderSpellOrLamp(h, scene, shipped, LocalLightSource::Spell);
    const LitFrame spellBesideDimLamps = RenderSpellOrLamp(h, scene, dimLamps, LocalLightSource::Spell);
    const LitFrame lampWithoutSurfaces = RenderSpellOrLamp(h, scene, dimLamps, LocalLightSource::World);
    const LitFrame spellWithoutSurfaces = RenderSpellOrLamp(h, scene, noSpellSurfaces, LocalLightSource::Spell);
    const LitFrame lampBesideNoSpellSurfaces = RenderSpellOrLamp(h, scene, noSpellSurfaces, LocalLightSource::World);
    const int spellLift = GreenLift(spellShipped.image, unlit.image, x, y);
    const int dimLampsSpellLift = GreenLift(spellBesideDimLamps.image, unlit.image, x, y);
    const int lampLift = GreenLift(lampBesideNoSpellSurfaces.image, unlit.image, x, y);
    std::printf("     ground under a spell light: green lift %d as shipped, %d with the lamps' surface light off and "
                "their intensity at 0.25; surface passes with SurfaceLights=0 for a lamp %u, with "
                "SpellSurfaceLights=0 for a spell %u; a lamp's lift with SpellSurfaceLights=0 %d\n",
                spellLift, dimLampsSpellLift, lampWithoutSurfaces.draws, spellWithoutSurfaces.draws, lampLift);
    Check(spellShipped.draws == 1 && spellBesideDimLamps.draws == 1 && spellLift >= kLitAtLeast &&
              image_quality_checks::SameImage(spellShipped.image, spellBesideDimLamps.image),
          "a spell light lights the ground with SpellSurfaceLights exactly as before when SurfaceLights, "
          "LocalLightIntensity and the region's lamp intensity are turned down");
    Check(lampWithoutSurfaces.draws == 0 && spellWithoutSurfaces.draws == 0 &&
              lampBesideNoSpellSurfaces.draws == 1 && lampLift >= kLitAtLeast,
          "SurfaceLights=0 keeps lamps off the surfaces and SpellSurfaceLights=0 keeps spells off them, each "
          "without touching the other");
    vf_test_set_config(&saved);
}

IDirect3DTexture9* CreateClientShadowMap(IDirect3DDevice9* dev)
{
    IDirect3DTexture9* map = nullptr;
    if (FAILED(dev->CreateTexture(kShadowMapSize, kShadowMapSize, 1, 0, D3DFMT_R32F, D3DPOOL_MANAGED, &map, nullptr)))
        return nullptr;
    D3DLOCKED_RECT locked = {};
    if (FAILED(map->LockRect(0, &locked, nullptr, 0)))
    {
        map->Release();
        return nullptr;
    }
    for (UINT y = 0; y < kShadowMapSize; ++y)
    {
        float* row = reinterpret_cast<float*>(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch);
        std::fill_n(row, kShadowMapSize, kShadowCasterDepth);
    }
    map->UnlockRect(0);
    return map;
}

WorldShadowInputs ClientShadows(IDirect3DTexture9* map, int count, int shadowingMap)
{
    WorldShadowInputs shadows;
    shadows.count = count;
    for (int i = 0; i < count; ++i)
    {
        shadows.textures[i] = map;
        shadows.texelSize[i][0] = shadows.texelSize[i][1] = 1.0f / kShadowMapSize;
        shadows.viewToShadow[i][3] = i < shadowingMap ? kOffTheShadowMap : 0.0f;
        shadows.viewToShadow[i][11] = kLitReceiverDepth;
    }
    if (shadowingMap >= 0)
    {
        shadows.viewToShadow[shadowingMap][3] = 0.0f;
        shadows.viewToShadow[shadowingMap][11] = kShadowedReceiverDepth;
    }
    return shadows;
}

LitFrame RenderUnderClientShadows(Harness& h, Scene& scene, const Config& config, const WorldShadowInputs& shadows,
                                  uint32_t ambientColor)
{
    vf_test_override_world_shadows(&shadows);
    const LitFrame frame = RenderLit(h, scene, config, kFullStrength, true, ambientColor, kNoDirectLight);
    vf_test_override_world_shadows(nullptr);
    return frame;
}

void CheckLightsFillClientShadows(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    UINT x = 0;
    UINT y = 0;
    IDirect3DTexture9* map = CreateClientShadowMap(h.dev);
    if (!map || !GroundPixel(scene, kLightAhead, x, y))
    {
        Check(false, "the client shadow map fixture is created and the ground under the light is in view");
        if (map)
            map->Release();
        return;
    }
    Config withoutShafts = saved;
    withoutShafts.lightShafts = false;
    withoutShafts.worldShadows = true;
    Config withoutWorldShadows = withoutShafts;
    withoutWorldShadows.worldShadows = false;
    const LitFrame unlit = RenderLit(h, scene, withoutShafts, 0.0f, true, kOpenGroundAmbient, kNoDirectLight);
    const LitFrame openGround =
        RenderLit(h, scene, withoutShafts, kFullStrength, true, kOpenGroundAmbient, kNoDirectLight);
    const LitFrame brighterSky =
        RenderLit(h, scene, withoutShafts, kFullStrength, true, kShadedGroundAmbient, kNoDirectLight);
    const LitFrame open = RenderUnderClientShadows(
        h, scene, withoutShafts, ClientShadows(map, kOneShadowMap, kNoShadowingMap), kOpenGroundAmbient);
    const LitFrame openCascades = RenderUnderClientShadows(
        h, scene, withoutShafts, ClientShadows(map, kWorldShadowMapCount, kNoShadowingMap), kOpenGroundAmbient);
    const LitFrame ignored = RenderUnderClientShadows(
        h, scene, withoutWorldShadows, ClientShadows(map, kWorldShadowMapCount, kNearMap), kOpenGroundAmbient);
    const int openLift = GreenLift(openGround.image, unlit.image, x, y);
    const int brighterSkyLift = GreenLift(brighterSky.image, unlit.image, x, y);
    std::printf("     ground of one colour under a light: green lift %d under an ambient of 175, %d under an ambient "
                "of 250 that the client did not shade\n",
                openLift, brighterSkyLift);
    Check(openLift >= kLitAtLeast && brighterSkyLift < openLift - kShadowFillAtLeast,
          "ground as bright under a brighter sky has a darker albedo and takes less of a light");
    Check(image_quality_checks::SameImage(open.image, openGround.image) &&
              image_quality_checks::SameImage(openCascades.image, openGround.image) &&
              image_quality_checks::SameImage(ignored.image, openGround.image),
          "ground the client's shadow maps leave lit takes a light exactly as without the maps, and WorldShadows=0 "
          "leaves the maps unread");
    struct ShadowedCase
    {
        const char* name;
        int count;
        int shadowingMap;
    };
    const ShadowedCase cases[] = {
        {"the character map alone", kOneShadowMap, kCharacterMap},
        {"the character map beside open cascades", kWorldShadowMapCount, kCharacterMap},
        {"the near cascade", kWorldShadowMapCount, kNearMap},
        {"the middle cascade", kWorldShadowMapCount, kMiddleMap},
        {"the far cascade", kWorldShadowMapCount, kFarMap},
    };
    for (const ShadowedCase& shadowed : cases)
    {
        const LitFrame frame = RenderUnderClientShadows(
            h, scene, withoutShafts, ClientShadows(map, shadowed.count, shadowed.shadowingMap), kShadedGroundAmbient);
        const int lift = GreenLift(frame.image, unlit.image, x, y);
        char label[240];
        std::snprintf(label, sizeof(label),
                      "ground under an ambient of 250 that the client shaded to 0.7 through %s takes the light of "
                      "the same ground under an ambient of 175: green lift %d against %d",
                      shadowed.name, lift, openLift);
        Check(frame.draws == 1 && std::abs(lift - openLift) <= 1, label);
    }
    map->Release();
    vf_test_set_config(&saved);
}

void ShadeTheGround(std::vector<SceneVertex>& vertices, float share)
{
    constexpr size_t kGroundVertices = 6;
    for (size_t i = 0; i < kGroundVertices; ++i)
    {
        DWORD shaded = 0xFF000000;
        for (int shift = 0; shift <= 16; shift += 8)
            shaded |= static_cast<DWORD>(((vertices[i].color >> shift) & 0xFF) * share) << shift;
        vertices[i].color = shaded;
    }
}

float ShareOfTheOpenGround(const LitFrame& shaded, const LitFrame& open, UINT x, UINT y)
{
    return static_cast<float>(shaded.image.At(x, y)[1]) / open.image.At(x, y)[1];
}

void CheckLampLightUndoesTheClientsShadowOnGround(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    const FrameInputs probe = SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    const float reach = probe.localLights.pointLightCount ? probe.localLights.pointLights[0].cutoff : 0.0f;
    UINT x = 0;
    UINT y = 0;
    UINT farX = 0;
    UINT farY = 0;
    IDirect3DTexture9* map = CreateClientShadowMap(h.dev);
    if (!map || !GroundPixel(scene, kLightAhead, x, y) ||
        !GroundPixel(scene, kLightAhead + reach + kBeyondReach, farX, farY))
    {
        Check(false, "the client shadow map fixture is created and the ground under and beyond the light is in view");
        if (map)
            map->Release();
        return;
    }
    Config withoutShafts = saved;
    withoutShafts.lightShafts = false;
    withoutShafts.worldShadows = true;
    const WorldShadowInputs open = ClientShadows(map, kWorldShadowMapCount, kNoShadowingMap);
    const WorldShadowInputs shadowed = ClientShadows(map, kWorldShadowMapCount, kNearMap);
    const std::vector<SceneVertex> openGround = h.scene;
    const auto renderAtNight = [&](const WorldShadowInputs& shadows, float strength) {
        Config config = withoutShafts;
        config.surfaceLights = strength;
        vf_test_override_world_shadows(&shadows);
        const LitFrame frame = RenderLit(h, scene, config, strength, true, kDarkAmbient, kNoDirectLight);
        vf_test_override_world_shadows(nullptr);
        return frame;
    };
    const LitFrame openOff = renderAtNight(open, 0.0f);
    const LitFrame openLit = renderAtNight(open, kFullStrength);
    ShadeTheGround(h.scene, kClientShadowShare);
    const LitFrame shadowedOff = renderAtNight(shadowed, 0.0f);
    const LitFrame shadowedLit = renderAtNight(shadowed, kFullStrength);
    h.scene = openGround;
    map->Release();
    const float without = ShareOfTheOpenGround(shadowedOff, openOff, x, y);
    const float under = ShareOfTheOpenGround(shadowedLit, openLit, x, y);
    const float beyondWithout = ShareOfTheOpenGround(shadowedOff, openOff, farX, farY);
    const float beyond = ShareOfTheOpenGround(shadowedLit, openLit, farX, farY);
    std::printf("     ground the client shaded to %.2f of the open ground, green against the open ground: %.2f "
                "without the light and %.2f under it; beyond its reach %.2f and %.2f\n",
                kClientShadowShare, without, under, beyondWithout, beyond);
    Check(shadowedLit.draws == 1 && std::abs(without - kClientShadowShare) < kShareTolerance &&
              std::abs(beyondWithout - beyond) < kShareTolerance && under > kShadowLeftUnderALamp,
          "ground the client's shadow map shades, lit at night by a lamp over the client's own light, is left "
          "closer to the open ground's brightness than it is beside the lamp, and ground the light does not reach "
          "keeps its shadow");
    vf_test_set_config(&saved);
}

void CheckSurfacesBrighterThanTheirLightingAreNotLitAgain(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    UINT x = 0;
    UINT y = 0;
    if (!GroundPixel(scene, kLightAhead, x, y))
    {
        Check(false, "the ground under the light is in view");
        return;
    }
    UINT wallX = 0;
    UINT wallY = 0;
    if (!ScenePixel(scene, kWallPoint, wallX, wallY))
    {
        Check(false, "the wall beside the light is in view");
        return;
    }
    const std::vector<SceneVertex> withoutTheWall = h.scene;
    AddBox(h.scene, kWallLow, kWallHigh, kWallColour);
    const LitFrame plausibleOff = RenderLit(h, scene, saved, 0.0f, true, kOpenGroundAmbient, kNoDirectLight);
    const LitFrame plausible = RenderLit(h, scene, saved, kFullStrength, true, kOpenGroundAmbient, kNoDirectLight);
    const LitFrame settled = RenderLit(h, scene, saved, kFullStrength, true, kOpenGroundAmbient, kNoDirectLight);
    const LitFrame darkOff = RenderLit(h, scene, saved, 0.0f, true, kDarkAmbient, kNoDirectLight);
    const LitFrame dark = RenderLit(h, scene, saved, kFullStrength, true, kDarkAmbient, kNoDirectLight);
    h.scene = withoutTheWall;
    const int plausibleLift = GreenLift(settled.image, plausibleOff.image, x, y);
    const int darkLift = GreenLift(dark.image, darkOff.image, x, y);
    const int plausibleWallLift = GreenLift(settled.image, plausibleOff.image, wallX, wallY);
    const int darkWallLift = GreenLift(dark.image, darkOff.image, wallX, wallY);
    std::printf("     green lift where the lighting explains the brightness and where the surface is far brighter "
                "than that: ground under the light %d and %d, a wall beside it %d and %d\n",
                plausibleLift, darkLift, plausibleWallLift, darkWallLift);
    Check(plausible.draws == 1 && dark.draws == 1 && plausibleWallLift >= kLitUnderASkyAtLeast && darkWallLift == 0,
          "a wall far brighter than the sky's ambient and sun light can explain, because the client lit it "
          "already or it glows, gets no second light from SurfaceLights, while the same wall under a plausible "
          "ambient does");
    Check(plausibleLift >= kLitAtLeast && darkLift >= kLitAtLeast,
          "ground far brighter than the sky can explain, as under a lamp the client lights the ground with "
          "itself at night, still takes the light, so no dark patch is left where the light is strongest");
    vf_test_set_config(&saved);
}

struct NightFrames
{
    LitFrame unlit;
    LitFrame lit;
};

LitFrame RenderSettledAtNight(Harness& h, Scene& scene, const Config& saved, float strength, float lightScale)
{
    RenderLit(h, scene, saved, strength, true, kDarkAmbient, kNoDirectLight, lightScale);
    return RenderLit(h, scene, saved, strength, true, kDarkAmbient, kNoDirectLight, lightScale);
}

NightFrames RenderWithAndWithoutSurfaceLights(Harness& h, Scene& scene, const Config& saved, float lightScale)
{
    return {RenderSettledAtNight(h, scene, saved, 0.0f, lightScale),
            RenderSettledAtNight(h, scene, saved, kFullStrength, lightScale)};
}

float LinearLight(unsigned char onDisplay)
{
    return std::pow(onDisplay / 255.0f, kDisplayGamma);
}

float LinearGreenLift(const NightFrames& frames, UINT x, UINT y)
{
    return LinearLight(frames.lit.image.At(x, y)[1]) - LinearLight(frames.unlit.image.At(x, y)[1]);
}

float BrightestLinearLift(const NightFrames& frames, UINT x, UINT y)
{
    const unsigned char* lit = frames.lit.image.At(x, y);
    const unsigned char* unlit = frames.unlit.image.At(x, y);
    float lift = 0.0f;
    for (int channel = 0; channel < 3; ++channel)
        lift = std::max(lift, LinearLight(lit[channel]) - LinearLight(unlit[channel]));
    return lift;
}

float RoomUnderAFullyLitSurface(const Image& unlit, UINT x, UINT y)
{
    const unsigned char* pixel = unlit.At(x, y);
    return kBrightestAssumedAlbedo - LinearLight(std::max({pixel[0], pixel[1], pixel[2]}));
}

void SaveWorldView(const std::wstring& outDir, const wchar_t* name, const LitFrame& frame, const D3DVIEWPORT9& world)
{
    Image view;
    view.w = world.Width;
    view.h = world.Height;
    view.bgra.resize(static_cast<size_t>(view.w) * view.h * 4);
    for (UINT y = 0; y < view.h; ++y)
        std::copy_n(frame.image.At(world.X, world.Y + y), static_cast<size_t>(view.w) * 4,
                    view.bgra.data() + static_cast<size_t>(y) * view.w * 4);
    SavePng(outDir + L"\\" + name, view.w, view.h, view.bgra);
}

void CheckLightsAddToTheSceneSoftly(Harness& h, const std::wstring& outDir)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    scene.world = kPictureView;
    SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    UINT darkX = 0;
    UINT darkY = 0;
    UINT patchX = 0;
    UINT patchY = 0;
    if (!ScenePixel(scene, kDarkGroundPoint, darkX, darkY) || !ScenePixel(scene, kLitPatchPoint, patchX, patchY))
    {
        Check(false, "the dark ground and the lit patch on either side of the light are in view");
        return;
    }
    Scene twoLights;
    twoLights.world = kPictureView;
    twoLights.lights = kTwoLights;
    const std::vector<SceneVertex> withoutThePatch = h.scene;
    AddBox(h.scene, kLitPatchLow, kLitPatchHigh, kLitPatchColour);
    const NightFrames lamp = RenderWithAndWithoutSurfaceLights(h, scene, saved, 1.0f);
    const NightFrames strong = RenderWithAndWithoutSurfaceLights(h, scene, saved, kStrongLight);
    const NightFrames twoStrong = RenderWithAndWithoutSurfaceLights(h, twoLights, saved, kStrongLight);
    const NightFrames overpowering = RenderWithAndWithoutSurfaceLights(h, scene, saved, kOverpoweringLight);
    h.scene = withoutThePatch;
    SaveWorldView(outDir, L"surface-lights-night-off.png", lamp.unlit, scene.world);
    SaveWorldView(outDir, L"surface-lights-night-lamp.png", lamp.lit, scene.world);
    SaveWorldView(outDir, L"surface-lights-night-strong.png", strong.lit, scene.world);
    SaveWorldView(outDir, L"surface-lights-night-two-strong.png", twoStrong.lit, scene.world);
    SaveWorldView(outDir, L"surface-lights-night-overpowering.png", overpowering.lit, scene.world);
    const float oneLift = LinearGreenLift(strong, darkX, darkY);
    const float twoLift = LinearGreenLift(twoStrong, darkX, darkY);
    const float groundLift = LinearGreenLift(lamp, darkX, darkY);
    const float patchLift = LinearGreenLift(lamp, patchX, patchY);
    const float overpoweredGround = BrightestLinearLift(overpowering, darkX, darkY);
    const float overpoweredPatch = BrightestLinearLift(overpowering, patchX, patchY);
    const float groundRoom = RoomUnderAFullyLitSurface(lamp.unlit.image, darkX, darkY);
    const float patchRoom = RoomUnderAFullyLitSurface(lamp.unlit.image, patchX, patchY);
    std::printf("     linear green lift of dark ground: %.4f under one light %.0f times a lamp, %.4f under two of "
                "them at one place (%.2f times); under a lamp %.4f on dark ground and %.4f on ground the client "
                "lit already, the same distance away (%.2f of the dark ground's); under a light %.0f times a lamp "
                "the brightest channel lifts by %.3f on the dark ground and %.3f on the lit patch, where a fully "
                "lit surface leaves room for %.3f and %.3f\n",
                oneLift, kStrongLight, twoLift, oneLift > 0.0f ? twoLift / oneLift : 0.0f, groundLift, patchLift,
                groundLift > 0.0f ? patchLift / groundLift : 0.0f, kOverpoweringLight, overpoweredGround,
                overpoweredPatch, groundRoom, patchRoom);
    Check(strong.lit.draws == 1 && twoStrong.lit.draws == 1 && oneLift > 0.0f && twoLift > oneLift &&
              twoLift < kLargestShareOfTwiceTheLight * 2.0f * oneLift,
          "two equal lights at one place light the ground more than one of them and clearly less than twice as "
          "much");
    Check(lamp.lit.draws == 1 && patchLift > 0.0f && patchLift < kLargestShareOnLitGround * groundLift,
          "ground the client lit already takes less of a lamp than the darker ground beside it, so the two "
          "lightings do not stack");
    Check(overpowering.lit.draws == 1 && overpoweredGround > BrightestLinearLift(strong, darkX, darkY) &&
              overpoweredGround <= groundRoom + kLinearLightTolerance &&
              overpoweredPatch <= patchRoom + kLinearLightTolerance,
          "a light many times too strong lifts the dark ground and the lit patch no further than a fully lit "
          "surface of the brightest albedo the pass assumes, short of white");
    vf_test_set_config(&saved);
}

void CheckSurfaceLightsFadeOutInDaylight(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    RenderLit(h, scene, saved, kFullStrength, true);
    const LitFrame dayFaint = RenderLit(h, scene, saved, kFullStrength, true, kBrightAmbient, kFullDirectLight,
                                        kFaintLight);
    const LitFrame dayStrong =
        RenderLit(h, scene, saved, kFullStrength, true, kBrightAmbient, kFullDirectLight, 1.0f);
    const LitFrame nightFaint =
        RenderLit(h, scene, saved, kFullStrength, true, kDarkAmbient, kNoDirectLight, kFaintLight);
    std::printf("     surface passes: faint light in daylight %u, strong light in daylight %u, faint light at night "
                "%u\n",
                dayFaint.draws, dayStrong.draws, nightFaint.draws);
    Check(dayFaint.draws == 0 && dayFaint.surfaceLightCopies == 0 &&
              dayFaint.colourCopies == dayFaint.compositeCopies,
          "a light too faint against the daylight to change a surface by more than a few per cent costs no surface "
          "pass and no copy of the scene for it");
    Check(dayStrong.draws == 1 && nightFaint.draws == 1,
          "a strong light in daylight and the same faint light at night are still drawn on surfaces");
    RenderLit(h, scene, saved, kFullStrength, false);
    vf_test_set_config(&saved);
}

struct AreaFrames
{
    LitFrame inArea;
    LitFrame wholeView;
};

AreaFrames RenderInAreaAndOverTheWholeView(Harness& h, Scene& scene, const Config& saved, float lightScale)
{
    AreaFrames frames;
    frames.inArea = RenderLit(h, scene, saved, kFullStrength, true, kDuskAmbient, kNoDirectLight, lightScale);
    vf_test_light_surfaces_over_the_whole_view(1);
    frames.wholeView = RenderLit(h, scene, saved, kFullStrength, true, kDuskAmbient, kNoDirectLight, lightScale);
    vf_test_light_surfaces_over_the_whole_view(0);
    return frames;
}

void CheckSurfaceLightsAreDrawnOnlyWhereTheyReach(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    SceneInputs(scene, SurfaceConfig(saved, kFullStrength));
    UINT x = 0;
    UINT y = 0;
    if (!GroundPixel(scene, kLightAhead, x, y))
    {
        Check(false, "the ground under the light is in view");
        return;
    }
    const LitFrame unlit = RenderLit(h, scene, saved, 0.0f, true, kDuskAmbient, kNoDirectLight, kSmallLight);
    const AreaFrames handed = RenderInAreaAndOverTheWholeView(h, scene, saved, kSmallLight);
    vf_test_keep_surface_lights_in_target(1);
    const AreaFrames kept = RenderInAreaAndOverTheWholeView(h, scene, saved, kSmallLight);
    vf_test_keep_surface_lights_in_target(0);
    const int lift = RedLift(handed.inArea.image, unlit.image, x, y);
    std::printf("     a light whose sphere the camera stands outside: its rectangle is %.1f%% of the view (%.1f%% "
                "with the lights kept in the target), red lift under it %d; colour copies for the lights and "
                "for the composite %u and %u in the rectangle, %u and %u over the whole view\n",
                100.0f * handed.inArea.share, 100.0f * kept.inArea.share, lift, handed.inArea.surfaceLightCopies,
                handed.inArea.compositeCopies, handed.wholeView.surfaceLightCopies, handed.wholeView.compositeCopies);
    Check(handed.inArea.draws == 1 && kept.inArea.draws == 1 && handed.inArea.share > 0.0f &&
              handed.inArea.share < kLargestShareLitInPlace && kept.inArea.share == handed.inArea.share &&
              handed.wholeView.share == 1.0f && lift >= kLitUnderASkyAtLeast,
          "a light whose sphere covers part of the view is drawn on surfaces inside the rectangle the sphere "
          "projects to, and still lights the ground under it");
    Check(image_quality_checks::SameImage(handed.inArea.image, handed.wholeView.image) &&
              image_quality_checks::SameImage(kept.inArea.image, kept.wholeView.image) &&
              image_quality_checks::SameImage(handed.inArea.image, kept.inArea.image),
          "lighting only the rectangle draws the frame of the whole-view pass pixel for pixel, into the "
          "composite's scene copy and into the target");
    Check(handed.inArea.surfaceLightCopies == 1 && handed.inArea.compositeCopies == 1 &&
              handed.wholeView.surfaceLightCopies == 0 && handed.wholeView.compositeCopies == 1 &&
              kept.inArea.surfaceLightCopies == 1,
          "the rectangle is copied once for the lights beside the composite's own copy of the viewport");
    Check(handed.inArea.statesKept && kept.inArea.statesKept,
          "lighting a rectangle restores the client's state, in the composite's scene copy and in the target");
    Scene behind;
    behind.lightAhead = kLightBehind;
    const LitFrame behindOff = RenderLit(h, behind, saved, 0.0f, true);
    const LitFrame behindLit = RenderLit(h, behind, saved, kFullStrength, true);
    vf_test_light_surfaces_over_the_whole_view(1);
    const LitFrame behindOverTheWholeView = RenderLit(h, behind, saved, kFullStrength, true);
    vf_test_light_surfaces_over_the_whole_view(0);
    std::printf("     a light %.0f yd behind the camera: surface passes %u, %u when the whole view is lit "
                "regardless\n",
                -kLightBehind, behindLit.draws, behindOverTheWholeView.draws);
    Check(behindLit.draws == 0 && behindLit.surfaceLightCopies == 0 && behindOverTheWholeView.draws == 1 &&
              image_quality_checks::SameImage(behindLit.image, behindOverTheWholeView.image) &&
              image_quality_checks::SameImage(behindLit.image, behindOff.image),
          "a light whose sphere lies behind the camera costs no surface pass and no copy, and the frame is the "
          "one the whole-view pass draws");
    vf_test_set_config(&saved);
}

void CheckGodRaysTakeTheirSceneFromTheCompositeCopy(Harness& h)
{
    Config saved;
    vf_test_get_config(&saved);
    Scene scene;
    SceneInputs(scene, SurfaceConfig(saved, 0.0f));
    constexpr float kRays = 1.5f;
    const LitFrame noRays = RenderLit(h, scene, saved, 0.0f, false);
    RenderLit(h, scene, saved, 0.0f, false, 0, 0, 1.0f, kRays);
    RenderLit(h, scene, saved, 0.0f, false, 0, 0, 1.0f, kRays);
    const LitFrame fromCopy = RenderLit(h, scene, saved, 0.0f, false, 0, 0, 1.0f, kRays);
    vf_test_take_god_rays_from_target(1);
    const LitFrame fromTarget = RenderLit(h, scene, saved, 0.0f, false, 0, 0, 1.0f, kRays);
    vf_test_take_god_rays_from_target(0);
    std::printf("     god rays: low-resolution copies %u from the scene copy, %u from the target; colour copies %u "
                "against %u\n",
                fromCopy.godRayCopies, fromTarget.godRayCopies, fromCopy.colourCopies, fromTarget.colourCopies);
    Check(fromCopy.godRayCopies == 1 && fromTarget.godRayCopies == 1 &&
              !image_quality_checks::SameImage(noRays.image, fromCopy.image),
          "the surface light scene draws god rays with their low-resolution scene copy");
    Check(image_quality_checks::SameImage(fromCopy.image, fromTarget.image),
          "taking the god rays' low-resolution scene from the composite's scene copy instead of the target draws the "
          "same frame");
    vf_test_set_config(&saved);
}

void CheckShippedSetting(const std::wstring& shippedIni)
{
    const Config defaults = {};
    wchar_t text[16] = {};
    GetPrivateProfileStringW(L"CoAGraphics", L"SurfaceLights", L"", text, 16, shippedIni.c_str());
    Check(defaults.surfaceLights == 1.0f && std::wcscmp(text, L"1.0") == 0,
          "lights on surfaces are on at 1 by default and in the shipped INI");
    GetPrivateProfileStringW(L"CoAGraphics", L"SpellSurfaceLights", L"", text, 16, shippedIni.c_str());
    Check(defaults.spellSurfaceLights == 0.5f && std::wcscmp(text, L"0.5") == 0,
          "spell lights on surfaces are on at 0.5 by default and in the shipped INI");
}
}
