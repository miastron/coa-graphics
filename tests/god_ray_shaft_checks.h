#pragma once

namespace god_ray_shafts
{
constexpr D3DVIEWPORT9 kWorld = {0, 0, 1280, 688, 0, 1};
constexpr float kTrunkDistance = 30.0f;
constexpr float kTrunkWidth = 12.0f;
constexpr float kTrunkPeriod = 56.0f;
constexpr float kTrunkSpan = 360.0f;
constexpr DWORD kTrunkColour = 0xFF3A3028;
constexpr float kCanopyHalfWidth = 320.0f;
constexpr float kCanopyAbove = 230.0f;
constexpr float kCanopyBelow = 120.0f;
constexpr float kLeafCell = 18.0f;
constexpr DWORD kLeafColour = 0xFF2E4A26;
constexpr float kSkyRawDepth = 1.0f;
constexpr float kSkyBandHeight = 4.0f;
constexpr DWORD kDuskZenith = 0xFF405A80;
constexpr DWORD kDuskHorizon = 0xFFF0C090;
constexpr float kArcRadius = 140.0f;
constexpr int kArcSamples = 181;
constexpr float kOpenSkyMeanLimit = 0.5f / 255.0f;
constexpr int kOpenSkyLargestLimit = 3;
constexpr float kMinShaftContrast = 8.0f / 255.0f;
constexpr float kMaxShaftDarkening = 1.0f / 255.0f;
constexpr double kMinLeafShafts = 3.0 / 255.0;
constexpr float kStrengths[] = {0.0f, 1.0f, 4.0f};
constexpr float kCharacterDistance = 8.0f;
constexpr float kZoomedInCharacterDistance = 2.0f;
constexpr float kCharacterFeetBelowTheEye = 1.0f;
constexpr float kCharacterBesideTheSun = 2.5f;
constexpr DWORD kCharacterColour = 0xFF6A5A48;
constexpr int kMaxRaysOverTheCharacter = 1;
constexpr float kNearTrunksBesideTheCharacter = 4.0f;
constexpr int kNearTrunksPerSide = 5;
constexpr float kNearShaftRowBelowTheSun = 100.0f;
constexpr float kMinNearShaftShare = 0.75f;

struct CharacterPart
{
    float left;
    float right;
    float bottom;
    float top;
};

constexpr CharacterPart kCharacterParts[] = {
    {-0.45f, -0.08f, 0.0f, 0.85f}, {0.08f, 0.45f, 0.0f, 0.85f}, {-0.45f, 0.45f, 0.85f, 1.45f},
    {-0.75f, 0.75f, 1.45f, 1.7f},  {-0.2f, 0.2f, 1.7f, 2.1f},   {0.8f, 0.9f, 0.6f, 2.6f},
};
constexpr size_t kCharacterPartCount = sizeof(kCharacterParts) / sizeof(kCharacterParts[0]);

struct ScreenBox
{
    float x0 = 0;
    float y0 = 0;
    float x1 = 0;
    float y1 = 0;
};

struct SunView
{
    Vec3 eye = Add({0, 0, 9}, kGameLikeWorldOffset);
    float view[16] = {};
    float proj[16] = {};
    FrameInputs in = {};
    float sunX = 0;
    float sunY = 0;
    float horizonY = 0;

    SunView()
    {
        const Vec3 sun = Norm({-1.0f, 0.15f, 0.3f});
        const Vec3 at = Add(eye, {sun.x * 100.0f, sun.y * 100.0f, sun.z * 100.0f - 14.0f});
        CameraRelativeLookAt(eye, at, view);
        EngineProjection(static_cast<float>(kWorld.Width) / kWorld.Height, proj);
        in = MakeInputs(view, proj, eye, at, kWorld);
        in.toLight[0] = sun.x;
        in.toLight[1] = sun.y;
        in.toLight[2] = sun.z;
        float sunView[3];
        TransformDirection(in.toLight, view, sunView);
        sunX = kWorld.X + (sunView[0] / sunView[2] * proj[0] * 0.5f + 0.5f) * kWorld.Width;
        sunY = kWorld.Y + (0.5f - sunView[1] / sunView[2] * proj[5] * 0.5f) * kWorld.Height;
        horizonY = (0.5f + view[10] / view[9] * proj[5] * 0.5f) * kWorld.Height;
    }
};

struct Character
{
    Vec3 feet = {};
    float rawDepth = 0;
    ScreenBox parts[kCharacterPartCount] = {};

    bool Covers(float x, float y) const
    {
        for (const ScreenBox& part : parts)
            if (x >= part.x0 && x < part.x1 && y >= part.y0 && y < part.y1)
                return true;
        return false;
    }
};

LightOnScreen OnTheWorldViewport(const SunView& v, Vec3 world)
{
    const float fromEye[3] = {world.x - v.eye.x, world.y - v.eye.y, world.z - v.eye.z};
    float inView[3];
    TransformDirection(fromEye, v.view, inView);
    return ProjectLightOntoViewport(inView, v.proj, kWorld);
}

Vec3 AheadToTheSun(const SunView& v)
{
    return Norm({v.in.toLight[0], v.in.toLight[1], 0.0f});
}

Vec3 RightOfTheSun(const SunView& v)
{
    return Norm(Cross(AheadToTheSun(v), {0.0f, 0.0f, 1.0f}));
}

Character CharacterBelowTheSun(const SunView& v, float besideTheSun, float distance = kCharacterDistance)
{
    const Vec3 ahead = AheadToTheSun(v);
    const Vec3 right = RightOfTheSun(v);
    Character character;
    character.feet = Add(Add(v.eye, {ahead.x * distance, ahead.y * distance, -kCharacterFeetBelowTheEye}),
                         {right.x * besideTheSun, right.y * besideTheSun, 0.0f});
    character.rawDepth = RawDepthAtViewDistance(v.proj, OnTheWorldViewport(v, character.feet).viewZ);
    for (size_t i = 0; i < kCharacterPartCount; ++i)
    {
        const CharacterPart& part = kCharacterParts[i];
        const LightOnScreen lowLeft = OnTheWorldViewport(
            v, Add(character.feet, {right.x * part.left, right.y * part.left, part.bottom}));
        const LightOnScreen highRight = OnTheWorldViewport(
            v, Add(character.feet, {right.x * part.right, right.y * part.right, part.top}));
        character.parts[i] = {std::round((std::min)(lowLeft.pixel[0], highRight.pixel[0])),
                              std::round((std::min)(lowLeft.pixel[1], highRight.pixel[1])),
                              std::round((std::max)(lowLeft.pixel[0], highRight.pixel[0])),
                              std::round((std::max)(lowLeft.pixel[1], highRight.pixel[1]))};
    }
    return character;
}

void DrawCharacter(Harness& h, const Character& character)
{
    for (const ScreenBox& part : character.parts)
        h.DrawPretransformedQuadAtRawDepth(part.x0, part.y0, part.x1, part.y1, character.rawDepth, kCharacterColour);
}

enum class Occluders
{
    None,
    Trunks,
    Canopy,
    TrunksBesideTheCharacter,
};

enum class Sky
{
    Clear,
    BrighterTowardTheHorizon,
};

bool LeafAt(int column, int row)
{
    uint32_t hash = static_cast<uint32_t>(column) * 73856093u ^ static_cast<uint32_t>(row) * 19349663u;
    hash ^= hash >> 13;
    hash *= 0x5bd1e995u;
    hash ^= hash >> 15;
    return (hash & 0xFF) < 150;
}

void DrawTrunks(Harness& h, float sunX, float bottom, float rawDepth)
{
    for (float x = sunX - kTrunkSpan; x < sunX + kTrunkSpan; x += kTrunkPeriod)
        h.DrawPretransformedQuadAtRawDepth(x, 0.0f, x + kTrunkWidth, bottom, rawDepth, kTrunkColour);
}

struct NearTrunks
{
    float feetX = 0;
    float firstOffset = 0;
    float lastOffset = 0;
    float rawDepth = 0;

    explicit NearTrunks(const SunView& v)
    {
        const Character character = CharacterBelowTheSun(v, 0.0f);
        const Vec3 right = RightOfTheSun(v);
        const Vec3 beside = Add(character.feet, {right.x * kNearTrunksBesideTheCharacter,
                                                 right.y * kNearTrunksBesideTheCharacter, 0.0f});
        feetX = OnTheWorldViewport(v, character.feet).pixel[0];
        firstOffset = std::fabs(OnTheWorldViewport(v, beside).pixel[0] - feetX);
        lastOffset = firstOffset + (kNearTrunksPerSide - 1) * kTrunkPeriod + kTrunkWidth;
        rawDepth = character.rawDepth;
    }
};

void DrawTrunksBesideTheCharacter(Harness& h, const SunView& v)
{
    const NearTrunks trunks(v);
    for (int i = 0; i < kNearTrunksPerSide; ++i)
    {
        const float offset = trunks.firstOffset + i * kTrunkPeriod;
        for (float x : {trunks.feetX + offset, trunks.feetX - offset - kTrunkWidth})
            h.DrawPretransformedQuadAtRawDepth(x, 0.0f, x + kTrunkWidth, v.horizonY, trunks.rawDepth, kTrunkColour);
    }
}

void DrawOccluders(Harness& h, const SunView& v, Occluders occluders, float rawDepth)
{
    if (occluders == Occluders::TrunksBesideTheCharacter)
        DrawTrunksBesideTheCharacter(h, v);
    if (occluders == Occluders::Trunks)
        DrawTrunks(h, v.sunX, v.horizonY, rawDepth);
    if (occluders == Occluders::Canopy)
        for (int row = 0; row * kLeafCell < kCanopyAbove + kCanopyBelow; ++row)
            for (int column = 0; column * kLeafCell < 2 * kCanopyHalfWidth; ++column)
                if (LeafAt(column, row))
                {
                    const float x = v.sunX - kCanopyHalfWidth + column * kLeafCell;
                    const float y = v.sunY - kCanopyAbove + row * kLeafCell;
                    h.DrawPretransformedQuadAtRawDepth(x, y, x + kLeafCell * 1.3f, y + kLeafCell * 1.1f, rawDepth,
                                                       kLeafColour);
                }
}

DWORD DuskSkyAt(float towardTheHorizon)
{
    DWORD colour = 0xFF000000;
    for (int shift = 0; shift < 24; shift += 8)
    {
        const float zenith = static_cast<float>((kDuskZenith >> shift) & 0xFF);
        const float horizon = static_cast<float>((kDuskHorizon >> shift) & 0xFF);
        colour |= static_cast<DWORD>(std::lround(zenith + (horizon - zenith) * towardTheHorizon)) << shift;
    }
    return colour;
}

void DrawSky(Harness& h, const SunView& v, Sky sky)
{
    if (sky == Sky::Clear)
        return;
    DWORD depthTest = 0;
    h.dev->GetRenderState(D3DRS_ZFUNC, &depthTest);
    h.dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
    for (float y = 0; y < v.horizonY; y += kSkyBandHeight)
        h.DrawPretransformedQuadAtRawDepth(0.0f, y, static_cast<float>(kWorld.Width), y + kSkyBandHeight,
                                           kSkyRawDepth, DuskSkyAt((y + 0.5f * kSkyBandHeight) / v.horizonY));
    h.dev->SetRenderState(D3DRS_ZFUNC, depthTest);
}

Image Render(Harness& h, const SunView& v, float godRays, Occluders occluders, Sky sky = Sky::Clear,
             const Character* character = nullptr)
{
    Config cfg;
    cfg.temporal = 0.0f;
    cfg.noiseAmount = 0.0f;
    cfg.godRays = godRays;
    vf_test_set_config(&cfg);
    const float trunkDepth = RawDepthAtViewDistance(v.proj, kTrunkDistance);
    Image image;
    for (int frame = 0; frame < 2; ++frame)
    {
        h.BeginFrame();
        h.DrawScene(v.eye, v.view, v.proj, kWorld);
        DrawSky(h, v, sky);
        if (character)
            DrawCharacter(h, *character);
        DrawOccluders(h, v, occluders, trunkDepth);
        FrameInputs in = v.in;
        if (character)
        {
            in.cameraFocus[0] = character->feet.x;
            in.cameraFocus[1] = character->feet.y;
            in.cameraFocus[2] = character->feet.z;
        }
        const char* skip = "";
        vf_test_render(&in, &skip);
        if (frame == 1)
            image = Capture(h.dev);
        h.dev->EndScene();
        h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    }
    return image;
}

struct Change
{
    double mean = 0;
    int largest = 0;
    int darkest = 0;
};

Change Compare(const Image& before, const Image& after)
{
    Change change;
    for (UINT y = 0; y < kWorld.Height; ++y)
        for (UINT x = 0; x < kWorld.Width; ++x)
            for (int channel = 0; channel < 3; ++channel)
            {
                const int difference = after.At(x, y)[channel] - before.At(x, y)[channel];
                change.mean += std::abs(difference);
                change.largest = (std::max)(change.largest, std::abs(difference));
                change.darkest = (std::min)(change.darkest, difference);
            }
    change.mean /= 255.0 * 3.0 * kWorld.Width * kWorld.Height;
    return change;
}

int LargestBrighteningOn(const Character& character, const Image& before, const Image& after)
{
    int largest = 0;
    for (UINT y = 0; y < kWorld.Height; ++y)
        for (UINT x = 0; x < kWorld.Width; ++x)
            if (character.Covers(x + 0.5f, y + 0.5f))
                for (int channel = 0; channel < 3; ++channel)
                    largest = (std::max)(largest, after.At(x, y)[channel] - before.At(x, y)[channel]);
    return largest;
}

struct ArcContrast
{
    float brightest = 0;
    float dimmest = 0;
};

ArcContrast AlongTheArc(const SunView& v, const Image& before, const Image& after)
{
    ArcContrast arc = {-1, 1};
    for (int i = 0; i < kArcSamples; ++i)
    {
        const float angle = 3.14159265f * i / (kArcSamples - 1);
        const float x = v.sunX + kArcRadius * std::cos(angle);
        const float y = v.sunY + kArcRadius * std::sin(angle);
        if (x < 0 || y < 0 || x >= kWorld.Width || y >= v.horizonY - 2)
            continue;
        const float change = after.Luma(static_cast<UINT>(x), static_cast<UINT>(y)) -
                             before.Luma(static_cast<UINT>(x), static_cast<UINT>(y));
        arc.brightest = (std::max)(arc.brightest, change);
        arc.dimmest = (std::min)(arc.dimmest, change);
    }
    return arc;
}

ArcContrast AcrossTheNearTrunks(const SunView& v, const Image& before, const Image& after)
{
    const NearTrunks trunks(v);
    const UINT y = static_cast<UINT>(v.sunY + kNearShaftRowBelowTheSun);
    ArcContrast row = {-1, 1};
    for (float offset = trunks.firstOffset; offset < trunks.lastOffset; offset += 1.0f)
        for (float x : {trunks.feetX + offset, trunks.feetX - offset})
        {
            if (x < 0 || x >= kWorld.Width)
                continue;
            const float change = after.Luma(static_cast<UINT>(x), y) - before.Luma(static_cast<UINT>(x), y);
            row.brightest = (std::max)(row.brightest, change);
            row.dimmest = (std::min)(row.dimmest, change);
        }
    return row;
}

void CheckGodRaysAddOnlyShafts(Harness& h, const std::wstring& outDir)
{
    Config saved;
    vf_test_get_config(&saved);
    const SunView v;
    Image open[3];
    Image dusk[3];
    Image trunks[3];
    Image canopy[3];
    for (int i = 0; i < 3; ++i)
    {
        open[i] = Render(h, v, kStrengths[i], Occluders::None);
        dusk[i] = Render(h, v, kStrengths[i], Occluders::None, Sky::BrighterTowardTheHorizon);
        trunks[i] = Render(h, v, kStrengths[i], Occluders::Trunks);
        canopy[i] = Render(h, v, kStrengths[i], Occluders::Canopy);
        const std::pair<const wchar_t*, const Image*> named[] = {
            {L"open-sky", &open[i]}, {L"dusk-sky", &dusk[i]}, {L"trunks", &trunks[i]}, {L"canopy", &canopy[i]}};
        for (const auto& [scene, image] : named)
        {
            wchar_t name[64];
            swprintf(name, 64, L"\\god-rays-%ls-%.0f.png", scene, kStrengths[i]);
            SavePng(outDir + name, image->w, image->h, image->bgra);
        }
    }
    vf_test_set_config(&saved);
    const Change openSky = Compare(open[0], open[2]);
    const Change duskSky = Compare(dusk[0], dusk[2]);
    const Change behindTrunks = Compare(trunks[0], trunks[2]);
    const Change throughLeavesFaint = Compare(canopy[0], canopy[1]);
    const Change throughLeaves = Compare(canopy[0], canopy[2]);
    const ArcContrast faint = AlongTheArc(v, trunks[0], trunks[1]);
    const ArcContrast strong = AlongTheArc(v, trunks[0], trunks[2]);
    std::printf("     god rays 4 with the sun at (%.0f, %.0f) above the horizon row %.0f: open sky changes by %.2f/255 "
                "on average, at most %d/255; a dusk sky brightening toward the horizon %.2f/255, at most %d/255; "
                "behind trunks %.2f/255 on average, darkest %d/255; across the shafts %.3f to %.3f at GodRays 1, "
                "%.3f to %.3f at 4; through leaves %.2f/255 at 1, %.2f/255 at 4, darkest %d/255\n",
                v.sunX, v.sunY, v.horizonY, openSky.mean * 255.0, openSky.largest, duskSky.mean * 255.0,
                duskSky.largest, behindTrunks.mean * 255.0,
                behindTrunks.darkest, faint.dimmest, faint.brightest, strong.dimmest, strong.brightest,
                throughLeavesFaint.mean * 255.0, throughLeaves.mean * 255.0, throughLeaves.darkest);
    Check(v.sunY > 0 && v.sunY < v.horizonY && openSky.mean <= kOpenSkyMeanLimit &&
              openSky.largest <= kOpenSkyLargestLimit && duskSky.mean <= kOpenSkyMeanLimit &&
              duskSky.largest <= kOpenSkyLargestLimit,
          "god rays add nothing to an open sky with nothing in front of the sun, a clear one or a dusk sky "
          "brightening toward the skyline below a low sun, so they do not brighten the sun's glow like Sun scatter");
    Check(strong.brightest - strong.dimmest >= kMinShaftContrast &&
              strong.brightest - strong.dimmest > faint.brightest - faint.dimmest &&
              strong.dimmest >= -kMaxShaftDarkening && behindTrunks.darkest >= -1 &&
              throughLeaves.mean >= kMinLeafShafts && throughLeaves.mean > throughLeavesFaint.mean &&
              throughLeaves.darkest >= -1,
          "god rays add shafts of light between trunks and through leaves in front of the sun, stronger as God rays "
          "rises, and darken nothing");
}

void CheckTheOwnCharacterCastsNoShafts(Harness& h, const std::wstring& outDir)
{
    Config saved;
    vf_test_get_config(&saved);
    const SunView v;
    const Character below = CharacterBelowTheSun(v, 0.0f);
    const Character beside = CharacterBelowTheSun(v, kCharacterBesideTheSun);
    const Image belowTheSun[2] = {Render(h, v, kStrengths[0], Occluders::None, Sky::Clear, &below),
                                  Render(h, v, kStrengths[2], Occluders::None, Sky::Clear, &below)};
    const Image besideTheSun[2] = {Render(h, v, kStrengths[0], Occluders::None, Sky::Clear, &beside),
                                   Render(h, v, kStrengths[2], Occluders::None, Sky::Clear, &beside)};
    const Image withTrunks[2] = {Render(h, v, kStrengths[0], Occluders::Trunks, Sky::Clear, &below),
                                 Render(h, v, kStrengths[2], Occluders::Trunks, Sky::Clear, &below)};
    const Character close = CharacterBelowTheSun(v, 0.0f, kZoomedInCharacterDistance);
    const Image zoomedIn[2] = {Render(h, v, kStrengths[0], Occluders::None, Sky::Clear, &close),
                               Render(h, v, kStrengths[2], Occluders::None, Sky::Clear, &close)};
    vf_test_set_config(&saved);
    SavePng(outDir + L"\\god-rays-character-below-4.png", belowTheSun[1].w, belowTheSun[1].h, belowTheSun[1].bgra);
    SavePng(outDir + L"\\god-rays-character-beside-4.png", besideTheSun[1].w, besideTheSun[1].h,
            besideTheSun[1].bgra);
    SavePng(outDir + L"\\god-rays-character-trunks-4.png", withTrunks[1].w, withTrunks[1].h, withTrunks[1].bgra);
    SavePng(outDir + L"\\god-rays-character-zoomed-in-4.png", zoomedIn[1].w, zoomedIn[1].h, zoomedIn[1].bgra);
    const Change aroundBelow = Compare(belowTheSun[0], belowTheSun[1]);
    const Change aroundBeside = Compare(besideTheSun[0], besideTheSun[1]);
    const Change aroundClose = Compare(zoomedIn[0], zoomedIn[1]);
    const ArcContrast betweenTheTrunks = AlongTheArc(v, withTrunks[0], withTrunks[1]);
    const int overTheCharacter = LargestBrighteningOn(below, withTrunks[0], withTrunks[1]);
    std::printf("     god rays 4 with the own character %.0f yd away, feet %.0f yd below the eye: just below the sun "
                "the open sky changes by %.2f/255 on average, at most %d/255; %.1f yd beside it %.2f/255, at most "
                "%d/255; with trunks behind it %.3f to %.3f across their shafts, the character brightened by at "
                "most %d/255; zoomed in to %.0f yd %.2f/255, at most %d/255\n",
                kCharacterDistance, kCharacterFeetBelowTheEye, aroundBelow.mean * 255.0, aroundBelow.largest,
                kCharacterBesideTheSun, aroundBeside.mean * 255.0, aroundBeside.largest, betweenTheTrunks.dimmest,
                betweenTheTrunks.brightest, overTheCharacter, kZoomedInCharacterDistance, aroundClose.mean * 255.0,
                aroundClose.largest);
    Check(aroundBelow.mean <= kOpenSkyMeanLimit && aroundBelow.largest <= kOpenSkyLargestLimit &&
              aroundBeside.mean <= kOpenSkyMeanLimit && aroundBeside.largest <= kOpenSkyLargestLimit &&
              betweenTheTrunks.brightest - betweenTheTrunks.dimmest >= kMinShaftContrast &&
              overTheCharacter <= kMaxRaysOverTheCharacter && aroundClose.mean <= kOpenSkyMeanLimit &&
              aroundClose.largest <= kOpenSkyLargestLimit,
          "the character the camera follows casts no god-ray shafts of its own, just below the sun, beside it over "
          "the ground or with the camera zoomed in closer than 3 yd, and the shafts of trunks behind it are not "
          "drawn over it");
}

void CheckTrunksBesideTheOwnCharacterKeepTheirShafts(Harness& h, const std::wstring& outDir)
{
    Config saved;
    vf_test_get_config(&saved);
    const SunView v;
    const Character below = CharacterBelowTheSun(v, 0.0f);
    const Image withTheCharacter[2] = {
        Render(h, v, kStrengths[0], Occluders::TrunksBesideTheCharacter, Sky::Clear, &below),
        Render(h, v, kStrengths[2], Occluders::TrunksBesideTheCharacter, Sky::Clear, &below)};
    const Image withoutTheCharacter[2] = {Render(h, v, kStrengths[0], Occluders::TrunksBesideTheCharacter),
                                          Render(h, v, kStrengths[2], Occluders::TrunksBesideTheCharacter)};
    vf_test_set_config(&saved);
    SavePng(outDir + L"\\god-rays-character-near-trunks-4.png", withTheCharacter[1].w, withTheCharacter[1].h,
            withTheCharacter[1].bgra);
    const ArcContrast with = AcrossTheNearTrunks(v, withTheCharacter[0], withTheCharacter[1]);
    const ArcContrast without = AcrossTheNearTrunks(v, withoutTheCharacter[0], withoutTheCharacter[1]);
    const float withContrast = with.brightest - with.dimmest;
    const float withoutContrast = without.brightest - without.dimmest;
    std::printf("     god rays 4 with trunks %.0f yd beside the own character at its distance: %.3f to %.3f across "
                "their shafts, %.3f to %.3f without the character\n",
                kNearTrunksBesideTheCharacter, with.dimmest, with.brightest, without.dimmest, without.brightest);
    Check(withContrast >= kMinShaftContrast && withContrast >= kMinNearShaftShare * withoutContrast &&
              with.dimmest >= -kMaxShaftDarkening,
          "trunks a few yards beside the character the camera follows, at its distance, keep their god-ray shafts");
}
}
