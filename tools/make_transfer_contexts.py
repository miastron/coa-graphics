import argparse
import csv
import math
import sys

sys.dont_write_bytecode = True

from convert_client_lights import (AMBIENT_BAND, CLIENT_UNITS_PER_YARD, DIRECT_BAND, FLOAT_BANDS_PER_PARAMS,
                                   FOG_BAND, FOG_DISTANCE_BAND, HALF_MINUTES_PER_DAY, SUN_BAND, ClientLighting,
                                   ClientTables, is_map_wide, world_position)

DESCRIPTION = ("Write the sky light of the transfer sheet's places, hours and weathers (tests/transfer_contexts.csv) "
               "from the client's Light, LightParams, LightIntBand and LightFloatBand tables (a DBFilesClient folder "
               "or zip), blended the way the client blends them.")

NO_ZONE_OUTLINE = None
BOREAN_TUNDRA_OUTLINE_LIGHT = 914
STORM_PEAKS_OUTLINE_LIGHT = 1777
PLACES = (
    ("elwynn", 0, (-9448.55, 68.236, 56.3225), NO_ZONE_OUTLINE),
    ("duskwood", 0, (-10573.0, -1182.51, 28.0148), NO_ZONE_OUTLINE),
    ("tirisfal", 0, (2259.25, 290.43, 34.1137), NO_ZONE_OUTLINE),
    ("tanaris", 1, (-7177.15, -3785.34, 8.36981), NO_ZONE_OUTLINE),
    ("hellfire", 530, (-748.211, 2681.52, 100.35), NO_ZONE_OUTLINE),
    ("nagrand", 530, (-1321.34, 7239.12, 32.7371), NO_ZONE_OUTLINE),
    ("borean", 571, (2213.95, 5273.15, 11.2565), BOREAN_TUNDRA_OUTLINE_LIGHT),
    ("stormpeaks", 571, (6123.7, -1059.19, 402.624), STORM_PEAKS_OUTLINE_LIGHT),
)
HOURS = (("dawn", 7.0 / 24.0), ("noon", 12.0 / 24.0), ("dusk", 19.5 / 24.0), ("midnight", 0.0))
WEATHERS = (("clear", 0.0), ("storm", 1.0))
COLUMNS = ("place", "hour", "weather", "map", "x", "y", "z", "day", "storm", "moon", "to_light_x", "to_light_y",
           "to_light_z", "direct", "ambient", "fog", "sun", "fog_start_share", "zone_fog_yards", "lights")

LIGHT_ID = 0
LIGHT_MAP = 1
LIGHT_FALLOFF_START = 5
LIGHT_FALLOFF_END = 6
LIGHT_FIRST_PARAMS = 7
CLEAR_PARAMS = 0
STORM_PARAMS = 2
FOG_START_BAND = 1
LARGEST_FOG_START_SHARE = 1.0
SHORTEST_FOG_END_YARDS = 10.0
FALLBACK_LIGHT_ID = 1
SMALLEST_AREA_LIGHT_END_YARDS = 3.0
DEEP_INSIDE_AN_OUTLINE = 1.0
BLACK = (0, 0, 0)

SUNRISE = 0.22916667
NOON = 0.5
SUNSET = 0.89583331
MOONSET = 0.16666667
MOONRISE = 0.91666669
SUN_LIGHTS_FROM = 0.2013889
SUN_LIGHTS_UNTIL = 0.9236111
BODY_AZIMUTH = 0.78539819
SUN_ANGLE_FROM_ZENITH = ((SUNRISE, 1.7453293), (0.49652779, 0.087266468), (NOON, 0.087266468),
                         (0.50347221, 0.087266468), (SUNSET, 1.7453293))
MOON_ANGLE_FROM_ZENITH = ((0.0, 0.61086524), (0.0034722222, 0.61086524), (MOONSET, 1.7453293),
                          (MOONRISE, 1.7453293), (0.99652779, 0.61086524))
COINCIDING_TABLE_TIMES = 0.001


def table_value(table, day):
    following = next((index for index, (time, _) in enumerate(table) if day <= time), 0)
    previous = (following - 1) % len(table)
    span = table[following][0] - table[previous][0]
    if abs(span) < COINCIDING_TABLE_TIMES:
        return table[previous][1]
    fraction = ((day - table[previous][0]) % 1.0) / (span % 1.0)
    return table[previous][1] + (table[following][1] - table[previous][1]) * fraction


def lit_by_the_moon(day):
    return not SUN_LIGHTS_FROM <= day <= SUN_LIGHTS_UNTIL


def direction_to_light(day):
    angle = table_value(MOON_ANGLE_FROM_ZENITH if lit_by_the_moon(day) else SUN_ANGLE_FROM_ZENITH, day)
    return (math.cos(BODY_AZIMUTH) * math.sin(angle), math.sin(BODY_AZIMUTH) * math.sin(angle), math.cos(angle))


def band_time(day):
    return int(round(day * HALF_MINUTES_PER_DAY - 0.5))


class SkyLight:
    def __init__(self, direct, ambient, fog, sun, fog_start_share, zone_fog_yards):
        self.direct = direct
        self.ambient = ambient
        self.fog = fog
        self.sun = sun
        self.fog_start_share = fog_start_share
        self.zone_fog_yards = zone_fog_yards

    def mixed(self, other, share):
        def colour(a, b):
            return tuple(int(round(x + (y - x) * share)) for x, y in zip(a, b))

        def number(a, b):
            return a + (b - a) * share

        return SkyLight(colour(self.direct, other.direct), colour(self.ambient, other.ambient),
                        colour(self.fog, other.fog), colour(self.sun, other.sun),
                        number(self.fog_start_share, other.fog_start_share),
                        number(self.zone_fog_yards, other.zone_fog_yards))


def band_colour(lighting, params_id, band, time):
    entry = lighting.int_band(params_id, band)
    value = entry.at(time) if entry else None
    return tuple(int(round(channel)) for channel in value) if value else BLACK


def band_number(lighting, params_id, band, time):
    entry = lighting.floats.get(params_id * FLOAT_BANDS_PER_PARAMS - (FLOAT_BANDS_PER_PARAMS - 1) + band)
    value = entry.at(time) if entry else None
    return value if value is not None else 0.0


def params_sky_light(lighting, params_id, time):
    fog_start_share = band_number(lighting, params_id, FOG_START_BAND, time)
    zone_fog_yards = band_number(lighting, params_id, FOG_DISTANCE_BAND, time) / CLIENT_UNITS_PER_YARD
    return SkyLight(band_colour(lighting, params_id, DIRECT_BAND, time),
                    band_colour(lighting, params_id, AMBIENT_BAND, time),
                    band_colour(lighting, params_id, FOG_BAND, time), band_colour(lighting, params_id, SUN_BAND, time),
                    min(max(fog_start_share, 0.0), LARGEST_FOG_START_SHARE),
                    max(zone_fog_yards, SHORTEST_FOG_END_YARDS))


def light_sky_light(lighting, light, time, storm):
    clear = params_sky_light(lighting, light[LIGHT_FIRST_PARAMS + CLEAR_PARAMS], time)
    storm_params = light[LIGHT_FIRST_PARAMS + STORM_PARAMS]
    if storm <= 0.0 or storm_params not in lighting.light_params:
        return clear
    return clear.mixed(params_sky_light(lighting, storm_params, time), min(storm, 1.0))


def distance_between(a, b):
    return math.sqrt(sum((p - q) ** 2 for p, q in zip(a, b)))


def area_weight(light, distance):
    start = light[LIGHT_FALLOFF_START] / CLIENT_UNITS_PER_YARD
    end = light[LIGHT_FALLOFF_END] / CLIENT_UNITS_PER_YARD
    if distance <= start:
        return 1.0
    return max(1.0 - (distance - start) / (end - start), 0.0)


def map_lights(lighting, map_id):
    on_map = [light for light in lighting.lights if light[LIGHT_MAP] == map_id]
    map_wide = [light for light in on_map if is_map_wide(light)]
    default = map_wide[-1] if map_wide else light_with_id(lighting, FALLBACK_LIGHT_ID)
    return default, [light for light in on_map if not is_map_wide(light)]


def light_with_id(lighting, light_id):
    return next(light for light in lighting.lights if light[LIGHT_ID] == light_id)


def blended_sky_light(lighting, map_id, position, outline_light, time, storm):
    default, placed = map_lights(lighting, map_id)
    sky = light_sky_light(lighting, default, time, storm)
    blend = [(default[LIGHT_ID], 1.0)]
    if outline_light is not NO_ZONE_OUTLINE:
        sky = sky.mixed(light_sky_light(lighting, light_with_id(lighting, outline_light), time, storm),
                        DEEP_INSIDE_AN_OUTLINE)
        blend.append((outline_light, DEEP_INSIDE_AN_OUTLINE))
    reaching = []
    for light in placed:
        end = light[LIGHT_FALLOFF_END] / CLIENT_UNITS_PER_YARD
        distance = distance_between(world_position(light), position)
        if end >= SMALLEST_AREA_LIGHT_END_YARDS and distance < end:
            reaching.append((distance, light))
    for distance, light in sorted(reaching, key=lambda entry: entry[0], reverse=True):
        weight = area_weight(light, distance)
        sky = sky.mixed(light_sky_light(lighting, light, time, storm), weight)
        blend.append((light[LIGHT_ID], weight))
    return sky, blend


def packed(colour):
    return "%02X%02X%02X" % colour


def context_row(lighting, place, hour, weather):
    place_name, map_id, position, outline_light = place
    hour_name, day = hour
    weather_name, storm = weather
    sky, blend = blended_sky_light(lighting, map_id, position, outline_light, band_time(day), storm)
    to_light = direction_to_light(day)
    return (place_name, hour_name, weather_name, map_id, "%.2f" % position[0], "%.2f" % position[1],
            "%.2f" % position[2], "%.6f" % day, "%.1f" % storm, int(lit_by_the_moon(day)), "%.4f" % to_light[0],
            "%.4f" % to_light[1], "%.4f" % to_light[2], packed(sky.direct), packed(sky.ambient), packed(sky.fog),
            packed(sky.sun), "%.5f" % sky.fog_start_share, "%.1f" % sky.zone_fog_yards,
            " ".join("%d:%.2f" % entry for entry in blend))


def main():
    parser = argparse.ArgumentParser(description=DESCRIPTION)
    parser.add_argument("client", help="the client's DBFilesClient folder or a zip archive with its light tables")
    parser.add_argument("output", help="output path, normally tests/transfer_contexts.csv")
    arguments = parser.parse_args()
    with ClientTables(arguments.client) as tables:
        lighting = ClientLighting(tables)
    rows = [context_row(lighting, place, hour, weather) for place in PLACES for hour in HOURS for weather in WEATHERS]
    with open(arguments.output, "w", newline="\n", encoding="ascii") as handle:
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(COLUMNS)
        writer.writerows(rows)
    print("%d contexts written to %s" % (len(rows), arguments.output))
    return 0


if __name__ == "__main__":
    sys.exit(main())
