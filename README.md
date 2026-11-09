# Inkplate Dashboard

An ESPHome firmware for the Inkplate 10 (v2) that draws a Home Assistant
dashboard directly on the panel: clock, calendar, SMHI weather, bus departures
from HASL and a guest Wi-Fi QR code. Nothing renders screenshots; the ESP32
draws text and icons itself.

The panel runs in 1-bit mode with partial updates, so the clock and departures
change every minute without the whole screen flashing. A full refresh runs
once an hour to clear ghosting.

## Files

| File | What it is |
|---|---|
| `dashboard.yaml` | ESPHome device config: hardware, data sources, fonts, HA buttons |
| `dashboard.h` | Layout and drawing (C++) |
| `ha/inkplate_dashboard.yaml` | HA package with the three template sensors the device reads |
| `secrets.yaml` | Wi-Fi, API key, guest Wi-Fi QR. Gitignored; see `secrets.example.yaml` |
| `preview/` | Renders `dashboard.h` to PNGs on your computer, no flashing needed |

## Data flow

Home Assistant turns the forecast, calendar and departures into three sensors
(`sensor.inkplate_forecast`, `sensor.inkplate_calendar`,
`sensor.inkplate_departures`). Each has a `rows` attribute with one record per
line and fields separated by `|`. The device subscribes to those attributes
over the ESPHome API and redraws on the minute.

Bus times are sent as timestamps, so the device works out "N min" itself every
minute, even when HASL updates less often.

## Setup

1. **HA package.** Copy `ha/inkplate_dashboard.yaml` to `<config>/packages/`
   and make sure `configuration.yaml` loads packages:
   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```
   Restart HA, then check in *Developer tools → States* that
   `sensor.inkplate_forecast`, `sensor.inkplate_calendar` and
   `sensor.inkplate_departures` exist and have a `rows` attribute.
2. **Secrets.** Copy `secrets.example.yaml` to `secrets.yaml` and fill it in.
3. **First flash over USB:**
   ```sh
   esphome run dashboard.yaml --device /dev/ttyUSB0
   ```
4. **Adopt in HA.** HA should discover the device. Add it with the
   `api_encryption_key` from `secrets.yaml`. Later flashes can go over the air
   from the ESPHome add-on or with `esphome run dashboard.yaml`.

### Network

HA connects *to* the device on TCP 6053, the ESPHome API. If the Inkplate is
on an isolated IoT VLAN, allow HA to reach it on that port. OTA is TCP 3232
from whichever machine is flashing, to the device.

## Previewing without flashing

```sh
preview/render.sh
```

This compiles `dashboard.h` natively against a small stand-in for ESPHome's
display and font code and writes one PNG per scenario to `preview/out/`
(`photo`, `busy`, `evening`, `guest`). Fonts, sizes and glyph lists are read
from `dashboard.yaml`, and text is measured and placed the way ESPHome does
it, so the result matches the panel closely. A glyph a font doesn't include
is drawn as a box and reported as a warning.

It needs `g++`, FreeType and libpng headers, and one earlier
`esphome compile dashboard.yaml`, which downloads the fonts and the QR
library into `.esphome/`. The canned data lives in `preview/main.cpp`; add a
scenario there when you find a layout edge case.

## Changing things

- Entities and the stop name: `substitutions` at the top of `dashboard.yaml`,
  plus the entity IDs in `ha/inkplate_dashboard.yaml`.
- More calendars: add them to the `entity_id` list of the calendar trigger in
  the HA package.
- Layout, sizes and texts: `dashboard.h`. Box positions are constants at the top.
- New icons: add the code point to the right `icons_*` font in
  `dashboard.yaml`. ESPHome only bundles the glyphs you list.
