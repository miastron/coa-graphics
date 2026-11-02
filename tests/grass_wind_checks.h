#pragma once

namespace grass_wind_checks
{
constexpr size_t kPermutations = 6;
constexpr size_t kStockVariantCount = 3;
constexpr uint32_t kBlsMagic = 0x47585348;
constexpr uint32_t kBlsVersion = 0x10003;
constexpr size_t kBlsHeaderBytes = 12;
constexpr size_t kBlsEntryHeaderBytes = 16;
constexpr float kTolerance = 1.0e-5f;
constexpr float kTwoPi = 6.2831853f;
constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr unsigned kMotionFirstRegister = 35;
constexpr uint32_t kRegisterFill = 0xABABABABu;
constexpr size_t kUploadedFloats = engine::kDetailDoodadUploadedRegisters * 4;

struct StockShaders
{
    std::vector<DWORD> variants[kPermutations];
    bool loaded = false;
};

StockShaders LoadStock(const char* fileName)
{
    StockShaders out;
    const std::string path = std::string(GRASS_SHADER_DIR) + "\\" + fileName;
    FILE* file = std::fopen(path.c_str(), "rb");
    if (!file)
        return out;
    std::vector<unsigned char> bytes;
    unsigned char chunk[4096];
    size_t read = 0;
    while ((read = std::fread(chunk, 1, sizeof(chunk), file)) > 0)
        bytes.insert(bytes.end(), chunk, chunk + read);
    std::fclose(file);
    uint32_t header[3] = {};
    if (bytes.size() < kBlsHeaderBytes)
        return out;
    std::memcpy(header, bytes.data(), sizeof(header));
    if (header[0] != kBlsMagic || header[1] != kBlsVersion)
        return out;
    size_t at = kBlsHeaderBytes;
    for (size_t i = 0; i < kPermutations; ++i)
    {
        uint32_t size = 0;
        if (at + kBlsEntryHeaderBytes > bytes.size())
            return out;
        std::memcpy(&size, bytes.data() + at + kBlsEntryHeaderBytes - sizeof(size), sizeof(size));
        at += kBlsEntryHeaderBytes;
        if (at + size > bytes.size())
            return out;
        out.variants[i].resize(size / sizeof(DWORD));
        std::memcpy(out.variants[i].data(), bytes.data() + at, size);
        at += (size + 3) & ~3u;
    }
    out.loaded = true;
    return out;
}

std::string Disassembled(const ShaderCompiler& compiler, const DWORD* code)
{
    ID3DBlob* text = nullptr;
    const size_t bytes = ShaderTokenCount(code) * sizeof(DWORD);
    if (!compiler.disassemble || FAILED(compiler.disassemble(code, bytes, 0, nullptr, &text)) || !text)
        return std::string();
    std::string out(static_cast<const char*>(text->GetBufferPointer()), text->GetBufferSize());
    text->Release();
    return out;
}

bool Contains(const std::string& text, const char* needle)
{
    return text.find(needle) != std::string::npos;
}

void CheckStockBlobs(const StockShaders& vs20, const StockShaders& vs30)
{
    bool sized = vs20.loaded && vs30.loaded;
    for (const StockShaders* set : {&vs20, &vs30})
        for (const std::vector<DWORD>& variant : set->variants)
            sized = sized && !variant.empty() && ShaderTokenCount(variant.data()) == variant.size();
    Check(sized, "the shader length reader finds the end token of all twelve stock grass vertex shaders");
}

void CheckRewrite(const StockShaders& stock, const char* label, bool mustInject)
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    GrassShaderRewriter rewriter;
    rewriter.UseCompiler(compiler);
    rewriter.Enable(true);
    bool injected = true;
    bool assembled = true;
    for (size_t i = 0; i < kStockVariantCount; ++i)
    {
        const DWORD* original = stock.variants[i].data();
        const DWORD* result = rewriter.Rewrite(original);
        injected = injected && result != original;
        if (result != original)
        {
            const std::string text = Disassembled(compiler, result);
            assembled = assembled && result[0] == original[0] && Contains(text, "sincos") &&
                        Contains(text, "loop aL, i0") && Contains(text, "c104[aL]") && Contains(text, "c40[a0.x]") &&
                        Contains(text, "add r0, r0, c3");
        }
    }
    bool lightsUntouched = true;
    for (size_t i = kStockVariantCount; i < kPermutations; ++i)
        lightsUntouched = lightsUntouched && rewriter.Rewrite(stock.variants[i].data()) == stock.variants[i].data();
    std::printf("     %s grass shaders: %u rewritten, %u left stock\n", label, rewriter.Injected(), rewriter.Failed());
    if (mustInject)
        Check(injected && assembled && rewriter.Injected() == kStockVariantCount,
              "the three grass vertex shaders without point lights are reassembled with the motion block after the "
              "view transform");
    else
        Check(!injected || assembled, "a grass vertex shader that does not take the motion stays the stock one");
    Check(lightsUntouched, "the point light variants of the grass vertex shader, which read c14 and up, stay stock");
}

std::vector<DWORD> ForeignShaderOfLength(const ShaderCompiler& compiler, size_t bytes)
{
    std::string source = "vs_3_0\ndcl_position v0\ndcl_position o0\nmov o0, v0\n";
    auto assemble = [&](const std::string& text) {
        std::vector<DWORD> out;
        ID3DBlob* blob = nullptr;
        if (SUCCEEDED(compiler.assemble(text.c_str(), text.size(), "foreign", nullptr, nullptr, 0, &blob, nullptr)) &&
            blob)
        {
            out.resize(blob->GetBufferSize() / sizeof(DWORD));
            std::memcpy(out.data(), blob->GetBufferPointer(), out.size() * sizeof(DWORD));
            blob->Release();
        }
        return out;
    };
    const size_t base = assemble(source).size() * sizeof(DWORD);
    if (!base || base > bytes)
        return {};
    for (size_t padding = (bytes - base) / sizeof(DWORD); padding > 0; --padding)
        source += "nop\n";
    return assemble(source);
}

void CheckPassThrough(const StockShaders& stock)
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    GrassShaderRewriter off;
    off.UseCompiler(compiler);
    const DWORD* original = stock.variants[0].data();
    Check(off.Rewrite(original) == original, "grass shaders pass through untouched until the rewrite is enabled");

    GrassShaderRewriter on;
    on.UseCompiler(compiler);
    on.Enable(true);
    const DWORD* fullscreen = reinterpret_cast<const DWORD*>(g_vs_fullscreen);
    Check(on.Rewrite(fullscreen) == fullscreen && on.Rewrite(nullptr) == nullptr && on.Injected() == 0,
          "a vertex shader of another length or model passes through untouched");

    const std::vector<DWORD> foreign = ForeignShaderOfLength(compiler, stock.variants[0].size() * sizeof(DWORD));
    GrassShaderRewriter other;
    other.UseCompiler(compiler);
    other.Enable(true);
    const size_t logStart = ReadText(g_harnessLog).size();
    Check(foreign.size() == stock.variants[0].size() && other.Rewrite(foreign.data()) == foreign.data() &&
              other.Foreign() == 1 && other.Failed() == 0 && other.Injected() == 0,
          "another vertex shader of a grass length is recognised as not grass, left alone and not counted as a "
          "failure");
    const std::string logged = ReadText(g_harnessLog).substr(logStart);
    Check(logged.find("is not the grass shader (no defc13,0,1,0,0, dcl_normalv1") != std::string::npos,
          "a vertex shader of a grass length that is not grass logs the signature lines it lacks");

    GrassShaderRewriter lookalikeFirst;
    lookalikeFirst.UseCompiler(compiler);
    lookalikeFirst.Enable(true);
    const bool lookalikeLeftAlone = lookalikeFirst.Rewrite(foreign.data()) == foreign.data();
    const DWORD* later = lookalikeFirst.Rewrite(original);
    Check(foreign.size() == stock.variants[0].size() && lookalikeLeftAlone && later != original &&
              lookalikeFirst.Foreign() == 1 && lookalikeFirst.Injected() == 1,
          "the grass vertex shader is still rewritten after another shader of its length was seen and left alone");

    GrassShaderRewriter noCompiler;
    noCompiler.UseCompiler(ShaderCompiler());
    noCompiler.Enable(true);
    Check(noCompiler.Rewrite(original) == original && noCompiler.Rewrite(original) == original &&
              noCompiler.Failed() == 1,
          "without d3dcompiler_47 the stock grass stays and the failure is counted once");

    GrassShaderRewriter first;
    first.UseCompiler(compiler);
    first.Enable(true);
    const DWORD* once = first.Rewrite(original);
    Check(once != original && first.Rewrite(original) == once && first.Injected() == 1,
          "a grass shader is reassembled once and the result is reused");
}

unsigned HighestConstantRegister(const std::string& text)
{
    unsigned highest = 0;
    for (size_t i = 0; i + 1 < text.size(); ++i)
        if (text[i] == 'c' && std::isdigit(static_cast<unsigned char>(text[i + 1])) &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i - 1]))))
            highest = std::max(highest, static_cast<unsigned>(std::strtoul(text.c_str() + i + 1, nullptr, 10)));
    return highest;
}

bool ReadsConstantRegister(const std::string& text, unsigned wanted)
{
    for (size_t i = 0; i + 1 < text.size(); ++i)
        if (text[i] == 'c' && std::isdigit(static_cast<unsigned char>(text[i + 1])) &&
            (i == 0 || !std::isalnum(static_cast<unsigned char>(text[i - 1]))) &&
            static_cast<unsigned>(std::strtoul(text.c_str() + i + 1, nullptr, 10)) == wanted)
            return true;
    return false;
}

void CheckStockRegistersStayFree(const StockShaders& stock)
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    unsigned highest = 0;
    for (size_t i = 0; i < kStockVariantCount; ++i)
        highest = std::max(highest, HighestConstantRegister(Disassembled(compiler, stock.variants[i].data())));
    Check(highest > 0 && highest < kMotionFirstRegister,
          "the three stock grass vertex shaders read no constant register from c35 up, where the motion block keeps "
          "its own");
    bool readsCopyRegister = false;
    for (size_t i = 0; i < kStockVariantCount; ++i)
    {
        const std::string text = Disassembled(compiler, stock.variants[i].data());
        readsCopyRegister = readsCopyRegister || ReadsConstantRegister(text, kGrassCopyRegister);
    }
    Check(!readsCopyRegister,
          "the three stock grass vertex shaders never read c20, the point light register that holds the copy offset");
}

wind::WindModel SteadyModel(float strength)
{
    wind::WindModel model;
    model.SetProfiles({wind_model_checks::GlobalRow(strength, strength)});
    return model;
}

GrassInputs InputsAt(const float camera[3], const float* player, const NearbyUnitFrame* units)
{
    GrassInputs in = {};
    std::memcpy(in.camera, camera, sizeof(in.camera));
    if (player)
    {
        std::memcpy(in.player, player, sizeof(in.player));
        in.havePlayer = true;
    }
    in.dt = kFrameSeconds;
    in.units = units;
    return in;
}

float Register(const GrassFrame& frame, size_t index, size_t lane)
{
    return frame.registers[index][lane];
}

void CheckFrameRegisters()
{
    const GrassWindSettings defaults = GrassWindSettingsFrom(Config());
    const float camera[3] = {100.0f, 200.0f, 30.0f};
    const float player[3] = {103.0f, 201.0f, 29.0f};
    GrassMotion motion;
    wind::WindModel model = SteadyModel(0.5f);
    GrassFrame frame = {};
    motion.Build(InputsAt(camera, player, nullptr), model, frame);
    const float phase = std::fmod(kFrameSeconds * defaults.flutterSpeed, kTwoPi);
    Check(Register(frame, 0, 0) == camera[0] && Register(frame, 0, 1) == camera[1] &&
              Register(frame, 0, 2) == camera[2] && std::fabs(Register(frame, 0, 3) - phase) < kTolerance,
          "the grass constants carry the camera and the flutter phase");
    Check(std::fabs(Register(frame, 1, 0) - defaults.anchor) < kTolerance &&
              std::fabs(Register(frame, 1, 1) - 1.0f / (1.0f - defaults.anchor)) < kTolerance &&
              Register(frame, 1, 2) == defaults.amplitude && Register(frame, 1, 3) == defaults.flutter,
          "the grass constants carry the anchor, its reciprocal, the amplitude and the flutter");
    Check(Register(frame, 2, 0) == 1.0f / kGrassGridSpacing && Register(frame, 2, 1) == 1.0f / kGrassGridSpacing &&
              Register(frame, 2, 2) == 8.5f && Register(frame, 2, 3) == 21.5f,
          "the wind grid origin snaps to whole cells around the player and the lookup is by cell centre");
    Check(Register(frame, 3, 0) == 0.0f && Register(frame, 3, 3) == defaults.force &&
              Register(frame, 4, 0) == player[0] && Register(frame, 4, 1) == player[1] &&
              Register(frame, 4, 2) == player[2],
          "the grass constants carry the parting force and the player as the centre of the unit range");

    GrassFrame fallback = {};
    GrassMotion other;
    wind::WindModel otherModel = SteadyModel(0.5f);
    other.Build(InputsAt(camera, nullptr, nullptr), otherModel, fallback);
    Check(Register(fallback, 4, 0) == camera[0] && Register(fallback, 4, 1) == camera[1] &&
              Register(fallback, 4, 2) == camera[2] && Register(fallback, 2, 2) == 8.5f &&
              Register(fallback, 2, 3) == 21.5f,
          "without a player the grass is centred on the camera");

    GrassWindSettings extreme = defaults;
    extreme.anchor = 5.0f;
    GrassMotion clamped;
    clamped.Configure(extreme);
    GrassFrame clampedFrame = {};
    wind::WindModel clampedModel = SteadyModel(0.5f);
    clamped.Build(InputsAt(camera, player, nullptr), clampedModel, clampedFrame);
    extreme.anchor = -2.0f;
    GrassMotion negative;
    negative.Configure(extreme);
    GrassFrame negativeFrame = {};
    wind::WindModel negativeModel = SteadyModel(0.5f);
    negative.Build(InputsAt(camera, player, nullptr), negativeModel, negativeFrame);
    Check(std::fabs(Register(clampedFrame, 1, 0) - 0.9f) < kTolerance &&
              std::fabs(Register(clampedFrame, 1, 1) - 10.0f) < 1.0e-3f && Register(negativeFrame, 1, 0) == 0.0f &&
              Register(negativeFrame, 1, 1) == 1.0f,
          "the anchor is held inside the range the shader divides by");

    GrassMotion phaseMotion;
    wind::WindModel phaseModel = SteadyModel(0.5f);
    GrassFrame phaseFrame = {};
    bool wrapped = true;
    for (int i = 0; i < 4000; ++i)
    {
        phaseMotion.Build(InputsAt(camera, player, nullptr), phaseModel, phaseFrame);
        wrapped = wrapped && Register(phaseFrame, 0, 3) >= 0.0f && Register(phaseFrame, 0, 3) < kTwoPi;
    }
    Check(wrapped, "the flutter phase stays inside one turn however long the game runs");
}

void CheckWindGrid()
{
    const GrassWindSettings defaults = GrassWindSettingsFrom(Config());
    const float camera[3] = {100.0f, 200.0f, 30.0f};
    const float player[3] = {103.0f, 201.0f, 29.0f};
    GrassMotion motion;
    wind::WindModel model = SteadyModel(0.5f);
    GrassFrame frame = {};
    motion.Build(InputsAt(camera, player, nullptr), model, frame);
    const float* grid = GrassGridRegisters(frame);
    bool cells = true;
    bool steady = true;
    for (int iy = 0; iy < kGrassGridSide; ++iy)
        for (int ix = 0; ix < kGrassGridSide; ++ix)
        {
            const float at[3] = {(8 + ix + 0.5f) * kGrassGridSpacing, (21 + iy + 0.5f) * kGrassGridSpacing, 0.0f};
            const wind::Sample sample = model.At(at);
            const float* cell = grid + (iy * kGrassGridSide + ix) * 4;
            cells = cells && std::fabs(cell[0] - sample.dirX * sample.strength) < kTolerance &&
                    std::fabs(cell[1] - sample.dirY * sample.strength) < kTolerance &&
                    std::fabs(cell[2] - sample.gust) < kTolerance && cell[3] == 0.0f;
            steady = steady && std::fabs(std::sqrt(cell[0] * cell[0] + cell[1] * cell[1]) - 0.5f) < kTolerance;
        }
    Check(cells && steady,
          "each cell of the wind grid holds the wind model's direction times strength and its gust at the cell centre");

    GrassWindSettings calm = defaults;
    calm.wind = false;
    GrassMotion still;
    still.Configure(calm);
    GrassFrame stillFrame = {};
    wind::WindModel stillModel = SteadyModel(0.5f);
    still.Build(InputsAt(camera, player, nullptr), stillModel, stillFrame);
    bool zero = true;
    for (size_t i = 0; i < kGrassGridRegisters * 4; ++i)
        zero = zero && GrassGridRegisters(stillFrame)[i] == 0.0f;
    Check(zero && Register(stillFrame, 1, 2) == 0.0f && Register(stillFrame, 1, 3) == 0.0f &&
              Register(stillFrame, 3, 3) == defaults.force,
          "with the wind off the grid, the amplitude and the flutter are zero and the parting stays");

    GrassMotion eased;
    wind::WindModel easedModel = SteadyModel(0.5f);
    GrassFrame easedFrame = {};
    eased.Build(InputsAt(camera, player, nullptr), easedModel, easedFrame);
    const float before = GrassGridRegisters(easedFrame)[0] * GrassGridRegisters(easedFrame)[0] +
                         GrassGridRegisters(easedFrame)[1] * GrassGridRegisters(easedFrame)[1];
    GrassWindSettings gale = defaults;
    gale.windStrength = 2.0f;
    eased.Configure(gale);
    eased.Build(InputsAt(camera, player, nullptr), easedModel, easedFrame);
    const float first = GrassGridRegisters(easedFrame)[0] * GrassGridRegisters(easedFrame)[0] +
                        GrassGridRegisters(easedFrame)[1] * GrassGridRegisters(easedFrame)[1];
    for (int i = 0; i < 600; ++i)
        eased.Build(InputsAt(camera, player, nullptr), easedModel, easedFrame);
    const float last = GrassGridRegisters(easedFrame)[0] * GrassGridRegisters(easedFrame)[0] +
                       GrassGridRegisters(easedFrame)[1] * GrassGridRegisters(easedFrame)[1];
    Check(first > before * 1.0001f && first < before * 2.0f && std::fabs(std::sqrt(last) - 1.0f) < 0.02f,
          "a change of the wind reaches the grid gradually and arrives within a few seconds");

    GrassMotion walker;
    wind::WindModel walkerModel = SteadyModel(0.5f);
    GrassFrame walkerFrame = {};
    float travelled[3] = {0.0f, 0.0f, 0.0f};
    for (int step = 0; step < 8; ++step)
    {
        travelled[0] = step * 8.0f * kGrassGridSide;
        walker.Build(InputsAt(travelled, travelled, nullptr), walkerModel, walkerFrame);
    }
    const size_t crowded = walker.TrackedCells();
    for (int i = 0; i < 400; ++i)
        walker.Build(InputsAt(travelled, travelled, nullptr), walkerModel, walkerFrame);
    Check(crowded > kGrassGridRegisters * 4 && walker.TrackedCells() <= kGrassGridRegisters * 4 &&
              walker.TrackedCells() >= kGrassGridRegisters,
          "cells the player has left are forgotten once they are a few seconds old and too many are kept, so the "
          "cell table does not grow");

    GrassMotion timed;
    wind::WindModel timedModel = SteadyModel(0.5f);
    GrassFrame timedFrame = {};
    GrassInputs bad = InputsAt(camera, player, nullptr);
    bad.dt = std::numeric_limits<float>::quiet_NaN();
    timed.Build(bad, timedModel, timedFrame);
    bad.dt = -1.0f;
    timed.Build(bad, timedModel, timedFrame);
    bad.dt = 9.0f;
    timed.Build(bad, timedModel, timedFrame);
    const float expectedPhase = std::fmod(3.0f * kFrameSeconds * defaults.flutterSpeed, kTwoPi);
    Check(std::fabs(Register(timedFrame, 0, 3) - expectedPhase) < 1.0e-4f,
          "a frame time that is negative, huge or not a number counts as one frame");
}

void CheckParting()
{
    const GrassWindSettings defaults = GrassWindSettingsFrom(Config());
    const float camera[3] = {0.0f, 0.0f, 10.0f};
    NearbyUnitFrame units;
    const NearbyUnit list[] = {
        {1, {10.0f, 0.0f, 5.0f}, 0.4f, false},
        {2, {12.0f, 1.0f, 6.0f}, 0.2f, false},
        {3, {14.0f, -1.0f, 7.0f}, 0.4f, true},
    };
    for (const NearbyUnit& unit : list)
        engine::SelectNearbyUnit(units, unit, camera, kGrassActorRange);
    GrassMotion motion;
    wind::WindModel model = SteadyModel(0.5f);
    GrassFrame frame = {};
    motion.Build(InputsAt(camera, nullptr, &units), model, frame);
    const float* actors = GrassActorRegisters(frame);
    const float radii[] = {0.4f * defaults.radiusScale, defaults.minRadius,
                           0.4f * defaults.radiusScale * defaults.mountedScale};
    bool listed = true;
    for (size_t i = 0; i < 3; ++i)
    {
        const float* a = actors + i * 4;
        listed = listed && a[0] == list[i].position[0] && a[1] == list[i].position[1] &&
                 a[2] == list[i].position[2] && std::fabs(a[3] - radii[i]) < kTolerance;
    }
    bool unused = true;
    for (size_t i = 3; i < kGrassActorRegisters; ++i)
    {
        const float* a = actors + i * 4;
        unused = unused && a[0] == kGrassFarAway && a[1] == kGrassFarAway && a[2] == kGrassFarAway && a[3] == 0.0f;
    }
    Check(listed && unused,
          "every nearby unit becomes an actor with its position and a radius of its collision radius times the "
          "scale, the minimum radius for small ones and the mounted factor for riders; unused slots hold nothing");

    GrassWindSettings off = defaults;
    off.parting = false;
    GrassMotion apart;
    apart.Configure(off);
    GrassFrame offFrame = {};
    wind::WindModel offModel = SteadyModel(0.5f);
    apart.Build(InputsAt(camera, nullptr, &units), offModel, offFrame);
    bool empty = true;
    for (size_t i = 0; i < kGrassActorRegisters; ++i)
        empty = empty && GrassActorRegisters(offFrame)[i * 4 + 3] == 0.0f;
    Check(empty && Register(offFrame, 3, 3) == 0.0f, "with parting off no unit and no force reach the shader");

    NearbyUnitFrame crowd;
    for (uint32_t i = 0; i < kMaxNearbyUnits + 4; ++i)
        engine::SelectNearbyUnit(crowd, {100 + i, {1.0f + i, 0.0f, 0.0f}, 0.5f, false}, camera, kGrassActorRange);
    GrassMotion crowded;
    GrassFrame crowdFrame = {};
    wind::WindModel crowdModel = SteadyModel(0.5f);
    crowded.Build(InputsAt(camera, nullptr, &crowd), crowdModel, crowdFrame);
    const float* last = GrassActorRegisters(crowdFrame) + (kGrassActorRegisters - 1) * 4;
    Check(crowd.count == kMaxNearbyUnits && last[0] == 1.0f + (kMaxNearbyUnits - 1) && last[3] > 0.0f,
          "a crowd fills every actor slot with its nearest units");
}

void CheckSettingsMapping()
{
    Config c;
    c.grassWind = false;
    c.grassWindAmplitude = 1.5f;
    c.grassWindFlutter = 0.3f;
    c.grassWindFlutterSpeed = 7.0f;
    c.grassWindAnchor = 0.4f;
    c.windStrength = 2.5f;
    c.grassParting = false;
    c.grassPartingForce = 1.25f;
    c.grassPartingRadiusScale = 3.5f;
    c.grassPartingMinRadius = 1.5f;
    c.grassPartingMountedScale = 2.75f;
    const GrassWindSettings s = GrassWindSettingsFrom(c);
    Check(!s.wind && s.amplitude == 1.5f && s.flutter == 0.3f && s.flutterSpeed == 7.0f && s.anchor == 0.4f &&
              s.windStrength == 2.5f && !s.parting && s.force == 1.25f && s.radiusScale == 3.5f &&
              s.minRadius == 1.5f && s.mountedScale == 2.75f,
          "each grass setting takes its value from its own key");
}

struct SetupCall
{
    int calls = 0;
    const float* matrix = nullptr;
    int index = -1;
};

SetupCall g_setupCall;
std::vector<float> g_uploaded;
unsigned g_uploadCalls = 0;
unsigned g_gridUploads = 0;
unsigned g_uploadFirst = 0;
unsigned g_uploadCount = 0;
float* g_initializeRegisters = nullptr;
int g_initializeCalls = 0;

void __cdecl FakeInitializeShaderConstants()
{
    ++g_initializeCalls;
    std::memset(g_initializeRegisters, 0xAB, kUploadedFloats * sizeof(float));
}

void __cdecl FakeSetupShader(const float* world, int index)
{
    ++g_setupCall.calls;
    g_setupCall.matrix = world;
    g_setupCall.index = index;
}

void FakeUpload(unsigned first, const float* data, unsigned count)
{
    ++g_uploadCalls;
    if (first != engine::kGrassGridFirstRegister)
        return;
    ++g_gridUploads;
    g_uploadFirst = first;
    g_uploadCount = count;
    g_uploaded.assign(data, data + count * 4);
}

void CheckThunks()
{
    std::vector<float> block(kUploadedFloats, 0.0f);
    g_initializeRegisters = block.data();
    g_initializeCalls = 0;
    g_setupCall = {};
    g_uploadCalls = 0;
    g_gridUploads = 0;
    g_uploaded.clear();

    const float camera[3] = {100.0f, 200.0f, 30.0f};
    const float player[3] = {103.0f, 201.0f, 29.0f};
    NearbyUnitFrame units;
    engine::SelectNearbyUnit(units, {1, {110.0f, 200.0f, 29.0f}, 0.4f, false}, player, kGrassActorRange);
    const GrassInputs inputs = InputsAt(camera, player, &units);
    const void* setupThunk = nullptr;
    const void* constantsThunk =
        vf_test_grass_thunks(reinterpret_cast<uintptr_t>(&FakeInitializeShaderConstants),
                             reinterpret_cast<uintptr_t>(&FakeSetupShader),
                             block.data() + engine::kDetailDoodadFirstFreeRegister * 4, &inputs, &FakeUpload,
                             &setupThunk);
    const float world[16] = {};
    using SetupFn = void(__cdecl*)(const float*, int);
    reinterpret_cast<SetupFn>(setupThunk)(world, 0);
    Check(g_setupCall.calls == 1 && g_setupCall.matrix == world && g_setupCall.index == 0 && g_uploadCalls == 0,
          "the chunk thunk runs the client's shader setup with its own arguments and uploads nothing before a frame "
          "was built");

    reinterpret_cast<void(__cdecl*)()>(constantsThunk)();
    bool untouched = true;
    for (size_t i = 0; i < kUploadedFloats; ++i)
    {
        const size_t first = engine::kDetailDoodadFirstFreeRegister * 4;
        const bool motion = i >= first && i < first + kGrassConstantRegisters * 4;
        uint32_t bits;
        std::memcpy(&bits, &block[i], sizeof(bits));
        if (!motion && bits != kRegisterFill)
            untouched = false;
    }
    const float* motionStart = block.data() + engine::kDetailDoodadFirstFreeRegister * 4;
    Check(g_initializeCalls == 1 && untouched && motionStart[0] == camera[0] && motionStart[1] == camera[1] &&
              motionStart[2] == camera[2] && motionStart[4 * 4] == player[0],
          "the grass constants thunk runs the client's setup first, then rewrites only c14 to c18");

    wind::WindModel expectedModel;
    GrassMotion expected;
    GrassInputs timed = inputs;
    timed.dt = kFrameSeconds;
    GrassFrame expectedFrame = {};
    expected.Build(timed, expectedModel, expectedFrame);
    reinterpret_cast<SetupFn>(setupThunk)(world, 0);
    bool same = g_uploaded.size() == kGrassUploadRegisters * 4;
    for (size_t i = 0; same && i < g_uploaded.size(); ++i)
        same = std::fabs(g_uploaded[i] - (&expectedFrame.upload[0][0])[i]) < 1.0e-3f;
    Check(g_setupCall.calls == 2 && g_gridUploads == 1 && g_uploadCalls == 2 &&
              g_uploadFirst == engine::kGrassGridFirstRegister && g_uploadCount == kGrassUploadRegisters && same,
          "after the client's shader setup of a chunk the thunk uploads the wind grid and the units from c40 on, and "
          "a table of fallback entries while the doodad hooks are not installed");

    reinterpret_cast<SetupFn>(setupThunk)(world, 1);
    Check(g_setupCall.calls == 3 && g_setupCall.index == 1 && g_uploadCalls == 2,
          "the point light variant of the grass shader gets no grid and the client's call still runs");
}

void CheckGuards()
{
    size_t count = 0;
    const ClientCodeGuard* guards = engine::GrassWindGuards(count);
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
    const bool matches = engine::GrassWindCodeMatches(view);
    bool everyByteMatters = true;
    for (size_t i = 0; i < count; ++i)
    {
        std::vector<unsigned char> changed = bytes;
        changed[guards[i].address - low] ^= 0xFF;
        const ClientCodeView altered = {low, changed.data(), changed.size()};
        everyByteMatters = everyByteMatters && !engine::GrassWindCodeMatches(altered);
    }
    Check(matches && everyByteMatters,
          "the grass wind hooks need every guarded byte of the client's detail doodad code");
}

bool SameGrassSettings(const GrassWindSettings& a, const GrassWindSettings& b)
{
    return a.wind == b.wind && a.amplitude == b.amplitude && a.flutter == b.flutter &&
           a.flutterSpeed == b.flutterSpeed && a.anchor == b.anchor && a.windStrength == b.windStrength &&
           a.parting == b.parting && a.force == b.force && a.radiusScale == b.radiusScale &&
           a.minRadius == b.minRadius && a.mountedScale == b.mountedScale;
}

void CheckShippedSettings(const std::wstring& shippedIni)
{
    ConfigStore store;
    store.Load(NarrowPath(shippedIni));
    Check(SameGrassSettings(GrassWindSettingsFrom(store.Get()), GrassWindSettingsFrom(Config())),
          "the shipped INI carries the grass wind and parting defaults");
}

void CheckGrassWind()
{
    const StockShaders vs20 = LoadStock("DetailDoodad_vs_2_0.bls");
    const StockShaders vs30 = LoadStock("DetailDoodad_vs_3_0.bls");
    Check(vs20.loaded && vs30.loaded, "the stock grass vertex shaders were read from the test data");
    if (!vs20.loaded || !vs30.loaded)
        return;
    CheckStockBlobs(vs20, vs30);
    CheckStockRegistersStayFree(vs30);
    CheckRewrite(vs30, "vs_3_0", true);
    CheckRewrite(vs20, "vs_2_0", false);
    CheckPassThrough(vs30);
    CheckFrameRegisters();
    CheckWindGrid();
    CheckParting();
    CheckSettingsMapping();
    CheckThunks();
    CheckGuards();
}

constexpr UINT kMotionTargetSide = 128;
constexpr float kMotionRangeYards = 2.0f;
constexpr float kMotionPixelsPerYard = kMotionTargetSide / (2.0f * kMotionRangeYards);
constexpr float kMotionPixelTolerance = 0.75f;
constexpr float kMotionPointSize = 3.0f;
constexpr UINT kMotionViewRegisters = 14;

struct MotionVertex
{
    float position[3];
    float normal[3];
    DWORD colour;
    float uv[2];
};

struct MotionRig
{
    IDirect3DDevice9* device = nullptr;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DPixelShader9* alphaPixel = nullptr;
    IDirect3DVertexShader9* vertex = nullptr;
    bool showAlpha = false;
    unsigned peak = 0;
    float normal[3] = {0.0f, 0.0f, 1.0f};

    ~MotionRig()
    {
        if (state)
            state->Apply();
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        for (IUnknown* resource : {static_cast<IUnknown*>(state), static_cast<IUnknown*>(savedTarget),
                                   static_cast<IUnknown*>(savedDepth), static_cast<IUnknown*>(target),
                                   static_cast<IUnknown*>(readback), static_cast<IUnknown*>(declaration),
                                   static_cast<IUnknown*>(pixel), static_cast<IUnknown*>(alphaPixel),
                                   static_cast<IUnknown*>(vertex)})
            if (resource)
                resource->Release();
    }

    bool Create(const std::vector<DWORD>& stock)
    {
        const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
            {0, 24, D3DDECLTYPE_D3DCOLOR, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
            {0, 28, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            D3DDECL_END()};
        const std::string whitePixel = "ps_3_0\ndef c0, 1, 1, 1, 1\nmov oC0, c0\n";
        const ShaderCompiler compiler = LoadSystemShaderCompiler();
        ID3DBlob* blob = nullptr;
        if (!compiler.assemble ||
            FAILED(compiler.assemble(whitePixel.c_str(), whitePixel.size(), "motionPixel", nullptr, nullptr, 0, &blob,
                                     nullptr)) ||
            !blob)
            return false;
        const HRESULT pixelResult =
            device->CreatePixelShader(static_cast<const DWORD*>(blob->GetBufferPointer()), &pixel);
        blob->Release();
        const std::string alphaPixelSource = "ps_3_0\ndcl_color0 v0\nmov oC0, v0.wwww\n";
        ID3DBlob* alphaBlob = nullptr;
        if (FAILED(compiler.assemble(alphaPixelSource.c_str(), alphaPixelSource.size(), "motionAlpha", nullptr,
                                     nullptr, 0, &alphaBlob, nullptr)) ||
            !alphaBlob)
            return false;
        const HRESULT alphaResult =
            device->CreatePixelShader(static_cast<const DWORD*>(alphaBlob->GetBufferPointer()), &alphaPixel);
        alphaBlob->Release();
        return SUCCEEDED(pixelResult) && SUCCEEDED(alphaResult) &&
               SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state)) &&
               SUCCEEDED(device->GetRenderTarget(0, &savedTarget)) &&
               SUCCEEDED(device->GetDepthStencilSurface(&savedDepth)) &&
               SUCCEEDED(device->CreateRenderTarget(kMotionTargetSide, kMotionTargetSide, D3DFMT_A8R8G8B8,
                                                    D3DMULTISAMPLE_NONE, 0, FALSE, &target, nullptr)) &&
               SUCCEEDED(device->CreateOffscreenPlainSurface(kMotionTargetSide, kMotionTargetSide, D3DFMT_A8R8G8B8,
                                                             D3DPOOL_SYSTEMMEM, &readback, nullptr)) &&
               SUCCEEDED(device->CreateVertexDeclaration(elements, &declaration)) &&
               SUCCEEDED(device->CreateVertexShader(stock.data(), &vertex));
    }

    void SetViewConstants(float translationX, bool heightOnScreen = false, float distance = 0.0f,
                          float heightScale = 1.0f)
    {
        float view[kMotionViewRegisters][4] = {};
        view[0][0] = view[1][1] = view[2][2] = 1.0f;
        view[3][0] = translationX + distance;
        view[3][3] = 1.0f;
        view[4][0] = view[5][1] = 1.0f / kMotionRangeYards;
        view[7][0] = -distance / kMotionRangeYards;
        view[7][2] = 0.5f;
        view[7][3] = 1.0f;
        view[8][1] = view[8][2] = 1.0f;
        if (heightOnScreen)
            view[6][1] = heightScale / kMotionRangeYards;
        device->SetVertexShaderConstantF(0, &view[0][0], kMotionViewRegisters);
    }

    void SetCopyRegisters(const float (&first)[4], const float (&second)[4])
    {
        float registers[kGrassCopyRegisterCount][4];
        std::memcpy(registers[0], first, sizeof(first));
        std::memcpy(registers[1], second, sizeof(second));
        device->SetVertexShaderConstantF(kGrassCopyRegister, &registers[0][0], kGrassCopyRegisterCount);
    }

    void SetCopyOffset(float x, float y)
    {
        const float offset[4] = {x, y, 0.0f, 0.0f};
        const float none[4] = {};
        SetCopyRegisters(offset, none);
    }

    void SetFrame(const GrassFrame& frame)
    {
        SetCopyOffset(0.0f, 0.0f);
        device->SetVertexShaderConstantF(engine::kDetailDoodadFirstFreeRegister, &frame.registers[0][0],
                                         static_cast<UINT>(kGrassConstantRegisters));
        device->SetVertexShaderConstantF(engine::kGrassGridFirstRegister, &frame.upload[0][0],
                                         static_cast<UINT>(kGrassUploadRegisters));
        float table[kGrassDoodadEntries][4];
        GrassDoodads().BuildTable(nullptr, table);
        SetTable(table);
    }

    void SetTable(const float (&table)[kGrassDoodadEntries][4])
    {
        device->SetVertexShaderConstantF(kGrassDoodadFirstRegister, &table[0][0], kGrassDoodadEntries);
    }

    bool Draw(float translationX, bool binding, float (&centre)[2], DWORD colour = 0xFFFFFFFFu, float v = 0.0f)
    {
        device->SetRenderTarget(0, target);
        device->SetDepthStencilSurface(nullptr);
        const D3DVIEWPORT9 viewport = {0, 0, kMotionTargetSide, kMotionTargetSide, 0, 1};
        device->SetViewport(&viewport);
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_STENCILENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
        device->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);
        device->SetRenderState(D3DRS_POINTSIZE, *reinterpret_cast<const DWORD*>(&kMotionPointSize));
        device->SetVertexDeclaration(declaration);
        device->SetPixelShader(showAlpha ? alphaPixel : pixel);
        if (binding)
            device->SetVertexShader(vertex);
        const MotionVertex point = {{-translationX, 0.0f, 0.0f}, {normal[0], normal[1], normal[2]}, colour,
                                    {0.0f, v}};
        const HRESULT drawn = device->DrawPrimitiveUP(D3DPT_POINTLIST, 1, &point, sizeof(point));
        D3DLOCKED_RECT locked = {};
        if (FAILED(drawn) || FAILED(device->GetRenderTargetData(target, readback)) ||
            FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        double sumX = 0.0;
        double sumY = 0.0;
        unsigned lit = 0;
        peak = 0;
        for (UINT y = 0; y < kMotionTargetSide; ++y)
            for (UINT x = 0; x < kMotionTargetSide; ++x)
            {
                const DWORD pixelValue =
                    reinterpret_cast<const DWORD*>(static_cast<const BYTE*>(locked.pBits) + y * locked.Pitch)[x];
                peak = std::max(peak, static_cast<unsigned>((pixelValue >> 16) & 0xFFu));
                if ((pixelValue & 0xFF0000u) > 0x800000u)
                {
                    sumX += x;
                    sumY += y;
                    ++lit;
                }
            }
        readback->UnlockRect();
        if (!lit)
            return false;
        centre[0] = static_cast<float>(sumX / lit);
        centre[1] = static_cast<float>(sumY / lit);
        return true;
    }
};

GrassFrame MotionFrame(const GrassWindSettings& settings, const NearbyUnitFrame* units)
{
    GrassMotion motion;
    motion.Configure(settings);
    wind::WindModel model = SteadyModel(0.5f);
    const float origin[3] = {0.0f, 0.0f, 0.0f};
    GrassFrame frame = {};
    motion.Build(InputsAt(origin, origin, units), model, frame);
    return frame;
}

void WindDisplacement(const GrassFrame& frame, float amplitude, float (&out)[2])
{
    const float* grid = GrassGridRegisters(frame);
    const int base = 3;
    out[0] = out[1] = 0.0f;
    for (int corner = 0; corner < 4; ++corner)
    {
        const float* cell = grid + ((base + corner / 2) * kGrassGridSide + base + corner % 2) * 4;
        out[0] += 0.25f * cell[0] * amplitude;
        out[1] += 0.25f * cell[1] * amplitude;
    }
}

void PushDisplacement(const NearbyUnit& unit, float radius, float force, float (&out)[2])
{
    const float dx = -unit.position[0];
    const float dy = -unit.position[1];
    const float squared = dx * dx + dy * dy + 0.001f;
    const float inverse = 1.0f / std::sqrt(squared);
    const float falloff = std::max(1.0f - squared * inverse / radius, 0.0f);
    out[0] = dx * falloff * inverse * force;
    out[1] = dy * falloff * inverse * force;
}

bool ShiftedBy(const float (&centre)[2], const float (&baseline)[2], const float (&displacement)[2])
{
    const float expectedX = displacement[0] * kMotionPixelsPerYard;
    const float expectedY = -displacement[1] * kMotionPixelsPerYard;
    return std::fabs(centre[0] - baseline[0] - expectedX) < kMotionPixelTolerance &&
           std::fabs(centre[1] - baseline[1] - expectedY) < kMotionPixelTolerance;
}

void CheckMotionOnTheDevice(IDirect3DDevice9* dev)
{
    const StockShaders stock = LoadStock("DetailDoodad_vs_3_0.bls");
    if (!stock.loaded)
        return;
    vf_test_enable_grass_shaders(1);
    GrassWindSettings quiet = GrassWindSettingsFrom(Config());
    quiet.wind = false;
    quiet.parting = false;
    GrassWindSettings windOnly = quiet;
    windOnly.wind = true;
    windOnly.flutter = 0.0f;
    GrassWindSettings partOnly = quiet;
    partOnly.parting = true;
    GrassWindSettings both = windOnly;
    both.parting = true;
    NearbyUnitFrame units;
    const NearbyUnit neighbour = {1, {0.3f, 0.0f, 0.0f}, 0.25f, false};
    engine::SelectNearbyUnit(units, neighbour, neighbour.position, kGrassActorRange);
    const float zero[3] = {};
    NearbyUnitFrame nobody;
    engine::SelectNearbyUnit(nobody, {2, {0.0f, 0.0f, 50.0f}, 0.25f, false}, zero, kGrassActorRange);

    const GrassFrame quietFrame = MotionFrame(quiet, nullptr);
    const GrassFrame windFrame = MotionFrame(windOnly, nullptr);
    const GrassFrame partFrame = MotionFrame(partOnly, &units);
    const GrassFrame bothFrame = MotionFrame(both, &units);

    for (size_t variant = 0; variant < kStockVariantCount; ++variant)
    {
        MotionRig rig;
        rig.device = dev;
        char label[96];
        std::snprintf(label, sizeof(label), "grass shader variant %u", static_cast<unsigned>(variant));
        if (!rig.Create(stock.variants[variant]))
        {
            Check(false, "the grass motion rig could be created on the device");
            continue;
        }
        dev->SetVertexShader(nullptr);
        float baseline[2] = {};
        rig.SetViewConstants(0.0f);
        rig.SetFrame(quietFrame);
        const bool drewBaseline = rig.Draw(0.0f, true, baseline);

        float windDisplacement[2];
        WindDisplacement(windFrame, windOnly.amplitude, windDisplacement);
        float windCentre[2] = {};
        rig.SetFrame(windFrame);
        const bool drewWind = rig.Draw(0.0f, false, windCentre);

        float pushDisplacement[2];
        PushDisplacement(neighbour, std::max(neighbour.radius * partOnly.radiusScale, partOnly.minRadius),
                         partOnly.force, pushDisplacement);
        float pushCentre[2] = {};
        rig.SetFrame(partFrame);
        const bool drewPush = rig.Draw(0.0f, false, pushCentre);

        float bothCentre[2] = {};
        rig.SetFrame(bothFrame);
        const bool drewBoth = rig.Draw(0.0f, false, bothCentre);
        const float summed[2] = {windDisplacement[0] + pushDisplacement[0], windDisplacement[1] + pushDisplacement[1]};

        float otherChunk[2] = {};
        rig.SetViewConstants(0.5f);
        rig.SetFrame(bothFrame);
        const bool drewOther = rig.Draw(0.5f, false, otherChunk);

        float unitsAway[2] = {};
        rig.SetViewConstants(0.0f);
        rig.SetFrame(MotionFrame(both, &nobody));
        const bool drewAway = rig.Draw(0.0f, false, unitsAway);

        std::printf("     %s: wind %.2f,%.2f yd -> %.2f,%.2f px; push %.2f,%.2f yd -> %.2f,%.2f px\n", label,
                    windDisplacement[0], windDisplacement[1], windCentre[0] - baseline[0],
                    windCentre[1] - baseline[1], pushDisplacement[0], pushDisplacement[1],
                    pushCentre[0] - baseline[0], pushCentre[1] - baseline[1]);
        Check(drewBaseline && drewWind && drewPush && drewBoth && drewOther && drewAway,
              "a point drawn with the rewritten grass shader reaches the target in every case of the motion check");
        Check(std::fabs(windDisplacement[0]) + std::fabs(windDisplacement[1]) > 0.05f &&
                  ShiftedBy(windCentre, baseline, windDisplacement),
              "the rewritten grass shader moves a blade tip by the wind grid times the amplitude");
        Check(std::fabs(pushDisplacement[0]) > 0.05f && ShiftedBy(pushCentre, baseline, pushDisplacement),
              "the rewritten grass shader leans a blade away from a unit beside it by the push strength");
        Check(ShiftedBy(bothCentre, baseline, summed),
              "the wind and the unit push add up in the rewritten grass shader");
        Check(ShiftedBy(otherChunk, baseline, summed),
              "another chunk that sets its own view constants and the same grid still gets the grid");
        Check(ShiftedBy(unitsAway, baseline, windDisplacement),
              "a unit far above the blade does not push it");
    }
    vf_test_enable_grass_shaders(0);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
}

DWORD TaggedColour(unsigned index)
{
    return (0xFF806040u & ~kGrassTagMask) | GrassEntryTag(index);
}

void ScaledDisplacement(const float (&wind)[2], const float (&push)[2], float windScale, float pushScale,
                        float weight, float (&out)[2])
{
    out[0] = weight * (wind[0] * windScale + push[0] * pushScale);
    out[1] = weight * (wind[1] * windScale + push[1] * pushScale);
}

void CheckBendOnTheDevice(IDirect3DDevice9* dev)
{
    const StockShaders stock = LoadStock("DetailDoodad_vs_3_0.bls");
    if (!stock.loaded)
        return;
    vf_test_enable_grass_shaders(1);
    constexpr float kBladeV = 0.75f;
    GrassWindSettings quiet = GrassWindSettingsFrom(Config());
    quiet.wind = false;
    quiet.parting = false;
    quiet.anchor = 0.0f;
    GrassWindSettings bent = quiet;
    bent.wind = true;
    bent.parting = true;
    bent.flutter = 0.0f;
    bent.modelBend = true;
    GrassWindSettings straight = bent;
    straight.modelBend = false;
    NearbyUnitFrame units;
    const NearbyUnit neighbour = {1, {0.3f, 0.0f, 0.0f}, 0.25f, false};
    engine::SelectNearbyUnit(units, neighbour, neighbour.position, kGrassActorRange);
    const GrassFrame quietFrame = MotionFrame(quiet, nullptr);
    const GrassFrame bentFrame = MotionFrame(bent, &units);
    const GrassFrame straightFrame = MotionFrame(straight, &units);

    float wind[2];
    WindDisplacement(bentFrame, bent.amplitude, wind);
    float push[2];
    PushDisplacement(neighbour, std::max(neighbour.radius * bent.radiusScale, bent.minRadius), bent.force, push);

    float table[kGrassDoodadEntries][4];
    GrassDoodads().BuildTable(nullptr, table);
    const float rooted[4] = {kBladeV, 1.0f, 1.0f, 1.0f};
    const float fromBelow[4] = {0.25f, 2.0f, 1.0f, 1.0f};
    const float windHalf[4] = {0.25f, 2.0f, 0.5f, 0.0f};
    std::memcpy(table[3], rooted, sizeof(rooted));
    std::memcpy(table[17], fromBelow, sizeof(fromBelow));
    std::memcpy(table[30], windHalf, sizeof(windHalf));

    for (size_t variant = 0; variant < kStockVariantCount; ++variant)
    {
        MotionRig rig;
        rig.device = dev;
        if (!rig.Create(stock.variants[variant]))
        {
            Check(false, "the grass motion rig could be created on the device");
            continue;
        }
        dev->SetVertexShader(nullptr);
        rig.SetViewConstants(0.0f);
        float baseline[2] = {};
        rig.SetFrame(quietFrame);
        rig.SetTable(table);
        const bool drewBaseline = rig.Draw(0.0f, true, baseline, TaggedColour(17), kBladeV);

        float full[2] = {};
        float half[2] = {};
        float atRoot[2] = {};
        float white[2] = {};
        float ignored[2] = {};
        rig.SetFrame(bentFrame);
        rig.SetTable(table);
        const bool drewFull = rig.Draw(0.0f, false, full, TaggedColour(17), kBladeV);
        const bool drewHalf = rig.Draw(0.0f, false, half, TaggedColour(30), kBladeV);
        const bool drewRoot = rig.Draw(0.0f, false, atRoot, TaggedColour(3), kBladeV);
        const bool drewWhite = rig.Draw(0.0f, false, white, 0xFFFFFFFFu, kBladeV);
        rig.SetFrame(straightFrame);
        rig.SetTable(table);
        const bool drewIgnored = rig.Draw(0.0f, false, ignored, TaggedColour(17), kBladeV);

        float expectedFull[2];
        float expectedHalf[2];
        float expectedRoot[2] = {};
        float expectedFallback[2];
        ScaledDisplacement(wind, push, 1.0f, 1.0f, 1.0f, expectedFull);
        ScaledDisplacement(wind, push, 0.5f, 0.0f, 1.0f, expectedHalf);
        ScaledDisplacement(wind, push, 1.0f, 1.0f, 0.0625f, expectedFallback);
        std::printf("     grass shader variant %u: tagged entries -> %.2f,%.2f px, %.2f,%.2f px, %.2f,%.2f px\n",
                    static_cast<unsigned>(variant), full[0] - baseline[0], full[1] - baseline[1],
                    half[0] - baseline[0], half[1] - baseline[1], atRoot[0] - baseline[0], atRoot[1] - baseline[1]);
        Check(drewBaseline && drewFull && drewHalf && drewRoot && drewWhite && drewIgnored,
              "a tagged point drawn with the rewritten grass shader reaches the target in every case of the bend "
              "check");
        Check(ShiftedBy(full, baseline, expectedFull),
              "a blade tagged with an entry whose root lies below it bends by the whole wind and push");
        Check(ShiftedBy(half, baseline, expectedHalf),
              "an entry's wind scale and push scale multiply the wind and the push of its blades");
        Check(ShiftedBy(atRoot, baseline, expectedRoot),
              "a blade at the root row of its entry does not move");
        Check(ShiftedBy(white, baseline, expectedFallback),
              "a blade with a white colour takes the fallback entry, root at row 1 and tip at row 0");
        Check(ShiftedBy(ignored, baseline, expectedFallback),
              "with the bend by model off the shader ignores the tag in the colour and takes the fallback entry");
    }
    vf_test_enable_grass_shaders(0);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
}

DWORD PlantColour(unsigned alpha)
{
    return (static_cast<DWORD>(alpha) << 24) | 0x00FFFFFFu;
}

struct CopyCase
{
    float normal[3];
    float offset[2];
};

void ExpectedCopyShift(const CopyCase& copy, unsigned alpha, float (&out)[2])
{
    const unsigned bits = alpha & ((1u << kGrassPlantBitCount) - 1);
    const GrassCopyOffset moved = GrassPlantOffset({copy.offset[0], copy.offset[1]}, bits);
    const float height = -(copy.normal[0] * moved.x + copy.normal[1] * moved.y) / copy.normal[2];
    out[0] = moved.x;
    out[1] = moved.y + height;
}

void CheckCopiesOnTheDevice(IDirect3DDevice9* dev)
{
    const StockShaders stock = LoadStock("DetailDoodad_vs_3_0.bls");
    if (!stock.loaded)
        return;
    vf_test_enable_grass_shaders(1);
    GrassWindSettings quiet = GrassWindSettingsFrom(Config());
    quiet.wind = false;
    quiet.parting = false;
    const GrassFrame quietFrame = MotionFrame(quiet, nullptr);
    const CopyCase cases[] = {
        {{0.0f, 0.0f, 1.0f}, {0.4f, -0.25f}},
        {{0.5f, 0.0f, 0.8660254f}, {0.4f, 0.0f}},
        {{0.0f, -0.6f, 0.8f}, {0.0f, 0.5f}},
    };
    const unsigned alphas[] = {0xFF, 0xC0, 0xD5, 0xEA, 0x00, 0x2A};

    for (size_t variant = 0; variant < kStockVariantCount; ++variant)
    {
        MotionRig rig;
        rig.device = dev;
        if (!rig.Create(stock.variants[variant]))
        {
            Check(false, "the grass motion rig could be created on the device");
            continue;
        }
        dev->SetVertexShader(nullptr);
        rig.SetViewConstants(0.0f, true);
        rig.SetFrame(quietFrame);
        float baseline[2] = {};
        const bool drewBaseline = rig.Draw(0.0f, true, baseline, PlantColour(0xFF));

        bool drewAll = drewBaseline;
        bool shiftedRight = true;
        bool alone = true;
        float worst[2] = {};
        for (const CopyCase& copy : cases)
            for (const unsigned alpha : alphas)
            {
                rig.normal[0] = copy.normal[0];
                rig.normal[1] = copy.normal[1];
                rig.normal[2] = copy.normal[2];
                rig.SetCopyOffset(copy.offset[0], copy.offset[1]);
                float centre[2] = {};
                drewAll = rig.Draw(0.0f, false, centre, PlantColour(alpha)) && drewAll;
                float expected[2];
                ExpectedCopyShift(copy, alpha, expected);
                if (!ShiftedBy(centre, baseline, expected))
                {
                    shiftedRight = false;
                    worst[0] = centre[0] - baseline[0];
                    worst[1] = centre[1] - baseline[1];
                }
                rig.SetCopyOffset(0.0f, 0.0f);
                float back[2] = {};
                const float none[2] = {};
                drewAll = rig.Draw(0.0f, false, back, PlantColour(alpha)) && drewAll;
                alone = alone && ShiftedBy(back, baseline, none);
            }
        std::printf("     grass shader variant %u: copy offsets %s (last miss %.2f,%.2f px)\n",
                    static_cast<unsigned>(variant), shiftedRight ? "all as expected" : "differ", worst[0], worst[1]);
        Check(drewAll, "a point drawn with the rewritten grass shader reaches the target in every case of the copy "
                       "check");
        Check(shiftedRight,
              "a copy offset in c20 turns by the plant's own angle from the low bits of its alpha, scales by its own "
              "length, and drops onto the plane of the ground that tilts under it, whatever the alpha's top bits");
        Check(alone, "a zero copy offset leaves a plant where the client put it whatever its alpha bits");
    }
    vf_test_enable_grass_shaders(0);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
}

void CheckSinkOnTheDevice(IDirect3DDevice9* dev)
{
    const StockShaders stock = LoadStock("DetailDoodad_vs_3_0.bls");
    if (!stock.loaded)
        return;
    vf_test_enable_grass_shaders(1);
    GrassWindSettings quiet = GrassWindSettingsFrom(Config());
    quiet.wind = false;
    quiet.parting = false;
    const GrassFrame quietFrame = MotionFrame(quiet, nullptr);
    const GrassDensitySettings settings = {2.0f, 60.0f, 1.0f};
    const GrassCopyWindow window = GrassCopyWindowFor(settings, 1);
    constexpr float kHeightScale = 0.5f;
    const unsigned patterns[] = {0, 13, 29, 46, 63};
    const float distances[] = {20.0f, 47.0f, 52.0f, 56.0f, 59.5f, 70.0f};
    const float registerOne[4] = {0.0f, 0.0f, window.outerDistance, window.width};
    const float registerTwo[4] = {window.inverseShare, window.inverseFade, 0.0f, 1.0f};
    const float none[4] = {};

    for (size_t variant = 0; variant < kStockVariantCount; ++variant)
    {
        MotionRig rig;
        rig.device = dev;
        if (!rig.Create(stock.variants[variant]))
        {
            Check(false, "the grass motion rig could be created on the device");
            continue;
        }
        dev->SetVertexShader(nullptr);
        bool drewAll = true;
        bool sunkRight = true;
        bool plainStays = true;
        bool bound = true;
        float worst[2] = {};
        for (const float distance : distances)
            for (const unsigned bits : patterns)
            {
                rig.SetViewConstants(0.0f, true, distance, kHeightScale);
                rig.SetFrame(quietFrame);
                float baseline[2] = {};
                drewAll = rig.Draw(0.0f, bound, baseline, PlantColour(0xC0 | bits)) && drewAll;
                bound = false;
                rig.SetCopyRegisters(none, none);
                float plain[2] = {};
                drewAll = rig.Draw(0.0f, false, plain, PlantColour(0xC0 | bits)) && drewAll;
                const float noShift[2] = {};
                plainStays = plainStays && ShiftedBy(plain, baseline, noShift);
                rig.SetCopyRegisters(registerOne, registerTwo);
                float sunk[2] = {};
                const bool drew = rig.Draw(0.0f, false, sunk, PlantColour(0xC0 | bits));
                const float depth = GrassPlantSink(window, bits, distance);
                const float expected[2] = {0.0f, -depth * kHeightScale};
                if (drew && !ShiftedBy(sunk, baseline, expected))
                {
                    sunkRight = false;
                    worst[0] = sunk[0] - baseline[0];
                    worst[1] = sunk[1] - baseline[1];
                }
                drewAll = drew && drewAll;
            }
        std::printf("     grass shader variant %u: sink %s (last miss %.2f,%.2f px)\n", static_cast<unsigned>(variant),
                    sunkRight ? "as expected" : "differs", worst[0], worst[1]);
        Check(drewAll, "a point drawn with the rewritten grass shader reaches the target in every case of the sink "
                       "check");
        Check(sunkRight,
              "a copy plant sinks into the ground along its normal by the amount its own threshold and its distance "
              "from the camera give, from nothing near the camera to the full depth beyond the copy's outer distance");
        Check(plainStays, "a draw without a copy window sinks nothing at any distance");
    }
    vf_test_enable_grass_shaders(0);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
}

void CheckAlphaOnTheDevice(IDirect3DDevice9* dev)
{
    const StockShaders stock = LoadStock("DetailDoodad_vs_3_0.bls");
    if (!stock.loaded)
        return;
    vf_test_enable_grass_shaders(1);
    GrassWindSettings quiet = GrassWindSettingsFrom(Config());
    quiet.wind = false;
    quiet.parting = false;
    const GrassFrame quietFrame = MotionFrame(quiet, nullptr);
    constexpr unsigned kFullLit = 255;
    for (size_t variant = 0; variant < kStockVariantCount; ++variant)
    {
        MotionRig rig;
        rig.device = dev;
        if (!rig.Create(stock.variants[variant]))
        {
            Check(false, "the grass motion rig could be created on the device");
            continue;
        }
        dev->SetVertexShader(nullptr);
        rig.SetViewConstants(0.0f, true);
        rig.SetFrame(quietFrame);
        rig.showAlpha = true;
        bool litExact = true;
        bool shadowExact = true;
        bool drewLit = true;
        float centre[2] = {};
        bool first = true;
        for (unsigned bits = 0; bits < (1u << kGrassPlantBitCount); ++bits)
        {
            drewLit = rig.Draw(0.0f, first, centre, PlantColour(0xC0 | bits)) && drewLit;
            first = false;
            litExact = litExact && rig.peak == kFullLit;
            rig.Draw(0.0f, false, centre, PlantColour(bits));
            shadowExact = shadowExact && rig.peak == 0;
        }
        Check(drewLit && litExact && shadowExact,
              "the rewritten grass shader gives the pixel shader the lit or shadowed alpha of the client, 1 or 0, "
              "whatever random bits the plant carries in the low six bits of its alpha");
    }
    vf_test_enable_grass_shaders(0);
    dev->SetVertexShader(nullptr);
    dev->SetPixelShader(nullptr);
}

void CheckShadersOnTheDevice(IDirect3DDevice9* dev)
{
    const StockShaders stock = LoadStock("DetailDoodad_vs_3_0.bls");
    if (!stock.loaded)
        return;
    vf_test_enable_grass_shaders(1);
    bool created = true;
    bool replaced = true;
    for (size_t i = 0; i < kStockVariantCount; ++i)
    {
        IDirect3DVertexShader9* shader = nullptr;
        const HRESULT result = dev->CreateVertexShader(stock.variants[i].data(), &shader);
        created = created && SUCCEEDED(result) && shader;
        if (!shader)
            continue;
        UINT size = 0;
        shader->GetFunction(nullptr, &size);
        replaced = replaced && size != stock.variants[i].size() * sizeof(DWORD);
        shader->Release();
    }
    vf_test_enable_grass_shaders(0);
    Check(created && replaced,
          "the device wrapper hands the client's grass vertex shaders to the driver with the motion block in them");
}
}
