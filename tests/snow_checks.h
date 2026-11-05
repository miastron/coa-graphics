#pragma once

namespace snow_checks
{
constexpr uint32_t kBlsMagic = 0x47585348;
constexpr uint32_t kBlsVersion = 0x10003;
constexpr size_t kBlsHeaderBytes = 12;
constexpr size_t kBlsEntryHeaderBytes = 16;
constexpr float kCameraX = 1234.0f;
constexpr float kCameraY = -567.0f;
constexpr int kTargetSide = 64;
constexpr float kRangeYards = 2.0f;
constexpr float kPixelTolerance = 1.5f;
constexpr int kRockFirstAlpha = 20;
constexpr int kRockLastAlpha = 27;

std::vector<std::vector<DWORD>> LoadTerrainShaders(const char* fileName)
{
    std::vector<std::vector<DWORD>> out;
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
    for (uint32_t i = 0; i < header[2]; ++i)
    {
        uint32_t size = 0;
        if (at + kBlsEntryHeaderBytes > bytes.size())
            return {};
        std::memcpy(&size, bytes.data() + at + kBlsEntryHeaderBytes - sizeof(size), sizeof(size));
        at += kBlsEntryHeaderBytes;
        if (at + size > bytes.size())
            return {};
        std::vector<DWORD> code(size / sizeof(DWORD));
        if (size)
            std::memcpy(code.data(), bytes.data() + at, size);
        out.push_back(code);
        at += (size + 3) & ~3u;
    }
    return out;
}

std::string Disassembly(const ShaderCompiler& compiler, const std::vector<DWORD>& code)
{
    ID3DBlob* text = nullptr;
    if (code.empty() || FAILED(compiler.disassemble(code.data(), code.size() * sizeof(DWORD), 0, nullptr, &text)) ||
        !text)
        return "";
    std::string out(static_cast<const char*>(text->GetBufferPointer()));
    text->Release();
    return out;
}

std::vector<DWORD> Assembled(const ShaderCompiler& compiler, const std::string& source)
{
    ID3DBlob* blob = nullptr;
    ID3DBlob* errors = nullptr;
    std::vector<DWORD> out;
    if (SUCCEEDED(compiler.assemble(source.c_str(), source.size(), "snowCheck", nullptr, nullptr, 0, &blob, &errors)) &&
        blob)
    {
        out.resize(blob->GetBufferSize() / sizeof(DWORD));
        std::memcpy(out.data(), blob->GetBufferPointer(), out.size() * sizeof(DWORD));
    }
    if (blob)
        blob->Release();
    if (errors)
        errors->Release();
    return out;
}

SnowSettings Flat(float depth = 0.35f)
{
    SnowSettings s = SnowSettingsFrom(Config());
    s.depth = depth;
    s.drift = 0.0f;
    return s;
}

class FakeSnowTerrain final : public SnowSource
{
public:
    uintptr_t ChunkIdentity(int chunkRow, int chunkCol) const override
    {
        ++identities;
        if (missingRow == chunkRow && missingCol == chunkCol)
            return 0;
        return static_cast<uintptr_t>(chunkRow * 4096 + chunkCol + 1) + generation * 0x1000000u;
    }

    bool ReadChunk(int chunkRow, int chunkCol, SnowChunk& out) const override
    {
        ++reads;
        std::memset(out.snow, 0, sizeof(out.snow));
        for (int a = 0; a < kTerrainChunkVertexSide; ++a)
            for (int b = 0; b < kTerrainChunkVertexSide; ++b)
                out.heights[a][b] = steep ? a * kTerrainVertexYards * 2.0f : 10.0f;
        if (chunkRow == topRowOnlyRow && chunkCol == topRowOnlyCol)
        {
            std::memset(out.snow, 255, kSnowAlphaSide * 2);
            return true;
        }
        if (snowEverywhere)
            std::memset(out.snow, 255, sizeof(out.snow));
        if (chunkRow == rockRow && chunkCol == rockCol)
            for (int a = kRockFirstAlpha; a <= kRockLastAlpha; ++a)
                std::memset(out.snow + a * kSnowAlphaSide + kRockFirstAlpha, 0, kRockLastAlpha - kRockFirstAlpha + 1);
        return true;
    }

    bool snowEverywhere = true;
    bool steep = false;
    int missingRow = -1;
    int missingCol = -1;
    int topRowOnlyRow = -1;
    int topRowOnlyCol = -1;
    int rockRow = -1;
    int rockCol = -1;
    uintptr_t generation = 0;
    mutable int identities = 0;
    mutable int reads = 0;
};

uint32_t TexelAt(const SnowMap& map, float x, float y)
{
    const int u = SnowTexelIndex(static_cast<int>(std::floor(x / kSnowTexelYards)));
    const int v = SnowTexelIndex(static_cast<int>(std::floor(y / kSnowTexelYards)));
    return map.Texels()[static_cast<size_t>(v) * kSnowMapTexels + u];
}

unsigned Channel(uint32_t texel, int shift)
{
    return (texel >> shift) & 0xFFu;
}

void FillCompletely(SnowMap& map, const SnowSource& source, const float* camera, int mapId)
{
    for (int i = 0; i < 64; ++i)
        map.Track(camera, mapId, source, kSnowChunksPerFrame);
}

void CheckSnowLayers()
{
    Check(SnowLayer(kSnowTerrainTypeSnow, nullptr) && SnowLayer(0, "Tileset\\Northrend\\Dragonblight\\SnowA.blp") &&
              SnowLayer(-1, "tileset\\winterspring\\WINTERSPRINGSNOW01.BLP"),
          "a layer is snow when its ground effect is of the Snow terrain type or its texture is named snow");
    Check(!SnowLayer(0, "Tileset\\Barrens\\BarrensDirt.blp") && !SnowLayer(5, nullptr) && !SnowLayer(7, "Sno.blp"),
          "dirt, grass and sand layers are not snow");
}

void CheckAlphaDecoding()
{
    unsigned char out[kSnowAlphaTexels];
    std::vector<unsigned char> runs;
    for (int i = 0; i < kSnowAlphaTexels / 64; ++i)
    {
        runs.push_back(0x80 | 32);
        runs.push_back(200);
        runs.push_back(32);
        for (int j = 0; j < 32; ++j)
            runs.push_back(static_cast<unsigned char>(j));
    }
    const size_t used = DecodeTerrainAlpha(runs.data(), runs.size(), kSnowLayerCompressed, true, true, out);
    Check(used == runs.size() && out[0] == 200 && out[31] == 200 && out[32] == 0 && out[63] == 31 && out[64] == 200,
          "a compressed alpha map expands its fill and copy runs row by row");
    Check(!DecodeTerrainAlpha(runs.data(), runs.size() / 2, kSnowLayerCompressed, true, true, out),
          "a compressed alpha map cut short is refused rather than read past its end");
    std::vector<unsigned char> packed(kSnowAlphaTexels / 2, 0x00);
    packed[0] = 0xF3;
    packed[31] = 0x50;
    packed[30] = 0x04;
    Check(DecodeTerrainAlpha(packed.data(), packed.size(), kSnowLayerHasAlpha, false, false, out) == packed.size() &&
              out[0] == 3 * 17 && out[1] == 255 && out[62] == 0 && out[63] == 5 * 17,
          "a 4-bit alpha map unpacks the low nibble first and scales it to 0..255");
    DecodeTerrainAlpha(packed.data(), packed.size(), kSnowLayerHasAlpha, false, true, out);
    Check(out[60] == 4 * 17 && out[62] == 0 && out[63] == 0 && out[63 * 64] == out[62 * 64],
          "a 4-bit alpha map that is not fixed repeats its second-last column and row in the last");
    std::vector<unsigned char> wide(kSnowAlphaTexels, 77);
    Check(DecodeTerrainAlpha(wide.data(), wide.size(), kSnowLayerHasAlpha, true, true, out) == wide.size() &&
              out[kSnowAlphaTexels - 1] == 77 &&
              !DecodeTerrainAlpha(wide.data(), wide.size() - 1, kSnowLayerHasAlpha, true, true, out),
          "an 8-bit alpha map is copied whole and refused when shorter than 4096 bytes");
}

void CheckDepthRules()
{
    const SnowSettings s = Flat();
    Check(std::fabs(SnowDepthAt(SnowBase(1.0f, 1.0f), 0.5f, s) - 0.35f) < 1e-5f && SnowBase(0.0f, 1.0f) == 0.0f &&
              SnowBase(0.45f, 1.0f) == 0.0f && SnowBase(0.7f, 1.0f) > 0.0f && SnowBase(0.7f, 1.0f) < 1.0f,
          "fully painted flat snow lies at the set depth; ground painted half or less with snow holds none");
    Check(SnowSlopeFactor(10.0f, s) == 1.0f && SnowSlopeFactor(45.0f, s) == 0.0f &&
              std::fabs(SnowSlopeFactor(32.5f, s) - 0.5f) < 1e-5f,
          "the snow thins out over the slope fade below the max slope and is gone above it");
    SnowSettings drifting = s;
    drifting.drift = 0.35f;
    Check(std::fabs(SnowDepthAt(1.0f, 0.0f, drifting) - 0.35f * 0.65f) < 1e-5f &&
              std::fabs(SnowDepthAt(1.0f, 1.0f, drifting) - 0.35f * 1.35f) < 1e-5f,
          "drifts vary the depth by the drift share either way");
    const uint32_t texel = SnowTexel(1.0f, 0.5f, 0.5f * kSnowMaxDepthYards, 0x7F);
    Check(Channel(texel, 24) == 128 && Channel(texel, 16) == 128 && Channel(texel, 8) == 255 &&
              Channel(texel, 0) == 0x7F,
          "a snow texel holds the slope factor in alpha, depth over 1.2 yd in red, coverage in green, noise in blue");
}

void CheckSnowMap()
{
    SnowMap map;
    map.Configure(Flat());
    FakeSnowTerrain terrain;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    map.Track(camera, 1, terrain, kSnowChunksPerFrame);
    Check(map.FilledChunks() == kSnowChunksPerFrame &&
              map.PendingChunks() == kSnowWindowChunks * kSnowWindowChunks - kSnowChunksPerFrame,
          "the snow map reads a few chunks per frame, the rest waits");
    Check(Channel(TexelAt(map, kCameraX, kCameraY), 8) == 255, "the chunks nearest the camera are read first");
    FillCompletely(map, terrain, camera, 1);
    float box[4];
    map.Box(box);
    Check(map.FilledChunks() == kSnowWindowChunks * kSnowWindowChunks && map.PendingChunks() == 0 &&
              map.SnowChunks() == map.FilledChunks(),
          "every chunk of the window is read in time");
    Check(box[0] < kCameraX && kCameraX < box[2] && box[1] < kCameraY && kCameraY < box[3] &&
              std::fabs(box[2] - box[0] - kSnowMapYards) < 0.01f && std::fabs(box[3] - box[1] - kSnowMapYards) < 0.01f,
          "the window holds the camera and spans 16 chunks each way");
    const uint32_t centre = TexelAt(map, kCameraX, kCameraY);
    Check(Channel(centre, 8) == 255 && Channel(centre, 24) == 255 &&
              Channel(centre, 16) == static_cast<unsigned>(0.35f / kSnowMaxDepthYards * 255.0f + 0.5f),
          "flat painted snow is fully covered, not sloped and at the set depth");
    const int readsBefore = terrain.reads;
    map.Track(camera, 1, terrain, kSnowChunksPerFrame);
    Check(terrain.reads == readsBefore, "an unchanged window reads no chunk again");

    const int row = TerrainChunkIndex(kCameraX);
    const int col = TerrainChunkIndex(kCameraY);
    terrain.topRowOnlyRow = row;
    terrain.topRowOnlyCol = col;
    terrain.generation = 1;
    FillCompletely(map, terrain, camera, 1);
    const float top = kTerrainMapOriginYards - row * kTerrainChunkYards;
    const float left = kTerrainMapOriginYards - col * kTerrainChunkYards;
    Check(Channel(TexelAt(map, top - 0.5f, left - 10.0f), 8) == 255 &&
              Channel(TexelAt(map, top - kTerrainChunkYards + 0.5f, left - 10.0f), 8) == 0,
          "alpha map rows run from the chunk's top corner down along world x");
    Check(terrain.reads > readsBefore, "a chunk whose identity changes is read again");

    terrain.missingRow = row + 1;
    terrain.missingCol = col;
    FillCompletely(map, terrain, camera, 1);
    Check(Channel(TexelAt(map, top - kTerrainChunkYards - 10.0f, left - 10.0f), 8) == 0,
          "a chunk that unloads leaves no snow behind");

    const float moved[3] = {kCameraX - 3.0f * kTerrainChunkYards, kCameraY, 0.0f};
    FillCompletely(map, terrain, moved, 1);
    float shifted[4];
    map.Box(shifted);
    Check(std::fabs(shifted[0] - (box[0] - 3.0f * kTerrainChunkYards)) < 0.01f &&
              Channel(TexelAt(map, shifted[0] + 5.0f, kCameraY), 8) == 255,
          "the window follows the camera and fills the chunks it gains");

    terrain.steep = true;
    terrain.generation = 2;
    terrain.missingRow = -1;
    FillCompletely(map, terrain, moved, 1);
    const uint32_t steep = TexelAt(map, moved[0], moved[1]);
    Check(Channel(steep, 16) == 0 && Channel(steep, 24) == 0 && Channel(steep, 8) == 255,
          "painted snow on ground steeper than the max slope keeps its coverage but has no depth");

    map.Track(moved, 2, terrain, 0);
    Check(map.FilledChunks() == 0 && Channel(TexelAt(map, moved[0], moved[1]), 8) == 0,
          "a new map starts the snow over");
}

float TexelCentre(int chunk, float texel)
{
    return kTerrainMapOriginYards - chunk * kTerrainChunkYards - (texel + 0.5f) * kSnowTexelYards;
}

void CheckRockStaysLow()
{
    SnowMap map;
    map.Configure(Flat());
    FakeSnowTerrain terrain;
    const int row = TerrainChunkIndex(kCameraX);
    const int col = TerrainChunkIndex(kCameraY);
    terrain.rockRow = row;
    terrain.rockCol = col;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    FillCompletely(map, terrain, camera, 1);
    const float y = TexelCentre(col, 11.0f);
    const uint32_t inRock = TexelAt(map, TexelCentre(row, 11.0f), y);
    const uint32_t beside = TexelAt(map, TexelCentre(row, 8.0f), y);
    const uint32_t away = TexelAt(map, TexelCentre(row, 7.0f), y);
    Check(Channel(inRock, 8) == 0 && Channel(inRock, 16) == 0,
          "a rock patch inside painted snow is neither covered nor raised");
    Check(Channel(beside, 8) == 255 && Channel(beside, 16) == 0,
          "snow within two texels of a rock patch keeps its coverage but is not raised, so the rock is not lifted");
    Check(Channel(away, 16) == Channel(TexelAt(map, kCameraX + 200.0f, kCameraY), 16) && Channel(away, 16) > 0,
          "snow three texels from the rock lies at its full depth");
}

void CheckConstants()
{
    SnowMap map;
    map.Configure(Flat());
    FakeSnowTerrain terrain;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    const SnowVertexConstants unplaced = SnowVertexConstantsFor(map, true);
    map.Track(camera, 1, terrain, 1);
    const SnowVertexConstants on = SnowVertexConstantsFor(map, true);
    const SnowVertexConstants off = SnowVertexConstantsFor(map, false);
    const SnowPixelConstants pixel = SnowPixelConstantsFor(map, true);
    float box[4];
    map.Box(box);
    Check(unplaced.registers[2][1] == 0.0f && on.registers[2][1] == 1.0f && off.registers[2][1] == 0.0f &&
              on.registers[0][2] == kSnowMaxDepthYards && std::fabs(on.registers[0][0] * kSnowMapYards - 1.0f) < 1e-5f,
          "the lift is on only once the map is placed and uploaded, scaling red by the 1.2 yd range");
    Check(std::memcmp(on.registers[1], box, sizeof(box)) == 0 && std::memcmp(pixel.registers[1], box, sizeof(box)) == 0,
          "both shaders fade the snow out at the window's box");
    Check(pixel.registers[0][3] == 1.0f && SnowPixelConstantsFor(map, false).registers[0][3] == 0.0f &&
              std::fabs(pixel.registers[3][0] * 0.5f + pixel.registers[3][1] - 0.5f) < 1e-6f,
          "the edges apply at their set strength and an edge value of 0.5 stays 0.5 after sharpening");
}

void CheckShaderEdits()
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    if (!compiler.assemble || !compiler.disassemble)
    {
        Check(false, "d3dcompiler_47 has D3DDisassemble and D3DAssemble for the snow shader checks");
        return;
    }
    const auto vertex = LoadTerrainShaders("Terrain_vs_3_0.bls");
    Check(vertex.size() == 128, "the 128 extracted terrain vertex shaders load");
    unsigned edited = 0;
    unsigned assembled = 0;
    unsigned tookOverView = 0;
    for (const std::vector<DWORD>& code : vertex)
    {
        std::string source = Disassembly(compiler, code);
        std::string why;
        const bool fullOutputs = source.find("dcl_texcoord6") != std::string::npos &&
                                 source.find("dcl_fog o10") != std::string::npos;
        if (!EditTerrainVertexSource(source, why))
            continue;
        ++edited;
        tookOverView += fullOutputs && source.find("dcl_texcoord3") == std::string::npos ? 1u : 0u;
        assembled += !Assembled(compiler, source).empty() ? 1u : 0u;
    }
    Check(edited == 128 && assembled == 128,
          "every terrain vertex shader takes the lift and reassembles as vs_3_0");
    Check(tookOverView == 32,
          "the 32 variants with all 11 outputs in use hand the world position over the unread view position");
    std::string first = Disassembly(compiler, vertex.empty() ? std::vector<DWORD>() : vertex[0]);
    std::string why;
    EditTerrainVertexSource(first, why);
    Check(first.find("mul r0, c1, r12.y") != std::string::npos &&
              first.find("dcl_texcoord7 o6\n") != std::string::npos &&
              first.find("mov o6.xyz, r12") != std::string::npos &&
              first.find("add o6.w, r9.x, c3.z") != std::string::npos,
          "the lifted position replaces v0 in the view transform and leaves through texcoord7 with its view depth");
    std::string twice = first;
    Check(!EditTerrainVertexSource(twice, why), "a rewritten terrain vertex shader is not rewritten again");

    struct PixelFamily
    {
        const char* file;
        unsigned count;
        unsigned edits;
    };
    const PixelFamily families[] = {
        {"Terrain0_ps_3_0.bls", 3, 1},
        {"Terrain1_ps_3_0.bls", 32, 32},
        {"Terrain2_ps_3_0.bls", 32, 32},
        {"Terrain3_ps_3_0.bls", 96, 96},
    };
    for (const PixelFamily& family : families)
    {
        const auto pixel = LoadTerrainShaders(family.file);
        unsigned pixelEdited = 0;
        unsigned pixelAssembled = 0;
        unsigned depthAssembled = 0;
        for (const std::vector<DWORD>& code : pixel)
        {
            std::string source = Disassembly(compiler, code);
            std::string deep = source;
            std::string reason;
            if (!EditTerrainPixelSource(source, reason, false))
                continue;
            ++pixelEdited;
            pixelAssembled += !Assembled(compiler, source).empty() ? 1u : 0u;
            depthAssembled += EditTerrainPixelSource(deep, reason, true) &&
                                      deep.find("oDepth") != std::string::npos && !Assembled(compiler, deep).empty()
                                  ? 1u
                                  : 0u;
        }
        char label[160];
        std::snprintf(label, sizeof(label),
                      "%s: %u variants, %u with the stock fog ending take the snow edges and reassemble, with and "
                      "without the trench depth",
                      family.file, family.count, family.edits);
        Check(pixel.size() == family.count && pixelEdited == family.edits && pixelAssembled == family.edits &&
                  depthAssembled == family.edits,
              label);
    }
}

struct LiftRig
{
    IDirect3DDevice9* device = nullptr;
    IDirect3DStateBlock9* state = nullptr;
    IDirect3DSurface9* savedTarget = nullptr;
    IDirect3DSurface9* savedDepth = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* readback = nullptr;
    IDirect3DVertexDeclaration9* declaration = nullptr;
    IDirect3DPixelShader9* pixel = nullptr;
    IDirect3DVertexShader9* vertex = nullptr;

    ~LiftRig()
    {
        if (state)
            state->Apply();
        if (savedTarget)
            device->SetRenderTarget(0, savedTarget);
        device->SetDepthStencilSurface(savedDepth);
        for (IUnknown* resource : {static_cast<IUnknown*>(state), static_cast<IUnknown*>(savedTarget),
                                   static_cast<IUnknown*>(savedDepth), static_cast<IUnknown*>(target),
                                   static_cast<IUnknown*>(readback), static_cast<IUnknown*>(declaration),
                                   static_cast<IUnknown*>(pixel), static_cast<IUnknown*>(vertex)})
            if (resource)
                resource->Release();
    }

    bool Create(const ShaderCompiler& compiler, const std::vector<DWORD>& vertexCode)
    {
        const D3DVERTEXELEMENT9 elements[] = {
            {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
            D3DDECL_END()};
        const std::vector<DWORD> white = Assembled(compiler, "ps_3_0\ndef c0, 1, 1, 1, 1\nmov oC0, c0\n");
        return !white.empty() && SUCCEEDED(device->CreatePixelShader(white.data(), &pixel)) &&
               SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, &state)) &&
               SUCCEEDED(device->GetRenderTarget(0, &savedTarget)) &&
               SUCCEEDED(device->GetDepthStencilSurface(&savedDepth)) &&
               SUCCEEDED(device->CreateRenderTarget(kTargetSide, kTargetSide, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE, 0,
                                                    FALSE, &target, nullptr)) &&
               SUCCEEDED(device->CreateOffscreenPlainSurface(kTargetSide, kTargetSide, D3DFMT_A8R8G8B8,
                                                             D3DPOOL_SYSTEMMEM, &readback, nullptr)) &&
               SUCCEEDED(device->CreateVertexDeclaration(elements, &declaration)) &&
               SUCCEEDED(device->CreateVertexShader(vertexCode.data(), &vertex));
    }

    bool Draw(float x, float y, float& column)
    {
        float constants[8][4] = {};
        constants[0][0] = constants[1][1] = constants[2][2] = constants[3][3] = 1.0f;
        constants[6][0] = 1.0f / kRangeYards;
        constants[7][2] = 0.5f;
        constants[7][3] = 1.0f;
        device->SetVertexShaderConstantF(0, &constants[0][0], 8);
        device->SetRenderTarget(0, target);
        device->SetDepthStencilSurface(nullptr);
        const D3DVIEWPORT9 viewport = {0, 0, kTargetSide, kTargetSide, 0.0f, 1.0f};
        device->SetViewport(&viewport);
        device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        device->SetRenderState(D3DRS_ZENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        device->SetVertexDeclaration(declaration);
        device->SetPixelShader(pixel);
        const float point[6] = {x, y, 0.0f, 0.0f, 0.0f, 1.0f};
        if (FAILED(device->BeginScene()))
            return false;
        const bool drawn = SUCCEEDED(device->DrawPrimitiveUP(D3DPT_POINTLIST, 1, point, sizeof(point)));
        device->EndScene();
        D3DLOCKED_RECT locked = {};
        if (!drawn || FAILED(device->GetRenderTargetData(target, readback)) ||
            FAILED(readback->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            return false;
        float weight = 0.0f;
        float sum = 0.0f;
        for (int row = 0; row < kTargetSide; ++row)
            for (int col = 0; col < kTargetSide; ++col)
            {
                const unsigned char* line = static_cast<const unsigned char*>(locked.pBits) + row * locked.Pitch;
                const uint32_t texel = *reinterpret_cast<const uint32_t*>(line + col * 4);
                const float lit = static_cast<float>(texel & 0xFF);
                weight += lit;
                sum += lit * (col + 0.5f);
            }
        readback->UnlockRect();
        if (weight <= 0.0f)
            return false;
        column = sum / weight;
        return true;
    }
};

void CheckLiftOnTheDevice(IDirect3DDevice9* dev)
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    const auto vertex = LoadTerrainShaders("Terrain_vs_3_0.bls");
    if (!compiler.assemble || vertex.empty())
        return;
    std::string source = Disassembly(compiler, vertex[0]);
    std::string why;
    const std::vector<DWORD> lifted = EditTerrainVertexSource(source, why) ? Assembled(compiler, source)
                                                                           : std::vector<DWORD>();
    LiftRig rig;
    rig.device = dev;
    if (lifted.empty() || !rig.Create(compiler, lifted))
    {
        Check(false, "the snow lift rig could be created on the device");
        return;
    }
    SnowMap map;
    map.Configure(Flat());
    FakeSnowTerrain terrain;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    FillCompletely(map, terrain, camera, 1);
    SnowSurface surface;
    surface.NoteTerrainShader(rig.vertex);
    const bool uploaded = surface.Upload(dev, map);
    float box[4];
    map.Box(box);

    float bare = 0.0f;
    float snowy = 0.0f;
    float outside = 0.0f;
    float edge = 0.0f;
    dev->SetVertexShader(rig.vertex);
    SnowFrame frame = {SnowVertexConstantsFor(map, false), SnowPixelConstantsFor(map, false), {}};
    surface.Bind(dev, frame);
    const bool drewBare = rig.Draw(kCameraX, kCameraY, bare);
    frame = {SnowVertexConstantsFor(map, true), SnowPixelConstantsFor(map, true), {}};
    surface.Bind(dev, frame);
    const bool drewSnowy = rig.Draw(kCameraX, kCameraY, snowy);
    const bool drewOutside = rig.Draw(box[0] - 20.0f, kCameraY, outside);
    const bool drewEdge = rig.Draw(box[0] + kSnowEdgeFadeYards * 0.5f, kCameraY, edge);
    surface.Release();
    dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
    dev->SetTexture(kSnowPixelSampler, nullptr);

    const float depth = Channel(TexelAt(map, kCameraX, kCameraY), 16) / 255.0f * kSnowMaxDepthYards;
    const float expected = depth / kRangeYards * kTargetSide * 0.5f;
    std::printf("     snow lift: bare column %.2f, snowy %.2f (expected +%.2f), outside %.2f, edge %.2f\n", bare, snowy,
                expected, outside, edge);
    Check(uploaded && drewBare && drewSnowy && drewOutside && drewEdge,
          "the snow map uploads and a point drawn with the lifted terrain shader reaches the target");
    Check(std::fabs(snowy - bare - expected) < kPixelTolerance,
          "the rewritten terrain shader raises the ground by the snow depth the map holds");
    Check(std::fabs(outside - bare) < kPixelTolerance,
          "ground outside the snow window stays where the client put it");
    Check(edge - bare > kPixelTolerance * 0.5f && snowy - edge > kPixelTolerance * 0.5f,
          "the lift fades in over the last yards of the window instead of stepping");
    const SnowVertexConstants on = SnowVertexConstantsFor(map, true);
    const float pixelsPerYard = kTargetSide * 0.5f / kRangeYards;
    const float cpuSnowy = SnowLiftAt(map, on, kCameraX, kCameraY);
    const float cpuEdge = SnowLiftAt(map, on, box[0] + kSnowEdgeFadeYards * 0.5f, kCameraY);
    std::printf("     snow lift on the CPU: %.3f yd at the camera, %.3f yd half way into the edge fade\n", cpuSnowy,
                cpuEdge);
    Check(std::fabs(cpuSnowy - depth) < 0.005f &&
              std::fabs(cpuEdge * pixelsPerYard - (edge - bare)) < kPixelTolerance &&
              SnowLiftAt(map, on, box[0] - 20.0f, kCameraY) == 0.0f &&
              SnowLiftAt(map, SnowVertexConstantsFor(map, false), kCameraX, kCameraY) == 0.0f,
          "the CPU reads the same lift as the rewritten terrain shader, for the ground circles laid on the snow");
}

void CheckDecalLift()
{
    std::vector<float> matrix(16, 0.0f);
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    matrix[12] = 100.0f;
    matrix[13] = 200.0f;
    matrix[14] = 30.0f;
    std::vector<float> vertices(145 * 3, 0.0f);
    std::vector<float> heights(145, 0.0f);
    for (int k = 0; k < 145; ++k)
    {
        vertices[k * 3] = -0.5f * k;
        vertices[k * 3 + 1] = -0.25f * k;
        heights[k] = 2.0f + k;
    }
    struct
    {
        const float* matrix;
        const float* vertices;
        const float* normals;
        const float* heights;
    } batch = {matrix.data(), vertices.data(), nullptr, heights.data()};
    const auto lift = [](float x, float y) { return 0.001f * x + 0.0001f * y; };
    const uintptr_t address = reinterpret_cast<uintptr_t>(&batch);
    const bool lifted = engine::LiftDecalBatch(address, lift);
    const float* swapped = batch.heights;
    const float expected = heights[10] + lift(100.0f - 5.0f, 200.0f - 2.5f);
    const bool liftedRight = lifted && swapped != heights.data() && std::fabs(swapped[10] - expected) < 1e-5f;
    engine::RestoreDecalBatch();
    Check(liftedRight && batch.heights == heights.data() && heights[10] == 12.0f,
          "a terrain batch of a ground circle draws with its heights raised by the snow at each vertex, and gets its "
          "own heights back after the build");
    batch.heights = nullptr;
    Check(!engine::LiftDecalBatch(address, lift) && batch.heights == nullptr,
          "a ground circle batch without terrain heights (a building) is left as it is");
    engine::RestoreDecalBatch();
}

void CheckTrenchShape()
{
    const float r = 0.5f;
    Check(std::fabs(TrenchPressAt(0.0f, r) - kTrenchPress) < 1e-5f &&
              std::fabs(TrenchPressAt(0.3f * r, r) - kTrenchPress) < 1e-5f && TrenchPressAt(r, r) == 0.0f &&
              TrenchPressAt(0.5f * r, r) > TrenchPressAt(0.9f * r, r) && TrenchPressAt(0.99f * r, r) < 0.01f,
          "a print has a flat floor and rounds smoothly up to the snow surface at its edge");
    Check(TrenchRimAt(0.5f * r, r) == 0.0f && TrenchRimAt(1.25f * r, r) > 0.99f && TrenchRimAt(1.7f * r, r) == 0.0f &&
              TrenchRimAt(2.0f * r, r) == 0.0f,
          "the rim rises around a print, highest at 1.25 of its size, and is gone at 1.7");
}

TrenchScuff Scuff(float fromX, float fromY, float toX, float toY, float radius, float share)
{
    return {{fromX, fromY}, {toX, toY}, radius, share, 0};
}

void CheckTrenchMap()
{
    TrenchMap map;
    map.Place(kCameraX, kCameraY);
    float box[4];
    map.Box(box);
    Check(box[0] < kCameraX && kCameraX < box[2] && std::fabs(box[2] - box[0] - kTrenchSpanYards) < 1e-3f &&
              std::fabs(kTrenchSpanYards - 64.0f) < 1e-3f && kTrenchTexelYards <= 0.0625f,
          "the footprint window holds the player, spans 64 yd and is fine enough for a foot");
    map.Texels();
    map.MarkUploaded();
    const unsigned before = map.Generation();
    map.StampScuff(Scuff(kCameraX, kCameraY, kCameraX + 2.0f, kCameraY, 0.5f, 1.0f));
    Check(map.Generation() != before && map.PressAt(kCameraX + 1.0f, kCameraY + 0.1f) > 0.8f &&
              map.PressAt(kCameraX + 1.0f, kCameraY + 0.6f) == 0.0f &&
              map.RimAt(kCameraX + 1.0f, kCameraY + 0.62f) > 0.5f &&
              map.RimAt(kCameraX + 1.0f, kCameraY + 0.1f) < 0.2f,
          "a stamped segment is pressed along its length with a rim on both sides and none in its floor");
    Check(map.ChangedRows() > 0 && map.ChangedRows() <= static_cast<int>(2.0f * 1.7f * 0.5f / kTrenchTexelYards) + 2,
          "a stamp marks only the rows it touches for upload");
    const unsigned stamped = map.Generation();
    map.StampScuff(Scuff(kCameraX, kCameraY, kCameraX + 2.0f, kCameraY, 0.5f, 1.0f));
    Check(map.Generation() == stamped, "stamping the same place again changes nothing and uploads nothing");
    map.StampScuff(Scuff(kCameraX, kCameraY + 3.0f, kCameraX + 2.0f, kCameraY + 3.0f, 0.5f, 0.3f));
    Check(map.PressAt(kCameraX + 1.0f, kCameraY + 3.0f) > 0.2f && map.PressAt(kCameraX + 1.0f, kCameraY + 3.0f) < 0.3f,
          "a shallow stamp presses its share of the depth");
    const uint32_t texel = map.Texels()[0];
    Check((texel >> 24) == 0xFFu, "footprint texels are opaque");
    map.MarkUploaded();
    map.Relax(15.0f, 30.0f);
    const float half = map.PressAt(kCameraX + 1.0f, kCameraY + 0.1f);
    Check(half > 0.3f && half < 0.5f, "a footprint relaxes linearly toward the surface over its fill-in time");
    Check(map.ChangedRows() > 0 && map.ChangedRows() < kTrenchTexels / 4,
          "relaxing uploads only the rows that hold footprints");
    map.Relax(30.0f, 30.0f);
    Check(map.ActiveTexels() == 0 && map.PressAt(kCameraX + 1.0f, kCameraY + 0.1f) == 0.0f,
          "a footprint is gone after its fill-in time");
    map.StampScuff(Scuff(kCameraX, kCameraY, kCameraX + 2.0f, kCameraY, 0.5f, 1.0f));
    map.Place(kCameraX + 5.0f, kCameraY);
    Check(map.PressAt(kCameraX + 1.0f, kCameraY) > 0.0f, "a small move keeps the window and its footprints");
    map.Place(kCameraX + 40.0f, kCameraY);
    Check(map.PressAt(kCameraX + 1.0f, kCameraY) == 0.0f &&
              map.PressAt(kCameraX + 1.0f + kTrenchSpanYards, kCameraY) == 0.0f && map.ActiveTexels() == 0,
          "footprints that leave the moved window are cleared, and none reappears at its wrapped place");
}

float DeepestAcross(const TrenchMap& map, float x, float y)
{
    float deepest = 0.0f;
    for (int i = -4; i <= 4; ++i)
        deepest = std::max(deepest, map.PressAt(x, y + i * 0.02f));
    return deepest;
}

void CheckFootprintGaits()
{
    const NearbyUnit walker = {1, {0.0f, 0.0f, 0.0f}, kFootprintHumanRadius, false};
    NearbyUnit rider = walker;
    rider.mounted = true;
    NearbyUnit wolf = walker;
    wolf.radius = 0.6f;
    wolf.fourLegged = true;
    const FootprintGait feet = GaitFor(walker);
    const FootprintGait hooves = GaitFor(rider);
    const FootprintGait paws = GaitFor(wolf);
    Check(!feet.fourLegs && hooves.fourLegs && paws.fourLegs && hooves.size >= kFootprintMountSize &&
              feet.halfLength > feet.halfWidth && hooves.halfLength == hooves.halfWidth,
          "people walk on two oval feet, mounts and four-legged beasts on four round ones");
    Check(feet.halfWidth >= kTrenchTexelYards && feet.halfLength >= 2.0f * kTrenchTexelYards,
          "a human foot spans several footprint texels");
    Check(StepLength(feet, kFootprintRunSpeed) > StepLength(feet, kFootprintWalkSpeed) &&
              StepLength(feet, kFootprintWalkSpeed) > 0.6f && StepLength(feet, kFootprintWalkSpeed) < 0.9f &&
              DragShare(kFootprintWalkSpeed) > DragShare(kFootprintRunSpeed),
          "steps lengthen with speed, and a running foot drags less than a walking one");
    Check(FourLeggedCreature(kCreatureTypeBeast, 1) && FourLeggedCreature(kCreatureTypeCritter, 0) &&
              !FourLeggedCreature(kCreatureTypeBeast, 11) && !FourLeggedCreature(kCreatureTypeBeast, 7) &&
              !FourLeggedCreature(7, 0),
          "wolves and critters walk on four legs, raptors, birds and humanoids on two");
}

void CheckFootprintStamper()
{
    TrenchMap map;
    FootprintStamper stamper;
    map.Place(kCameraX, kCameraY);
    NearbyUnitFrame units;
    units.count = 2;
    units.units[0] = {1, {kCameraX, kCameraY, 0.0f}, kFootprintHumanRadius, false};
    units.units[1] = {2, {kCameraX, kCameraY + 10.0f, 5.0f}, kFootprintHumanRadius, false};
    const bool onGround[2] = {true, false};
    const FootprintGait gait = GaitFor(units.units[0]);
    const float track = gait.halfTrack;
    Check(stamper.Step(map, units, onGround, 0.0f) == 1 && DeepestAcross(map, kCameraX, kCameraY + track) > 0.8f &&
              DeepestAcross(map, kCameraX, kCameraY - track) > 0.8f && map.PressAt(kCameraX, kCameraY) < 0.05f &&
              DeepestAcross(map, kCameraX, kCameraY + 10.0f) == 0.0f,
          "a unit standing on the ground leaves two footprints side by side, one off the ground leaves none");
    const float frame = 1.0f / 30.0f;
    const float speed = 3.0f;
    const float step = StepLength(gait, speed);
    for (int i = 1; i <= 60; ++i)
    {
        units.units[0].position[0] = kCameraX + speed * frame * i;
        stamper.Step(map, units, onGround, frame);
    }
    const float left = kCameraY + track;
    const float right = kCameraY - track;
    std::printf("     footprints: step %.2f yd; left track at 1 step %.2f, 2 steps %.2f; right track at 1 step %.2f, "
                "2 steps %.2f\n",
                step, DeepestAcross(map, kCameraX + step, left), DeepestAcross(map, kCameraX + 2.0f * step, left),
                DeepestAcross(map, kCameraX + step, right), DeepestAcross(map, kCameraX + 2.0f * step, right));
    Check(DeepestAcross(map, kCameraX + step, left) > 0.8f && DeepestAcross(map, kCameraX + 3.0f * step, left) > 0.8f &&
              DeepestAcross(map, kCameraX + 2.0f * step, right) > 0.8f &&
              DeepestAcross(map, kCameraX + 4.0f * step, right) > 0.8f,
          "a walking unit plants its left and right feet in turn, one step apart");
    const float scuff = DeepestAcross(map, kCameraX + 2.0f * step, left);
    Check(scuff > 0.1f && scuff < 0.5f && DeepestAcross(map, kCameraX + step, right) < 0.5f,
          "the lifted foot drags a shallow scuff between its own prints");
    Check(map.PressAt(kCameraX + 2.0f * step, kCameraY) < 0.05f,
          "the snow between the two tracks is not pressed");
    Check(map.FadeAt(kCameraX + step, left) != map.FadeAt(kCameraX + 2.0f * step, right),
          "each print carries its own fade-out distance");
    units.units[0].position[0] = kCameraX + 12.0f;
    stamper.Step(map, units, onGround, frame);
    Check(DeepestAcross(map, kCameraX + 8.0f, left) == 0.0f && DeepestAcross(map, kCameraX + 12.0f, left) > 0.8f,
          "a unit that jumps further than 3 yd in a frame lands with both feet and leaves no streak across the gap");

    TrenchMap herd;
    FootprintStamper hooves;
    herd.Place(kCameraX, kCameraY);
    NearbyUnitFrame mounts;
    mounts.count = 1;
    mounts.units[0] = {3, {kCameraX, kCameraY, 0.0f}, kFootprintHumanRadius, true};
    const FootprintGait horse = GaitFor(mounts.units[0]);
    hooves.Step(herd, mounts, onGround, 0.0f);
    const float hindTrack = horse.halfTrack * horse.hindTrackShare;
    Check(DeepestAcross(herd, kCameraX + horse.halfBody, kCameraY + horse.halfTrack) > 0.8f &&
              DeepestAcross(herd, kCameraX + horse.halfBody, kCameraY - horse.halfTrack) > 0.8f &&
              DeepestAcross(herd, kCameraX - horse.halfBody, kCameraY + hindTrack) > 0.8f &&
              DeepestAcross(herd, kCameraX - horse.halfBody, kCameraY - hindTrack) > 0.8f &&
              herd.PressAt(kCameraX, kCameraY) == 0.0f,
          "a standing mount leaves four hoof prints, front and hind, and none under its belly");
    TrenchLight light = {{1.0f, 2.0f, 3.0f}, {0.0f, 0.6f, 0.8f}, 0.4f};
    const TrenchReach reach = {28.0f, 8.0f};
    const TrenchConstants on = TrenchConstantsFor(map, true, 0.3f, light, reach);
    const TrenchConstants off = TrenchConstantsFor(map, false, 0.3f, light, reach);
    Check(on.registers[0][3] == 1.0f && off.registers[0][3] == 0.0f && on.registers[0][2] == 0.3f &&
              on.registers[2][0] == 1.0f && on.registers[3][1] == 0.6f && on.registers[3][3] == 0.4f &&
              on.fade[0] == 28.0f && on.fade[1] == 8.0f && on.fade[3] == 2.0f * kTrenchTexelYards,
          "the footprint constants carry the switch, the rim share, the camera, the light and the fade distances");
    Check(std::fabs(AmbientShare(0xFF404040u, 0xFFC0C0C0u) - 0.25f) < 0.01f && AmbientShare(0, 0) == 1.0f,
          "the ambient share is the ambient light's part of the total");
    float projection[16] = {};
    projection[10] = 1.0f;
    projection[14] = -2.0f;
    float row[4] = {};
    Check(TrenchDepthRow(projection, 0.0f, 1.0f, row) && row[0] == 1.0f && row[1] == -1.0f &&
              row[2] == kTrenchDepthPushBack && !TrenchDepthRow(projection, 0.5f, 0.5f, row),
          "the footprint depth row maps view depth to the depth buffer the way the fog reads it back");
}

const char* const kTrenchRigVertex = "vs_3_0\n"
                                     "def c10, 0, 1, 0.5, 0\n"
                                     "def c13, 30, 0, 0, 0\n"
                                     "dcl_position v0\n"
                                     "dcl_position o0\n"
                                     "dcl_color o1\n"
                                     "dcl_texcoord7 o2\n"
                                     "dcl_fog o3.x\n"
                                     "mad o0.xy, v0.xy, c11.xy, c11.zw\n"
                                     "mov o0.zw, c10.xxxy\n"
                                     "mov o1, c10.zzzy\n"
                                     "mov o2.xyz, v0\n"
                                     "mov o2.w, c13.x\n"
                                     "mov o3.x, c10.y\n";

const char* const kDepthPlaneVertex = "vs_3_0\n"
                                      "def c10, 0, 1, 0.5, 0\n"
                                      "dcl_position v0\n"
                                      "dcl_position o0\n"
                                      "mad o0.xy, v0.xy, c11.xy, c11.zw\n"
                                      "mov o0.z, c12.x\n"
                                      "mov o0.w, c10.y\n";

const char* const kTrenchRigPixel = "ps_3_0\n"
                                    "def c0, 0.5, 0.5, 0.5, 1\n"
                                    "dcl_color v0.xyz\n"
                                    "dcl_fog v1.x\n"
                                    "mov r0, c0\n"
                                    "mul r0.xyz, r0, v0\n"
                                    "add r0.xyz, r0, r0\n"
                                    "add r0.xyz, r0, -c2\n"
                                    "mad oC0.xyz, v1.x, r0, c2\n"
                                    "mov oC0.w, c0.w\n";

float RowBrightness(const D3DLOCKED_RECT& locked, int row)
{
    const unsigned char* line = static_cast<const unsigned char*>(locked.pBits) + row * locked.Pitch;
    return static_cast<float>(line[kTargetSide / 2 * 4 + 1]);
}

void CheckTrenchShadingOnTheDevice(IDirect3DDevice9* dev)
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    if (!compiler.assemble)
        return;
    std::string pixelSource = kTrenchRigPixel;
    std::string why;
    const bool edited = EditTerrainPixelSource(pixelSource, why, false);
    const std::vector<DWORD> pixelCode = edited ? Assembled(compiler, pixelSource) : std::vector<DWORD>();
    const std::vector<DWORD> vertexCode = Assembled(compiler, kTrenchRigVertex);
    LiftRig rig;
    rig.device = dev;
    IDirect3DPixelShader9* trenchPixel = nullptr;
    if (pixelCode.empty() || vertexCode.empty() || !rig.Create(compiler, vertexCode) ||
        FAILED(dev->CreatePixelShader(pixelCode.data(), &trenchPixel)))
    {
        Check(false, "the trench shading rig could be created on the device");
        return;
    }
    SnowMap snow;
    snow.Configure(Flat());
    FakeSnowTerrain terrain;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    FillCompletely(snow, terrain, camera, 1);
    TrenchMap trenches;
    trenches.Place(kCameraX, kCameraY);
    const float from[2] = {kCameraX - 3.0f, kCameraY};
    const float to[2] = {kCameraX + 3.0f, kCameraY};
    trenches.StampScuff(Scuff(from[0], from[1], to[0], to[1], 0.5f, 1.0f));
    SnowSurface surface;
    const bool uploaded = surface.Upload(dev, snow) && surface.UploadTrenches(dev, trenches);
    const TrenchLight light = {{kCameraX, kCameraY + 0.001f, 40.0f}, {0.0f, 0.7071f, 0.7071f}, 0.3f};

    const float halfSpan = 2.0f;
    const float placement[4] = {1.0f / halfSpan, 1.0f / halfSpan, -kCameraX / halfSpan, -kCameraY / halfSpan};
    const TrenchReach kPassReach[3] = {{64.0f, 1.0f}, {64.0f, 1.0f}, {10.0f, 1.0f}};
    float brightness[3][4] = {};
    bool drawn = uploaded;
    for (int pass = 0; pass < 3; ++pass)
    {
        SnowFrame frame = {SnowVertexConstantsFor(snow, false), SnowPixelConstantsFor(snow, false),
                           TrenchConstantsFor(trenches, pass >= 1, 0.3f, light, kPassReach[pass])};
        surface.Bind(dev, frame);
        dev->SetVertexShader(rig.vertex);
        dev->SetVertexShaderConstantF(11, placement, 1);
        dev->SetPixelShader(trenchPixel);
        dev->SetVertexDeclaration(rig.declaration);
        dev->SetRenderTarget(0, rig.target);
        dev->SetDepthStencilSurface(nullptr);
        const D3DVIEWPORT9 viewport = {0, 0, kTargetSide, kTargetSide, 0.0f, 1.0f};
        dev->SetViewport(&viewport);
        dev->SetRenderState(D3DRS_ZENABLE, FALSE);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
        const float quad[4][6] = {
            {kCameraX - halfSpan, kCameraY - halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
            {kCameraX + halfSpan, kCameraY - halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
            {kCameraX - halfSpan, kCameraY + halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
            {kCameraX + halfSpan, kCameraY + halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
        };
        D3DLOCKED_RECT locked = {};
        drawn = drawn && SUCCEEDED(dev->BeginScene()) &&
                SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]))) &&
                SUCCEEDED(dev->EndScene()) && SUCCEEDED(dev->GetRenderTargetData(rig.target, rig.readback)) &&
                SUCCEEDED(rig.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
        if (!drawn)
            break;
        const float rows[4] = {0.0f, -0.3f, 0.3f, 1.5f};
        for (int i = 0; i < 4; ++i)
        {
            const int row = static_cast<int>((1.0f - rows[i] / halfSpan) * 0.5f * kTargetSide);
            brightness[pass][i] = RowBrightness(locked, row);
        }
        rig.readback->UnlockRect();
    }
    surface.Release();
    trenchPixel->Release();
    dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
    dev->SetTexture(kSnowPixelSampler, nullptr);
    dev->SetTexture(kTrenchPixelSampler, nullptr);
    std::printf("     trench shading off: centre %.0f, wall toward light %.0f, wall away %.0f, outside %.0f; "
                "on: %.0f, %.0f, %.0f, %.0f; beyond the footprint distance: %.0f, %.0f, %.0f, %.0f\n",
                brightness[0][0], brightness[0][1], brightness[0][2], brightness[0][3], brightness[1][0],
                brightness[1][1], brightness[1][2], brightness[1][3], brightness[2][0], brightness[2][1],
                brightness[2][2], brightness[2][3]);
    Check(drawn, "the trench shading rig draws with the rewritten terrain pixel shader");
    Check(brightness[0][0] == brightness[0][3] && brightness[0][1] == brightness[0][3],
          "with the trenches switched off the pixel shader leaves the colour as the client drew it");
    Check(std::fabs(brightness[1][3] - brightness[0][3]) <= 1.0f,
          "untouched snow keeps the client's colour with the trenches on");
    Check(brightness[1][0] < brightness[1][3] - 4.0f, "the floor of a trench is darker than the snow around it");
    Check(brightness[1][1] > brightness[1][2] + 4.0f,
          "the trench wall facing the light is brighter than the wall facing away from it");
    Check(brightness[2][0] == brightness[0][0] && brightness[2][1] == brightness[0][1] &&
              brightness[2][2] == brightness[0][2],
          "footprints further from the camera than the footprint distance are not drawn");
}

void CheckSnowStaysBoundOnTheDevice(IDirect3DDevice9* dev)
{
    SnowMap snow;
    snow.Configure(Flat());
    FakeSnowTerrain terrain;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    FillCompletely(snow, terrain, camera, 1);
    SnowSurface surface;
    const SnowFrame frame = {SnowVertexConstantsFor(snow, true), SnowPixelConstantsFor(snow, true), {}};
    const bool uploaded = surface.Upload(dev, snow);
    surface.Bind(dev, frame);
    const bool boundAfterBind = surface.StillBound(dev, frame);
    surface.KeepBoundForDraw(dev, frame);
    const float clobbered[kSnowVertexRegisterCount][4] = {};
    dev->SetVertexShaderConstantF(kSnowVertexRegister, &clobbered[0][0], kSnowVertexRegisterCount);
    const bool boundAfterClobber = surface.StillBound(dev, frame);
    surface.KeepBoundForDraw(dev, frame);
    const bool restored = surface.StillBound(dev, frame);
    dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
    const bool boundWithoutMap = surface.StillBound(dev, frame);
    surface.KeepBoundForDraw(dev, frame);
    Check(uploaded && boundAfterBind && !boundAfterClobber && restored && !boundWithoutMap &&
              surface.StillBound(dev, frame) && surface.TerrainDraws() == 3 && surface.ReboundDraws() == 2,
          "a terrain draw that finds the snow constants or the snow map replaced since the shader was bound binds "
          "them again, and one that finds them in place leaves them");
    surface.Release();
    dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
    dev->SetTexture(kSnowPixelSampler, nullptr);
    dev->SetTexture(kTrenchPixelSampler, nullptr);
}

void CheckTrenchDepthOnTheDevice(IDirect3DDevice9* dev)
{
    const ShaderCompiler compiler = LoadSystemShaderCompiler();
    if (!compiler.assemble)
        return;
    std::string pixelSource = kTrenchRigPixel;
    std::string why;
    const bool edited = EditTerrainPixelSource(pixelSource, why, true);
    const std::vector<DWORD> pixelCode = edited ? Assembled(compiler, pixelSource) : std::vector<DWORD>();
    const std::vector<DWORD> vertexCode = Assembled(compiler, kTrenchRigVertex);
    const std::vector<DWORD> planeCode = Assembled(compiler, kDepthPlaneVertex);
    LiftRig rig;
    rig.device = dev;
    IDirect3DPixelShader9* deepPixel = nullptr;
    IDirect3DVertexShader9* plane = nullptr;
    IDirect3DSurface9* depth = nullptr;
    const bool created =
        !pixelCode.empty() && !vertexCode.empty() && !planeCode.empty() && rig.Create(compiler, vertexCode) &&
        SUCCEEDED(dev->CreatePixelShader(pixelCode.data(), &deepPixel)) &&
        SUCCEEDED(dev->CreateVertexShader(planeCode.data(), &plane)) &&
        SUCCEEDED(dev->CreateDepthStencilSurface(kTargetSide, kTargetSide, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0,
                                                 FALSE, &depth, nullptr));
    SnowMap snow;
    snow.Configure(Flat());
    FakeSnowTerrain terrain;
    const float camera[3] = {kCameraX, kCameraY, 0.0f};
    FillCompletely(snow, terrain, camera, 1);
    TrenchMap trenches;
    trenches.Place(kCameraX, kCameraY);
    const float from[2] = {kCameraX - 3.0f, kCameraY};
    const float to[2] = {kCameraX + 3.0f, kCameraY};
    trenches.StampScuff(Scuff(from[0], from[1], to[0], to[1], 0.5f, 1.0f));
    SnowSurface surface;
    bool drawn = created && surface.Upload(dev, snow) && surface.UploadTrenches(dev, trenches);
    const TrenchLight light = {{kCameraX, kCameraY + 0.001f, 40.0f}, {0.0f, 0.7071f, 0.7071f}, 0.3f};
    const float halfSpan = 2.0f;
    const float placement[4] = {1.0f / halfSpan, 1.0f / halfSpan, -kCameraX / halfSpan, -kCameraY / halfSpan};
    const float nearDepth = 1.0f;
    const float farDepth = 100.0f;
    const float depthRow[4] = {farDepth / (farDepth - nearDepth), -farDepth * nearDepth / (farDepth - nearDepth),
                               kTrenchDepthPushBack, 0.0f};
    const float planeViewDepth = 30.05f;
    const float planeDepth[4] = {depthRow[0] + depthRow[1] / planeViewDepth, 0.0f, 0.0f, 0.0f};
    const float quad[4][6] = {
        {kCameraX - halfSpan, kCameraY - halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
        {kCameraX + halfSpan, kCameraY - halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
        {kCameraX - halfSpan, kCameraY + halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
        {kCameraX + halfSpan, kCameraY + halfSpan, 10.0f, 0.0f, 0.0f, 1.0f},
    };
    float brightness[2] = {};
    D3DLOCKED_RECT locked = {};
    if (drawn)
    {
        SnowFrame frame = {SnowVertexConstantsFor(snow, false), SnowPixelConstantsFor(snow, false),
                           TrenchConstantsFor(trenches, true, 0.3f, light, {64.0f, 1.0f})};
        surface.Bind(dev, frame);
        dev->SetPixelShaderConstantF(kTrenchDepthRegister, depthRow, 1);
        dev->SetVertexShaderConstantF(11, placement, 1);
        dev->SetVertexShaderConstantF(12, planeDepth, 1);
        dev->SetVertexDeclaration(rig.declaration);
        dev->SetRenderTarget(0, rig.target);
        dev->SetDepthStencilSurface(depth);
        const D3DVIEWPORT9 viewport = {0, 0, kTargetSide, kTargetSide, 0.0f, 1.0f};
        dev->SetViewport(&viewport);
        dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        dev->SetRenderState(D3DRS_ZENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_ALWAYS);
        dev->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0, 1.0f, 0);
        drawn = SUCCEEDED(dev->BeginScene());
        dev->SetVertexShader(rig.vertex);
        dev->SetPixelShader(deepPixel);
        drawn = drawn && SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])));
        dev->SetRenderState(D3DRS_ZFUNC, D3DCMP_LESS);
        dev->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        dev->SetVertexShader(plane);
        dev->SetPixelShader(rig.pixel);
        drawn = drawn && SUCCEEDED(dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0])));
        drawn = SUCCEEDED(dev->EndScene()) && drawn && SUCCEEDED(dev->GetRenderTargetData(rig.target, rig.readback)) &&
                SUCCEEDED(rig.readback->LockRect(&locked, nullptr, D3DLOCK_READONLY));
    }
    if (drawn)
    {
        const float rows[2] = {0.0f, 1.5f};
        for (int i = 0; i < 2; ++i)
            brightness[i] = RowBrightness(locked, static_cast<int>((1.0f - rows[i] / halfSpan) * 0.5f * kTargetSide));
        rig.readback->UnlockRect();
    }
    surface.Release();
    for (IUnknown* resource :
         {static_cast<IUnknown*>(deepPixel), static_cast<IUnknown*>(plane), static_cast<IUnknown*>(depth)})
        if (resource)
            resource->Release();
    dev->SetTexture(D3DVERTEXTEXTURESAMPLER0, nullptr);
    dev->SetTexture(kSnowPixelSampler, nullptr);
    dev->SetTexture(kTrenchPixelSampler, nullptr);
    std::printf("     trench depth: plane 0.05 yd under the snow seen in the trench %.0f, on untouched snow %.0f\n",
                brightness[0], brightness[1]);
    Check(drawn, "the trench depth rig draws with the depth-writing terrain pixel shader");
    Check(brightness[0] == 255.0f && brightness[1] < 255.0f,
          "the depth-writing shader puts a trench floor behind a plane just under the snow and leaves untouched "
          "snow in front of it");
}

void CheckSnow()
{
    CheckSnowLayers();
    CheckAlphaDecoding();
    CheckDepthRules();
    CheckSnowMap();
    CheckRockStaysLow();
    CheckConstants();
    CheckTrenchShape();
    CheckTrenchMap();
    CheckFootprintGaits();
    CheckFootprintStamper();
    CheckDecalLift();
    CheckShaderEdits();
}
}
