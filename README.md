# Transit Tracker using HUB75 LED Matrix Panel with ESP32

Bus stops in the Waterloo / Kitchener / Cambridge (GRT) region use LED panels to display real‑time ETAs for arriving routes.
This project lets you host a similar sign yourself using an ESP32 and a HUB75 LED matrix panel. It is mostly plug and play besides a few configs you
have to set for your individual setup.

The firmware fetches **Google Transit Feed Specification (GTFS‑Realtime)** data from GRT, parses it using **Protocol Buffers**, and updates the display every 30 seconds.

> Data source: [GRT Open Data (GTFS / GTFS‑Realtime)](https://www.grt.ca/en/about-grt/open-data.aspx)

---

## Features

* **FreeRTOS‑based task architecture**

  * Network, parsing, and display handled in separate tasks
  * Tasks pinned across both ESP32 cores to reduce contention

* **DMA‑driven HUB75 display output**

  * Uses [ESP32‑HUB75‑MatrixPanel‑DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
  * Offloads refresh timing from the CPU for smooth, flicker‑free output

* **Low‑memory GTFS‑Realtime parsing**

  * Uses **nanopb** with streaming decode
  * No full message buffering
  * Early‑exit parsing to reduce RAM usage and latency

* **Text rendering via Adafruit GFX**

  * Familiar Arduino‑style graphics API
  * Built using PlatformIO while targeting ESP32

---

## Hardware Requirements

* ESP32 (tested with ESP32 DevKit V1)
* HUB75 RGB LED matrix panel
* 5V power supply capable of supplying the specified panel current
* Jumper wires to attach between HUB75 and ESP32.

---

## Software Requirements

* **PlatformIO** (VS Code recommended)
* ESP32 platform support installed in PlatformIO
* USB drivers for your ESP32 board.

---

## Setup

### 1. Clone the repository

### 2. Configure Wi‑Fi

Set your Wi‑Fi credentials in include\network.h:

```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
```

### 3. Configure the display

* Set the HUB75 panel size (width, height, chain length) in include\display.h
* Verify GPIO pin mappings match your wiring
* DMA and I2S pin routing is handled by the MatrixPanel‑DMA driver

### 4. Configure transit details

* Configure the URL where transit data is fetched from (e.g. [GRT API](https://webapps.regionofwaterloo.ca/api/grt-routes/api/tripupdates/1)).

### 4. Build and upload
1. Open the PIO extension on the VSCode extension sidebar
2. Click the dropdown for ESP32
3. "Build"
4. Then "Upload and Monitor".

## How It Works

1. **Network Task**

   * Connects to Wi‑Fi
   * Fetches GTFS‑Realtime protobuf data over HTTP

2. **Parsing Task**

   * Streams protobuf data using nanopb
   * Extracts route IDs, stop times, and ETAs
   * Discards unused fields early to save memory

3. **Display Task**

   * Renders route numbers and arrival times
   * Uses DMA to continuously refresh the HUB75 panel
   * Runs independently of network activity

The update cycle repeats every **30 seconds**.

---

## How to Use

1. Power the HUB75 panel and ESP32
2. ESP32 connects to Wi‑Fi automatically
3. The display initializes and shows arrival times for configured stops
4. ETAs refresh every 30 seconds

---

## Customization

* Change displayed routes or stops by modifying the GTFS parsing filters in parse.h.
* Customize fonts, layout, or colors via Adafruit GFX in display.cpp.

---

## Limitations / Notes

* Designed specifically for **GTFS‑Realtime** feeds
* Memory usage is tuned for ESP32; adding features may require careful heap management
* HUB75 DMA uses I2S internally, limiting some peripheral pin choices

---

## Credits

* ESP32 HUB75 DMA Driver: [https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA)
* Adafruit GFX Library
* nanopb (Protocol Buffers for embedded systems)
* Grand River Transit Open Data

---

## License

MIT License (or specify if different)
