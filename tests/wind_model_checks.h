#pragma once

namespace wind_model_checks
{
constexpr float kTolerance = 1.0e-4f;
constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr int kSettleFrames = 60 * 30;
constexpr uint32_t kWdbcMagic = 0x43424457;
constexpr size_t kShippedRows = 15;
constexpr int kDeadminesMap = 36;
constexpr uint32_t kZoneArea = 10;
constexpr uint32_t kSubArea = 11;

std::vector<unsigned char> TableBytes(uint32_t records, uint32_t fields, const std::vector<uint32_t>& cells,
                                      const std::string& strings, uint32_t recordSize = 0)
{
    const uint32_t header[5] = {kWdbcMagic, records, fields, recordSize ? recordSize : fields * 4,
                                static_cast<uint32_t>(strings.size())};
    std::vector<unsigned char> out(sizeof(header) + cells.size() * sizeof(uint32_t) + strings.size());
    std::memcpy(out.data(), header, sizeof(header));
    std::memcpy(out.data() + sizeof(header), cells.data(), cells.size() * sizeof(uint32_t));
    std::memcpy(out.data() + sizeof(header) + cells.size() * sizeof(uint32_t), strings.data(), strings.size());
    return out;
}

uint32_t Bits(float value)
{
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void CheckTableReader()
{
    const std::string strings("\0hi\0", 4);
    const std::vector<unsigned char> good =
        TableBytes(2, 3, {1, Bits(2.5f), 1, 7, Bits(-1.0f), 4}, strings);
    WdbcTable table;
    std::string error;
    const bool parsed = table.Parse(good.data(), good.size(), error);
    Check(parsed && table.Records() == 2 && table.Fields() == 3 && table.Int(0, 0) == 1 &&
              std::fabs(table.Float(0, 1) - 2.5f) < kTolerance && std::string(table.Text(0, 2)) == "hi" &&
              table.Int(1, 0) == 7 && table.Float(1, 1) == -1.0f && std::string(table.Text(1, 2)) == "",
          "a WDBC table is read as records of four-byte columns with its string block");
    Check(table.Raw(2, 0) == 0 && table.Raw(0, 3) == 0 && std::string(table.Text(1, 1)) == "" &&
              std::string(table.Text(0, 3)) == "",
          "a record, column or string offset outside the table reads as zero or empty");

    std::vector<unsigned char> badMagic = good;
    badMagic[0] ^= 0xFF;
    std::vector<unsigned char> truncated(good.begin(), good.end() - 3);
    std::vector<unsigned char> shortHeader(good.begin(), good.begin() + 12);
    const std::vector<unsigned char> oddRecord = TableBytes(2, 3, {1, 2, 3, 4, 5, 6}, strings, 16);
    const std::vector<unsigned char> noColumns = TableBytes(0, 0, {}, "");
    WdbcTable other;
    Check(!other.Parse(badMagic.data(), badMagic.size(), error) &&
              !other.Parse(truncated.data(), truncated.size(), error) &&
              !other.Parse(shortHeader.data(), shortHeader.size(), error) &&
              !other.Parse(oddRecord.data(), oddRecord.size(), error) &&
              !other.Parse(noColumns.data(), noColumns.size(), error) && other.Records() == 0,
          "a table with another magic, a wrong size, a short header or a record size that is not its columns is "
          "refused");
    Check(!other.Load("no-such-folder\\WindProfile.cdbc", error) && !error.empty(), "a missing table file is refused");
}

void CheckShippedTable()
{
    const std::string path = std::string(WIND_DATA_DIR) + "\\WindProfile.cdbc";
    wind::WindModel model;
    std::string error;
    const bool loaded = model.LoadProfiles(path, error);
    if (!loaded)
        std::printf("     %s\n", error.c_str());
    Check(loaded && model.RowCount() == kShippedRows && !model.BuiltIn(),
          "the shipped WindProfile.cdbc loads with its fifteen rows");

    WdbcTable table;
    Check(table.Load(path, error) && table.Fields() == 3 + wind::FieldCount + 2 &&
              table.Int(0, 1) == wind::ScopeGlobal && std::fabs(table.Float(0, 4) - 0.65f) < kTolerance &&
              table.Int(1, 1) == wind::ScopeMap && table.Int(1, 2) == kDeadminesMap,
          "the shipped WindProfile.cdbc starts with the global row, then the map rows, in the column order the model "
          "reads");

    wind::WindModel fileOnly;
    fileOnly.LoadProfiles(path, error);
    wind::Place place;
    place.mapId = 0;
    fileOnly.Update(kFrameSeconds, place);
    Check(std::fabs(fileOnly.Profile()[wind::GroundMax] - 0.65f) < kTolerance &&
              std::fabs(fileOnly.Profile()[wind::GustStrength] - 0.6f) < kTolerance,
          "a place with no row of its own takes the shipped global profile");
    wind::WindModel deadmines;
    deadmines.LoadProfiles(path, error);
    place.mapId = kDeadminesMap;
    deadmines.Update(kFrameSeconds, place);
    Check(deadmines.Profile()[wind::GroundMax] == 0.0f && deadmines.Steady() == 0.0f,
          "the shipped Deadmines map row is calm");
}

wind::Row RowOf(int scopeType, int32_t scopeId, float fill)
{
    wind::Row row;
    row.scopeType = scopeType;
    row.scopeId = scopeId;
    for (float& v : row.value)
        v = fill;
    return row;
}

wind::Row GlobalRow(float groundMin, float groundMax, float weatherInfluence = 0.0f, float gustStrength = 0.0f)
{
    wind::Row row = RowOf(wind::ScopeGlobal, 0, 0.0f);
    row.value[wind::GroundMin] = groundMin;
    row.value[wind::GroundMax] = groundMax;
    row.value[wind::WeatherInfluence] = weatherInfluence;
    row.value[wind::GustStrength] = gustStrength;
    row.value[wind::GustFrequency] = 1.0f;
    return row;
}

void Settle(wind::WindModel& model, const wind::Place& place, int frames = kSettleFrames)
{
    for (int i = 0; i < frames; ++i)
        model.Update(kFrameSeconds, place);
}

void CheckResolution()
{
    wind::Row map = RowOf(wind::ScopeMap, 5, -1.0f);
    map.value[wind::GroundMax] = 0.9f;
    wind::Row zone = RowOf(wind::ScopeArea, kZoneArea, -1.0f);
    zone.value[wind::GroundMin] = 0.2f;
    zone.value[wind::GroundMax] = 0.25f;
    wind::Row sub = RowOf(wind::ScopeArea, kSubArea, -1.0f);
    sub.value[wind::GroundMax] = 0.3f;
    wind::WindModel model;
    model.SetProfiles({GlobalRow(0.1f, 0.5f, 0.4f), map, zone, sub});

    wind::Place place;
    place.mapId = 7;
    model.Update(kFrameSeconds, place);
    Check(std::fabs(model.Profile()[wind::GroundMax] - 0.5f) < kTolerance &&
              std::fabs(model.Profile()[wind::WeatherInfluence] - 0.4f) < kTolerance,
          "a place without a map or area row takes the global profile");

    place.mapId = 5;
    wind::WindModel mapped;
    mapped.SetProfiles({GlobalRow(0.1f, 0.5f, 0.4f), map, zone, sub});
    mapped.Update(kFrameSeconds, place);
    Check(std::fabs(mapped.Profile()[wind::GroundMax] - 0.9f) < kTolerance &&
              std::fabs(mapped.Profile()[wind::GroundMin] - 0.1f) < kTolerance,
          "a map row overrides the global profile and a negative field takes the global value");

    place.areaChain[0] = kSubArea;
    place.areaChain[1] = kZoneArea;
    place.areaCount = 2;
    wind::WindModel areas;
    areas.SetProfiles({GlobalRow(0.1f, 0.5f, 0.4f), map, zone, sub});
    areas.Update(kFrameSeconds, place);
    Check(std::fabs(areas.Profile()[wind::GroundMax] - 0.3f) < kTolerance &&
              std::fabs(areas.Profile()[wind::GroundMin] - 0.2f) < kTolerance &&
              std::fabs(areas.Profile()[wind::WeatherInfluence] - 0.4f) < kTolerance,
          "the sub-area row wins over its zone, the zone over the map and the map over the global row, field by "
          "field");

    wind::WindModel none;
    none.SetProfiles({});
    none.Update(kFrameSeconds, place);
    const float at[3] = {10.0f, 20.0f, 0.0f};
    Check(none.Steady() == 0.0f && none.At(at).strength == 0.0f,
          "a table without a global row is a calm world, not the built-in wind");
}

void CheckBlending()
{
    wind::Row map = RowOf(wind::ScopeMap, 5, -1.0f);
    map.value[wind::GroundMin] = 0.8f;
    map.value[wind::GroundMax] = 0.8f;
    wind::WindModel model;
    model.SetProfiles({GlobalRow(0.1f, 0.1f), map});
    wind::Place place;
    place.mapId = 1;
    Settle(model, place, 2);
    const float before = model.Steady();
    place.mapId = 5;
    model.Update(kFrameSeconds, place);
    const float first = model.Steady();
    Settle(model, place, 60 * 20);
    Check(std::fabs(before - 0.1f) < kTolerance && first > before && first < 0.2f &&
              std::fabs(model.Steady() - 0.8f) < 0.01f,
          "crossing into another profile blends over a second or two instead of jumping");
}

void CheckSteadyWind()
{
    wind::WindModel model;
    model.SetProfiles({GlobalRow(0.2f, 0.8f, 1.0f)});
    wind::Place calm;
    float low = 10.0f;
    float high = -10.0f;
    for (int i = 0; i < kSettleFrames; ++i)
    {
        model.Update(kFrameSeconds, calm);
        low = std::min(low, model.Steady());
        high = std::max(high, model.Steady());
    }
    Check(low >= 0.2f - kTolerance && high <= 0.2f + 0.6f * 0.7f + kTolerance && high > low,
          "the steady wind wanders inside the lower part of its range in fair weather");

    wind::Place storm = calm;
    storm.weather = 1.0f;
    Settle(model, storm, 10);
    Check(std::fabs(model.Steady() - 0.8f) < kTolerance, "full weather with full influence pushes the steady wind to "
                                                         "its maximum");

    wind::WindModel deaf;
    deaf.SetProfiles({GlobalRow(0.2f, 0.8f, 0.0f)});
    deaf.Update(kFrameSeconds, storm);
    Check(deaf.Steady() < 0.8f - kTolerance, "a profile with no weather influence ignores the weather");

    wind::Row locked = GlobalRow(0.3f, 0.3f);
    locked.value[wind::LockedDirection] = 90.0f;
    locked.value[wind::LockStrength] = 1.0f;
    wind::WindModel steered;
    steered.SetProfiles({locked});
    float worst = 0.0f;
    for (int i = 0; i < 600; ++i)
    {
        steered.Update(kFrameSeconds, calm);
        worst = std::max(worst, std::fabs(steered.Bearing() - 90.0f));
    }
    Check(worst < 1.0e-3f, "a fully locked direction holds the bearing");

    wind::WindModel drifting;
    drifting.SetProfiles({GlobalRow(0.3f, 0.3f)});
    float bearingLow = 720.0f;
    float bearingHigh = -720.0f;
    for (int i = 0; i < kSettleFrames * 4; ++i)
    {
        drifting.Update(kFrameSeconds, calm);
        bearingLow = std::min(bearingLow, drifting.Bearing());
        bearingHigh = std::max(bearingHigh, drifting.Bearing());
    }
    Check(bearingLow >= 0.0f && bearingHigh < 360.0f, "the bearing stays a compass heading below 360");
}

void CheckSamples()
{
    wind::WindModel model;
    model.SetProfiles({GlobalRow(0.4f, 0.6f, 0.5f, 0.8f)});
    wind::Place place;
    place.mapId = 1;
    Settle(model, place, 600);
    const float here[3] = {100.0f, 200.0f, 10.0f};
    const wind::Sample a = model.At(here);
    const wind::Sample b = model.At(here);
    Check(a.dirX == b.dirX && a.dirY == b.dirY && a.strength == b.strength && a.gust == b.gust,
          "the wind at a point does not change between two reads in one frame");
    Check(std::fabs(std::sqrt(a.dirX * a.dirX + a.dirY * a.dirY) - 1.0f) < kTolerance,
          "the wind direction is a unit vector");

    bool gustsVary = false;
    bool boundedGusts = true;
    bool strengthInRange = true;
    for (int i = 0; i < 400; ++i)
    {
        const float p[3] = {i * 17.0f, i * -9.0f, 0.0f};
        const wind::Sample s = model.At(p);
        gustsVary = gustsVary || std::fabs(s.gust - a.gust) > 0.05f;
        boundedGusts = boundedGusts && s.gust >= 0.0f && s.gust <= 1.0f;
        strengthInRange = strengthInRange && s.strength >= model.Steady() - kTolerance &&
                          s.strength <= model.Steady() * 1.8f + kTolerance;
    }
    Check(gustsVary && boundedGusts, "gusts pass over some places and not others and stay between 0 and 1");
    Check(strengthInRange, "the strength at a point is the steady wind plus at most the gust strength of it");

    const float later[3] = {100.0f, 200.0f, 10.0f};
    Settle(model, place, 120);
    Check(std::fabs(model.At(later).gust - a.gust) > 1.0e-4f || std::fabs(model.At(later).dirX - a.dirX) > 1.0e-4f,
          "gusts travel: the same place sees a different wind two seconds later");

    model.SetStrength(0.0f);
    Check(model.At(here).strength == 0.0f, "a wind strength of 0 is calm");
    model.SetStrength(-3.0f);
    Check(model.At(here).strength == 0.0f, "a negative wind strength is held at calm");
    model.SetStrength(2.0f);
    const wind::Sample doubled = model.At(here);
    model.SetStrength(1.0f);
    const wind::Sample plain = model.At(here);
    Check(std::fabs(doubled.strength - 2.0f * plain.strength) < kTolerance && doubled.dirX == plain.dirX,
          "the wind strength scales the strength and not the direction");
}

void CheckBadFrameTimes()
{
    wind::WindModel model;
    model.SetProfiles({GlobalRow(0.3f, 0.6f)});
    wind::Place place;
    const float at[3] = {1.0f, 2.0f, 3.0f};
    model.Update(std::numeric_limits<float>::quiet_NaN(), place);
    model.Update(-1.0f, place);
    model.Update(30.0f, place);
    const wind::Sample s = model.At(at);
    Check(std::isfinite(model.Steady()) && std::isfinite(model.Bearing()) && std::isfinite(s.strength) &&
              std::isfinite(s.dirX) && std::isfinite(s.dirY),
          "a frame time that is negative, huge or not a number leaves the wind finite");
}

uint32_t ParentOf(uint32_t area)
{
    switch (area)
    {
    case 5:
        return 3;
    case 3:
        return 1;
    case 1:
        return 0;
    case 20:
        return 21;
    case 21:
        return 20;
    case 30:
        return 30;
    default:
        return 0;
    }
}

void CheckAreaChain()
{
    uint32_t chain[wind::kMaxAreaChain] = {};
    int count = wind::AreaChain(5, &ParentOf, chain, wind::kMaxAreaChain);
    Check(count == 3 && chain[0] == 5 && chain[1] == 3 && chain[2] == 1,
          "an area chain lists the area, then its parents, most specific first, and ends at the root");
    count = wind::AreaChain(0, &ParentOf, chain, wind::kMaxAreaChain);
    Check(count == 0, "no area is an empty chain");
    count = wind::AreaChain(5, &ParentOf, chain, 2);
    Check(count == 2 && chain[1] == 3, "an area chain stops at the room it is given");
    count = wind::AreaChain(20, &ParentOf, chain, wind::kMaxAreaChain);
    Check(count == wind::kMaxAreaChain, "two areas that are each other's parent end the chain at its capacity");
    count = wind::AreaChain(30, &ParentOf, chain, wind::kMaxAreaChain);
    Check(count == 1 && chain[0] == 30, "an area that is its own parent ends the chain");
}

void CheckWeatherIntensity()
{
    Check(wind::WeatherIntensity(0.0f) == 0.0f && wind::WeatherIntensity(wind::kWeatherKnee) == 0.0f &&
              std::fabs(wind::WeatherIntensity(0.625f) - 0.5f) < kTolerance && wind::WeatherIntensity(1.0f) == 1.0f &&
              wind::WeatherIntensity(3.0f) == 1.0f && wind::WeatherIntensity(-1.0f) == 0.0f,
          "the weather the wind reads has the client's dead zone below a quarter and tops out at 1");
}

void CheckBuiltInProfile()
{
    wind::WindModel model;
    wind::Place place;
    model.Update(kFrameSeconds, place);
    Check(model.BuiltIn() && model.RowCount() == 1 && model.Steady() >= 0.3f - kTolerance &&
              model.Steady() <= 0.65f + kTolerance,
          "a model with no table has one built-in global profile, so the grass still sways");
    model.SetProfiles({GlobalRow(0.9f, 0.9f)});
    model.UseBuiltInProfiles();
    model.Update(kFrameSeconds, place);
    Check(model.BuiltIn() && model.RowCount() == 1 && model.Steady() < 0.7f,
          "the built-in profile can be restored after a table was set");
    std::string error;
    wind::WindModel missing;
    Check(!missing.LoadProfiles("no-such-folder\\WindProfile.cdbc", error) && missing.BuiltIn(),
          "a table that cannot be read leaves the built-in profile in place");
}

void CheckWindModel()
{
    CheckTableReader();
    CheckShippedTable();
    CheckResolution();
    CheckBlending();
    CheckSteadyWind();
    CheckSamples();
    CheckBadFrameTimes();
    CheckAreaChain();
    CheckWeatherIntensity();
    CheckBuiltInProfile();
}
}
