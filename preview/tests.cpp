// Tests for the logic that doesn't show up in a picture until it goes wrong:
// day arithmetic across DST, the forecast window and range, wind chill,
// parsing of the HA records and the guest Wi-Fi payload, and text wrapping.
// render.sh runs these before rendering; a failure stops it.

#include "harness.h"

#include <cmath>
#include <string>
#include <type_traits>

using namespace dash;

static int failures = 0, checks = 0;

#define CHECK(cond)                                                       \
  do {                                                                    \
    checks++;                                                             \
    if (!(cond)) {                                                        \
      failures++;                                                         \
      fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
    }                                                                     \
  } while (0)

#define CHECK_EQ(a, b)                                                                              \
  do {                                                                                              \
    checks++;                                                                                       \
    auto va = (a);                                                                                  \
    auto vb = (b);                                                                                  \
    if (!(va == vb)) {                                                                              \
      failures++;                                                                                   \
      fprintf(stderr, "%s:%d: CHECK_EQ failed: %s == %s\n  got: %s\n  want: %s\n", __FILE__, __LINE__, \
              #a, #b, show(va).c_str(), show(vb).c_str());                                         \
    }                                                                                               \
  } while (0)

template <class T> static std::string show(const T &v) {
  if constexpr (std::is_same_v<T, bool>)
    return v ? "true" : "false";
  else if constexpr (std::is_arithmetic_v<T>)
    return std::to_string(v);
  else
    return "\"" + std::string(v) + "\"";
}

// Stockholm wall time → epoch.
static time_t at(int y, int mo, int d, int h, int mi = 0) {
  struct tm t = {};
  t.tm_year = y - 1900;
  t.tm_mon = mo - 1;
  t.tm_mday = d;
  t.tm_hour = h;
  t.tm_min = mi;
  t.tm_isdst = -1;
  return mktime(&t);
}

static void testDayStart() {
  // Ordinary day.
  CHECK_EQ((long long) dayStart(at(2026, 11, 9, 12, 53), 0), (long long) at(2026, 11, 9, 0));
  CHECK_EQ((long long) dayStart(at(2026, 11, 9, 0, 0), 1), (long long) at(2026, 11, 10, 0));
  // Into, during and out of the spring change (29 March 2026, 02→03).
  CHECK_EQ((long long) dayStart(at(2026, 3, 28, 22), 1), (long long) at(2026, 3, 29, 0));
  CHECK_EQ((long long) dayStart(at(2026, 3, 29, 12), 0), (long long) at(2026, 3, 29, 0));
  CHECK_EQ((long long) dayStart(at(2026, 3, 29, 12), 1), (long long) at(2026, 3, 30, 0));
  CHECK_EQ((long long) (dayStart(at(2026, 3, 29, 12), 1) - dayStart(at(2026, 3, 29, 12), 0)), 23LL * 3600);
  // Autumn change (25 October 2026, 03→02): a 25-hour day.
  CHECK_EQ((long long) dayStart(at(2026, 10, 25, 12), 0), (long long) at(2026, 10, 25, 0));
  CHECK_EQ((long long) (dayStart(at(2026, 10, 25, 12), 1) - dayStart(at(2026, 10, 25, 12), 0)), 25LL * 3600);
  // Month and year roll-over.
  CHECK_EQ((long long) dayStart(at(2026, 12, 31, 18), 1), (long long) at(2027, 1, 1, 0));
}

static void testForecastWindow() {
  bool tomorrow;
  // Daytime: from the next whole hour.
  CHECK_EQ((long long) forecastStart(at(2026, 11, 9, 12, 53), tomorrow), (long long) at(2026, 11, 9, 13));
  CHECK_EQ(tomorrow, false);
  // Early morning: today from 07.
  CHECK_EQ((long long) forecastStart(at(2026, 11, 9, 4, 10), tomorrow), (long long) at(2026, 11, 9, 7));
  CHECK_EQ(tomorrow, false);
  // From 19:00 the next hour is 20, so it switches to tomorrow from 07.
  CHECK_EQ((long long) forecastStart(at(2026, 11, 9, 19, 5), tomorrow), (long long) at(2026, 11, 10, 7));
  CHECK_EQ(tomorrow, true);
  CHECK_EQ((long long) forecastStart(at(2026, 11, 9, 18, 59), tomorrow), (long long) at(2026, 11, 9, 19));
  CHECK_EQ(tomorrow, false);
}

static void testForecastRange() {
  // 13:00..23:00 is what a table starting at 13 spans.
  time_t start = at(2026, 11, 9, 13);
  std::vector<Hour> hs;
  for (int h = 12; h <= 26; h++) // 12:00 to 02:00 the next day
    hs.push_back({start + (h - 13) * 3600, "cloudy", 5.0f, 0, 3});
  hs[1 + 3].temp = 2.6f;  // 16:00, inside
  hs[1 + 10].temp = 6.4f; // 23:00, the last hour inside
  hs[0].temp = -9.0f;     // 12:00, before the table
  hs[1 + 11].temp = 9.0f; // 00:00, after it
  float lo, hi;
  CHECK(forecastRange(hs, start, false, NAN, lo, hi));
  CHECK_EQ(wholeDegrees(lo), std::string("3°"));
  CHECK_EQ(wholeDegrees(hi), std::string("6°"));
  // Today's range includes the current reading; tomorrow's doesn't.
  CHECK(forecastRange(hs, start, false, 1.2f, lo, hi));
  CHECK_EQ(wholeDegrees(lo), std::string("1°"));
  CHECK(forecastRange(hs, start, true, 1.2f, lo, hi));
  CHECK_EQ(wholeDegrees(lo), std::string("3°"));
  // No data for those hours.
  CHECK(!forecastRange(hs, start + 30 * 3600, false, 1.2f, lo, hi));
}

static void testTemperatures() {
  CHECK_EQ(degrees(2.9f), std::string("2.9°"));
  CHECK_EQ(degrees(3.02f), std::string("3°"));
  CHECK_EQ(degrees(-0.02f), std::string("0°"));
  CHECK_EQ(degrees(-12.4f), std::string("-12.4°"));
  CHECK_EQ(wholeDegrees(-0.4f), std::string("0°"));
  CHECK_EQ(wholeDegrees(-0.6f), std::string("-1°"));

  // Wind chill: -10 °C in 10 m/s is about -20 (JAG/TI tables).
  CHECK(std::fabs(feelsLike(-10.f, 10.f) - -20.3f) < 0.2f);
  // Not defined when warm or calm: the air temperature.
  CHECK_EQ((long long) std::lround(feelsLike(15.f, 10.f)), 15LL);
  CHECK_EQ((long long) std::lround(feelsLike(-5.f, 1.f)), -5LL);
  CHECK(std::isnan(feelsLike(NAN, 3.f)));
}

static void testWeather() {
  CHECK_EQ(std::string(weatherIcon("partlycloudy", false)), std::string("\U000F0595"));
  CHECK_EQ(std::string(weatherIcon("partlycloudy", true)), std::string("\U000F0F31"));
  CHECK_EQ(std::string(weatherIcon("something-new", false)), std::string(ICON_UNKNOWN));
  CHECK_EQ(weatherText("lightning-rainy"), std::string("Thunderstorm"));
  CHECK_EQ(weatherText("something-new"), std::string(""));
  CHECK(isNight(at(2026, 11, 9, 20, 0)));
  CHECK(isNight(at(2026, 11, 9, 5, 59)));
  CHECK(!isNight(at(2026, 11, 9, 6, 0)));
}

static void testParsing() {
  // Forecast: missing precipitation counts as dry; missing wind (an older HA
  // package) is NAN so the row is left out.
  auto hs = hours("1794229200|cloudy|4.4|0.0|3.1\n1794232800|rainy|4.1||\n1794236400|sunny|3.2\nbad\n");
  CHECK_EQ((long long) hs.size(), 3LL);
  CHECK(std::fabs(hs[0].wind - 3.1f) < 1e-4f);
  CHECK_EQ(hs[1].precip == 0.f, true);
  CHECK(std::isnan(hs[2].wind));

  auto ev = events("100|200|1|Lina födelsedag|\n300|400|0|Fysio|Karolinska\n");
  CHECK_EQ((long long) ev.size(), 2LL);
  CHECK_EQ(ev[0].allDay, true);
  CHECK_EQ(ev[1].location, std::string("Karolinska"));

  // Departures: drop ones that left more than 30 s ago, cap at BUS_ROWS,
  // read the cancelled state and the notice flag; old 4-field rows work.
  time_t now = 1000000;
  std::string rows = "875|Gullmarsplan|999900|EXPECTED|0\n"       // left 100 s ago
                     "875|Gullmarsplan|999980|EXPECTED|0\n"       // leaving now
                     "824|Rudsjöterrassen|1000300|CANCELLED|1\n"  //
                     "815C|Slussen|1000600|EXPECTED\n"            // no notice field
                     "875|Gullmarsplan|1000900|EXPECTED|0\n"      //
                     "875|Gullmarsplan|1001200|EXPECTED|0\n";     // beyond BUS_ROWS
  auto deps = departures(rows, now);
  CHECK_EQ((long long) deps.size(), (long long) BUS_ROWS);
  CHECK_EQ(deps[0].when, (time_t) 999980);
  CHECK_EQ(deps[1].cancelled, true);
  CHECK_EQ(deps[1].notice, true);
  CHECK_EQ(deps[2].line, std::string("815C"));
  CHECK_EQ(deps[2].notice, false);
}

static void testWifiPayload() {
  std::string p = "WIFI:T:WPA;S:My\\;Net\\:work;P:pa\\\\ss;;";
  CHECK_EQ(wifiField(p, 'T'), std::string("WPA"));
  CHECK_EQ(wifiField(p, 'S'), std::string("My;Net:work"));
  CHECK_EQ(wifiField(p, 'P'), std::string("pa\\ss"));
  CHECK_EQ(wifiField("WIFI:S:Open;;", 'P'), std::string(""));
}

static void testWrap(const Fonts &f) {
  esphome::display::Display it(W, H);
  std::string text = "Workshop: quarterly planning for the platform team and friends";
  auto lines = wrap(it, f.body, text, 400, 2);
  CHECK_EQ((long long) lines.size(), 2LL);
  for (auto &l : lines)
    CHECK(textWidth(it, f.body, l) <= 400);
  CHECK(lines[1].size() >= 3 && lines[1].compare(lines[1].size() - 3, 3, "…") == 0);
  // Short text is left alone; ellipsize keeps to the width.
  CHECK_EQ(wrap(it, f.body, "Standup", 400, 2).size(), (size_t) 1);
  std::string e = ellipsize(it, f.small, "Mottagning Medicinsk psykologi, Karolinska Universitetssjukhuset", 300);
  CHECK(textWidth(it, f.small, e) <= 300);
}

int main() {
  useDeviceTimezone();
  Fonts f;
  std::map<std::string, esphome::display::BaseFont *> byId;
  if (!loadFonts(f, byId))
    return 1;

  testDayStart();
  testForecastWindow();
  testForecastRange();
  testTemperatures();
  testWeather();
  testParsing();
  testWifiPayload();
  testWrap(f);

  if (failures) {
    fprintf(stderr, "tests: %d of %d checks failed\n", failures, checks);
    return 1;
  }
  printf("tests: %d checks passed\n", checks);
  return 0;
}
