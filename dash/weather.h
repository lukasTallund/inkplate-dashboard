#pragma once
// Weather conditions, temperatures and wind chill.
#include <cmath>
#include <cstdio>
#include <string>

#include "dates.h"

namespace dash {

// Whole degrees, never "-0°".
static std::string wholeDegrees(float v) {
  char buf[16];
  long n = std::lround(v);
  snprintf(buf, sizeof(buf), "%ld°", n);
  return buf;
}

// One decimal, or none when it would be ".0": "3.2°", "3°".
static std::string degrees(float v) {
  if (std::fabs(v - std::round(v)) < 0.05f)
    return wholeDegrees(v);
  char buf[16];
  snprintf(buf, sizeof(buf), "%.1f°", v);
  return buf;
}

// Wind chill (the JAG/TI formula SMHI also uses) from °C and m/s. Only
// defined in the cold with some wind; otherwise the air temperature.
static float feelsLike(float t, float wind) {
  if (std::isnan(t) || std::isnan(wind))
    return t;
  float kmh = wind * 3.6f;
  if (t > 10.f || kmh < 4.8f)
    return t;
  float p = std::pow(kmh, 0.16f);
  return 13.12f + 0.6215f * t - 11.37f * p + 0.3965f * t * p;
}

// For a condition missing from the table: weather-cloudy-alert.
#define ICON_UNKNOWN "\U000F0F2F"

// HA weather conditions: MDI icon by day and by night, and a short text.
// Every icon here must be listed in the icons_xl and icons_l fonts.
struct Weather {
  const char *condition, *day, *night, *text;
};
static const Weather WEATHER[] = {
    {"sunny", "\U000F0599", "\U000F0594", "Sunny"},
    {"clear-night", "\U000F0594", "\U000F0594", "Clear"},
    {"partlycloudy", "\U000F0595", "\U000F0F31", "Partly cloudy"},
    {"cloudy", "\U000F0590", "\U000F0590", "Cloudy"},
    {"fog", "\U000F0591", "\U000F0591", "Fog"},
    {"hail", "\U000F0592", "\U000F0592", "Hail"},
    {"lightning", "\U000F0593", "\U000F0593", "Thunder"},
    {"lightning-rainy", "\U000F067E", "\U000F067E", "Thunderstorm"},
    {"pouring", "\U000F0596", "\U000F0596", "Heavy rain"},
    {"rainy", "\U000F0597", "\U000F0597", "Rain"},
    {"snowy", "\U000F0598", "\U000F0598", "Snow"},
    {"snowy-rainy", "\U000F067F", "\U000F067F", "Sleet"},
    {"windy", "\U000F059D", "\U000F059D", "Windy"},
    {"windy-variant", "\U000F059E", "\U000F059E", "Windy"},
};

static const Weather *weather(const std::string &condition) {
  for (auto &w : WEATHER)
    if (condition == w.condition)
      return &w;
  return nullptr;
}

static const char *weatherIcon(const std::string &condition, bool night) {
  const Weather *w = weather(condition);
  return w == nullptr ? ICON_UNKNOWN : night ? w->night : w->day;
}

static std::string weatherText(const std::string &condition) {
  const Weather *w = weather(condition);
  return w == nullptr ? "" : w->text;
}

// Night icons between 20:00 and 06:00.
static bool isNight(time_t t) {
  int h = local(t).tm_hour;
  return h < 6 || h >= 20;
}

} // namespace dash
