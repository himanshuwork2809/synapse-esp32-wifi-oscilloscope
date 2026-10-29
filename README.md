# SYNAPSE – ESP32 Wi-Fi Oscilloscope Firmware

Firmware for an **ESP32-based digital oscilloscope front-end**. It samples an
analog signal with an external 12-bit SPI ADC (MCP3201), shows status messages
on a small OLED, and streams the raw samples over Wi-Fi/TCP to a PC application
(referred to in the code as **SYNAPSE_GUI**, a Python app with a `dso_page.py`;
it is *not* part of this archive).

The code is written in C against **ESP-IDF** (FreeRTOS, `esp_wifi`, `lwip`,
`esp_http_server`, the new `driver/i2c_master`, `driver/gptimer` and
`driver/spi_master` APIs). The auto-generated `CMakeLists.txt` suggests it was
built as a PlatformIO `espidf` project, but it is a standard ESP-IDF component.

---

## 1. What it does (the big picture)

```
 Analog signal ──► MCP3201 ADC ──SPI──► ESP32 ──Wi-Fi/TCP:5000──► PC GUI
                                          │
                                          └──I2C──► SSD1306 OLED (status text)
```

1. On power-up the ESP32 starts its own Wi-Fi access point called **`SYNAPSE`**.
2. The user connects the PC to that AP and sends the home Wi-Fi credentials
   to the board over HTTP (`POST /wifi`).
3. The board drops its AP, joins the home network as a station, and prints its
   new IP on the OLED.
4. It starts the ADC sampling task and a **TCP server on port 5000**.
5. The PC GUI connects to that IP/port and requests or streams sample data.

---

## 2. Hardware

| Part | Purpose | Connection (from `config.h`) |
|---|---|---|
| ESP32 | MCU, Wi-Fi | – |
| SSD1306 128×64 OLED (I2C, addr `0x3C`) | Status display | SDA = GPIO 21, SCL = GPIO 22, 400 kHz |
| MCP3201 12-bit SPI ADC | Signal acquisition | CLK = GPIO 14, DOUT (MISO) = GPIO 27, CS = GPIO 13 |

SPI uses `SPI3_HOST` (VSPI), mode 0, 800 kHz clock (the MCP3201 maximum at
3.3 V). The ADC is read-only, so no MOSI pin is used.

---

## 3. Boot and connection flow

1. `main.c → app_main()` initialises the display, starts the Wi-Fi manager
   (SoftAP mode) and shows the "Welcome to SYNAPSE" screen with `192.168.4.1`.
   It then idles forever; everything else is event driven.
2. **`WIFI_EVENT_AP_STACONNECTED`** (a PC joins the AP) → OLED shows
   "PC Connected" and the HTTP server starts.
3. PC sends `POST /wifi` with JSON `{"ssid":"...","password":"..."}`.
   The board shows the received credentials, replies `OK`, then 500 ms later
   (in a separate task, so the reply can flush) stops the AP and connects as a
   station.
4. **`IP_EVENT_STA_GOT_IP`** → OLED shows "WiFi Connected" + IP, and
   `tcp_server_start()` runs: it initialises acquisition and launches the TCP
   server task.
5. A TCP client connecting shows "Setup Successful" on the OLED.

If the station connection drops, the OLED shows "WiFi Failed – Check SSID or
Password".

---

## 4. Network interfaces

### HTTP (SoftAP, port 80) – provisioning only
| Method | Path | Body | Response |
|---|---|---|---|
| GET | `/` | – | `SYNAPSE Online` (plain text, health check) |
| POST | `/wifi` | `{"ssid":"<name>","password":"<pass>"}` | `OK` (or 400/500 error) |

The JSON is parsed with a small hand-written string extractor
(`json_get_string`), not a full JSON library; max request size is 255 bytes.

### TCP (station mode, port 5000) – data
Plain-text commands, one client at a time:

| Command | Reply |
|---|---|
| `PING` | `PONG` |
| `GET_DATA` | One **frame** (see below) with up to 2048 buffered samples |
| `STREAM` | Server takes over the connection and keeps sending frames until the client disconnects |

**Frame format (all little-endian):**

| Bytes | Type | Meaning |
|---|---|---|
| 0–3 | `float32` | Measured sample rate in Hz |
| 4–7 | `uint32` | Number of samples `N` that follow |
| 8 … 8+2N−1 | `uint16[N]` | Raw 12-bit ADC codes (0–4095) |

A frame with `N = 0` is possible if no samples were buffered (`GET_DATA`).
To convert a code to volts: `V = code / 4095 × Vref` (Vref of the MCP3201).

---

## 5. Data acquisition (`acquisition.c`)

* A dedicated FreeRTOS task (`acq_task`) is pinned to **core 1** at high
  priority and busy-waits on a 1 MHz **GPTimer** to hit a fixed period
  (target `ADC_SAMPLE_RATE_HZ = 50 000` Sa/s → 20 µs).
* Each sample is a 16-clock SPI read from the MCP3201; the result is shifted
  to give a 12-bit value.
* Samples go into a **4096-entry lock-free ring buffer** (single producer,
  single consumer). If the reader falls behind, new samples are dropped and an
  overrun counter is incremented.
* The **actual** sample rate is measured every 1000 samples from real elapsed
  timer time and exposed via `acquisition_get_sample_rate_hz()`, so the GUI can
  use the true rate rather than the configured one.
* Once per 1000-sample window the task calls `vTaskDelay(1)` so core 1's idle
  task can run and the watchdog doesn't trigger; this small gap is included in
  the measured rate.

Public API (`acquisition.h`): `acquisition_init`, `acquisition_read_samples`,
`acquisition_available`, `acquisition_get_overrun_count`,
`acquisition_get_sample_rate_hz`.

---

## 6. Display and UI

* `display.c` – SSD1306 driver on top of `esp_lcd` (I2C), with a 1 KB
  framebuffer (`128×64/8`), `display_clear/draw_char/draw_string/update`.
  Uses a 5×7 font drawn on a 6-pixel pitch.
* `glcdfont.c/.h` – the classic Adafruit GLCD 5×7 font table (`font[]`) used by
  the display code.
* `ui.c/.h` – one function per screen ("AP mode", "PC connected", "Connecting…",
  "WiFi Connected", "WiFi Failed", "TCP listening", "Setup Successful",
  "Received credentials", "Oscilloscope streaming").

---

## 7. File reference

| File | Role |
|---|---|
| `main.c` | Entry point (`app_main`): init display + Wi-Fi, show AP screen |
| `config.h` | All pin numbers, I2C/SPI settings, sample rate, buffer size |
| `wifi_manager.c/.h` | SoftAP setup, STA connect, Wi-Fi/IP event handling |
| `http_server.c/.h` | Provisioning HTTP server (`GET /`, `POST /wifi`) |
| `tcp_server.c/.h` | Port-5000 data server (`PING`, `GET_DATA`, `STREAM`) |
| `acquisition.c/.h` | MCP3201 sampling task, ring buffer, rate measurement |
| `display.c/.h` | SSD1306 OLED driver and text rendering |
| `ui.c/.h` | Status screens shown on the OLED |
| `glcdfont.c/.h` | 5×7 font data actually used for text |
| `CMakeLists.txt` | Registers every file under `src/` as component sources |
| `functions.c/.h` | Older/leftover I2C + OLED helpers (see notes) |
| `font5x7.c/.h` | Empty font table placeholder (unused) |
| `generator.c/.h` | Empty placeholder files, probably for a future signal generator |

---

## 8. Building and using

1. Install ESP-IDF (or PlatformIO with the `espidf` framework) with a version
   that has the new `i2c_master`, `gptimer` and `esp_lcd_panel_ssd1306` APIs
   (ESP-IDF 5.x). The SSD1306 panel driver needs to be available in your IDF
   version or as a managed component.
2. Wire the hardware per section 2, build and flash.
3. On the PC, join the **`SYNAPSE`** Wi-Fi network.
4. Send your home Wi-Fi credentials:
   ```bash
   curl -X POST http://192.168.4.1/wifi \
        -d '{"ssid":"MyWiFi","password":"MyPassword"}'
   ```
5. Reconnect the PC to your home network, read the board's IP from the OLED,
   and connect to `<ip>:5000`:
   ```bash
   printf 'PING' | nc <ip> 5000        # → PONG
   ```
6. Use `GET_DATA` for single captures or `STREAM` for continuous data.

---

## 9. Known issues and things to be aware of

These are observations from reading the code, not changes I made.

* **Open AP despite a password.** `wifi_manager.c` sets a password
  (`synapse123`) but `authmode = WIFI_AUTH_OPEN`, so the AP is actually
  unprotected and the password is ignored. Use `WIFI_AUTH_WPA2_PSK` to enforce it.
* **Credentials are exposed.** The Wi-Fi password is printed to the serial log
  and drawn on the OLED, and is sent over the unencrypted open AP.
* **Acquisition init can run twice.** `tcp_server_start()` (which calls
  `acquisition_init()`) runs on every `GOT_IP` event; a reconnect would try to
  re-create the SPI bus, timer and task.
* **No automatic Wi-Fi retry.** After `STA_DISCONNECTED` the board only shows an
  error; it neither reconnects nor returns to AP mode.
* **Commands are compared with `strcmp` on a raw `recv`.** A command that
  arrives with extra bytes/newline, or split across packets, won't match.
* **Single data client**, and after `STREAM` ends the loop only exits cleanly
  when a `send` fails.
* **Declared but undefined / unused code:** `ui.h` declares
  `ui_show_waiting_credentials`, `ui_show_connecting_wifi` and `ui_show_ready`
  with no implementation, while `ui_show_acquisition_running` is implemented but
  not declared. `functions.h` declares `oled_initialize/command/data` that
  `functions.c` doesn't define (only `i2c_initialize` exists), and `functions.c`
  would conflict with the display driver if both used I2C port 0.
  `font5x7.c` and `generator.c/.h` are empty.
* **Sample-rate margin.** At 800 kHz SPI a 16-clock read takes about 20 µs
  before driver overhead, which is right at the 50 kSa/s period, so the real
  rate will likely come out lower, which is why the firmware reports the
  measured rate.
* **Samples have no timestamps.** The data is a continuous stream with dropped
  samples on buffer overrun; the overrun counter is not sent to the PC.

---

## 10. Ideas for next steps

* Implement the signal generator (`generator.c/.h`).
* Send the overrun count in each frame header.
* Add Wi-Fi credential storage in NVS and auto-reconnect.
* Secure the AP (WPA2) and stop logging passwords.
* Remove or finish the unused `functions.*` and `font5x7.*` files.
