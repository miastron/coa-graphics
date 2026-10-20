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
constexpr uint32_t kFreshTick = 123456;
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
            assembled = assembled && result[0] == original[0] && Contains(text, "sincos") && Contains(text, "c21") &&
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
    Check(foreign.size() == stock.variants[0].size() && other.Rewrite(foreign.data()) == foreign.data() &&
              other.Foreign() == 1 && other.Failed() == 0 && other.Injected() == 0,
          "another vertex shader of a grass length is recognised as not grass, left alone and not counted as a "
          "failure");

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

float ExpectedLength(const float* c)
{
    return std::sqrt(c[0] * c[0] + c[1] * c[1]);
}

void CheckConstants()
{
    const GrassWindSettings defaults = GrassWindSettingsFrom(Config());
    const float camera[3] = {100.0f, 200.0f, 30.0f};
    const float player[3] = {103.0f, 201.0f, 29.0f};
    const GrassConstants withPlayer = GrassFrameConstants(defaults, camera, player, kFreshTick);
    const GrassConstants withoutPlayer = GrassFrameConstants(defaults, camera, nullptr, kFreshTick);
    const float* r = &withPlayer.registers[0][0];
    const bool positions = r[0] == camera[0] && r[1] == camera[1] && r[2] == camera[2] &&
                           withPlayer.registers[2][0] == player[0] && withPlayer.registers[2][1] == player[1] &&
                           withPlayer.registers[2][2] == player[2] && withoutPlayer.registers[2][0] == camera[0] &&
                           withoutPlayer.registers[2][2] == camera[2];
    Check(positions, "the grass constants carry the camera and the player, falling back to the camera");
    Check(std::fabs(withPlayer.registers[3][1] - defaults.force) < kTolerance &&
              std::fabs(withPlayer.registers[3][0] - (defaults.edgeForce - defaults.force)) < kTolerance &&
              withoutPlayer.registers[3][1] == 0.0f && withoutPlayer.registers[3][0] == 0.0f,
          "the parting force reaches the shader only while a player is known");
    Check(std::fabs(ExpectedLength(withPlayer.registers[4]) - defaults.amplitude) < kTolerance &&
              std::fabs(ExpectedLength(withPlayer.registers[5]) - defaults.crossAmplitude) < kTolerance &&
              std::fabs(withPlayer.registers[2][3] - defaults.lean) < kTolerance,
          "the wind constants hold the two wave amplitudes and the lean");

    GrassWindSettings off = defaults;
    off.wind = false;
    off.parting = false;
    const GrassConstants quiet = GrassFrameConstants(off, camera, player, kFreshTick);
    Check(quiet.registers[4][0] == 0.0f && quiet.registers[4][1] == 0.0f && quiet.registers[5][0] == 0.0f &&
              quiet.registers[5][1] == 0.0f && quiet.registers[2][3] == 0.0f && quiet.registers[3][0] == 0.0f &&
              quiet.registers[3][1] == 0.0f,
          "with wind and parting off the grass constants hold no motion");

    GrassWindSettings extreme = defaults;
    extreme.variance = 7.0f;
    extreme.anchor = 5.0f;
    extreme.coneHeight = 0.0f;
    extreme.radius = 0.0f;
    const GrassConstants clamped = GrassFrameConstants(extreme, camera, player, kFreshTick);
    Check(clamped.registers[7][1] == 1.0f && std::fabs(clamped.registers[7][2] + 0.9f) < kTolerance &&
              std::isfinite(clamped.registers[7][3]) && std::isfinite(clamped.registers[1][0]) &&
              std::isfinite(clamped.registers[1][1]),
          "the variance, anchor, cone height and radius are held inside the range the shader divides by");
}

struct InitializeCall
{
    int calls = 0;
};

InitializeCall g_initializeCall;
float* g_initializeRegisters = nullptr;

void __cdecl FakeInitializeShaderConstants()
{
    ++g_initializeCall.calls;
    std::memset(g_initializeRegisters, 0xAB, kUploadedFloats * sizeof(float));
}

void CheckThunk()
{
    std::vector<float> block(kUploadedFloats, 0.0f);
    g_initializeRegisters = block.data();
    g_initializeCall = {};
    static const float camera[3] = {100.0f, 200.0f, 30.0f};
    const void* thunk = vf_test_grass_constants_thunk(reinterpret_cast<uintptr_t>(&FakeInitializeShaderConstants),
                                                      block.data() + engine::kDetailDoodadFirstFreeRegister * 4,
                                                      camera);
    reinterpret_cast<void(__cdecl*)()>(thunk)();
    bool untouched = true;
    for (size_t i = 0; i < kUploadedFloats; ++i)
    {
        const bool motion = i >= engine::kDetailDoodadFirstFreeRegister * 4 &&
                            i < (engine::kDetailDoodadFirstFreeRegister + kGrassConstantRegisters) * 4;
        uint32_t bits;
        std::memcpy(&bits, &block[i], sizeof(bits));
        if (!motion && bits != kRegisterFill)
            untouched = false;
    }
    const float* motionStart = block.data() + engine::kDetailDoodadFirstFreeRegister * 4;
    const GrassWindSettings defaults = GrassWindSettingsFrom(Config());
    Check(g_initializeCall.calls == 1 && untouched && motionStart[0] == camera[0] && motionStart[1] == camera[1] &&
              motionStart[2] == camera[2] && std::fabs(motionStart[2 * 4 + 3] - defaults.lean) < kTolerance,
          "the grass constants thunk runs the client's setup first, then rewrites only c14 to c21");
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
    bytes[guards[count - 1].address - low] ^= 0xFF;
    const bool changed = engine::GrassWindCodeMatches(view);
    Check(matches && !changed, "the grass wind hook needs every guarded byte of the client's detail doodad code");
}

bool SameGrassSettings(const GrassWindSettings& a, const GrassWindSettings& b)
{
    return a.wind == b.wind && a.parting == b.parting && a.directionDeg == b.directionDeg && a.speed == b.speed &&
           a.amplitude == b.amplitude && a.wavelength == b.wavelength && a.crossAmplitude == b.crossAmplitude &&
           a.crossWavelength == b.crossWavelength && a.crossAngleDeg == b.crossAngleDeg && a.lean == b.lean &&
           a.variance == b.variance && a.distanceFade == b.distanceFade && a.anchor == b.anchor &&
           a.radius == b.radius && a.force == b.force && a.edgeForce == b.edgeForce &&
           a.coneHeight == b.coneHeight && a.minBladeHeight == b.minBladeHeight;
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
    CheckRewrite(vs30, "vs_3_0", true);
    CheckRewrite(vs20, "vs_2_0", false);
    CheckPassThrough(vs30);
    CheckConstants();
    CheckThunk();
    CheckGuards();
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
