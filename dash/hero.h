#pragma once
// Top row: clock, date and current weather. Shared by both screens.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "dates.h"
#include "layout.h"
#include "text.h"
#include "weather.h"

namespace dash {

static void drawHero(Display &it, const Fonts &f, const Data &d) {
  // Time
  std::string t = d.now ? fmt(d.now, "%H:%M") : "--:--";
  it.print(MX - 3, HERO_Y - 8, f.clock, INK, TextAlign::TOP_LEFT, t.c_str());
  int tw = textWidth(it, f.clock, t);

  // Date, stacked next to the time. `free` is where the weather line may start.
  int x = MX + tw + 26, free = x;
  if (d.now) {
    std::string wd = fmt(d.now, "%A"), dm = fmt(d.now, "%e %B");
    it.print(x, HERO_Y + 30, f.date, INK, TextAlign::TOP_LEFT, wd.c_str());
    it.print(x, HERO_Y + 72, f.dateLight, INK, TextAlign::TOP_LEFT, dm.c_str());
    free = x + std::max(textWidth(it, f.date, wd), textWidth(it, f.dateLight, dm)) + 32;
  }

  // Current weather, laid out like HA's weather card: icon, condition and
  // wind on the left of the right column (lined up with the forecast below),
  // the temperature right-aligned with feels-like under it.
  if (std::isnan(d.temperature))
    return;
  int right = W - MX;
  std::string icon = weatherIcon(d.condition, d.now && isNight(d.now));
  Ink ik = ink(f.iconXL, icon);
  char temp[16];
  snprintf(temp, sizeof(temp), "%.1f°", d.temperature);
  BaseFont *tempFont = d.temperature < 0 && temp[0] == '-' ? f.tempNeg : f.temp;
  int tempW = textWidth(it, tempFont, temp);

  std::string cond = weatherText(d.condition), feels, wind;
  float fl = feelsLike(d.temperature, d.windSpeed);
  if (!std::isnan(fl) && std::lround(fl) != std::lround(d.temperature))
    feels = wholeDegrees(fl);
  // Wind as "4 (8) m/s", gusts in brackets; just "4 m/s" without notable gusts.
  if (!std::isnan(d.windSpeed)) {
    char buf[32];
    if (!std::isnan(d.windGust) && d.windGust >= d.windSpeed + 2)
      snprintf(buf, sizeof(buf), "%.0f (%.0f) m/s", d.windSpeed, d.windGust);
    else
      snprintf(buf, sizeof(buf), "%.0f m/s", d.windSpeed);
    wind = buf;
  }
  // If the date is wide enough to reach the right column, start after it.
  int iconX = std::max(RIGHT_X, free) - ik.left;
  int textX = iconX + ik.right + 12;
  const int baseY = HERO_Y + 72; // temperature baseline
  const int lowY = HERO_Y + 86;  // top of the second line
  it.print(right, baseY, tempFont, INK, TextAlign::BASELINE_RIGHT, temp);

  // Second line: wind left, feels-like right.
  std::string feelsText = feels.empty() ? "" : "Feels like " + feels;
  it.print(textX, lowY, f.body, INK, TextAlign::TOP_LEFT, wind.c_str());
  if (!feelsText.empty())
    it.print(right, lowY, f.body, INK, TextAlign::TOP_RIGHT, feelsText.c_str());

  // Condition, its last line on the temperature's baseline. If one line
  // would touch the temperature, try two lines, then the smaller label font;
  // only then leave it out (the icon shows it anyway).
  int condW = right - tempW - 16 - textX;
  auto fits = [&](BaseFont *font, const std::vector<std::string> &lines) {
    for (auto &l : lines)
      if (l.find("…") != std::string::npos || textWidth(it, font, l) > condW)
        return false;
    return !lines.empty();
  };
  if (!cond.empty()) {
    BaseFont *font = f.heading;
    std::vector<std::string> lines = wrap(it, f.heading, cond, condW, 1);
    if (!fits(font, lines))
      lines = wrap(it, f.heading, cond, condW, 2);
    if (!fits(font, lines)) {
      font = f.label;
      lines = wrap(it, f.label, cond, condW, 1);
    }
    if (fits(font, lines)) {
      int lh = lineHeight(it, font);
      for (size_t i = 0; i < lines.size(); i++)
        it.print(textX, baseY - (int) (lines.size() - 1 - i) * lh, font, INK, TextAlign::BASELINE_LEFT,
                 lines[i].c_str());
    }
  }

  it.print(iconX, HERO_Y + 60 - (ik.top + ik.bottom) / 2, f.iconXL, INK, TextAlign::TOP_LEFT, icon.c_str());
}

} // namespace dash
