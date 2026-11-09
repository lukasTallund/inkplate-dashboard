#pragma once
// Right column, top: the hourly forecast.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "dates.h"
#include "layout.h"
#include "text.h"
#include "model.h"
#include "weather.h"

namespace dash {

static void drawForecast(Display &it, const Fonts &f, const Data &d, const std::vector<Hour> &hs) {
  bool tomorrow = false;
  time_t start = d.now ? forecastStart(d.now, tomorrow) : 0;

  // Low and high for the hours the table spans, on the label line.
  std::string range;
  float lo, hi;
  if (d.now && forecastRange(hs, start, tomorrow, d.temperature, lo, hi))
    range = wholeDegrees(lo) + " – " + wholeDegrees(hi);
  sectionLabel(it, f, RIGHT_X, FORECAST_Y, RIGHT_W, tomorrow ? "Tomorrow" : "Later today", range);

  int y = FORECAST_Y + LABEL_GAP;
  if (hs.empty() || !d.now) {
    it.print(RIGHT_X, y, f.body, INK, TextAlign::TOP_LEFT, "Waiting for forecast…");
    return;
  }
  // Wind and rain rows are bare numbers; their units label the rows on the
  // left, like a table. The rain row only appears when some slot has rain.
  const int legendW = 44;
  const int colW = (RIGHT_W - legendW) / FORECAST_SLOTS;
  const int colX = RIGHT_X + legendW;
  bool anyRain = false, anyWind = false;
  for (int i = 0; i < FORECAST_SLOTS; i++) {
    time_t slot = start + (time_t) i * FORECAST_STEP_H * 3600;
    const Hour *h = nullptr;
    for (auto &x : hs)
      if (std::llabs((long long) (x.t - slot)) < 1800) {
        h = &x;
        break;
      }
    int cx = colX + colW * i + colW / 2;
    // Everything in a column is centred on its ink, not its advance width.
    printInkCentred(it, f.small, cx, y, fmt(slot, "%H"));
    if (h == nullptr)
      continue;
    // Icons also share a vertical centre line, whatever their shape.
    std::string icon = weatherIcon(h->condition, isNight(slot));
    Ink k = ink(f.iconL, icon);
    printInkCentred(it, f.iconL, cx, y + 54 - (k.top + k.bottom) / 2, icon);
    if (!std::isnan(h->temp)) {
      // Centre the number and let the ° hang.
      std::string t = degrees(h->temp);
      printInkCentred(it, f.heading, cx, y + 84, t, TextAlign::TOP, INK, t.substr(0, t.size() - strlen("°")));
    }
    char buf[16];
    if (!std::isnan(h->wind)) {
      snprintf(buf, sizeof(buf), "%.0f", h->wind);
      printInkCentred(it, f.small, cx, y + 120, buf);
      anyWind = true;
    }
    if (h->precip >= 0.1f) {
      snprintf(buf, sizeof(buf), "%.1f", h->precip);
      printInkCentred(it, f.small, cx, y + 148, buf);
      anyRain = true;
    }
  }
  if (anyWind)
    it.print(RIGHT_X, y + 120, f.small, INK, TextAlign::TOP_LEFT, "m/s");
  if (anyRain)
    it.print(RIGHT_X, y + 148, f.small, INK, TextAlign::TOP_LEFT, "mm");
}

} // namespace dash
