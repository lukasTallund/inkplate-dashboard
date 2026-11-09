#pragma once
// Left column: the calendar.
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

static void drawAgenda(Display &it, const Fonts &f, const Data &d, const std::vector<Event> &events) {
  sectionLabel(it, f, LEFT_X, COL_Y, LEFT_W, "Calendar");
  if (!d.now)
    return;

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

} // namespace dash
