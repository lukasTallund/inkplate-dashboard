#pragma once
// Canned data for the preview: each scenario is a fixed moment plus the
// sensor text Home Assistant would send. Add one when a layout edge case
// turns up; main.cpp renders every scenario to out/<name>.png.
//
// Included by main.cpp after harness.h.

#include <ctime>
#include <functional>
#include <string>
#include <vector>


// Local wall time → epoch (TZ is set in main).
static time_t at(int y, int mo, int d, int h, int mi) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = -1;
  return mktime(&t);
}

static std::string num(long long v) { return std::to_string(v); }

// Hourly forecast from `from` for 30 h: epoch|condition|temp|precip|wind.
// `f(hour index)` fills in each hour.
struct HourSpec {
  const char *cond;
  float temp, precip, wind;
};
static std::string forecast(time_t from, std::function<HourSpec(int)> f) {
  std::string out;
  time_t start = from - from % 3600 + 3600;
  for (int i = 0; i < 30; i++) {
    HourSpec h = f(i);
    char buf[96];
    snprintf(buf, sizeof(buf), "%lld|%s|%.1f|%.1f|%.1f\n", (long long) (start + i * 3600), h.cond, h.temp, h.precip,
             h.wind);
    out += buf;
  }
  return out;
}

static std::string event(time_t s, time_t e, bool allDay, const std::string &sum, const std::string &loc = "") {
  return num(s) + "|" + num(e) + "|" + (allDay ? "1" : "0") + "|" + sum + "|" + loc + "\n";
}

static std::string bus(const char *line, const char *dest, time_t when, const char *state = "EXPECTED") {
  bool notice = std::string(state) == "CANCELLED";
  return std::string(line) + "|" + dest + "|" + num(when) + "|" + state + "|" + (notice ? "1" : "0") + "\n";
}

struct Scenario {
  const char *name;
  std::function<void(dash::Data &)> fill;
};

static const char *QR = "WIFI:T:WPA;S:Guest Network;P:example-password;;";

// What every scenario starts from; a scenario can override any of it.
static void defaults(dash::Data &d) {
  d.stopName = "Rotvik";
  d.highlightLines = "875,804,805,815C";
  d.qrPayload = QR;
}

static std::vector<Scenario> scenarios() {
  return {
      // The photo from 2026-11-09: the hero line ran into the date.
      {"photo",
       [](dash::Data &d) {
         d.now = at(2026, 11, 9, 12, 53);
         d.condition = "partlycloudy";
         d.temperature = 4.9f;
         d.windSpeed = 4;
         d.windGust = 8;
         d.forecast = forecast(d.now, [](int i) {
           static const float t[] = {4.9, 4.6, 4.3, 3.8, 3.3, 3.2, 3.2, 3.3, 3.3, 3.1, 3.0, 2.8};
           return HourSpec{i < 5 ? "partlycloudy" : "cloudy", i < 12 ? t[i] : 2.5f, 0, (float) (4 + i % 3)};
         });
         time_t t0 = dash::dayStart(d.now, 0);
         d.events = event(t0 + 86400, t0 + 2 * 86400, true, "Lina födelsedag") +
                    event(t0 + 2 * 86400 + 13 * 3600, t0 + 2 * 86400 + 14 * 3600 + 1800, false, "Nils Psykolog",
                          "Mottagning Medicinsk psykologi, Karolinska Universitetssjukhuset") +
                    event(t0 + 2 * 86400 + 14 * 3600, t0 + 2 * 86400 + 15 * 3600, false, "Nils Fysio",
                          "Mottagning Fysioterapi, Karolinska Universitetssjukhuset") +
                    event(t0 + 2 * 86400 + 15 * 3600 + 600, t0 + 2 * 86400 + 15 * 3600 + 1200, false, "Nils Vaccin",
                          "Mottagning Barnhjärta, Karolinska Universitetssjukhuset");
         d.departures = bus("875", "Gullmarsplan", d.now + 12 * 60) + bus("824", "Rudsjöterrassen", d.now + 27 * 60) +
                        bus("875", "Gullmarsplan", d.now + 27 * 60) + bus("875", "Gullmarsplan", d.now + 42 * 60);
       }},
      // Busy week, wide date, rain and a long condition name.
      {"busy",
       [](dash::Data &d) {
         d.now = at(2026, 9, 30, 7, 41);
         d.condition = "lightning-rainy";
         d.temperature = -12.4f;
         d.windSpeed = 11;
         d.windGust = 19;
         d.forecast = forecast(d.now, [](int i) {
           return HourSpec{i % 4 == 0 ? "pouring" : "rainy", -12.f + i * 0.5f, i % 3 == 0 ? 2.4f : 0.3f,
                           (float) (9 + i % 5)};
         });
         time_t t0 = dash::dayStart(d.now, 0);
         auto h = [&](int day, int hh, int mm) { return t0 + day * 86400 + hh * 3600 + mm * 60; };
         d.events = event(h(0, 8, 0), h(0, 9, 0), false, "Standup") +
                    event(h(0, 10, 30), h(0, 12, 0), false,
                          "Workshop: quarterly planning for the platform team and friends", "Room Ada") +
                    event(h(0, 17, 15), h(0, 18, 0), false, "Hämta på förskolan") +
                    event(t0 + 86400, t0 + 2 * 86400, true, "Lina födelsedag") +
                    event(h(1, 9, 0), h(1, 9, 30), false, "Tandläkare", "Folktandvården Gullmarsplan") +
                    event(h(1, 18, 0), h(1, 21, 0), false, "Middag hos Anna och Erik") +
                    event(h(2, 7, 30), h(2, 8, 0), false, "Löpning") +
                    event(h(3, 12, 0), h(3, 13, 0), false, "Lunch med Sara");
         d.departures = bus("875", "Gullmarsplan", d.now) + bus("824", "Rudsjöterrassen", d.now + 4 * 60, "CANCELLED") +
                        bus("815C", "Slussen", d.now + 9 * 60) + bus("875", "Gullmarsplan", d.now + 75 * 60);
       }},
      // After 20:00 the forecast switches to tomorrow.
      {"evening",
       [](dash::Data &d) {
         d.now = at(2026, 11, 9, 21, 15);
         d.condition = "clear-night";
         d.temperature = 1.0f;
         d.windSpeed = 2;
         d.windGust = 3;
         d.forecast = forecast(d.now, [](int i) { return HourSpec{"sunny", -2.f + i * 0.3f, 0, 3}; });
         d.events = "";
         d.departures = "";
       }},
      {"guest",
       [](dash::Data &d) {
         scenarios()[0].fill(d);
         d.guest = true;
       }},
  };
}
