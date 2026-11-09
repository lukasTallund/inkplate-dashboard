# Inkplate Dashboard

ESPHome firmware that draws a Home Assistant dashboard natively on an
Inkplate 10 **v2**. Replaced the HomePlate + Puppet setup, which hung every
12–36 h. See README.md for setup and data flow.

## Device

- Inkplate 10 v2: PCAL6416A IO expander at 0x20. The v1 board had an
  MCP23017. 8 MB PSRAM.
- Wall powered and always on. Redraws every minute on the minute. A Li-ion
  battery may be fitted as backup only, so no deep sleep or power saving.
- `inkplate-dashboard.local` = 192.168.20.46 on the IoT VLAN. HA is
  192.168.1.23 and reaches the API on 6053.
- Flash: `esphome run dashboard.yaml --device OTA`. OTA from the laptop on
  192.168.10.x works. Over USB it shows up as CH340 on `/dev/ttyUSB0`, and the
  port sometimes needs a retry.
- Preview: `preview/render.sh` renders every scenario to `preview/out/*.png`.
  Check layout changes there before flashing.
- Logs: `esphome logs dashboard.yaml --device OTA`. Each redraw logs a
  `[dash]` line with the byte count of every data feed.

## Gotchas

- In the Inkplate driver `COLOR_ON` is **white**. Use `dash::INK` and
  `dash::PAPER`.
- ESPHome 2026 no longer sets the libc TZ, so `localtime_r()` and `mktime()`
  run in UTC. Use `dash::local()` and `dash::dayStart()`, which go through
  `ESPTime::from_epoch_local`.
- Fonts only contain the glyphs you list. A new MDI icon must be added to the
  right `icons_*` font, and characters must not repeat within one font's
  `glyphs`.
- The HA package lives at `/homeassistant/config_split/inkplate_dashboard.yaml`,
  loaded from `configuration.yaml` under `homeassistant: packages:`. After
  editing `ha/inkplate_dashboard.yaml`, copy it there and reload template
  entities.

## User preferences

- Clean, modern look: white background, no boxes, small uppercase section
  labels with a hairline.
- Data that matters for dressing for the commute: hourly forecast with wind,
  low–high for the hours shown, feels-like, wind and gusts as `4 (8) m/s`.
  No wind direction.
- Layout is like HA's weather card: weather icon lined up with the right
  column, temperature right-aligned.
- 1-bit text only: dithered anti-aliasing was tried and looked worse on the
  panel. Greyscale mode would flash the whole screen every minute.
- Iterate in the preview; flash and commit only once the user is happy.
- Everything controllable from HA. Guest Wi-Fi only shows full-screen on a
  button press, for 5 min.
- Full refresh interval is a HA number. The user chose 15 min.
