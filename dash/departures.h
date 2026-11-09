#pragma once
// Right column, bottom: bus departures.
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

namespace dash {

static void drawDepartures(Display &it, const Fonts &f, const Data &d, const std::vector<Departure> &rows) {
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

} // namespace dash
