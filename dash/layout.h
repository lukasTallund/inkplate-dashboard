#pragma once
// Colours, page geometry, the fonts the YAML hands over and the raw sensor
// data. Everything else in dash/ builds on this.
#include <cmath>
#include <ctime>
#include <string>

namespace dash {

using esphome::Color;
using esphome::display::BaseFont;
using esphome::display::Display;
using esphome::display::TextAlign;

// On the Inkplate driver "on" is white, so name the real colours explicitly.
static const Color INK = Color(0, 0, 0);
static const Color PAPER = Color(255, 255, 255);

// ---------------------------------------------------------------- layout --

static const int W = 1200, H = 825;
static const int MX = 44;          // side margin
static const int TOP = 24;         // top margin
static const int BOTTOM = H - 30;  // last usable y

static const int HERO_Y = TOP, HERO_H = 128;

static const int COL_Y = HERO_Y + HERO_H + 24;
static const int LABEL_GAP = 40;  // section label to its first line
static const int LEFT_X = MX, LEFT_W = 580;
static const int RIGHT_X = 676, RIGHT_W = W - MX - RIGHT_X;

// Right column: hourly forecast on top (next to the current weather), then
// departures.
static const int FORECAST_Y = COL_Y;
static const int FORECAST_SLOTS = 6;   // columns
static const int FORECAST_STEP_H = 2;  // hours between columns
static const int FORECAST_H = 174;     // hour, icon, temp, wind, rain
static const int BUS_Y = FORECAST_Y + LABEL_GAP + FORECAST_H + 22;
static const int BUS_ROWS = 4;
static const int BUS_ROW_H = 62;
static const int CAL_DAYS = 5;

// Material Design Icons code point for the icons_s font.
#define ICON_ALERT "\U000F0026"

// ------------------------------------------------------------- data types --

// Assigned member by member in the display lambda of dashboard.yaml.
struct Fonts {
  BaseFont *clock = nullptr;     // hero time, digits only
  BaseFont *temp = nullptr;      // hero temperature
  BaseFont *tempNeg = nullptr;   // hero temperature below zero (the minus makes it wide)
  BaseFont *date = nullptr;      // hero date, bold
  BaseFont *dateLight = nullptr; // hero date, regular
  BaseFont *label = nullptr;     // uppercase section labels
  BaseFont *heading = nullptr;   // bold body
  BaseFont *body = nullptr;      // regular body
  BaseFont *small = nullptr;     // secondary text
  BaseFont *eta = nullptr;       // departure minutes
  BaseFont *iconXL = nullptr;    // hero weather icon
  BaseFont *iconL = nullptr;     // forecast icons
  BaseFont *iconS = nullptr;     // label-sized icons (traffic notice)
};

struct Data {
  time_t now = 0; // 0 until the clock has synced
  std::string condition;
  float temperature = NAN, windSpeed = NAN, windGust = NAN;
  std::string forecast, departures, events;
  std::string stopName;
  std::string highlightLines; // comma-separated lines drawn as filled badges
  // Guest Wi-Fi screen
  bool guest = false;
  esphome::qr_code::QrCode *qr = nullptr;
  std::string qrPayload; // the WIFI:... string, parsed for SSID/password
};

} // namespace dash
