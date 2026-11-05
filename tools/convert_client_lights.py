import argparse
import math
import os
import struct
import sys
import zipfile

DESCRIPTION = ("Add fog lights for every map the Classic data in a fogdata.bin does not cover, placed and coloured by "
               "the 3.3.5 client's Light, LightIntBand, LightFloatBand, LightParams and LightSkybox tables (a "
               "DBFilesClient folder or zip), with the layers and glow of the Classic light whose 3.3.5 lighting is "
               "most alike.")
CLIENT_TABLES = ("Light.dbc", "LightIntBand.dbc", "LightFloatBand.dbc", "LightParams.dbc", "LightSkybox.dbc")
DBC_MAGIC = b"WDBC"
DBC_HEADER_FORMAT = "<4s4I"
DBC_LIGHT_FORMAT = "<2i5f8i"
DBC_INT_BAND_FORMAT = "<2I16I16I"
DBC_FLOAT_BAND_FORMAT = "<2I16I16f"
DBC_LIGHT_PARAMS_FORMAT = "<4if4f"
DBC_SKYBOX_FORMAT = "<3I"
LIGHT_PARAMS_SKYBOX = 2
SKYBOX_DRAWS_SUN_AND_MOON = 0x2
DBC_BAND_KEYS = 16
INT_BANDS_PER_PARAMS = 18
FLOAT_BANDS_PER_PARAMS = 6
DIRECT_BAND = 0
AMBIENT_BAND = 1
SKY_TOP_BAND = 2
SKY_MIDDLE_BAND = 3
FOG_BAND = 7
SUN_BAND = 9
FOG_DISTANCE_BAND = 0
MATCHED_INT_BANDS = (FOG_BAND, SKY_TOP_BAND, SKY_MIDDLE_BAND, DIRECT_BAND, AMBIENT_BAND)
FOG_DISTANCE_MATCH_WEIGHT = 0.05

CLIENT_COORDINATE_ORIGIN = 17066.666
CLIENT_UNITS_PER_YARD = 36.0
SAME_LIGHT_TOLERANCE_YARDS = 5.0

FILE_MAGIC = b"VFD1"
FORMAT_VERSION = 4
HEADER_FORMAT = "<4s8I"
LIGHT_FORMAT = "<Ii5f8I"
PARAMS_FORMAT = "<3If"
KEY_FORMAT = "<2H3I"
LAYER_FORMAT = "<4I11fI11f"
ZONE_LIGHT_FORMAT = "<IiI2f2I"
ZONE_POINT_FORMAT = "<2f"
GRADING_CURVE_FORMAT = "<I32B"

LIGHT_PARAMS_SLOTS = 8
CLEAR_SLOT = 0
GENERATED_ID_BASE = 1000000
HALF_MINUTES_PER_DAY = 2880
MATCH_SAMPLES = 24
DISPLAY_GAMMA = 2.2
LUMINANCE_WEIGHTS = (0.2126, 0.7152, 0.0722)
DARK_LUMINANCE = 1.0e-4
BRIGHTNESS_RATIO_RANGE = (0.5, 2.0)
DENSITY_RATIO_RANGE = (0.5, 2.0)
RGB_MASK = 0xFFFFFF

LAYER_DIFFUSE, LAYER_EMISSIVE, LAYER_SHADOW_EMISSIVE = 0, 1, 2
LAYER_FLAGS = 3
LAYER_DENSITY = 5
LAYER_UPPER_HEIGHT = 8
LAYER_LOWER_HEIGHT = 10
RELATIVE_HEIGHTS_FLAG = 0x2
NOISE_FLAG = 0x4
NO_SUN_FLAG = 0x10
CLEAR_SKY_FLAG = 0x20
GROUND_FOG_ONLY_SKYBOXES = {"hellfireskybox.mdx": (0.0, 0.0, 0.3)}
HEIGHT_SHIFT_STEP = 25.0
LAYER_NOISE_FADE = 15
KEY_DIRECT = 3


class ClientTables:
    def __init__(self, source):
        self.source = source
        self.archive = zipfile.ZipFile(source) if zipfile.is_zipfile(source) else None
        self.members = {m.rsplit("/", 1)[-1].lower(): m for m in self.archive.namelist()} if self.archive else {}

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        if self.archive:
            self.archive.close()

    def read(self, name):
        if self.archive:
            member = self.members.get(name.lower())
            if member is None:
                raise SystemExit("%s has no %s" % (self.source, name))
            return self.archive.read(member)
        for candidate in os.listdir(self.source):
            if candidate.lower() == name.lower():
                with open(os.path.join(self.source, candidate), "rb") as handle:
                    return handle.read()
        raise SystemExit("%s has no %s" % (self.source, name))

    def strings(self, name):
        data = self.read(name)
        _, count, _, size, string_size = struct.unpack_from(DBC_HEADER_FORMAT, data)
        start = struct.calcsize(DBC_HEADER_FORMAT) + count * size
        return data[start:start + string_size]

    def records(self, name, record_format):
        data = self.read(name)
        magic, count, _, size, _ = struct.unpack_from(DBC_HEADER_FORMAT, data)
        if magic != DBC_MAGIC or size != struct.calcsize(record_format):
            raise SystemExit("%s is not a 3.3.5 %s" % (name, record_format))
        start = struct.calcsize(DBC_HEADER_FORMAT)
        return [struct.unpack_from(record_format, data, start + i * size) for i in range(count)]


class Band:
    def __init__(self, times, values):
        self.times = times
        self.values = values

    def at(self, half_minute):
        count = len(self.times)
        if count == 0:
            return None
        if count == 1:
            return self.values[0]
        following = next((i for i, t in enumerate(self.times) if t > half_minute), 0)
        previous = (following - 1) % count
        start = self.times[previous]
        end = self.times[following]
        if start > half_minute:
            start -= HALF_MINUTES_PER_DAY
        if end <= start:
            end += HALF_MINUTES_PER_DAY
        fraction = min(max((half_minute - start) / (end - start), 0.0), 1.0)
        return self.mix(self.values[previous], self.values[following], fraction)

    @staticmethod
    def mix(a, b, fraction):
        if isinstance(a, tuple):
            return tuple(x + (y - x) * fraction for x, y in zip(a, b))
        return a + (b - a) * fraction


def rgb_bytes(value):
    return ((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF)


def int_bands(tables):
    bands = {}
    for record in tables.records("LightIntBand.dbc", DBC_INT_BAND_FORMAT):
        count = min(record[1], DBC_BAND_KEYS)
        bands[record[0]] = Band(record[2:2 + count], [rgb_bytes(v) for v in record[18:18 + count]])
    return bands


def float_bands(tables):
    bands = {}
    for record in tables.records("LightFloatBand.dbc", DBC_FLOAT_BAND_FORMAT):
        count = min(record[1], DBC_BAND_KEYS)
        bands[record[0]] = Band(record[2:2 + count], list(record[18:18 + count]))
    return bands


def skyboxes(tables):
    names = tables.strings("LightSkybox.dbc")
    found = {}
    for skybox_id, name_offset, flags in tables.records("LightSkybox.dbc", DBC_SKYBOX_FORMAT):
        path = names[name_offset:names.index(b"\0", name_offset)].decode("ascii", "replace")
        found[skybox_id] = (path.replace("/", "\\").rsplit("\\", 1)[-1].lower(), flags)
    return found


class ClientLighting:
    def __init__(self, tables):
        self.lights = tables.records("Light.dbc", DBC_LIGHT_FORMAT)
        self.ints = int_bands(tables)
        self.floats = float_bands(tables)
        self.light_params = {p[0]: p for p in tables.records("LightParams.dbc", DBC_LIGHT_PARAMS_FORMAT)}
        self.skyboxes = skyboxes(tables)

    def skybox(self, params_id):
        params = self.light_params.get(params_id)
        return self.skyboxes.get(params[LIGHT_PARAMS_SKYBOX]) if params else None

    def draws_sun(self, params_id):
        skybox = self.skybox(params_id)
        if skybox and not skybox[1] & SKYBOX_DRAWS_SUN_AND_MOON:
            return False
        sun = self.int_band(params_id, SUN_BAND)
        return not sun or any(any(rgb) for rgb in sun.values)

    def int_band(self, params_id, band):
        return self.ints.get(params_id * INT_BANDS_PER_PARAMS - (INT_BANDS_PER_PARAMS - 1) + band)

    def colour(self, params_id, band, half_minute):
        entry = self.int_band(params_id, band)
        value = entry.at(half_minute) if entry else None
        return linear(value) if value else (0.0, 0.0, 0.0)

    def fog_distance(self, params_id, half_minute):
        entry = self.floats.get(params_id * FLOAT_BANDS_PER_PARAMS - (FLOAT_BANDS_PER_PARAMS - 1) + FOG_DISTANCE_BAND)
        value = entry.at(half_minute) if entry else None
        return value / CLIENT_UNITS_PER_YARD if value and value > 0.0 else None

    def signature(self, params_id):
        features = []
        for sample in range(MATCH_SAMPLES):
            half_minute = sample * HALF_MINUTES_PER_DAY / MATCH_SAMPLES
            for band in MATCHED_INT_BANDS:
                features.extend(self.colour(params_id, band, half_minute))
            distance = self.fog_distance(params_id, half_minute)
            features.append(math.log(distance) * FOG_DISTANCE_MATCH_WEIGHT if distance else 0.0)
        return features


def linear(rgb):
    return tuple((c / 255.0) ** DISPLAY_GAMMA for c in rgb)


def luminance(rgb):
    return sum(w * c for w, c in zip(LUMINANCE_WEIGHTS, rgb))


def clamped(value, bounds):
    return min(max(value, bounds[0]), bounds[1])


def is_map_wide(light):
    return light[2:5] == (0.0, 0.0, 0.0) and light[6] <= 0.0


def world_position(light):
    if is_map_wide(light):
        return 0.0, 0.0, 0.0
    _, _, x, y, z = light[:5]
    return (CLIENT_COORDINATE_ORIGIN - z / CLIENT_UNITS_PER_YARD, CLIENT_COORDINATE_ORIGIN - x / CLIENT_UNITS_PER_YARD,
            y / CLIENT_UNITS_PER_YARD)


def is_map_wide_classic(classic):
    return classic[2:4] == (0.0, 0.0) and classic[6] <= 0.0


def placed_in_height(z):
    return z != 0.0


def height_shift_between(classic, light, z):
    if is_map_wide(light) or is_map_wide_classic(classic):
        return 0.0
    if not placed_in_height(z) or not placed_in_height(classic[4]):
        return 0.0
    return float(round((z - classic[4]) / HEIGHT_SHIFT_STEP) * HEIGHT_SHIFT_STEP)


def recoloured(packed, template_client, target_client):
    authored = linear(rgb_bytes(packed))
    authored_luminance = luminance(authored)
    if authored_luminance <= DARK_LUMINANCE:
        return packed
    target_luminance = luminance(target_client)
    template_luminance = luminance(template_client)
    hue_source = target_client if target_luminance > DARK_LUMINANCE else authored
    hue = [c / luminance(hue_source) for c in hue_source]
    ratio = target_luminance / template_luminance if template_luminance > DARK_LUMINANCE else 1.0
    brightness = authored_luminance * clamped(ratio, BRIGHTNESS_RATIO_RANGE)
    gamma = [max(c * brightness, 0.0) ** (1.0 / DISPLAY_GAMMA) for c in hue]
    peak = max(gamma)
    if peak > 1.0:
        gamma = [c / peak for c in gamma]
    r, g, b = (int(round(c * 255.0)) for c in gamma)
    return (r << 16) | (g << 8) | b


class FogData:
    def __init__(self, data):
        magic, version, *counts = struct.unpack_from(HEADER_FORMAT, data)
        if magic != FILE_MAGIC or version != FORMAT_VERSION:
            raise SystemExit("the input is not a format %d fogdata.bin" % FORMAT_VERSION)
        offset = struct.calcsize(HEADER_FORMAT)
        tables = []
        for record_format, count in zip((LIGHT_FORMAT, PARAMS_FORMAT, KEY_FORMAT, LAYER_FORMAT, ZONE_LIGHT_FORMAT,
                                         ZONE_POINT_FORMAT, GRADING_CURVE_FORMAT), counts):
            size = struct.calcsize(record_format)
            tables.append([struct.unpack_from(record_format, data, offset + i * size) for i in range(count)])
            offset += size * count
        if offset != len(data):
            raise SystemExit("the input fogdata.bin has %d bytes, its header promises %d" % (len(data), offset))
        lights, params, self.keys, self.layers, self.zone_lights, self.zone_points, self.curves = tables
        self.lights = [light for light in lights if light[0] < GENERATED_ID_BASE]
        self.params = {p[0]: p for p in params if p[0] < GENERATED_ID_BASE}

    def keys_of(self, params_id):
        params = self.params.get(params_id)
        if not params:
            return []
        return [(key, self.layers[key[2]:key[2] + key[1]]) for key in self.keys[params[1]:params[1] + params[2]]]

    def has_fog(self, params_id):
        return any(layers for _, layers in self.keys_of(params_id))


def classic_maps(fog):
    return {light[1] for light in fog.lights}


def templates(fog, client):
    by_id = {light[0]: light for light in client.lights}
    found = []
    for classic in fog.lights:
        twin = by_id.get(classic[0])
        if not twin or twin[1] != classic[1] or not fog.has_fog(classic[7 + CLEAR_SLOT]) or not twin[7 + CLEAR_SLOT]:
            continue
        x, y, _ = world_position(twin)
        if abs(x - classic[2]) > SAME_LIGHT_TOLERANCE_YARDS or abs(y - classic[3]) > SAME_LIGHT_TOLERANCE_YARDS:
            continue
        found.append((classic, twin, client.signature(twin[7 + CLEAR_SLOT])))
    return found


def nearest_template(pool, signature):
    return min(pool, key=lambda t: sum((a - b) ** 2 for a, b in zip(t[2], signature)))


class Builder:
    def __init__(self, fog, client):
        self.fog = fog
        self.client = client
        self.params = dict(fog.params)
        self.keys = []
        self.layers = []
        self.lights = list(fog.lights)
        self.generated = {}
        self.rebuild_classic_keys()

    def rebuild_classic_keys(self):
        for params_id in sorted(self.params):
            first = len(self.keys)
            for key, layers in self.fog.keys_of(params_id):
                self.keys.append(key[:2] + (len(self.layers),) + key[3:])
                self.layers.extend(layers)
            old = self.params[params_id]
            self.params[params_id] = (params_id, first, len(self.keys) - first, old[3])

    def add_light(self, light, template):
        classic, twin = template[:2]
        light_id = GENERATED_ID_BASE + light[0]
        x, y, z = world_position(light)
        height_shift = height_shift_between(classic, light, z)
        slots = []
        for slot in range(LIGHT_PARAMS_SLOTS):
            target_params = light[7 + slot]
            if not target_params:
                slots.append(0)
                continue
            classic_params = classic[7 + slot] if self.fog.has_fog(classic[7 + slot]) else classic[7 + CLEAR_SLOT]
            glow_params = classic[7 + slot] if classic[7 + slot] in self.fog.params else classic[7 + CLEAR_SLOT]
            twin_params = twin[7 + slot] or twin[7 + CLEAR_SLOT]
            recipe = (classic_params, twin_params, target_params, glow_params, height_shift)
            if recipe not in self.generated:
                self.generated[recipe] = GENERATED_ID_BASE + len(self.generated)
                self.add_params(self.generated[recipe], *recipe)
            slots.append(self.generated[recipe])
        start, end = light[5] / CLIENT_UNITS_PER_YARD, light[6] / CLIENT_UNITS_PER_YARD
        self.lights.append((light_id, light[1], x, y, z, start, end, *slots))

    def add_params(self, params_id, classic_params, twin_params, target_params, glow_params, height_shift):
        first = len(self.keys)
        added_flags = 0 if self.client.draws_sun(target_params) else NO_SUN_FLAG
        skybox = self.client.skybox(target_params)
        layer_densities = GROUND_FOG_ONLY_SKYBOXES.get(skybox[0]) if skybox else None
        if layer_densities:
            added_flags |= CLEAR_SKY_FLAG
        for key, layers in self.fog.keys_of(classic_params):
            half_minute = key[0]
            colour = lambda params, band: self.client.colour(params, band, half_minute)
            twin_fog, target_fog = colour(twin_params, FOG_BAND), colour(target_params, FOG_BAND)
            twin_direct, target_direct = colour(twin_params, DIRECT_BAND), colour(target_params, DIRECT_BAND)
            density = self.density_ratio(twin_params, target_params, half_minute)
            self.keys.append((half_minute, len(layers), len(self.layers),
                              recoloured(key[KEY_DIRECT], twin_direct, target_direct), key[4]))
            for index, layer in enumerate(layers):
                layer = list(layer)
                for field in (LAYER_EMISSIVE, LAYER_SHADOW_EMISSIVE, LAYER_NOISE_FADE):
                    layer[field] = recoloured(layer[field], twin_fog, target_fog) & RGB_MASK
                layer[LAYER_DIFFUSE] = recoloured(layer[LAYER_DIFFUSE], twin_direct, target_direct) & RGB_MASK
                layer[LAYER_DENSITY] *= density * (layer_densities[index] if layer_densities else 1.0)
                layer[LAYER_FLAGS] = layer[LAYER_FLAGS] & ~NOISE_FLAG | added_flags
                if not layer[LAYER_FLAGS] & RELATIVE_HEIGHTS_FLAG:
                    layer[LAYER_UPPER_HEIGHT] += height_shift
                    layer[LAYER_LOWER_HEIGHT] += height_shift
                self.layers.append(tuple(layer))
        self.params[params_id] = (params_id, first, len(self.keys) - first, self.fog.params[glow_params][3])

    def density_ratio(self, twin_params, target_params, half_minute):
        twin = self.client.fog_distance(twin_params, half_minute)
        target = self.client.fog_distance(target_params, half_minute)
        return clamped(twin / target, DENSITY_RATIO_RANGE) if twin and target else 1.0

    def blob(self):
        params = [self.params[p] for p in sorted(self.params)]
        tables = ((LIGHT_FORMAT, self.lights), (PARAMS_FORMAT, params), (KEY_FORMAT, self.keys),
                  (LAYER_FORMAT, self.layers), (ZONE_LIGHT_FORMAT, self.fog.zone_lights),
                  (ZONE_POINT_FORMAT, self.fog.zone_points), (GRADING_CURVE_FORMAT, self.fog.curves))
        header = struct.pack(HEADER_FORMAT, FILE_MAGIC, FORMAT_VERSION, *(len(rows) for _, rows in tables))
        return header + b"".join(struct.pack(f, *row) for f, rows in tables for row in rows)


def convert(fog, client):
    pool = templates(fog, client)
    if not pool:
        raise SystemExit("no Classic light with fog has a twin in the client's Light table")
    covered = classic_maps(fog)
    builder = Builder(fog, client)
    chosen = {}
    for light in client.lights:
        if light[1] in covered or not light[7 + CLEAR_SLOT]:
            continue
        template = nearest_template(pool, client.signature(light[7 + CLEAR_SLOT]))
        builder.add_light(light, template)
        chosen.setdefault(light[1], []).append((light[0], template[0][0]))
    return builder.blob(), chosen, len(pool)


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("fogdata", help="the fogdata.bin with the Classic lights, normally data/fogdata.bin")
    parser.add_argument("client", help="the client's DBFilesClient folder or a zip archive with %s"
                                       % ", ".join(CLIENT_TABLES))
    parser.add_argument("output", help="output path, normally data/fogdata.bin")
    parser.add_argument("--list", action="store_true", help="print the Classic light each new light takes after")
    args = parser.parse_args()
    with open(args.fogdata, "rb") as handle:
        fog = FogData(handle.read())
    with ClientTables(args.client) as tables:
        client = ClientLighting(tables)
    blob, chosen, pool = convert(fog, client)
    with open(args.output, "wb") as handle:
        handle.write(blob)
    maps = ", ".join("%d: %d" % (m, len(lights)) for m, lights in sorted(chosen.items()))
    print("%s: %d Classic lights kept, %d client lights added from %d templates on maps %s; %d bytes"
          % (args.output, len(fog.lights), sum(len(v) for v in chosen.values()), pool, maps, len(blob)))
    if args.list:
        for map_id, lights in sorted(chosen.items()):
            print("map %d: %s" % (map_id, " ".join("%d<-%d" % pair for pair in lights)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
