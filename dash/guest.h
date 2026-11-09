#pragma once
// The guest Wi-Fi screen: network, password and QR code under the hero.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "dates.h"
#include "layout.h"
#include "text.h"

namespace dash {

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

} // namespace dash
