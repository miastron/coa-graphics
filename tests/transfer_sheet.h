#pragma once

namespace transfer_sheet
{
constexpr float kSheetFarClip = 1557.3f;
constexpr float kShortestFogReplacedByFarClip = 27.777779f;
constexpr int kFirstMapWithFarClipFog = 530;
constexpr UINT kTileWidth = 800;
constexpr UINT kTileHeight = 450;
constexpr float kEyeBehindCharacter = 10.0f;
constexpr float kEyeAboveGround = 4.0f;
constexpr float kCharacterChestHeight = 1.5f;
constexpr float kNearYards = 150.0f;
constexpr float kFarYards = 1000.0f;
constexpr float kAmbientYards = 400.0f;
constexpr float kNearlyOpaque = 0.999999f;
constexpr float kNumberMovedByAtLeast = 0.005f;
constexpr float kLightMovedByAtLeast = 0.03f;
constexpr float kSmallestVisibleStep = 0.5f;
constexpr float kClearlyVisibleSteps = 2.0f;
constexpr float kSameEffectWithin = 0.01f;
constexpr float kSamePowerWithin = 0.02f;
constexpr int kFewestCasesForARule = 4;
constexpr float kSmallestMeasure = 1e-9f;
constexpr int kSettleFrames = 5;
constexpr int kProbeRadius = 2;
constexpr int kBoardRadius = 8;
constexpr float kDarkestBaseStep = 1.0f;

constexpr float kGrassAlbedo[3] = {0.10f, 0.17f, 0.06f};
constexpr float kSkinAlbedo[3] = {0.30f, 0.22f, 0.18f};
constexpr float kPostAlbedo[3] = {0.05f, 0.05f, 0.05f};
constexpr float kBarkAlbedo[3] = {0.09f, 0.06f, 0.04f};
constexpr float kLeafAlbedo[3] = {0.05f, 0.12f, 0.03f};
constexpr float kBoardAlbedo[3] = {0.02f, 0.02f, 0.02f};
constexpr float kGroundHalfSize = 4000.0f;
constexpr Vec3 kCharacterLow = {-0.4f, -0.4f, 0.0f};
constexpr Vec3 kCharacterHigh = {0.4f, 0.4f, 2.0f};
constexpr Vec3 kLampLight = {8.0f, 6.0f, 4.6f};
constexpr Vec3 kLampPostLow = {7.85f, 6.45f, 0.0f};
constexpr Vec3 kLampPostHigh = {8.15f, 6.75f, 5.0f};
constexpr Vec3 kAirAboveTheLamp = {8.0f, 6.0f, 6.1f};
constexpr Vec3 kSpellLight = {2.0f, -5.0f, 1.0f};
constexpr Vec3 kAirAboveTheSpell = {2.0f, -5.0f, 2.5f};
constexpr float kSpellColour[3] = {0.7f, 0.3f, 1.0f};
constexpr float kSpellReach = 8.0f;
constexpr float kGroundDistances[] = {1.0f, 3.0f, 6.0f};
constexpr int kGroundDistanceCount = 3;
constexpr int kMatrixGroundDistance = 1;
constexpr float kTreeLineAhead = 45.0f;
constexpr float kFirstTreeAside = 30.0f;
constexpr float kTreeSpacing = 7.0f;
constexpr int kTrees = 3;
constexpr Vec3 kNearBoardLow = {150.0f, -38.0f, 0.0f};
constexpr Vec3 kNearBoardHigh = {152.0f, -10.0f, 18.0f};
constexpr Vec3 kNearBoardPoint = {150.0f, -24.0f, 9.0f};
constexpr Vec3 kFarBoardLow = {400.0f, 10.0f, 0.0f};
constexpr Vec3 kFarBoardHigh = {404.0f, 72.0f, 45.0f};
constexpr Vec3 kFarBoardPoint = {400.0f, 41.0f, 22.0f};
constexpr Vec3 kSkyAboveTheHorizon = {1000.0f, -600.0f, 60.0f};

struct Context
{
    std::string place;
    std::string hour;
    std::string weather;
    int map = 0;
    Vec3 ground = {};
    float day = 0.0f;
    float storm = 0.0f;
    bool moon = false;
    Vec3 toLight = {};
    uint32_t direct = 0;
    uint32_t ambient = 0;
    uint32_t fog = 0;
    uint32_t sun = 0;
    float fogStartShare = 0.0f;
    float zoneFogYards = 0.0f;
};

struct Report
{
    std::string text;

    void Add(const char* format, ...)
    {
        char line[4096];
        va_list arguments;
        va_start(arguments, format);
        std::vsnprintf(line, sizeof(line), format, arguments);
        va_end(arguments);
        text += line;
    }
};

std::vector<std::string> Fields(const std::string& line)
{
    std::vector<std::string> fields;
    std::stringstream stream(line);
    for (std::string field; std::getline(stream, field, ',');)
        fields.push_back(field);
    return fields;
}

uint32_t OpaqueColour(const std::string& hex)
{
    return 0xFF000000u | static_cast<uint32_t>(std::strtoul(hex.c_str(), nullptr, 16));
}

bool LoadContexts(const std::string& path, std::vector<Context>& contexts)
{
    std::ifstream file(path);
    std::string line;
    if (!file || !std::getline(file, line))
        return false;
    const std::vector<std::string> columns = Fields(line);
    while (std::getline(file, line))
    {
        const std::vector<std::string> fields = Fields(line);
        if (fields.size() != columns.size())
            return false;
        const auto text = [&](const char* column) {
            const auto found = std::find(columns.begin(), columns.end(), column);
            return found != columns.end() ? fields[found - columns.begin()] : std::string();
        };
        const auto number = [&](const char* column) { return static_cast<float>(std::atof(text(column).c_str())); };
        Context context;
        context.place = text("place");
        context.hour = text("hour");
        context.weather = text("weather");
        context.map = std::atoi(text("map").c_str());
        context.ground = {number("x"), number("y"), number("z")};
        context.day = number("day");
        context.storm = number("storm");
        context.moon = text("moon") == "1";
        context.toLight = {number("to_light_x"), number("to_light_y"), number("to_light_z")};
        context.direct = OpaqueColour(text("direct"));
        context.ambient = OpaqueColour(text("ambient"));
        context.fog = OpaqueColour(text("fog"));
        context.sun = OpaqueColour(text("sun"));
        context.fogStartShare = number("fog_start_share");
        context.zoneFogYards = number("zone_fog_yards");
        contexts.push_back(context);
    }
    return !contexts.empty();
}

std::string NameOf(const Context& context)
{
    return context.place + " " + context.hour + " " + context.weather;
}

struct Camera
{
    Vec3 eye = {};
    Vec3 at = {};
    float view[16] = {};
    float projection[16] = {};
    D3DVIEWPORT9 viewport = {0, 0, kTileWidth, kTileHeight, 0.0f, kClientWorldMaxZ};
};

Camera CameraBehindTheCharacter(const Context& context)
{
    Camera camera;
    camera.eye = Add(context.ground, {-kEyeBehindCharacter, 0, kEyeAboveGround});
    camera.at = Add(context.ground, {0, 0, kCharacterChestHeight});
    CameraRelativeLookAt(camera.eye, camera.at, camera.view);
    EngineGlDepthProjection(1.0f / std::tan(kFovY * 0.5f),
                            static_cast<float>(camera.viewport.Width) / camera.viewport.Height, kNear, kSheetFarClip,
                            camera.projection);
    return camera;
}

float ClientFogEnd(const Context& context)
{
    const bool farClipFog =
        context.map >= kFirstMapWithFarClipFog && context.zoneFogYards >= kShortestFogReplacedByFarClip;
    return farClipFog ? kSheetFarClip : std::min(context.zoneFogYards, kSheetFarClip);
}

FrameInputs ContextInputs(const Context& context, const Camera& camera)
{
    FrameInputs in = MakeInputs(camera.view, camera.projection, camera.eye, camera.at, camera.viewport);
    for (float* point : {in.camTarget, in.cameraFocus})
    {
        point[0] = camera.at.x;
        point[1] = camera.at.y;
        point[2] = camera.at.z;
    }
    in.mapId = context.map;
    in.dayFraction = context.day;
    in.toLight[0] = context.toLight.x;
    in.toLight[1] = context.toLight.y;
    in.toLight[2] = context.toLight.z;
    in.lightIsMoon = context.moon;
    in.fogColor = context.fog;
    in.sunColor = context.sun;
    in.directColor = context.direct;
    in.ambientColor = context.ambient;
    in.farClip = kSheetFarClip;
    in.zoneFogDistance = context.zoneFogYards;
    in.fogEnd = ClientFogEnd(context);
    in.fogStart = in.fogEnd * context.fogStartShare;
    in.lightParams.stormBlend = context.storm;
    return in;
}

struct TestedSetting
{
    const char* key;
    float Config::*number;
    int Config::*choice;
    bool Config::*toggle;
};

constexpr TestedSetting Number(const char* key, float Config::*member)
{
    return {key, member, nullptr, nullptr};
}

constexpr TestedSetting Choice(const char* key, int Config::*member)
{
    return {key, nullptr, member, nullptr};
}

constexpr TestedSetting Toggle(const char* key, bool Config::*member)
{
    return {key, nullptr, nullptr, member};
}

constexpr TestedSetting kFogSettings[] = {
    Number("Density", &Config::density),
    Number("AzerothDensity", &Config::azerothDensity),
    Number("OutlandDensity", &Config::outlandDensity),
    Number("NorthrendDensity", &Config::northrendDensity),
    Number("Haze", &Config::haze),
    Number("GroundFog", &Config::groundFog),
    Number("FarFog", &Config::farFog),
    Choice("StockFog", &Config::stockFog),
    Choice("DataMode", &Config::dataMode),
    Choice("OutlandClassicFog", &Config::outlandClassicFog),
    Choice("NorthrendClassicFog", &Config::northrendClassicFog),
    Toggle("ClassicNoise", &Config::classicNoise),
    Number("MaxDistance", &Config::maxDistance),
    Number("Ambient", &Config::ambient),
    Number("Exposure", &Config::exposure),
    Number("ClassicExposure", &Config::classicExposure),
};

constexpr TestedSetting kImageSettings[] = {
    Number("Density", &Config::density),
    Number("GroundFog", &Config::groundFog),
    Number("Ambient", &Config::ambient),
    Number("Exposure", &Config::exposure),
    Number("ClassicExposure", &Config::classicExposure),
    Number("SunScatter", &Config::sunScatter),
    Number("MoonScatter", &Config::moonScatter),
    Choice("ClassicPhase", &Config::classicPhase),
    Choice("ColorSpace", &Config::colorSpace),
    Number("AzerothSkyColor", &Config::azerothSkyColor),
    Number("OutlandSkyColor", &Config::outlandSkyColor),
    Number("NorthrendSkyColor", &Config::northrendSkyColor),
    Number("NoiseAmount", &Config::noiseAmount),
    Number("NoiseScale", &Config::noiseScale),
    Toggle("ClassicNoise", &Config::classicNoise),
    Toggle("LightShafts", &Config::lightShafts),
    Number("LocalLightIntensity", &Config::localLightIntensity),
    Number("AzerothLocalLights", &Config::azerothLocalLights),
    Number("OutlandLocalLights", &Config::outlandLocalLights),
    Number("NorthrendLocalLights", &Config::northrendLocalLights),
    Number("LocalLightPhase", &Config::localLightPhase),
    Toggle("DecodeLightColours", &Config::decodeLightColours),
    Number("SurfaceLights", &Config::surfaceLights),
    Number("SpellLightIntensity", &Config::spellLightIntensity),
    Number("SpellSurfaceLights", &Config::spellSurfaceLights),
};

constexpr const char* kSettingsTheSceneCannotShow =
    "TransparentFog, GlowCompensation, ForeverGlow, ColorGrading (they act on the client's glow pass and on models "
    "drawn after the world), NoiseWindSpeed (motion), InteriorDensity, InteriorFarFog (no context is indoors)";

std::string ShortNumber(float value)
{
    char text[32];
    std::snprintf(text, sizeof(text), "%g", value);
    return text;
}

Config Varied(const Config& shipped, const TestedSetting& setting, std::string& change)
{
    Config varied = shipped;
    if (setting.number)
    {
        const float before = shipped.*setting.number;
        const SettingRange range = FloatSettingRange(setting.number);
        const float after = before <= 0.0f ? range.hi : before * 2.0f <= range.hi ? before * 2.0f : before * 0.5f;
        varied.*setting.number = after;
        change = ShortNumber(before) + " to " + ShortNumber(after);
    }
    else if (setting.choice)
    {
        varied.*setting.choice = shipped.*setting.choice ? 0 : 1;
        change = std::to_string(shipped.*setting.choice) + " to " + std::to_string(varied.*setting.choice);
    }
    else
    {
        varied.*setting.toggle = !(shipped.*setting.toggle);
        change = std::string(shipped.*setting.toggle ? "1" : "0") + " to " + (varied.*setting.toggle ? "1" : "0");
    }
    return varied;
}

struct Change
{
    float ratio = 1.0f;
    bool appeared = false;
    bool moved = false;
};

Change ChangeOfNumber(float before, float after)
{
    Change change;
    if (before > kSmallestMeasure)
    {
        change.ratio = after / before;
        change.moved = std::fabs(change.ratio - 1.0f) > kNumberMovedByAtLeast;
    }
    else
    {
        change.appeared = after > kSmallestMeasure;
        change.moved = change.appeared;
    }
    return change;
}

struct SettingEffects
{
    const TestedSetting* setting = nullptr;
    std::string change;
    std::vector<Change> changes;
};

struct Matrix
{
    std::vector<const char*> effects;
    size_t contexts = 0;
    std::vector<SettingEffects> rows;

    const Change& At(const SettingEffects& row, size_t context, size_t effect) const
    {
        return row.changes[context * effects.size() + effect];
    }
};

struct Spread
{
    float lowest = 0.0f;
    float median = 0.0f;
    float highest = 0.0f;
    int moved = 0;
    int appeared = 0;
};

Spread SpreadOf(const Matrix& matrix, const SettingEffects& row, size_t effect)
{
    std::vector<float> ratios;
    Spread spread;
    for (size_t context = 0; context < matrix.contexts; ++context)
    {
        const Change& change = matrix.At(row, context, effect);
        spread.moved += change.moved ? 1 : 0;
        spread.appeared += change.appeared ? 1 : 0;
        if (!change.appeared)
            ratios.push_back(change.ratio);
    }
    if (ratios.empty())
        return spread;
    std::sort(ratios.begin(), ratios.end());
    spread.lowest = ratios.front();
    spread.median = ratios[ratios.size() / 2];
    spread.highest = ratios.back();
    return spread;
}

bool SameChange(const Change& a, const Change& b)
{
    return a.appeared == b.appeared && std::fabs(a.ratio - b.ratio) <= kSameEffectWithin * std::max(a.ratio, b.ratio);
}

struct Overlap
{
    int casesOfFirst = 0;
    int casesOfSecond = 0;
    int shared = 0;
    int alike = 0;
    bool onePower = true;
    float power = 0.0f;
};

Overlap OverlapOf(const SettingEffects& first, const SettingEffects& second)
{
    Overlap overlap;
    bool powerKnown = false;
    for (size_t i = 0; i < first.changes.size(); ++i)
    {
        const Change& a = first.changes[i];
        const Change& b = second.changes[i];
        overlap.casesOfFirst += a.moved ? 1 : 0;
        overlap.casesOfSecond += b.moved ? 1 : 0;
        if (!a.moved || !b.moved)
            continue;
        ++overlap.shared;
        overlap.alike += SameChange(a, b) ? 1 : 0;
        const bool aHasNoRatio = a.appeared || a.ratio <= 0.0f;
        const bool bHasNoRatio = b.appeared || b.ratio <= 0.0f;
        if (aHasNoRatio || bHasNoRatio)
        {
            overlap.onePower = overlap.onePower && aHasNoRatio == bHasNoRatio;
            continue;
        }
        const float power = std::log(a.ratio) / std::log(b.ratio);
        if (!powerKnown)
            overlap.power = power;
        powerKnown = true;
        overlap.onePower = overlap.onePower && std::fabs(power - overlap.power) <= kSamePowerWithin * std::fabs(power);
    }
    overlap.onePower = overlap.onePower && powerKnown;
    return overlap;
}

void AddOverlap(Report& report, const char* first, const char* second, const Overlap& overlap)
{
    if (overlap.shared < kFewestCasesForARule)
        return;
    const bool everywhere = overlap.shared == overlap.casesOfFirst && overlap.shared == overlap.casesOfSecond;
    if (overlap.onePower && std::fabs(overlap.power - 1.0f) <= kSamePowerWithin)
        report.Add("  %s and %s: the same change %s (%d cases; %s moves %d, %s %d)\n", first, second,
                   everywhere ? "in every case" : "wherever both act", overlap.shared, first, overlap.casesOfFirst,
                   second, overlap.casesOfSecond);
    else if (overlap.onePower)
        report.Add("  %s and %s: %s changes the effect as %s does, raised to the power %.2f, %s (%d cases)\n", first,
                   second, first, second, overlap.power, everywhere ? "in every case" : "wherever both act",
                   overlap.shared);
    else if (overlap.alike * 2 >= overlap.shared)
        report.Add("  %s and %s: the same change in %d of the %d cases both move\n", first, second, overlap.alike,
                   overlap.shared);
}

void AddSensitivity(Report& report, const Matrix& matrix, const std::vector<Context>& contexts, std::FILE* csv)
{
    report.Add("\nwhat each setting moves: the effect after the change over the effect before, lowest, median and "
               "highest across the %zu contexts, and in how many it moved at all\n",
               matrix.contexts);
    report.Add("%-21s %-14s", "setting", "change");
    for (const char* name : matrix.effects)
        report.Add(" | %-26s", name);
    report.Add("\n");
    std::fprintf(csv, "setting,change,place,hour,weather");
    for (const char* name : matrix.effects)
        std::fprintf(csv, ",%s", name);
    std::fprintf(csv, "\n");
    for (const SettingEffects& row : matrix.rows)
    {
        report.Add("%-21s %-14s", row.setting->key, row.change.c_str());
        for (size_t effect = 0; effect < matrix.effects.size(); ++effect)
        {
            const Spread spread = SpreadOf(matrix, row, effect);
            if (spread.moved == 0)
                report.Add(" | %-26s", "-");
            else
                report.Add(" | %5.2f %5.2f %6.2f  %2d/%zu%s", spread.lowest, spread.median, spread.highest,
                           spread.moved, matrix.contexts, spread.appeared ? "*" : " ");
        }
        report.Add("\n");
        for (size_t context = 0; context < matrix.contexts; ++context)
        {
            std::fprintf(csv, "%s,%s,%s,%s,%s", row.setting->key, row.change.c_str(), contexts[context].place.c_str(),
                         contexts[context].hour.c_str(), contexts[context].weather.c_str());
            for (size_t effect = 0; effect < matrix.effects.size(); ++effect)
            {
                const Change& change = matrix.At(row, context, effect);
                if (change.appeared)
                    std::fprintf(csv, ",appeared");
                else
                    std::fprintf(csv, ",%.4f", change.ratio);
            }
            std::fprintf(csv, "\n");
        }
    }
    report.Add("* the effect was absent as shipped in some contexts and the change brought it in\n");
}

void AddOverlaps(Report& report, const Matrix& matrix)
{
    report.Add("\nsettings that overlap (a case is one effect in one context)\n");
    for (size_t a = 0; a < matrix.rows.size(); ++a)
        for (size_t b = a + 1; b < matrix.rows.size(); ++b)
            AddOverlap(report, matrix.rows[a].setting->key, matrix.rows[b].setting->key,
                       OverlapOf(matrix.rows[a], matrix.rows[b]));
    for (const SettingEffects& row : matrix.rows)
    {
        const bool moves =
            std::any_of(row.changes.begin(), row.changes.end(), [](const Change& change) { return change.moved; });
        if (!moves)
            report.Add("  %s: moves none of these effects in any context\n", row.setting->key);
    }
}

enum FogEffect
{
    kThicknessNear,
    kThicknessFar,
    kThicknessAtTheHorizon,
    kAmbientBrightness,
    kFogEffectCount,
};

constexpr const char* kFogEffectNames[kFogEffectCount] = {"thickness 150 yd", "thickness 1000 yd",
                                                          "thickness horizon", "ambient 400 yd"};

struct FogMeasure
{
    float effect[kFogEffectCount] = {};
    float opacityNear = 0.0f;
    float opacityFar = 0.0f;
    float opacityAtTheHorizon = 0.0f;
    bool classicLayers = false;
};

float OpticalDepth(float opacity)
{
    return -std::log(1.0f - std::min(opacity, kNearlyOpaque));
}

FogMeasure MeasuredFog(const FogData& data, const Context& context, const Config& settings)
{
    const FrameInputs in = ContextInputs(context, CameraBehindTheCharacter(context));
    const Config regional = ConfigForMap(settings, in.mapId);
    AuthoredFog authored = {};
    const bool resolved = regional.dataMode == 1 &&
                          data.Resolve(in.mapId, in.camPos, in.dayFraction, in.lightParams, authored);
    const FogParams fog = BuildFogParams(in, regional, resolved ? &authored : nullptr);
    FogMeasure measure;
    measure.classicLayers = fog.authored;
    measure.opacityNear = LevelRayFogAt(fog, kNearYards).opacity;
    measure.opacityFar = LevelRayFogAt(fog, kFarYards).opacity;
    measure.opacityAtTheHorizon = LevelRayFogAt(fog, fog.maxDistance).opacity;
    measure.effect[kThicknessNear] = OpticalDepth(measure.opacityNear);
    measure.effect[kThicknessFar] = OpticalDepth(measure.opacityFar);
    measure.effect[kThicknessAtTheHorizon] = OpticalDepth(measure.opacityAtTheHorizon);
    measure.effect[kAmbientBrightness] = LevelRayFogAt(fog, kAmbientYards).emission *
                                         (fog.authored ? regional.classicExposure : regional.exposure);
    return measure;
}

void AddFogBaseline(Report& report, const std::vector<Context>& contexts, const std::vector<FogMeasure>& baseline,
                    std::FILE* csv)
{
    report.Add("\nfog as shipped, level ray from the camera (opacity; ambient is the fog's own light at 400 yd)\n");
    report.Add("%-28s %-7s %8s %8s %8s %9s\n", "context", "layers", "150 yd", "1000 yd", "horizon", "ambient");
    std::fprintf(csv, "place,hour,weather,layers,opacity_150,opacity_1000,opacity_horizon,ambient_400\n");
    for (size_t i = 0; i < contexts.size(); ++i)
    {
        const FogMeasure& fog = baseline[i];
        const char* layers = fog.classicLayers ? "classic" : "derived";
        report.Add("%-28s %-7s %8.3f %8.3f %8.3f %9.4f\n", NameOf(contexts[i]).c_str(), layers, fog.opacityNear,
                   fog.opacityFar, fog.opacityAtTheHorizon, fog.effect[kAmbientBrightness]);
        std::fprintf(csv, "%s,%s,%s,%s,%.5f,%.5f,%.5f,%.6f\n", contexts[i].place.c_str(), contexts[i].hour.c_str(),
                     contexts[i].weather.c_str(), layers, fog.opacityNear, fog.opacityFar, fog.opacityAtTheHorizon,
                     fog.effect[kAmbientBrightness]);
    }
    for (int effect = 0; effect < kFogEffectCount; ++effect)
    {
        float lowest = std::numeric_limits<float>::infinity();
        float highest = 0.0f;
        for (const FogMeasure& fog : baseline)
        {
            lowest = std::min(lowest, fog.effect[effect]);
            highest = std::max(highest, fog.effect[effect]);
        }
        report.Add("%s: %.5g to %.5g across the contexts, %.0f times\n", kFogEffectNames[effect], lowest, highest,
                   lowest > kSmallestMeasure ? highest / lowest : std::numeric_limits<float>::infinity());
    }
}

Config ShippedSettings(const std::wstring& ini)
{
    char path[MAX_PATH] = {};
    WideCharToMultiByte(CP_ACP, 0, ini.c_str(), -1, path, MAX_PATH, nullptr, nullptr);
    ConfigStore settings;
    settings.Load(path);
    Config shipped = settings.Get();
    shipped.overlay = false;
    shipped.logLevel = 0;
    return shipped;
}

std::FILE* OpenInFolder(const std::wstring& folder, const wchar_t* name)
{
    return _wfopen((folder + L"\\" + name).c_str(), L"w");
}

bool AddFogSettingMatrix(Report& report, const std::wstring& outDir, const FogData& data,
                         const std::vector<Context>& contexts, const Config& shipped,
                         std::vector<FogMeasure>& baseline)
{
    std::FILE* baselineCsv = OpenInFolder(outDir, L"transfer-fog-baseline.csv");
    std::FILE* sensitivityCsv = OpenInFolder(outDir, L"transfer-fog-sensitivity.csv");
    if (!baselineCsv || !sensitivityCsv)
        return false;
    for (const Context& context : contexts)
        baseline.push_back(MeasuredFog(data, context, shipped));
    Matrix matrix;
    matrix.effects.assign(std::begin(kFogEffectNames), std::end(kFogEffectNames));
    matrix.contexts = contexts.size();
    for (const TestedSetting& setting : kFogSettings)
    {
        SettingEffects row;
        row.setting = &setting;
        const Config varied = Varied(shipped, setting, row.change);
        for (size_t context = 0; context < contexts.size(); ++context)
        {
            const FogMeasure measure = MeasuredFog(data, contexts[context], varied);
            for (int effect = 0; effect < kFogEffectCount; ++effect)
                row.changes.push_back(ChangeOfNumber(baseline[context].effect[effect], measure.effect[effect]));
        }
        matrix.rows.push_back(row);
    }
    AddFogBaseline(report, contexts, baseline, baselineCsv);
    AddSensitivity(report, matrix, contexts, sensitivityCsv);
    AddOverlaps(report, matrix);
    std::fclose(baselineCsv);
    std::fclose(sensitivityCsv);
    return true;
}

float LinearChannelOf(uint32_t colour, int shift)
{
    return std::pow(static_cast<float>((colour >> shift) & 0xFF) / 255.0f, kDisplayGamma);
}

DWORD ShadedColour(const float* albedo, Vec3 normal, const Context& context)
{
    const float facing = std::clamp(Dot(normal, Norm(context.toLight)), 0.0f, 1.0f);
    DWORD colour = 0xFF000000;
    for (int channel = 0; channel < 3; ++channel)
    {
        const int shift = 16 - 8 * channel;
        const float light = LinearChannelOf(context.ambient, shift) + LinearChannelOf(context.direct, shift) * facing;
        const float shown = std::pow(std::clamp(albedo[channel] * light, 0.0f, 1.0f), 1.0f / kDisplayGamma);
        colour |= static_cast<DWORD>(shown * 255.0f + 0.5f) << shift;
    }
    return colour;
}

void AddShadedBox(std::vector<SceneVertex>& scene, Vec3 lo, Vec3 hi, const float* albedo, const Context& context)
{
    const Vec3 p[8] = {{lo.x, lo.y, lo.z}, {hi.x, lo.y, lo.z}, {hi.x, hi.y, lo.z}, {lo.x, hi.y, lo.z},
                       {lo.x, lo.y, hi.z}, {hi.x, lo.y, hi.z}, {hi.x, hi.y, hi.z}, {lo.x, hi.y, hi.z}};
    AddQuad(scene, p[0], p[1], p[5], p[4], ShadedColour(albedo, {0, -1, 0}, context), context.ground);
    AddQuad(scene, p[1], p[2], p[6], p[5], ShadedColour(albedo, {1, 0, 0}, context), context.ground);
    AddQuad(scene, p[2], p[3], p[7], p[6], ShadedColour(albedo, {0, 1, 0}, context), context.ground);
    AddQuad(scene, p[3], p[0], p[4], p[7], ShadedColour(albedo, {-1, 0, 0}, context), context.ground);
    AddQuad(scene, p[4], p[5], p[6], p[7], ShadedColour(albedo, {0, 0, 1}, context), context.ground);
}

void AddTree(std::vector<SceneVertex>& scene, float aside, const Context& context)
{
    AddShadedBox(scene, {kTreeLineAhead, aside - 0.5f, 0}, {kTreeLineAhead + 1.0f, aside + 0.5f, 7}, kBarkAlbedo,
                 context);
    AddShadedBox(scene, {kTreeLineAhead - 2.5f, aside - 3.0f, 6}, {kTreeLineAhead + 3.5f, aside + 3.0f, 12},
                 kLeafAlbedo, context);
}

std::vector<SceneVertex> SheetScene(const Context& context)
{
    std::vector<SceneVertex> scene;
    AddQuad(scene, {-kGroundHalfSize, -kGroundHalfSize, 0}, {kGroundHalfSize, -kGroundHalfSize, 0},
            {kGroundHalfSize, kGroundHalfSize, 0}, {-kGroundHalfSize, kGroundHalfSize, 0},
            ShadedColour(kGrassAlbedo, {0, 0, 1}, context), context.ground);
    AddShadedBox(scene, kCharacterLow, kCharacterHigh, kSkinAlbedo, context);
    AddShadedBox(scene, kLampPostLow, kLampPostHigh, kPostAlbedo, context);
    for (int tree = 0; tree < kTrees; ++tree)
        AddTree(scene, kFirstTreeAside + kTreeSpacing * tree, context);
    AddShadedBox(scene, kNearBoardLow, kNearBoardHigh, kBoardAlbedo, context);
    AddShadedBox(scene, kFarBoardLow, kFarBoardHigh, kBoardAlbedo, context);
    return scene;
}

struct Probe
{
    UINT x = 0;
    UINT y = 0;
};

bool PixelOf(const Camera& camera, const Context& context, Vec3 local, int margin, Probe& probe)
{
    const Vec3 relative = Sub(Add(context.ground, local), camera.eye);
    const float world[3] = {relative.x, relative.y, relative.z};
    float view[3];
    TransformDirection(world, camera.view, view);
    if (view[2] <= 0.0f)
        return false;
    const float* p = camera.projection;
    const float px = (view[0] / view[2] * p[0] + p[8]) * 0.5f + 0.5f;
    const float py = 0.5f - (view[1] / view[2] * p[5] + p[9]) * 0.5f;
    const int x = static_cast<int>(px * camera.viewport.Width);
    const int y = static_cast<int>(py * camera.viewport.Height);
    if (x < margin || y < margin || x >= static_cast<int>(camera.viewport.Width) - margin ||
        y >= static_cast<int>(camera.viewport.Height) - margin)
        return false;
    probe = {static_cast<UINT>(x), static_cast<UINT>(y)};
    return true;
}

struct Probes
{
    Probe nearBoard;
    Probe farBoard;
    Probe sky;
    Probe airAboveTheLamp;
    Probe airAboveTheSpell;
    Probe groundByTheLamp[kGroundDistanceCount];
    Probe groundByTheSpell[kGroundDistanceCount];
};

bool PlaceProbes(const Camera& camera, const Context& context, Probes& probes)
{
    bool placed = PixelOf(camera, context, kNearBoardPoint, kBoardRadius, probes.nearBoard) &&
                  PixelOf(camera, context, kFarBoardPoint, kBoardRadius, probes.farBoard) &&
                  PixelOf(camera, context, kSkyAboveTheHorizon, kProbeRadius, probes.sky) &&
                  PixelOf(camera, context, kAirAboveTheLamp, kProbeRadius, probes.airAboveTheLamp) &&
                  PixelOf(camera, context, kAirAboveTheSpell, kProbeRadius, probes.airAboveTheSpell);
    for (int i = 0; i < kGroundDistanceCount; ++i)
    {
        const Vec3 byTheLamp = {kLampLight.x - kGroundDistances[i], kLampLight.y, 0};
        const Vec3 byTheSpell = {kSpellLight.x - kGroundDistances[i], kSpellLight.y, 0};
        placed = placed && PixelOf(camera, context, byTheLamp, kProbeRadius, probes.groundByTheLamp[i]) &&
                 PixelOf(camera, context, byTheSpell, kProbeRadius, probes.groundByTheSpell[i]);
    }
    return placed;
}

struct Reading
{
    float linear = 0.0f;
    float steps = 0.0f;
};

constexpr float kLuminanceOfBlueGreenRed[3] = {0.0722f, 0.7152f, 0.2126f};

Reading LuminanceAround(const Image& image, Probe probe, int radius)
{
    Reading sum;
    int count = 0;
    for (int y = static_cast<int>(probe.y) - radius; y <= static_cast<int>(probe.y) + radius; ++y)
        for (int x = static_cast<int>(probe.x) - radius; x <= static_cast<int>(probe.x) + radius; ++x)
        {
            const unsigned char* pixel = image.At(static_cast<UINT>(x), static_cast<UINT>(y));
            for (int channel = 0; channel < 3; ++channel)
            {
                sum.linear += kLuminanceOfBlueGreenRed[channel] * std::pow(pixel[channel] / 255.0f, kDisplayGamma);
                sum.steps += kLuminanceOfBlueGreenRed[channel] * pixel[channel];
            }
            ++count;
        }
    return {sum.linear / count, sum.steps / count};
}

Reading RedShareAround(const Image& image, Probe probe, int radius)
{
    float red = 0.0f;
    float all = 0.0f;
    for (int y = static_cast<int>(probe.y) - radius; y <= static_cast<int>(probe.y) + radius; ++y)
        for (int x = static_cast<int>(probe.x) - radius; x <= static_cast<int>(probe.x) + radius; ++x)
        {
            const unsigned char* pixel = image.At(static_cast<UINT>(x), static_cast<UINT>(y));
            red += pixel[2];
            all += pixel[0] + pixel[1] + pixel[2];
        }
    const float share = all > 0.0f ? red / all : 0.0f;
    return {share, share * 255.0f};
}

Reading PatchinessAround(const Image& image, Probe probe, int radius)
{
    std::vector<float> steps;
    for (int y = static_cast<int>(probe.y) - radius; y <= static_cast<int>(probe.y) + radius; ++y)
        for (int x = static_cast<int>(probe.x) - radius; x <= static_cast<int>(probe.x) + radius; ++x)
            steps.push_back(255.0f * image.Luma(static_cast<UINT>(x), static_cast<UINT>(y)));
    float mean = 0.0f;
    for (float value : steps)
        mean += value / steps.size();
    float variance = 0.0f;
    for (float value : steps)
        variance += (value - mean) * (value - mean) / steps.size();
    const float deviation = std::sqrt(variance);
    return {mean > 0.0f ? deviation / mean : 0.0f, deviation};
}

Reading Added(const Reading& with, const Reading& without)
{
    return {std::max(with.linear - without.linear, 0.0f), with.steps - without.steps};
}

enum ImageEffect
{
    kVeilNear,
    kVeilFar,
    kSkyLight,
    kFarRedShare,
    kFarPatchiness,
    kLampInTheAir,
    kLampOnTheGround,
    kSpellInTheAir,
    kSpellOnTheGround,
    kImageEffectCount,
};

constexpr const char* kImageEffectNames[kImageEffectCount] = {
    "fog light 150 yd", "fog light 400 yd", "sky at the horizon", "red share 400 yd", "patchiness 400 yd",
    "lamp in the air",  "lamp on ground 3 yd", "spell in the air",  "spell on ground 3 yd"};

struct ImageMeasure
{
    Reading effect[kImageEffectCount];
    Reading lampOnGround[kGroundDistanceCount];
    Reading spellOnGround[kGroundDistanceCount];
    Reading groundByTheLampUnlit[kGroundDistanceCount];
    Reading groundByTheSpellUnlit[kGroundDistanceCount];
    Reading airAboveTheLampUnlit;
    Reading airAboveTheSpellUnlit;
};

Change ChangeOfReading(const Reading& before, const Reading& after)
{
    Change change;
    if (std::fabs(after.steps - before.steps) < kSmallestVisibleStep)
        return change;
    if (before.linear > kSmallestMeasure)
    {
        change.ratio = after.linear / before.linear;
        change.moved = std::fabs(change.ratio - 1.0f) > kLightMovedByAtLeast;
    }
    else
    {
        change.appeared = after.linear > kSmallestMeasure;
        change.moved = change.appeared;
    }
    return change;
}

void PlaceLight(LocalPointLight& light, const Context& context, Vec3 local)
{
    const Vec3 position = Add(context.ground, local);
    light.position[0] = position.x;
    light.position[1] = position.y;
    light.position[2] = position.z;
}

void AddLamp(FrameInputs& in, const Context& context, const Config& settings)
{
    LocalPointLight lamp;
    PlaceLight(lamp, context, kLampLight);
    std::memcpy(lamp.color, performance_scene::kElwynnLampColor, sizeof(lamp.color));
    std::memcpy(lamp.attenuation, performance_scene::kElwynnLampAttenuation, sizeof(lamp.attenuation));
    engine::SelectLocalPointLight(in.localLights, lamp, in.camPos, LocalLightUpload(settings, in.mapId));
}

void AddSpellLight(FrameInputs& in, const Context& context, const Config& settings)
{
    LocalPointLight spell;
    PlaceLight(spell, context, kSpellLight);
    const float extent = (kSpellReach - kSpellLightBaseReach) / kSpellLightReachPerExtent;
    const float brightness = SpellLightPower(extent) * (settings.spellLights ? settings.spellLightIntensity : 0.0f);
    for (int channel = 0; channel < 3; ++channel)
        spell.color[channel] = kSpellColour[channel] * brightness;
    spell.attenuation[0] = 1.0f;
    spell.attenuation[2] = (1.0f / kLocalPointLightContributionCutoff - 1.0f) / (kSpellReach * kSpellReach);
    spell.source = LocalLightSource::Spell;
    engine::SelectLocalPointLight(in.localLights, spell, in.camPos, LocalLightUpload(settings, in.mapId));
}

enum class Lights
{
    None,
    Lamp,
    Spell,
    Both,
};

struct SheetView
{
    Harness* harness = nullptr;
    const Context* context = nullptr;
    Camera camera;
    Probes probes;
    Reading nearBoardWithoutFog;
    Reading farBoardWithoutFog;
    unsigned skippedFrames = 0;
};

Image DrawnFrame(SheetView& view, const FrameInputs* in, const Config& settings, bool capture)
{
    Harness& h = *view.harness;
    vf_test_set_config(&settings);
    h.BeginFrame();
    h.DrawScene(view.camera.eye, view.camera.view, view.camera.projection, view.camera.viewport);
    const char* skip = "";
    if (in && !vf_test_render(in, &skip))
        ++view.skippedFrames;
    Image image;
    if (capture)
        image = Capture(h.dev);
    h.dev->EndScene();
    h.dev->Present(nullptr, nullptr, nullptr, nullptr);
    return image;
}

Image SettledFrame(SheetView& view, const Config& settings, Lights lights)
{
    FrameInputs in = ContextInputs(*view.context, view.camera);
    if (lights == Lights::Lamp || lights == Lights::Both)
        AddLamp(in, *view.context, settings);
    if (lights == Lights::Spell || lights == Lights::Both)
        AddSpellLight(in, *view.context, settings);
    Config withoutHistory = settings;
    withoutHistory.temporal = 0.0f;
    DrawnFrame(view, &in, withoutHistory, false);
    for (int frame = 1; frame < kSettleFrames; ++frame)
        DrawnFrame(view, &in, settings, false);
    return DrawnFrame(view, &in, settings, true);
}

ImageMeasure MeasuredImage(SheetView& view, const Config& settings)
{
    const Probes& probes = view.probes;
    const Image unlit = SettledFrame(view, settings, Lights::None);
    const Image lamp = SettledFrame(view, settings, Lights::Lamp);
    const Image spell = SettledFrame(view, settings, Lights::Spell);
    ImageMeasure measure;
    measure.effect[kVeilNear] = Added(LuminanceAround(unlit, probes.nearBoard, kBoardRadius), view.nearBoardWithoutFog);
    measure.effect[kVeilFar] = Added(LuminanceAround(unlit, probes.farBoard, kBoardRadius), view.farBoardWithoutFog);
    measure.effect[kSkyLight] = LuminanceAround(unlit, probes.sky, kProbeRadius);
    measure.effect[kFarRedShare] = RedShareAround(unlit, probes.farBoard, kBoardRadius);
    measure.effect[kFarPatchiness] = PatchinessAround(unlit, probes.farBoard, kBoardRadius);
    measure.airAboveTheLampUnlit = LuminanceAround(unlit, probes.airAboveTheLamp, kProbeRadius);
    measure.airAboveTheSpellUnlit = LuminanceAround(unlit, probes.airAboveTheSpell, kProbeRadius);
    measure.effect[kLampInTheAir] =
        Added(LuminanceAround(lamp, probes.airAboveTheLamp, kProbeRadius), measure.airAboveTheLampUnlit);
    measure.effect[kSpellInTheAir] =
        Added(LuminanceAround(spell, probes.airAboveTheSpell, kProbeRadius), measure.airAboveTheSpellUnlit);
    for (int i = 0; i < kGroundDistanceCount; ++i)
    {
        measure.groundByTheLampUnlit[i] = LuminanceAround(unlit, probes.groundByTheLamp[i], kProbeRadius);
        measure.groundByTheSpellUnlit[i] = LuminanceAround(unlit, probes.groundByTheSpell[i], kProbeRadius);
        measure.lampOnGround[i] =
            Added(LuminanceAround(lamp, probes.groundByTheLamp[i], kProbeRadius), measure.groundByTheLampUnlit[i]);
        measure.spellOnGround[i] =
            Added(LuminanceAround(spell, probes.groundByTheSpell[i], kProbeRadius), measure.groundByTheSpellUnlit[i]);
    }
    measure.effect[kLampOnTheGround] = measure.lampOnGround[kMatrixGroundDistance];
    measure.effect[kSpellOnTheGround] = measure.spellOnGround[kMatrixGroundDistance];
    return measure;
}

float Contrast(const Reading& added, const Reading& base)
{
    const float darkest = std::pow(kDarkestBaseStep / 255.0f, kDisplayGamma);
    return added.linear / std::max(base.linear, darkest);
}

std::wstring Wide(const std::string& text)
{
    return std::wstring(text.begin(), text.end());
}

std::string TileName(const Context& context)
{
    return "transfer-" + context.place + "-" + context.hour + "-" + context.weather + ".png";
}

struct LightSummary
{
    const char* name;
    std::vector<float> contrasts;
    int clearlyVisible = 0;
    int invisible = 0;
};

void Count(LightSummary& summary, const Reading& added, const Reading& base)
{
    summary.contrasts.push_back(Contrast(added, base));
    summary.clearlyVisible += added.steps >= kClearlyVisibleSteps ? 1 : 0;
    summary.invisible += added.steps < kSmallestVisibleStep ? 1 : 0;
}

void AddLightSummary(Report& report, LightSummary& summary, size_t contexts)
{
    std::sort(summary.contrasts.begin(), summary.contrasts.end());
    const float lowest = summary.contrasts.front();
    const float highest = summary.contrasts.back();
    report.Add("%-22s contrast %.4f to %.3f (median %.3f)%s; clearly visible (2 steps of 255 or more) in %d of %zu "
               "contexts, under half a step in %d\n",
               summary.name, lowest, highest, summary.contrasts[summary.contrasts.size() / 2],
               lowest > kSmallestMeasure ? "" : ", absent somewhere", summary.clearlyVisible, contexts,
               summary.invisible);
    if (lowest > kSmallestMeasure)
        report.Add("%-22s spread %.0f times\n", "", highest / lowest);
}

void AddImageBaseline(Report& report, const std::vector<Context>& contexts, const std::vector<ImageMeasure>& baseline,
                      std::FILE* csv)
{
    report.Add("\nlights as shipped: what a light adds, in steps of 255 on the display and as a contrast (linear "
               "light added over the linear light there without it)\n");
    report.Add("%-28s %15s %15s %15s %15s %15s %15s\n", "context", "lamp in air", "lamp ground 1yd",
               "lamp ground 3yd", "lamp ground 6yd", "spell in air", "spell ground 3yd");
    std::fprintf(csv, "place,hour,weather,fog_light_150_steps,fog_light_400_steps,sky_steps,red_share_400,"
                      "patchiness_400");
    for (const char* light : {"lamp", "spell"})
    {
        std::fprintf(csv, ",%s_air_steps,%s_air_contrast", light, light);
        for (float distance : kGroundDistances)
            std::fprintf(csv, ",%s_ground_%.0fyd_steps,%s_ground_%.0fyd_contrast", light, distance, light, distance);
    }
    std::fprintf(csv, "\n");
    LightSummary lampAir = {"lamp in the air"};
    LightSummary lampGround = {"lamp on ground 3 yd"};
    LightSummary spellAir = {"spell in the air"};
    LightSummary spellGround = {"spell on ground 3 yd"};
    for (size_t i = 0; i < contexts.size(); ++i)
    {
        const ImageMeasure& m = baseline[i];
        const auto cell = [&](const Reading& added, const Reading& base) {
            char text[32];
            std::snprintf(text, sizeof(text), "%+6.1f (%6.3f)", added.steps, Contrast(added, base));
            return std::string(text);
        };
        report.Add("%-28s %15s %15s %15s %15s %15s %15s\n", NameOf(contexts[i]).c_str(),
                   cell(m.effect[kLampInTheAir], m.airAboveTheLampUnlit).c_str(),
                   cell(m.lampOnGround[0], m.groundByTheLampUnlit[0]).c_str(),
                   cell(m.lampOnGround[1], m.groundByTheLampUnlit[1]).c_str(),
                   cell(m.lampOnGround[2], m.groundByTheLampUnlit[2]).c_str(),
                   cell(m.effect[kSpellInTheAir], m.airAboveTheSpellUnlit).c_str(),
                   cell(m.spellOnGround[1], m.groundByTheSpellUnlit[1]).c_str());
        std::fprintf(csv, "%s,%s,%s,%.2f,%.2f,%.2f,%.4f,%.4f", contexts[i].place.c_str(), contexts[i].hour.c_str(),
                     contexts[i].weather.c_str(), m.effect[kVeilNear].steps, m.effect[kVeilFar].steps,
                     m.effect[kSkyLight].steps, m.effect[kFarRedShare].linear, m.effect[kFarPatchiness].linear);
        std::fprintf(csv, ",%.2f,%.4f", m.effect[kLampInTheAir].steps,
                     Contrast(m.effect[kLampInTheAir], m.airAboveTheLampUnlit));
        for (int d = 0; d < kGroundDistanceCount; ++d)
            std::fprintf(csv, ",%.2f,%.4f", m.lampOnGround[d].steps,
                         Contrast(m.lampOnGround[d], m.groundByTheLampUnlit[d]));
        std::fprintf(csv, ",%.2f,%.4f", m.effect[kSpellInTheAir].steps,
                     Contrast(m.effect[kSpellInTheAir], m.airAboveTheSpellUnlit));
        for (int d = 0; d < kGroundDistanceCount; ++d)
            std::fprintf(csv, ",%.2f,%.4f", m.spellOnGround[d].steps,
                         Contrast(m.spellOnGround[d], m.groundByTheSpellUnlit[d]));
        std::fprintf(csv, "\n");
        Count(lampAir, m.effect[kLampInTheAir], m.airAboveTheLampUnlit);
        Count(lampGround, m.lampOnGround[kMatrixGroundDistance], m.groundByTheLampUnlit[kMatrixGroundDistance]);
        Count(spellAir, m.effect[kSpellInTheAir], m.airAboveTheSpellUnlit);
        Count(spellGround, m.spellOnGround[kMatrixGroundDistance], m.groundByTheSpellUnlit[kMatrixGroundDistance]);
    }
    report.Add("\n");
    for (LightSummary* summary : {&lampAir, &lampGround, &spellAir, &spellGround})
        AddLightSummary(report, *summary, contexts.size());
}

void WritePage(const std::wstring& outDir, const std::vector<Context>& contexts,
               const std::vector<FogMeasure>& fog, const std::vector<ImageMeasure>& images, const Report& report)
{
    std::FILE* page = OpenInFolder(outDir, L"transfer-sheet.html");
    if (!page)
        return;
    std::fprintf(page, "<!doctype html><meta charset=\"utf-8\"><title>CoAGraphics transfer sheet</title>\n"
                       "<style>body{font:13px sans-serif;background:#111;color:#ddd}td{vertical-align:top;"
                       "padding:4px}img{width:400px;display:block}th{text-align:left;padding:4px}"
                       "pre{font:12px monospace;overflow-x:auto}small{color:#aaa}</style>\n"
                       "<h1>CoAGraphics transfer sheet</h1>\n<p>One scene under each place, hour and weather, at "
                       "the shipped settings: a lamp left of the character, a purple spell light right of it, "
                       "dark boards 150 and 400 yd away. Under each picture: the fog's opacity along a level ray, "
                       "and what each light adds in steps of 255 with its contrast in brackets. A harness render, "
                       "not the game.</p>\n<table>\n");
    std::string row;
    for (size_t i = 0; i < contexts.size(); ++i)
    {
        const Context& context = contexts[i];
        if (context.place != row)
        {
            std::fprintf(page, "%s<tr><th colspan=\"8\">%s</th></tr>\n<tr>", row.empty() ? "" : "</tr>\n",
                         context.place.c_str());
            row = context.place;
        }
        const ImageMeasure& m = images[i];
        std::fprintf(page,
                     "<td><img src=\"%s\"><b>%s, %s</b><br><small>fog %.0f%% at 150 yd, %.0f%% at 1000 yd<br>"
                     "lamp: air %+.0f (%.2f), ground 3 yd %+.0f (%.2f)<br>"
                     "spell: air %+.0f (%.2f), ground 3 yd %+.0f (%.2f)</small></td>\n",
                     TileName(context).c_str(), context.hour.c_str(), context.weather.c_str(),
                     100.0f * fog[i].opacityNear, 100.0f * fog[i].opacityFar, m.effect[kLampInTheAir].steps,
                     Contrast(m.effect[kLampInTheAir], m.airAboveTheLampUnlit), m.effect[kLampOnTheGround].steps,
                     Contrast(m.effect[kLampOnTheGround], m.groundByTheLampUnlit[kMatrixGroundDistance]),
                     m.effect[kSpellInTheAir].steps, Contrast(m.effect[kSpellInTheAir], m.airAboveTheSpellUnlit),
                     m.effect[kSpellOnTheGround].steps,
                     Contrast(m.effect[kSpellOnTheGround], m.groundByTheSpellUnlit[kMatrixGroundDistance]));
    }
    std::fprintf(page, "</tr>\n</table>\n<h2>Numbers</h2>\n<pre>%s</pre>\n", report.text.c_str());
    std::fclose(page);
}

struct ImageResults
{
    std::vector<ImageMeasure> baseline;
    Matrix matrix;
    unsigned skippedFrames = 0;
    bool deviceReached = false;
    bool placed = false;
};

bool MeasureImages(Harness& h, const std::wstring& outDir, const std::vector<Context>& contexts,
                   const Config& shipped, ImageResults& results)
{
    results.deviceReached = true;
    results.matrix.effects.assign(std::begin(kImageEffectNames), std::end(kImageEffectNames));
    results.matrix.contexts = contexts.size();
    for (const TestedSetting& setting : kImageSettings)
    {
        SettingEffects row;
        row.setting = &setting;
        Varied(shipped, setting, row.change);
        results.matrix.rows.push_back(row);
    }
    for (const Context& context : contexts)
    {
        SheetView view;
        view.harness = &h;
        view.context = &context;
        view.camera = CameraBehindTheCharacter(context);
        results.placed = PlaceProbes(view.camera, context, view.probes);
        if (!results.placed)
            return false;
        h.scene = SheetScene(context);
        h.clearColor = context.fog;
        const Image withoutFog = DrawnFrame(view, nullptr, shipped, true);
        view.nearBoardWithoutFog = LuminanceAround(withoutFog, view.probes.nearBoard, kBoardRadius);
        view.farBoardWithoutFog = LuminanceAround(withoutFog, view.probes.farBoard, kBoardRadius);
        const ImageMeasure baseline = MeasuredImage(view, shipped);
        results.baseline.push_back(baseline);
        const Image tile = SettledFrame(view, shipped, Lights::Both);
        SavePng(outDir + L"\\" + Wide(TileName(context)), tile.w, tile.h, tile.bgra);
        for (SettingEffects& row : results.matrix.rows)
        {
            std::string change;
            const ImageMeasure varied = MeasuredImage(view, Varied(shipped, *row.setting, change));
            for (int effect = 0; effect < kImageEffectCount; ++effect)
                row.changes.push_back(ChangeOfReading(baseline.effect[effect], varied.effect[effect]));
        }
        results.skippedFrames += view.skippedFrames;
        std::printf("measured %s\n", NameOf(context).c_str());
        std::fflush(stdout);
    }
    return true;
}

bool OnSheetDevice(const std::function<bool(Harness&)>& measure)
{
    WNDCLASSW windowClass = {};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = L"vfog_transfer";
    RegisterClassW(&windowClass);
    Harness h;
    h.window = CreateWindowW(windowClass.lpszClassName, L"vfog transfer", WS_OVERLAPPEDWINDOW, 0, 0, kTileWidth,
                             kTileHeight, nullptr, nullptr, windowClass.hInstance, nullptr);
    HMODULE module = LoadLibraryA("d3d9.dll");
    const auto create =
        module ? reinterpret_cast<IDirect3D9*(WINAPI*)(UINT)>(GetProcAddress(module, "Direct3DCreate9")) : nullptr;
    if (h.window && create)
        h.d3d = vf_test_wrap_direct3d9(create, D3D_SDK_VERSION);
    h.pp.Windowed = TRUE;
    h.pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    h.pp.BackBufferWidth = kTileWidth;
    h.pp.BackBufferHeight = kTileHeight;
    h.pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    h.pp.EnableAutoDepthStencil = TRUE;
    h.pp.AutoDepthStencilFormat = kClientDepthFormat;
    h.pp.hDeviceWindow = h.window;
    h.pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    const DWORD flags = D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE | D3DCREATE_FPU_PRESERVE;
    bool completed = false;
    if (h.d3d && SUCCEEDED(h.d3d->CreateDevice(0, D3DDEVTYPE_HAL, h.window, flags, &h.pp, &h.dev)) && h.dev)
        completed = measure(h);
    if (h.dev)
        h.dev->Release();
    if (h.d3d)
        h.d3d->Release();
    if (module)
        FreeLibrary(module);
    if (h.window)
        DestroyWindow(h.window);
    UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
    return completed;
}

int RunTransferSheet(const std::wstring& outDir, const std::string& dataPath, const std::string& contextsPath,
                     const std::wstring& ini, bool withImages)
{
    FogData data;
    std::vector<Context> contexts;
    if (!data.Load(dataPath) || !LoadContexts(contextsPath, contexts))
    {
        std::printf("fog data %s or contexts %s did not load\n", dataPath.c_str(), contextsPath.c_str());
        return 1;
    }
    CreateDirectoryW(outDir.c_str(), nullptr);
    const Config shipped = ShippedSettings(ini);
    vf_test_set_config(&shipped);
    Report report;
    std::vector<FogMeasure> fog;
    if (!AddFogSettingMatrix(report, outDir, data, contexts, shipped, fog))
    {
        std::printf("the output folder %ls cannot be written\n", outDir.c_str());
        return 1;
    }
    std::printf("%s", report.text.c_str());
    if (!withImages)
        return 0;
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ImageResults images;
    const bool measured = OnSheetDevice([&](Harness& h) {
        return vf_test_fog_data_loaded() && MeasureImages(h, outDir, contexts, shipped, images);
    });
    if (!measured)
    {
        std::printf("the image half did not run: %s\n",
                    images.deviceReached ? "a probe lies outside the view"
                                         : "no device, or no fogdata.bin beside the test DLL");
        CoUninitialize();
        return 1;
    }
    Report imageReport;
    std::FILE* baselineCsv = OpenInFolder(outDir, L"transfer-image-baseline.csv");
    std::FILE* sensitivityCsv = OpenInFolder(outDir, L"transfer-image-sensitivity.csv");
    if (baselineCsv && sensitivityCsv)
    {
        AddImageBaseline(imageReport, contexts, images.baseline, baselineCsv);
        AddSensitivity(imageReport, images.matrix, contexts, sensitivityCsv);
        AddOverlaps(imageReport, images.matrix);
        imageReport.Add("not on the sheet: %s\n", kSettingsTheSceneCannotShow);
        imageReport.Add("frames the fog skipped: %u\n", images.skippedFrames);
    }
    if (baselineCsv)
        std::fclose(baselineCsv);
    if (sensitivityCsv)
        std::fclose(sensitivityCsv);
    std::printf("%s", imageReport.text.c_str());
    report.text += imageReport.text;
    WritePage(outDir, contexts, fog, images.baseline, report);
    CoUninitialize();
    std::printf("\ntransfer sheet written to %ls\n", outDir.c_str());
    return 0;
}
}
