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
#include <cmath>
#include <cstdio>
#include <cstdlib>
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

// ------------------------------------------------------------- data types --

struct Fonts {
  BaseFont *clock;     // hero time, digits only
  BaseFont *temp;      // hero temperature
  BaseFont *date;      // hero date, bold
  BaseFont *dateLight; // hero date, regular
  BaseFont *label;     // uppercase section labels
  BaseFont *heading;   // bold body
  BaseFont *body;      // regular body
  BaseFont *small;     // secondary text
  BaseFont *eta;       // departure minutes
  BaseFont *iconXL;    // hero weather icon
  BaseFont *iconL;     // forecast icons
};

struct Data {
  time_t now = 0; // 0 until the clock has synced
  std::string condition;
  float temperature = NAN, windSpeed = NAN, windGust = NAN;
  std::string forecast, departures, events;
  std::string stopName;
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

static std::string degrees(float v) {
  char buf[16];
  if (std::fabs(v - std::round(v)) < 0.05f)
    snprintf(buf, sizeof(buf), "%.0f°", v);
  else
    snprintf(buf, sizeof(buf), "%.1f°", v);
  return buf;
}

// Whole degrees, never "-0°".
static std::string wholeDegrees(float v) {
  char buf[16];
  long n = std::lround(v);
  snprintf(buf, sizeof(buf), "%ld°", n);
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

static const char *weatherIcon(const std::string &c, bool night) {
  if (c == "sunny")
    return night ? "\U000F0594" : "\U000F0599";
  if (c == "clear-night")
    return "\U000F0594";
  if (c == "partlycloudy")
    return night ? "\U000F0F31" : "\U000F0595";
  if (c == "cloudy")
    return "\U000F0590";
  if (c == "fog")
    return "\U000F0591";
  if (c == "hail")
    return "\U000F0592";
  if (c == "lightning")
    return "\U000F0593";
  if (c == "lightning-rainy")
    return "\U000F067E";
  if (c == "pouring")
    return "\U000F0596";
  if (c == "rainy")
    return "\U000F0597";
  if (c == "snowy")
    return "\U000F0598";
  if (c == "snowy-rainy")
    return "\U000F067F";
  if (c == "windy")
    return "\U000F059D";
  if (c == "windy-variant")
    return "\U000F059E";
  return "\U000F0F2F";
}

static std::string weatherText(const std::string &c) {
  if (c == "sunny")
    return "Sunny";
  if (c == "clear-night")
    return "Clear";
  if (c == "partlycloudy")
    return "Partly cloudy";
  if (c == "cloudy")
    return "Cloudy";
  if (c == "fog")
    return "Fog";
  if (c == "hail")
    return "Hail";
  if (c == "lightning")
    return "Thunder";
  if (c == "lightning-rainy")
    return "Thunderstorm";
  if (c == "pouring")
    return "Heavy rain";
  if (c == "rainy")
    return "Rain";
  if (c == "snowy")
    return "Snow";
  if (c == "snowy-rainy")
    return "Sleet";
  if (c == "windy" || c == "windy-variant")
    return "Windy";
  return "";
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

// Lowest and highest temperature in the forecast between 06:00 and 22:00 of
// the given day. False if there is no data for it.
static bool dayRange(const std::vector<Hour> &hs, time_t day, float &lo, float &hi) {
  lo = INFINITY;
  hi = -INFINITY;
  for (auto &h : hs)
    if (h.t >= day + 6 * 3600 && h.t <= day + 22 * 3600 && !std::isnan(h.temp)) {
      lo = std::min(lo, h.temp);
      hi = std::max(hi, h.temp);
    }
  return lo <= hi;
}

// Small uppercase label with a hairline running to the end of the column,
// or up to `aside`, which is printed right-aligned in the same style.
static void sectionLabel(Display &it, const Fonts &f, int x, int y, int w, const std::string &text,
                         const std::string &aside = "") {
  std::string t = upper(text);
  it.print(x, y, f.label, INK, TextAlign::TOP_LEFT, t.c_str());
  int start = x + textWidth(it, f.label, t) + 14;
  int end = x + w;
  if (!aside.empty()) {
    it.print(end, y, f.label, INK, TextAlign::TOP_RIGHT, aside.c_str());
    end -= textWidth(it, f.label, aside) + 14;
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
  int free = MX + tw + 26;
  if (d.now) {
    int x = MX + tw + 26;
    std::string wd = fmt(d.now, "%A"), dm = fmt(d.now, "%e %B");
    it.print(x, HERO_Y + 30, f.date, INK, TextAlign::TOP_LEFT, wd.c_str());
    it.print(x, HERO_Y + 72, f.dateLight, INK, TextAlign::TOP_LEFT, dm.c_str());
    free = x + std::max(textWidth(it, f.date, wd), textWidth(it, f.dateLight, dm)) + 32;
  }

  // Current weather, right-aligned: icon and temperature...
  int right = W - MX;
  if (!std::isnan(d.temperature)) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f°", d.temperature);
    it.print(right, HERO_Y - 6, f.temp, INK, TextAlign::TOP_RIGHT, buf);
    int w = textWidth(it, f.temp, buf);
    bool night = d.now && (local(d.now).tm_hour < 6 || local(d.now).tm_hour >= 20);
    it.print(right - w - 14, HERO_Y - 2, f.iconXL, INK, TextAlign::TOP_RIGHT, weatherIcon(d.condition, night));
  }

  // ...and below them what decides the jacket: condition · feels like · wind.
  // When it doesn't fit beside the date, drop the condition first (the icon
  // already shows it), then the gusts.
  std::string cond = weatherText(d.condition), feels, wind, gusts;
  float fl = feelsLike(d.temperature, d.windSpeed);
  if (!std::isnan(fl) && std::lround(fl) != std::lround(d.temperature))
    feels = "Feels like " + wholeDegrees(fl);
  if (!std::isnan(d.windSpeed)) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.0f m/s", d.windSpeed);
    wind = buf;
    if (!std::isnan(d.windGust) && d.windGust >= d.windSpeed + 2) {
      snprintf(buf, sizeof(buf), ", gusts %.0f", d.windGust);
      gusts = buf;
    }
  }
  auto join = [](std::initializer_list<std::string> parts) {
    std::string out;
    for (auto &p : parts)
      if (!p.empty())
        out += (out.empty() ? "" : "  ·  ") + p;
    return out;
  };
  std::string sub;
  for (auto &candidate : {join({cond, feels, wind + gusts}), join({feels, wind + gusts}), join({feels, wind})}) {
    sub = candidate;
    if (textWidth(it, f.body, sub) <= right - free)
      break;
  }
  it.print(right, HERO_Y + 86, f.body, INK, TextAlign::TOP_RIGHT, sub.c_str());
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

  for (int day = 0; day < CAL_DAYS; day++) {
    time_t ds = dayStart(d.now, day), de = dayStart(d.now, day + 1);
    std::vector<const Event *> todays;
    for (auto &e : events)
      if (e.start < de && e.end > ds)
        todays.push_back(&e);
    // An agenda only lists days with something on, except today.
    if (todays.empty() && day > 0)
      continue;
    if (full) {
      hidden += todays.size();
      continue;
    }
    if (y + hHead + hBody > BOTTOM) {
      full = true;
      hidden += todays.size();
      continue;
    }

    std::string title = day == 0 ? "Today" : day == 1 ? "Tomorrow" : fmt(ds, "%A");
    std::string date = fmt(ds, "%e %b");
    it.print(LEFT_X, y, f.heading, INK, TextAlign::TOP_LEFT, title.c_str());
    int dateX = LEFT_X + textWidth(it, f.heading, title) + 12;
    it.print(dateX, y + 3, f.small, INK, TextAlign::TOP_LEFT, date.c_str());

    // An empty today stays on its heading line instead of taking a row.
    if (todays.empty()) {
      it.print(dateX + textWidth(it, f.small, date) + 28, y, f.body, INK, TextAlign::TOP_LEFT, "Nothing planned");
      y += hHead + 18;
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
    y += 10;
  }

  if (hidden > 0 && y + hSmall <= BOTTOM + hSmall) {
    char buf[24];
    snprintf(buf, sizeof(buf), "+%d more", hidden);
    it.print(textX, std::min(y, BOTTOM - hSmall), f.small, INK, TextAlign::TOP_LEFT, buf);
  }
}

// ------------------------------------------------------------- departures --

static void drawDepartures(Display &it, const Fonts &f, const Data &d) {
  sectionLabel(it, f, RIGHT_X, BUS_Y, RIGHT_W, d.stopName);
  int right = RIGHT_X + RIGHT_W;
  int y = BUS_Y + LABEL_GAP + 8;
  int shown = 0;

  for (auto &r : records(d.departures, 3)) {
    if (shown >= BUS_ROWS)
      break;
    time_t when = (time_t) atoll(r[2].c_str());
    if (d.now && when < d.now - 30) // still listed until it has left
      continue;
    bool cancelled = r.size() > 3 && r[3] == "CANCELLED";
    int cy = y + 22; // row centre line

    // Line number: white on a black rounded badge.
    const int badgeW = 70, badgeH = 44;
    fillRound(it, RIGHT_X, cy - badgeH / 2, badgeW, badgeH, 10, INK);
    it.print(RIGHT_X + badgeW / 2, cy + 1, f.heading, PAPER, TextAlign::CENTER, r[0].c_str());

    // ETA: big number with a small unit, or a clock time an hour+ out.
    int etaW;
    if (cancelled) {
      it.print(right, cy, f.small, INK, TextAlign::CENTER_RIGHT, "Cancelled");
      etaW = textWidth(it, f.small, "Cancelled");
    } else {
      long mins = d.now ? (long) (when - d.now) / 60 : 999;
      if (mins <= 0) {
        it.print(right, cy, f.eta, INK, TextAlign::CENTER_RIGHT, "Now");
        etaW = textWidth(it, f.eta, "Now");
      } else if (mins < 60) {
        char n[8];
        snprintf(n, sizeof(n), "%ld", mins);
        int uw = textWidth(it, f.small, "min");
        it.print(right, cy + 6, f.small, INK, TextAlign::CENTER_RIGHT, "min");
        it.print(right - uw - 6, cy, f.eta, INK, TextAlign::CENTER_RIGHT, n);
        etaW = uw + 6 + textWidth(it, f.eta, n);
      } else {
        std::string hm = fmt(when, "%H:%M");
        it.print(right, cy, f.eta, INK, TextAlign::CENTER_RIGHT, hm.c_str());
        etaW = textWidth(it, f.eta, hm);
      }
    }

    int destX = RIGHT_X + badgeW + 18;
    std::string dest = ellipsize(it, f.heading, r[1], right - etaW - 16 - destX);
    it.print(destX, cy, f.heading, INK, TextAlign::CENTER_LEFT, dest.c_str());
    if (cancelled)
      it.filled_rectangle(destX, cy, textWidth(it, f.heading, dest), 2, INK);

    y += BUS_ROW_H;
    shown++;
  }
  if (shown == 0)
    it.print(RIGHT_X, y, f.body, INK, TextAlign::TOP_LEFT,
             d.departures.empty() ? "Waiting for departures…" : "No departures");
}

// --------------------------------------------------------------- forecast --

static void drawForecast(Display &it, const Fonts &f, const Data &d) {
  auto hs = hours(d.forecast);
  bool tomorrow = false;
  time_t start = d.now ? forecastStart(d.now, tomorrow) : 0;

  // The day's range sits on the label line: low–high from 06 to 22.
  std::string range;
  float lo, hi;
  if (d.now && dayRange(hs, dayStart(d.now, tomorrow ? 1 : 0), lo, hi)) {
    // Include the current reading so today's range never excludes "now".
    if (!tomorrow && !std::isnan(d.temperature)) {
      lo = std::min(lo, d.temperature);
      hi = std::max(hi, d.temperature);
    }
    range = wholeDegrees(lo) + " – " + wholeDegrees(hi);
  }
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
    it.print(cx, y, f.small, INK, TextAlign::TOP_CENTER, fmt(slot, "%H").c_str());
    if (h == nullptr)
      continue;
    int hr = local(slot).tm_hour;
    it.print(cx, y + 26, f.iconL, INK, TextAlign::TOP_CENTER, weatherIcon(h->condition, hr < 6 || hr >= 20));
    if (!std::isnan(h->temp))
      it.print(cx, y + 84, f.heading, INK, TextAlign::TOP_CENTER, degrees(h->temp).c_str());
    char buf[16];
    if (!std::isnan(h->wind)) {
      snprintf(buf, sizeof(buf), "%.0f", h->wind);
      it.print(cx, y + 120, f.small, INK, TextAlign::TOP_CENTER, buf);
      anyWind = true;
    }
    if (h->precip >= 0.1f) {
      snprintf(buf, sizeof(buf), "%.1f", h->precip);
      it.print(cx, y + 148, f.small, INK, TextAlign::TOP_CENTER, buf);
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
