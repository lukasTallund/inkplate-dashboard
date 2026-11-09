// Renders the dashboard to PNGs on a PC, with the canned data in
// scenarios.h, so layout changes can be checked without flashing.
//
//   preview <out dir> [<reference dir>]
//
// With a reference dir, each image is compared with the one there. A changed
// one gets an out/<name>.diff.png (changed pixels black, the rest faded) and
// the exit status is 2. Run it through render.sh.

#include "harness.h"

#include <png.h>

#include <vector>

using namespace esphome;

#include "scenarios.h"

static bool writePng(const std::string &path, const std::vector<uint8_t> &px, int w, int h) {
  png_image img = {};
  img.version = PNG_IMAGE_VERSION;
  img.width = w;
  img.height = h;
  img.format = PNG_FORMAT_GRAY;
  return png_image_write_to_file(&img, path.c_str(), 0, px.data(), 0, nullptr) != 0;
}

// False if the file is missing or unreadable.
static bool readPng(const std::string &path, std::vector<uint8_t> &px, int &w, int &h) {
  png_image img = {};
  img.version = PNG_IMAGE_VERSION;
  if (!png_image_begin_read_from_file(&img, path.c_str()))
    return false;
  img.format = PNG_FORMAT_GRAY;
  w = img.width;
  h = img.height;
  px.resize(PNG_IMAGE_SIZE(img));
  return png_image_finish_read(&img, nullptr, px.data(), 0, nullptr) != 0;
}

// Number of differing pixels (-1: no reference); writes a diff image when
// there are any.
static long compare(const display::Display &it, const std::string &refPath, const std::string &diffPath) {
  std::vector<uint8_t> ref;
  int w, h;
  if (!readPng(refPath, ref, w, h))
    return -1;
  if (w != it.width() || h != it.height())
    return (long) it.width() * it.height();
  const auto &px = it.pixels();
  std::vector<uint8_t> diff(px.size());
  long changed = 0;
  for (size_t i = 0; i < px.size(); i++) {
    bool same = px[i] == ref[i];
    changed += !same;
    diff[i] = same ? (px[i] ? 255 : 200) : 0;
  }
  if (changed)
    writePng(diffPath, diff, w, h);
  return changed;
}

int main(int argc, char **argv) {
  useDeviceTimezone();
  std::string outDir = argc > 1 ? argv[1] : "out";
  std::string refDir = argc > 2 ? argv[2] : "";

  dash::Fonts f;
  std::map<std::string, display::BaseFont *> byId;
  if (!loadFonts(f, byId))
    return 1;

  qr_code::QrCode qr(QR);
  int status = 0;
  for (auto &sc : scenarios()) {
    dash::Data d;
    defaults(d);
    sc.fill(d);
    d.qr = &qr;
    display::Display it(dash::W, dash::H);
    dash::draw(it, f, d);

    std::string path = outDir + "/" + sc.name + ".png";
    if (!writePng(path, it.pixels(), it.width(), it.height())) {
      fprintf(stderr, "could not write %s\n", path.c_str());
      status = 1;
    }
    if (refDir.empty()) {
      printf("%s\n", path.c_str());
      continue;
    }
    std::string diffPath = outDir + "/" + sc.name + ".diff.png";
    long changed = compare(it, refDir + "/" + sc.name + ".png", diffPath);
    if (changed < 0) {
      printf("new        %s (no reference yet)\n", sc.name);
      status = status ? status : 2;
    } else if (changed > 0) {
      printf("changed    %s: %ld px, see %s\n", sc.name, changed, diffPath.c_str());
      status = status ? status : 2;
    } else {
      printf("unchanged  %s\n", sc.name);
    }
  }
  for (auto &s : FONT_SPECS)
    for (uint32_t c : byId[s.id]->missing) {
      fprintf(stderr, "warning: %s has no glyph U+%04X\n", s.id, c);
      status = 1;
    }
  return status;
}
