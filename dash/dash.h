#pragma once
// Drawing code for the dashboard. Kept out of the YAML so the layout is plain
// C++ that an editor can check. The lambda in dashboard.yaml only gathers the
// fonts and the current sensor values and calls dash::draw().
//
// ESPHome copies this directory and #includes every header in it, in no
// particular order, so each file includes what it uses.
//
//   layout.h      colours, geometry, Fonts, Data
//   util.h        string helpers
//   dates.h       local time and day arithmetic
//   text.h        measuring, placing and wrapping text; section labels
//   weather.h     conditions, icons, temperatures, wind chill
//   model.h       the sensor text parsed into records
//   hero.h agenda.h forecast.h departures.h guest.h   one per area
//
// Data arrives from Home Assistant as compact text: one record per line,
// fields separated by '|'. See ha/inkplate_dashboard.yaml for the producers.
//
// Design: white page, no boxes. A hero row (time, date, current weather),
// then two columns with small uppercase section labels: an agenda on the
// left, the hourly forecast and departures on the right. The guest Wi-Fi
// screen keeps the hero and swaps the columns for the network and QR code.

#include "agenda.h"
#include "departures.h"
#include "forecast.h"
#include "guest.h"
#include "hero.h"
#include "layout.h"
#include "model.h"

namespace dash {

static void draw(Display &it, const Fonts &f, const Data &d) {
  it.fill(PAPER);
  drawHero(it, f, d);
  if (d.guest) {
    drawGuest(it, f, d);
    return;
  }
  Model m = parse(d);
  drawAgenda(it, f, d, m.events);
  drawDepartures(it, f, d, m.departures);
  drawForecast(it, f, d, m.hours);
}

} // namespace dash
