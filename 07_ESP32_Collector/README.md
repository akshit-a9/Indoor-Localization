# 07_ESP32_Collector

ESP32 firmware for re-recording the phone's 52 spots with the ESP32 itself,
using the same 90 s protocol. The goal is to find out how the ESP32's RSSI
relates to the phone's (a fixed offset? missing weak APs?) so the existing
phone data can be reused, or else replaced.

The ESP32 creates its own WiFi network. A phone joins it and opens a page
that walks through each spot: pick it, check the map, confirm you're
standing there, then 90 s of scanning with a beep every 11 s to turn 45°
right. The OLED and RGB LED repeat everything, and the recording runs on the
ESP32, so a phone that sleeps or disconnects loses nothing.

Same board and wiring as the WayPoint device (OLED, RGB LED, LED on D2). No
buttons needed.

---

## For the person collecting: how to scan a spot

**Bring:** the device, a power bank and its USB cable, your phone.

1. Plug the device into the power bank. The screen says **Ready** and the
   LED is green.
2. On your phone: **turn off mobile data**, then join the WiFi
   **WayPoint-Collect** (password **waypoint**). A page should open by
   itself. If not, open Chrome and go to **192.168.4.1**.
3. Pick the floor, then the spot. Spots you've already done show a green
   **done** badge.
4. The map shows a **red dot**. Walk there and stand exactly on it. Tick
   *I am standing on the red dot* and press **Start**.
5. Hold the device flat in front of your chest, like a phone. You get
   5 seconds to get ready.
6. For 90 seconds: **stand still**. When the phone beeps (and the device
   screen says **TURN RIGHT 45°**), turn one-eighth of a circle to your right,
   then stand still again. That happens 7 times, so you end up facing almost
   where you started.
7. **Saved ✓**. Press *Next location* and go to the next spot.

If something goes wrong: if you started at the wrong spot or moved a lot,
press **Cancel this recording** and start again. If the phone page stops
updating, stay put and keep following the device's screen; the recording
continues. The page reconnects when the phone is back on the WiFi.

| Device LED | Meaning |
|------------|---------|
| Green | Ready / last spot saved |
| Yellow | Get ready, starting in a few seconds |
| Blue | Recording, stand still |
| Flashing white | Turn 45° right now |
| Red | Error (the screen says what) |
| Small blue LED (D2) | A phone is connected |

---

## Setup (once)

### Flash

Arduino IDE 2 with the same board package and libraries as WayPoint (esp32
by Espressif, **U8g2**, **ArduinoJson 7**). Open `esp32_collector/esp32_collector.ino`, then:

- Board: **ESP32 Dev Module**
- Tools → Partition Scheme: **No OTA (2MB APP/2MB SPIFFS)**. That fits all
  52 spots; the default scheme also works but holds about 35.

Or:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=no_ota 07_ESP32_Collector/esp32_collector
arduino-cli upload  --fqbn esp32:esp32:esp32:PartitionScheme=no_ota -p <port> 07_ESP32_Collector/esp32_collector
```

Re-uploading the sketch keeps recorded takes, but changing the partition
scheme erases them. Download first.

Settings (WiFi name/password, timings, pins) are in `esp32_collector/config.h`.
Scans use the same settings as the WayPoint firmware (active, 120 ms per
channel), so the data matches what WayPoint sees when it localizes.

### Get the data off

Either:

- **From the device:** join WayPoint-Collect from a laptop (or a phone in
  Chrome, not the sign-in pop-up), open 192.168.4.1 → *Saved data & download*
  → **Download all (CSV)**. Save it into `02_Data/05_Wifi_ESP32/`.
- **Live over USB:** with the ESP32 plugged into a laptop instead of a power
  bank, run `python3 07_ESP32_Collector/laptop_logger.py`. Every finished take
  is written to `02_Data/05_Wifi_ESP32/` and compared with the phone recording
  of that spot on the spot (needs `pip install pyserial`, and if a package
  called `serial` is installed, uninstall it first; it hides pyserial).

Then:

```bash
python3 03_Training/07_Compare_ESP32_vs_Phone.py
```

### Suggested first round

Record about 5 spots spread over both floors and different surroundings
(room, corridor, stairs, lift), for example #10 D-152, #20 DLT-8/1,
#30 D-255…258, #40 Corner of FC Entrance 2, #44 Ground Floor Lift. Then run
the comparison:

- **Offset roughly the same at every spot, small spread, and (b)/(c) accuracy
  high:** the phone data is reusable. Use `calibration.json` to shift it (c),
  or shift ESP32 scans at prediction time (b), and skip re-recording the rest.
- **Offset varies a lot between spots, or the ESP32 misses many APs the
  phone hears:** a single correction won't do; record all 52 spots with the
  ESP32 and train on that.

---

## Files

| File | What it does |
|------|--------------|
| `esp32_collector.ino` | Setup, main loop, OLED/LED updates |
| `recorder.*` | Countdown, 90 s of back-to-back scans, turn timing |
| `storage.*` | Takes on flash, CSV download, copy of each take to Serial |
| `web.*`, `web_page.h` | Phone page and its API, captive portal |
| `display.*`, `leds.*` | OLED screens, LEDs |
| `locations.h`, `floor_maps.h` | **Generated** by `make_assets.py`: the 52 spots (label, floor, map pixel) and both floor maps as JPEGs |
| `config.h` | Pins, WiFi, timings |
| `../make_assets.py` | Rebuilds the two generated headers from `02_Data/` |
| `../laptop_logger.py` | Optional USB logger + instant comparison |

`locations.h` and `floor_maps.h` are plain data with no collector code in
them, so the navigation firmware can include them unchanged to draw
positions and routes.
