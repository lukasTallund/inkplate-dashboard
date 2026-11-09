#pragma once
// Measuring, placing and wrapping text, and the section label.
#include <algorithm>
#include <climits>
#include <string>
#include <vector>

#include "layout.h"
#include "util.h"

namespace dash {

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

} // namespace dash
