#pragma once
// The sensor text from Home Assistant, parsed. Each record format here must
// match its producer in ha/inkplate_dashboard.yaml (and the fakes in
// preview/scenarios.h): one record per line, fields separated by '|'.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#include "dates.h"
#include "layout.h"
#include "util.h"

namespace dash {

// Hourly forecast record: epoch | condition | temperature | precipitation | wind
struct Hour {
  time_t t;
  std::string condition;
  float temp, precip, wind;
};

// Optional numeric field; NAN when missing (older HA package) or empty.
static float field(const std::vector<std::string> &r, size_t i) {
  if (i >= r.size() || r[i].empty() || r[i] == "None")
    return NAN;
  return (float) atof(r[i].c_str());
}

static std::vector<Hour> hours(const std::string &s) {
  std::vector<Hour> out;
  for (auto &r : records(s, 3)) {
    float precip = field(r, 3);
    out.push_back({(time_t) atoll(r[0].c_str()), r[1], field(r, 2), std::isnan(precip) ? 0.f : precip, field(r, 4)});
  }
  return out;
}

// The stretch of the day worth dressing for: the rest of today until 20:00,
// otherwise tomorrow from 07:00. Returns the first slot; sets `tomorrow`.
static time_t forecastStart(time_t now, bool &tomorrow) {
  int h = local(now).tm_hour + 1;
  tomorrow = false;
  if (h >= 20) {
    tomorrow = true;
    return dayStart(now, 1) + 7 * 3600;
  }
  if (h < 6)
    return dayStart(now, 0) + 7 * 3600;
  return dayStart(now, 0) + h * 3600;
}

// Low and high over exactly the hours a forecast table starting at `start`
// spans (every hour, not only the shown slots). Today's range also includes
// the current reading, so "later today" never excludes now. False without
// data for those hours.
static bool forecastRange(const std::vector<Hour> &hs, time_t start, bool tomorrow, float current, float &lo,
                          float &hi) {
  lo = INFINITY;
  hi = -INFINITY;
  time_t end = start + (time_t) (FORECAST_SLOTS - 1) * FORECAST_STEP_H * 3600;
  for (auto &h : hs)
    if (h.t >= start - 1800 && h.t <= end + 1800 && !std::isnan(h.temp)) {
      lo = std::min(lo, h.temp);
      hi = std::max(hi, h.temp);
    }
  if (lo <= hi && !tomorrow && !std::isnan(current)) {
    lo = std::min(lo, current);
    hi = std::max(hi, current);
  }
  return lo <= hi;
}

// Calendar record: start epoch | end epoch | all-day (1/0) | summary | location
struct Event {
  time_t start, end;
  bool allDay;
  std::string summary, location;
};

static std::vector<Event> events(const std::string &s) {
  std::vector<Event> out;
  for (auto &r : records(s, 4))
    out.push_back({(time_t) atoll(r[0].c_str()), (time_t) atoll(r[1].c_str()), r[2] == "1", r[3],
                   r.size() > 4 ? r[4] : ""});
  return out;
}

// Departure record: line | destination | expected epoch | state | notice
struct Departure {
  std::string line, destination;
  time_t when;
  bool cancelled, notice;
};

// The departures worth showing: not yet left, at most BUS_ROWS.
static std::vector<Departure> departures(const std::string &s, time_t now) {
  std::vector<Departure> out;
  for (auto &r : records(s, 3)) {
    time_t when = (time_t) atoll(r[2].c_str());
    if (now && when < now - 30) // still listed until it has left
      continue;
    if ((int) out.size() == BUS_ROWS)
      break;
    out.push_back({r[0], r[1], when, r.size() > 3 && r[3] == "CANCELLED", r.size() > 4 && r[4] == "1"});
  }
  return out;
}

// Everything the views draw from, parsed once per redraw.
struct Model {
  std::vector<Hour> hours;
  std::vector<Event> events;
  std::vector<Departure> departures;
};

static Model parse(const Data &d) { return {hours(d.forecast), events(d.events), departures(d.departures, d.now)}; }

} // namespace dash
