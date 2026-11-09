#pragma once
// Just enough of ESPHome for dashboard.h to compile and draw on a PC.
//
// Text metrics and placement copy ESPHome's font component: FreeType at the
// font's pixel size, 1-bit glyphs, ascender as the top line, and the same
// alignment arithmetic as Display::get_text_bounds. Each font only knows the
// glyphs dashboard.yaml lists; a missing one is drawn as an outlined box, as
// on the device.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include <ft2build.h>
#include FT_FREETYPE_H

extern "C" {
#include "qrcodegen.h"
}

namespace esphome {

struct Color {
  uint8_t r, g, b, w;
  Color(uint8_t r = 0, uint8_t g = 0, uint8_t b = 0, uint8_t w = 0) : r(r), g(g), b(b), w(w) {}
};

struct ESPTime {
  uint8_t second, minute, hour, day_of_week, day_of_month;
  uint16_t day_of_year;
  uint8_t month;
  uint16_t year;
  bool is_dst;
  // The preview sets TZ itself, so libc local time is right here.
  static ESPTime from_epoch_local(time_t t) {
    struct tm l;
    localtime_r(&t, &l);
    ESPTime e;
    e.second = l.tm_sec;
    e.minute = l.tm_min;
    e.hour = l.tm_hour;
    e.day_of_week = l.tm_wday + 1;
    e.day_of_month = l.tm_mday;
    e.day_of_year = l.tm_yday + 1;
    e.month = l.tm_mon + 1;
    e.year = l.tm_year + 1900;
    e.is_dst = l.tm_isdst > 0;
    return e;
  }
};

namespace display {

enum class TextAlign {
  TOP = 0x00,
  CENTER_VERTICAL = 0x01,
  BASELINE = 0x02,
  BOTTOM = 0x04,
  LEFT = 0x00,
  CENTER_HORIZONTAL = 0x08,
  RIGHT = 0x10,
  TOP_LEFT = TOP | LEFT,
  TOP_CENTER = TOP | CENTER_HORIZONTAL,
  TOP_RIGHT = TOP | RIGHT,
  CENTER_LEFT = CENTER_VERTICAL | LEFT,
  CENTER = CENTER_VERTICAL | CENTER_HORIZONTAL,
  CENTER_RIGHT = CENTER_VERTICAL | RIGHT,
  BASELINE_LEFT = BASELINE | LEFT,
  BASELINE_CENTER = BASELINE | CENTER_HORIZONTAL,
  BASELINE_RIGHT = BASELINE | RIGHT,
  BOTTOM_LEFT = BOTTOM | LEFT,
  BOTTOM_CENTER = BOTTOM | CENTER_HORIZONTAL,
  BOTTOM_RIGHT = BOTTOM | RIGHT,
};

class Display;

static std::vector<uint32_t> codepoints(const char *s) {
  std::vector<uint32_t> out;
  const unsigned char *p = (const unsigned char *) s;
  while (*p) {
    uint32_t c = *p;
    int n = c < 0x80 ? 0 : c < 0xE0 ? 1 : c < 0xF0 ? 2 : 3;
    c &= n == 0 ? 0x7F : n == 1 ? 0x1F : n == 2 ? 0x0F : 0x07;
    p++;
    for (int i = 0; i < n && (*p & 0xC0) == 0x80; i++, p++)
      c = (c << 6) | (*p & 0x3F);
    out.push_back(c);
  }
  return out;
}

struct Glyph {
  int advance, offset_x, offset_y, width, height;
  std::vector<uint8_t> bits; // one byte per pixel, 0/1
};

class BaseFont {
 public:
  BaseFont(FT_Library lib, const char *path, int size, const char *glyphs) {
    if (FT_New_Face(lib, path, 0, &face_) != 0) {
      fprintf(stderr, "cannot load %s\n", path);
      exit(1);
    }
    FT_Set_Pixel_Sizes(face_, size, 0);
    ascender_ = px(face_->size->metrics.ascender);
    height_ = px(face_->size->metrics.height);
    for (uint32_t c : codepoints(glyphs))
      allowed_.push_back(c);
    // ESPHome's fallback advance is the first (lowest) glyph's.
    uint32_t first = allowed_.empty() ? ' ' : *std::min_element(allowed_.begin(), allowed_.end());
    fallback_advance_ = glyph(first) ? glyph(first)->advance : size / 2;
  }

  // ESPHome's Font::measure.
  void measure(const char *s, int *width, int *x_offset, int *baseline, int *height) {
    *baseline = ascender_;
    *height = height_;
    int x = 0, min_x = 0;
    bool any = false;
    for (uint32_t c : codepoints(s)) {
      const Glyph *g = glyph(c);
      if (!g) {
        x += fallback_advance_;
        continue;
      }
      min_x = any ? std::min(min_x, x + g->offset_x) : g->offset_x;
      x += g->advance;
      any = true;
    }
    *x_offset = min_x;
    *width = x - min_x;
  }

  void print(int x, int y, Display *d, Color color, const char *s);
  const Glyph *find_glyph(uint32_t c) { return glyph(c); }

  std::vector<uint32_t> missing; // code points asked for but not in the font

 private:
  static int px(long v) { return (int) std::lround(v / 64.0); }

  const Glyph *glyph(uint32_t c) {
    if (!allowed_.empty() && std::find(allowed_.begin(), allowed_.end(), c) == allowed_.end()) {
      if (std::find(missing.begin(), missing.end(), c) == missing.end())
        missing.push_back(c);
      return nullptr;
    }
    auto it = cache_.find(c);
    if (it != cache_.end())
      return it->second.width < 0 ? nullptr : &it->second;
    Glyph g{};
    if (FT_Get_Char_Index(face_, c) == 0 || FT_Load_Char(face_, c, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) != 0) {
      g.width = -1;
      if (std::find(missing.begin(), missing.end(), c) == missing.end())
        missing.push_back(c);
      cache_[c] = g;
      return nullptr;
    }
    FT_GlyphSlot s = face_->glyph;
    g.advance = px(s->metrics.horiAdvance);
    g.offset_x = s->bitmap_left;
    g.offset_y = ascender_ - s->bitmap_top;
    g.width = s->bitmap.width;
    g.height = s->bitmap.rows;
    g.bits.resize(g.width * g.height);
    for (int yy = 0; yy < g.height; yy++)
      for (int xx = 0; xx < g.width; xx++)
        g.bits[yy * g.width + xx] = (s->bitmap.buffer[yy * s->bitmap.pitch + xx / 8] >> (7 - xx % 8)) & 1;
    cache_[c] = g;
    return &cache_[c];
  }

  FT_Face face_;
  int ascender_, height_, fallback_advance_;
  std::vector<uint32_t> allowed_;
  std::map<uint32_t, Glyph> cache_;
};

}  // namespace display

namespace font {
using Font = display::BaseFont;
}

namespace qr_code {
class QrCode {
 public:
  explicit QrCode(const std::string &value) {
    ok_ = qrcodegen_encodeText(value.c_str(), tmp_, qr_, qrcodegen_Ecc_LOW, qrcodegen_VERSION_MIN,
                               qrcodegen_VERSION_MAX, qrcodegen_Mask_AUTO, true);
  }
  uint8_t get_size() { return ok_ ? qrcodegen_getSize(qr_) : 0; }
  bool module(int x, int y) { return qrcodegen_getModule(qr_, x, y); }

 private:
  bool ok_;
  uint8_t qr_[qrcodegen_BUFFER_LEN_MAX];
  uint8_t tmp_[qrcodegen_BUFFER_LEN_MAX];
};
}  // namespace qr_code

namespace display {

class Display {
 public:
  Display(int w, int h) : w_(w), h_(h), px_(w * h, 255) {}

  void draw_pixel_at(int x, int y, Color c) {
    if (x >= 0 && y >= 0 && x < w_ && y < h_)
      px_[y * w_ + x] = c.r;  // 1-bit panel: INK is 0, PAPER 255
  }
  void fill(Color c) { std::fill(px_.begin(), px_.end(), c.r); }
  void filled_rectangle(int x, int y, int w, int h, Color c) {
    for (int yy = y; yy < y + h; yy++)
      for (int xx = x; xx < x + w; xx++)
        draw_pixel_at(xx, yy, c);
  }
  void rectangle(int x, int y, int w, int h, Color c) {
    filled_rectangle(x, y, w, 1, c);
    filled_rectangle(x, y + h - 1, w, 1, c);
    filled_rectangle(x, y, 1, h, c);
    filled_rectangle(x + w - 1, y, 1, h, c);
  }
  void filled_circle(int cx, int cy, int r, Color c) {
    for (int dy = -r; dy <= r; dy++)
      for (int dx = -r; dx <= r; dx++)
        if (dx * dx + dy * dy <= r * r)
          draw_pixel_at(cx + dx, cy + dy, c);
  }

  // Display::get_text_bounds, verbatim arithmetic.
  void get_text_bounds(int x, int y, const char *text, BaseFont *font, TextAlign align, int *x1, int *y1,
                       int *width, int *height) {
    int x_offset, baseline;
    font->measure(text, width, &x_offset, &baseline, height);
    auto xa = TextAlign(int(align) & 0x18);
    auto ya = TextAlign(int(align) & 0x07);
    switch (xa) {
      case TextAlign::RIGHT: *x1 = x - *width - x_offset; break;
      case TextAlign::CENTER_HORIZONTAL: *x1 = x - (*width + x_offset) / 2; break;
      default: *x1 = x; break;
    }
    switch (ya) {
      case TextAlign::BOTTOM: *y1 = y - *height; break;
      case TextAlign::BASELINE: *y1 = y - baseline; break;
      case TextAlign::CENTER_VERTICAL: *y1 = y - *height / 2; break;
      default: *y1 = y; break;
    }
  }

  void print(int x, int y, BaseFont *font, Color color, TextAlign align, const char *text) {
    int x1, y1, w, h;
    get_text_bounds(x, y, text, font, align, &x1, &y1, &w, &h);
    font->print(x1, y1, this, color, text);
  }

  void qr_code(int x, int y, qr_code::QrCode *qr, Color color, int scale) {
    int n = qr->get_size();
    for (int my = 0; my < n; my++)
      for (int mx = 0; mx < n; mx++)
        if (qr->module(mx, my))
          filled_rectangle(x + mx * scale, y + my * scale, scale, scale, color);
  }

  int width() const { return w_; }
  int height() const { return h_; }
  const std::vector<uint8_t> &pixels() const { return px_; }

 private:
  int w_, h_;
  std::vector<uint8_t> px_;
};

inline void BaseFont::print(int x, int y, Display *d, Color color, const char *s) {
  int at = x;
  for (uint32_t c : codepoints(s)) {
    const Glyph *g = glyph(c);
    if (!g) {
      d->rectangle(at, y, fallback_advance_, height_, color);
      at += fallback_advance_;
      continue;
    }
    for (int yy = 0; yy < g->height; yy++)
      for (int xx = 0; xx < g->width; xx++)
        if (g->bits[yy * g->width + xx])
          d->draw_pixel_at(at + g->offset_x + xx, y + g->offset_y + yy, color);
    at += g->advance;
  }
}

}  // namespace display
}  // namespace esphome
