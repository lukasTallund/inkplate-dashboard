#pragma once
// Local time and day arithmetic.
#include <cstdio>
#include <ctime>
#include <string>

namespace dash {

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

} // namespace dash
