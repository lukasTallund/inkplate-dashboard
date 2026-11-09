#pragma once
// Drawing code for the dashboard. Kept out of the YAML so the layout is plain
// C++ that an editor can check. The lambda in dashboard.yaml only gathers the
// fonts and the current sensor values and calls dash::draw().
//
// Data arrives from Home Assistant as compact text: one record per line,
// fields separated by '|'. See ha/inkplate_dashboard.yaml for the producers.
//
// Design: white page, no boxes. A hero row (time, date, current weather),
// then two columns with small uppercase section labels: an agenda on the
// left, the hourly forecast and departures on the right. The guest Wi-Fi
// screen keeps the hero and swaps the columns for the network and QR code.

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

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

// Material Design Icons code points not covered by the weather table.
#define ICON_ALERT "\U000F0026"   // icons_s
#define ICON_UNKNOWN "\U000F0F2F" // weather-cloudy-alert, icons_xl/icons_l

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

// ---------------------------------------------------------------- helpers --

static std::vector<std::string> split(const std::string &s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    size_t pos = s.find(sep, start);
    out.push_back(s.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
    if (pos == std::string::npos)
      break;
    start = pos + 1;
  }
  return out;
}

static std::vector<std::vector<std::string>> records(const std::string &s, size_t minFields) {
  std::vector<std::vector<std::string>> out;
  for (auto &line : split(s, '\n')) {
    if (line.empty())
      continue;
    auto f = split(line, '|');
    if (f.size() >= minFields)
      out.push_back(f);
  }
  return out;
}

static int textWidth(Display &it, BaseFont *font, const std::string &s) {
  int x, y, w, h;
  it.get_text_bounds(0, 0, s.c_str(), font, TextAlign::TOP_LEFT, &x, &y, &w, &h);
  return w;
}

// Ink extent of `s`, relative to where print() puts the pen with TOP_LEFT.
// Display's own centring uses advance widths, which leaves a digit like "1"
// (lots of room on its left) or a lopsided icon visibly off centre.
struct Ink {
  int left, right, top, bottom;
};

static Ink ink(BaseFont *font, const std::string &s) {
  auto *ft = static_cast<esphome::font::Font *>(font);
  Ink r{INT_MAX, INT_MIN, INT_MAX, INT_MIN};
  int x = 0;
  const unsigned char *p = (const unsigned char *) s.c_str();
  while (*p) {
    uint32_t c = *p;
    int n = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
    c &= n == 0 ? 0x7F : n == 1 ? 0x1F : n == 2 ? 0x0F : 0x07;
    p++;
    for (int i = 0; i < n && (*p & 0xC0) == 0x80; i++, p++)
      c = (c << 6) | (*p & 0x3F);
    const auto *g = ft->find_glyph(c);
    if (g == nullptr)
      continue;
    if (g->width > 0 && g->height > 0) {
      r.left = std::min(r.left, x + g->offset_x);
      r.right = std::max(r.right, x + g->offset_x + g->width);
      r.top = std::min(r.top, g->offset_y);
      r.bottom = std::max(r.bottom, g->offset_y + g->height);
    }
    x += g->advance;
  }
  if (r.left > r.right)
    return {0, 0, 0, 0};
  return r;
}

// Print `s` with the ink of `centreOn` (default: all of `s`) centred on cx.
// `valign` is the vertical part of the alignment (TOP, CENTER_VERTICAL, ...).
static void printInkCentred(Display &it, BaseFont *font, int cx, int y, const std::string &s,
                            TextAlign valign = TextAlign::TOP, Color color = INK,
                            const std::string &centreOn = "") {
  Ink k = ink(font, centreOn.empty() ? s : centreOn);
  it.print(cx - (k.left + k.right) / 2, y, font, color, valign, s.c_str());
}

static int lineHeight(Display &it, BaseFont *font) {
  int x, y, w, h;
  it.get_text_bounds(0, 0, "ÅgÖy", font, TextAlign::TOP_LEFT, &x, &y, &w, &h);
  return h;
}

static void fillRound(Display &it, int x, int y, int w, int h, int r, Color c) {
  if (r * 2 > h)
    r = h / 2;
  if (r * 2 > w)
    r = w / 2;
  it.filled_rectangle(x + r, y, w - 2 * r, h, c);
  it.filled_rectangle(x, y + r, r, h - 2 * r, c);
  it.filled_rectangle(x + w - r, y + r, r, h - 2 * r, c);
  it.filled_circle(x + r, y + r, r, c);
  it.filled_circle(x + w - r - 1, y + r, r, c);
  it.filled_circle(x + r, y + h - r - 1, r, c);
  it.filled_circle(x + w - r - 1, y + h - r - 1, r, c);
}

// Greedy word wrap. The last allowed line gets an ellipsis if text remains.
static std::vector<std::string> wrap(Display &it, BaseFont *font, const std::string &text, int maxW,
                                     int maxLines) {
  std::vector<std::string> lines;
  std::string cur;
  for (auto &word : split(text, ' ')) {
    if (word.empty())
      continue;
    std::string candidate = cur.empty() ? word : cur + " " + word;
    if (cur.empty() || textWidth(it, font, candidate) <= maxW) {
      cur = candidate;
      continue;
    }
    lines.push_back(cur);
    cur = word;
    if ((int) lines.size() == maxLines) {
      std::string &last = lines.back();
      while (!last.empty() && textWidth(it, font, last + "…") > maxW) {
        size_t sp = last.rfind(' ');
        last = sp == std::string::npos ? std::string() : last.substr(0, sp);
      }
      last += "…";
      return lines;
    }
  }
  if (!cur.empty())
    lines.push_back(cur);
  return lines;
}

static std::string ellipsize(Display &it, BaseFont *font, const std::string &s, int maxW) {
  auto l = wrap(it, font, s, maxW, 1);
  return l.empty() ? std::string() : l[0];
}

// Local time. ESPHome no longer sets the libc TZ (it converts with its own
// timezone code), so localtime_r()/mktime() would silently work in UTC. Go
// through ESPTime instead and only use libc to format the resulting fields.
static struct tm local(time_t t) {
  esphome::ESPTime e = esphome::ESPTime::from_epoch_local(t);
  struct tm out = {};
  out.tm_sec = e.second;
  out.tm_min = e.minute;
  out.tm_hour = e.hour;
  out.tm_mday = e.day_of_month;
  out.tm_mon = e.month - 1;
  out.tm_year = e.year - 1900;
  out.tm_wday = e.day_of_week - 1;
  out.tm_yday = e.day_of_year - 1;
  out.tm_isdst = e.is_dst;
  return out;
}

// Days since 1970-01-01 for a proleptic Gregorian date (month 1-12; day may
// overflow the month, which just rolls forward).
static long long daysFromCivil(int y, int m, int d) {
  y -= m <= 2;
  long long era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned) (y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + (long long) doe - 719468;
}

// Seconds the local clock is ahead of UTC at instant t.
static long utcOffset(time_t t) {
  struct tm l = local(t);
  long long asUtc = daysFromCivil(l.tm_year + 1900, l.tm_mon + 1, l.tm_mday) * 86400LL + l.tm_hour * 3600 +
                    l.tm_min * 60 + l.tm_sec;
  return (long) (asUtc - (long long) t);
}

// Local midnight, `addDays` days from the day containing `now`.
static time_t dayStart(time_t now, int addDays) {
  struct tm l = local(now);
  long long wall = daysFromCivil(l.tm_year + 1900, l.tm_mon + 1, l.tm_mday + addDays) * 86400LL;
  // Midnight's own offset can differ from now's across a DST change.
  time_t guess = (time_t) (wall - utcOffset(now));
  return (time_t) (wall - utcOffset(guess));
}

static std::string fmt(time_t t, const char *f) {
  char buf[48];
  struct tm tm = local(t);
  strftime(buf, sizeof(buf), f, &tm);
  std::string s = buf;
  // %e pads single-digit days with a space.
  size_t p;
  while ((p = s.find("  ")) != std::string::npos)
    s.erase(p, 1);
  if (!s.empty() && s[0] == ' ')
    s.erase(0, 1);
  return s;
}

static std::string upper(std::string s) {
  for (auto &c : s)
    if (c >= 'a' && c <= 'z')
      c -= 32;
  return s;
}

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

// Small uppercase label with a hairline running to the end of the column,
// or up to `aside`, which is printed right-aligned in the same style.
static void sectionLabel(Display &it, const Fonts &f, int x, int y, int w, const std::string &text,
                         const std::string &aside = "", const char *asideIcon = nullptr) {
  std::string t = upper(text);
  it.print(x, y, f.label, INK, TextAlign::TOP_LEFT, t.c_str());
  int start = x + textWidth(it, f.label, t) + 14;
  int end = x + w;
  if (!aside.empty()) {
    it.print(end, y, f.label, INK, TextAlign::TOP_RIGHT, aside.c_str());
    end -= textWidth(it, f.label, aside) + 14;
  }
  if (asideIcon != nullptr) {
    // Icon centred on the label's capitals, just left of the aside text.
    Ink ik = ink(f.iconS, asideIcon), lk = ink(f.label, "X");
    int cy = y + (lk.top + lk.bottom) / 2;
    it.print(end + 6 - ik.right, cy - (ik.top + ik.bottom) / 2, f.iconS, INK, TextAlign::TOP_LEFT, asideIcon);
    end -= ik.right - ik.left + 8;
  }
  it.filled_rectangle(start, y + lineHeight(it, f.label) / 2, end - start, 2, INK);
}

// ------------------------------------------------------------------- hero --

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

// ----------------------------------------------------------------- agenda --

struct Event {
  time_t start, end;
  bool allDay;
  std::string summary, location;
};

static void drawAgenda(Display &it, const Fonts &f, const Data &d) {
  sectionLabel(it, f, LEFT_X, COL_Y, LEFT_W, "Calendar");
  if (!d.now)
    return;

  std::vector<Event> events;
  for (auto &r : records(d.events, 4))
    events.push_back({(time_t) atoll(r[0].c_str()), (time_t) atoll(r[1].c_str()), r[2] == "1", r[3],
                      r.size() > 4 ? r[4] : ""});

  const int timeW = 104;                 // time column
  const int textX = LEFT_X + timeW;
  const int textW = LEFT_W - timeW;
  const int hHead = lineHeight(it, f.heading);
  const int hBody = lineHeight(it, f.body);
  const int hSmall = lineHeight(it, f.small);

  int y = COL_Y + LABEL_GAP;
  int hidden = 0;
  bool full = false;

  int drawnDays = 0;
  for (int day = 0; day < CAL_DAYS; day++) {
    time_t ds = dayStart(d.now, day), de = dayStart(d.now, day + 1);
    std::vector<const Event *> todays;
    for (auto &e : events)
      if (e.start < de && e.end > ds)
        todays.push_back(&e);
    if (full) {
      hidden += todays.size();
      continue;
    }
    if (y + hHead + hBody > BOTTOM) {
      full = true;
      hidden += todays.size();
      continue;
    }

    // A very light dotted rule between days.
    if (drawnDays++ > 0) {
      y += 6;
      for (int x = LEFT_X; x < LEFT_X + LEFT_W; x += 4)
        it.draw_pixel_at(x, y, INK);
      y += 14;
    }

    std::string title = day == 0 ? "Today" : day == 1 ? "Tomorrow" : fmt(ds, "%A");
    std::string date = fmt(ds, "%e %b");
    it.print(LEFT_X, y, f.heading, INK, TextAlign::TOP_LEFT, title.c_str());
    int dateX = LEFT_X + textWidth(it, f.heading, title) + 12;
    it.print(dateX, y + 3, f.small, INK, TextAlign::TOP_LEFT, date.c_str());

    // An empty day stays on its heading line instead of taking a row.
    if (todays.empty()) {
      it.print(dateX + textWidth(it, f.small, date) + 16, y, f.body, INK, TextAlign::TOP_LEFT, "–  Nothing planned");
      y += hHead + 6;
      continue;
    }
    y += hHead + 8;

    for (size_t i = 0; i < todays.size(); i++) {
      const Event *e = todays[i];
      auto sum = wrap(it, f.body, e->summary, textW, 2);
      std::string loc = e->location.empty() ? "" : ellipsize(it, f.small, e->location, textW);
      int h = (int) sum.size() * (hBody + 2) + (loc.empty() ? 0 : hSmall + 4);
      h = std::max(h, e->allDay ? hSmall : hHead + hSmall + 4);
      if (y + h > BOTTOM) {
        full = true;
        hidden += todays.size() - i;
        break;
      }

      // Time column: start bold, end small below. All-day gets a label.
      if (e->allDay) {
        it.print(LEFT_X, y + 4, f.small, INK, TextAlign::TOP_LEFT, "All day");
      } else {
        // Multi-day events that started earlier show 00:00 today.
        time_t s = std::max(e->start, ds);
        it.print(LEFT_X, y, f.heading, INK, TextAlign::TOP_LEFT, fmt(s, "%H:%M").c_str());
        if (e->end < de)
          it.print(LEFT_X, y + hHead + 4, f.small, INK, TextAlign::TOP_LEFT, fmt(e->end, "%H:%M").c_str());
      }

      int ty = y;
      for (auto &l : sum) {
        it.print(textX, ty, f.body, INK, TextAlign::TOP_LEFT, l.c_str());
        ty += hBody + 2;
      }
      if (!loc.empty())
        it.print(textX, ty + 2, f.small, INK, TextAlign::TOP_LEFT, loc.c_str());
      y += h + 12;
    }
  }

  if (hidden > 0 && y + hSmall <= BOTTOM + hSmall) {
    char buf[24];
    snprintf(buf, sizeof(buf), "+%d more", hidden);
    it.print(textX, std::min(y, BOTTOM - hSmall), f.small, INK, TextAlign::TOP_LEFT, buf);
  }
}

// ------------------------------------------------------------- departures --

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

static void drawDepartures(Display &it, const Fonts &f, const Data &d) {
  auto rows = departures(d.departures, d.now);
  // HA flags a departure that has an SL deviation, is cancelled or runs
  // late. One flag is enough for a quiet hint to check the SL app.
  bool notice = std::any_of(rows.begin(), rows.end(), [](const Departure &r) { return r.notice; });
  sectionLabel(it, f, RIGHT_X, BUS_Y, RIGHT_W, d.stopName, notice ? "TRAFFIC NOTICE" : "",
               notice ? ICON_ALERT : nullptr);

  int right = RIGHT_X + RIGHT_W;
  int y = BUS_Y + LABEL_GAP + 8;
  if (rows.empty()) {
    it.print(RIGHT_X, y, f.body, INK, TextAlign::TOP_LEFT,
             d.departures.empty() ? "Waiting for departures…" : "No departures");
    return;
  }

  // One badge width for all rows, sized for a four-character line like
  // "815C" so it doesn't jump (and ghost) as lines come and go. A longer
  // line still widens it.
  const int badgeH = 44, stroke = 3, badgePad = 12;
  Ink ref = ink(f.heading, "888C");
  int badgeW = ref.right - ref.left + 2 * badgePad;
  for (auto &r : rows) {
    Ink k = ink(f.heading, r.line);
    badgeW = std::max(badgeW, k.right - k.left + 2 * badgePad);
  }
  auto highlight = split(d.highlightLines, ',');

  // ETA column, left-aligned and wide enough for its widest form.
  int etaW = std::max({textWidth(it, f.eta, "00:00"), textWidth(it, f.body, "Cancelled"),
                       textWidth(it, f.eta, "00") + 6 + textWidth(it, f.small, "min")});
  const int etaX = right - etaW;
  const int destX = RIGHT_X + badgeW + 18;

  for (auto &r : rows) {
    int cy = y + 22; // row centre line

    // Line number on a rounded badge: white on black for the lines that
    // matter most, black in an outline for the rest.
    bool filled = std::find(highlight.begin(), highlight.end(), r.line) != highlight.end();
    fillRound(it, RIGHT_X, cy - badgeH / 2, badgeW, badgeH, 10, INK);
    if (!filled)
      fillRound(it, RIGHT_X + stroke, cy - badgeH / 2 + stroke, badgeW - 2 * stroke, badgeH - 2 * stroke,
                10 - stroke, PAPER);
    printInkCentred(it, f.heading, RIGHT_X + badgeW / 2, cy + 1, r.line, TextAlign::CENTER_VERTICAL,
                    filled ? PAPER : INK);

    // ETA: numbers are big (minutes with a small unit, a clock time an hour
    // or more out, or "Now" at the same height); "Cancelled" is regular
    // weight, since the struck-through destination already says most of it.
    if (r.cancelled) {
      it.print(etaX, cy, f.body, INK, TextAlign::CENTER_LEFT, "Cancelled");
    } else {
      long mins = d.now ? (long) (r.when - d.now) / 60 : 999;
      if (mins <= 0) {
        it.print(etaX, cy, f.eta, INK, TextAlign::CENTER_LEFT, "Now");
      } else if (mins < 60) {
        std::string n = std::to_string(mins);
        it.print(etaX, cy, f.eta, INK, TextAlign::CENTER_LEFT, n.c_str());
        it.print(etaX + textWidth(it, f.eta, n) + 6, cy + 6, f.small, INK, TextAlign::CENTER_LEFT, "min");
      } else {
        it.print(etaX, cy, f.eta, INK, TextAlign::CENTER_LEFT, fmt(r.when, "%H:%M").c_str());
      }
    }

    std::string dest = ellipsize(it, f.heading, r.destination, etaX - 16 - destX);
    it.print(destX, cy, f.heading, INK, TextAlign::CENTER_LEFT, dest.c_str());
    if (r.cancelled)
      it.filled_rectangle(destX, cy, textWidth(it, f.heading, dest), 2, INK);

    y += BUS_ROW_H;
  }
}

// --------------------------------------------------------------- forecast --

static void drawForecast(Display &it, const Fonts &f, const Data &d) {
  auto hs = hours(d.forecast);
  bool tomorrow = false;
  time_t start = d.now ? forecastStart(d.now, tomorrow) : 0;

  // Low and high over exactly the hours the table spans (every hour, not
  // only the shown slots), on the label line.
  std::string range;
  float lo = INFINITY, hi = -INFINITY;
  time_t end = start + (time_t) (FORECAST_SLOTS - 1) * FORECAST_STEP_H * 3600;
  for (auto &h : hs)
    if (h.t >= start - 1800 && h.t <= end + 1800 && !std::isnan(h.temp)) {
      lo = std::min(lo, h.temp);
      hi = std::max(hi, h.temp);
    }
  // Include the current reading so "later today" never excludes now.
  if (lo <= hi && !tomorrow && !std::isnan(d.temperature)) {
    lo = std::min(lo, d.temperature);
    hi = std::max(hi, d.temperature);
  }
  if (lo <= hi)
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

// ------------------------------------------------------------- guest wifi --

// Field from a "WIFI:T:WPA;S:name;P:pass;;" payload, with \-escapes undone.
static std::string wifiField(const std::string &payload, char key) {
  std::string out;
  bool inField = false, esc = false;
  for (size_t i = 0; i < payload.size(); i++) {
    char c = payload[i];
    if (!inField) {
      bool atStart = i == 0 || payload[i - 1] == ';' || (i >= 5 && payload.compare(i - 5, 5, "WIFI:") == 0);
      if (atStart && c == key && i + 1 < payload.size() && payload[i + 1] == ':') {
        inField = true;
        i++;
      }
      continue;
    }
    if (esc) {
      out += c;
      esc = false;
    } else if (c == '\\') {
      esc = true;
    } else if (c == ';') {
      break;
    } else {
      out += c;
    }
  }
  return out;
}

// Same frame as the dashboard: the hero stays, the left column names the
// network, the right column holds the code.
static void drawGuest(Display &it, const Fonts &f, const Data &d) {
  sectionLabel(it, f, LEFT_X, COL_Y, LEFT_W, "Guest Wi-Fi");
  int y = COL_Y + LABEL_GAP + 4;
  auto item = [&](const char *label, const std::string &value) {
    it.print(LEFT_X, y, f.small, INK, TextAlign::TOP_LEFT, label);
    it.print(LEFT_X, y + 28, f.date, INK, TextAlign::TOP_LEFT, ellipsize(it, f.date, value, LEFT_W).c_str());
    y += 100;
  };
  item("Network", wifiField(d.qrPayload, 'S'));
  std::string pass = wifiField(d.qrPayload, 'P');
  if (!pass.empty())
    item("Password", pass);
  y += 12;
  int hBody = lineHeight(it, f.body);
  for (auto &l : wrap(it, f.body, "Point your phone camera at the code to join without typing.", LEFT_W, 3)) {
    it.print(LEFT_X, y, f.body, INK, TextAlign::TOP_LEFT, l.c_str());
    y += hBody + 4;
  }

  sectionLabel(it, f, RIGHT_X, COL_Y, RIGHT_W, "Scan to join");
  if (d.qr == nullptr)
    return;
  int size = d.qr->get_size();
  if (size == 0)
    return;
  // A couple of modules of white between the hairline and the code.
  int top = COL_Y + lineHeight(it, f.label) + 22;
  int scale = std::min(RIGHT_W / size, (BOTTOM - top) / size);
  int qrPx = size * scale;
  it.qr_code(RIGHT_X + (RIGHT_W - qrPx) / 2, top, d.qr, INK, scale);
}

// ------------------------------------------------------------------- main --

static void draw(Display &it, const Fonts &f, const Data &d) {
  it.fill(PAPER);
  drawHero(it, f, d);
  if (d.guest) {
    drawGuest(it, f, d);
    return;
  }
  drawAgenda(it, f, d);
  drawDepartures(it, f, d);
  drawForecast(it, f, d);
}

} // namespace dash
