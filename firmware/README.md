# WiFi-HiFi ESP32-C3 Receiver Firmware

This directory contains the production-grade ESP32-C3 firmware for the **WiFi-HiFi** lossless Wi-Fi audio receiver.

---

## 1. Hardware Architecture

- **Microcontroller**: Espressif ESP32-C3 RISC-V 32-bit single-core @ 160 MHz (4MB Flash, No PSRAM).
- **Audio DAC**: NXP UDA1334A I2S Low Power Stereo DAC.
- **Transport**: Standard Wi-Fi 802.11 b/g/n (UDP Port 50005 for binary PCM, UDP Port 50006 for Discovery, TCP Port 80 for HTTP / REST API).

### Exact NXP UDA1334A Wiring to ESP32-C3 Super Mini

```
              +-----------------------+
              |        [USB-C]        |
       5V  ---| [1]               [1] |--- GPIO 5  --> UDA1334A WSEL / LRCK
      GND  ---| [2]               [2] |--- GPIO 6  --> UDA1334A DIN / DATA
      3V3  ---| [3]   ESP32-C3    [3] |--- GPIO 7
   GPIO 0  ---| [4]  Super Mini   [4] |--- GPIO 8  (Onboard Blue LED)
   GPIO 1  ---| [5]               [5] |--- GPIO 9  (BOOT button)
   GPIO 2  ---| [6]               [6] |--- GPIO 10
   GPIO 3  ---| [7]               [7] |--- GPIO 20 (UART0 RX)
   GPIO 4  ---| [8]               [8] |--- GPIO 21 (UART0 TX)
              +-----------------------+
                |
                +--> UDA1334A BCLK
```

| ESP32-C3 Super Mini Pin | UDA1334A DAC Pin | Description |
| :--- | :--- | :--- |
| **3V3** (Pin 3 Left) or **5V** (Pin 1) | **VIN** | Power Supply (3.3V or 5V) |
| **GND** (Pin 2 Left) | **GND** | Audio & Logic Ground |
| **GPIO 4** (Pin 8 Left) | **BCLK** | Continuous Bit Clock (I2S) |
| **GPIO 5** (Pin 1 Right) | **WSEL / LRCK** | Word Select / Left-Right Clock (44.1 kHz) |
| **GPIO 6** (Pin 2 Right) | **DIN / DATA** | Serial Audio Data In |
| *(Not connected)* | **MCLK** | Leave floating (UDA1334A generates MCLK via internal PLL) |
| *(Onboard LED - GPIO 8)* | — | Visual indicator (Blinks during Wi-Fi setup, solid during stream) |

---

## 2. Partition Layout (4MB Flash with Dual OTA)

Configured via `partitions.csv`:
- `nvs`: 20 KB (Persistent settings: Wi-Fi credentials, volume, latency mode)
- `otadata`: 8 KB (Active boot slot indicator)
- `app0` (ota_0): 1,536 KB (1.5 MB primary application slot)
- `app1` (ota_1): 1,536 KB (1.5 MB fallback OTA upgrade slot)
- `spiffs`: 896 KB (Reserved storage)
- `coredump`: 64 KB (Post-mortem crash analytics)

---

## 3. How to Build & Flash

### Prerequisites
- [PlatformIO Core (CLI)](https://platformio.org/) or PlatformIO IDE extension in VS Code.

### Compilation
```bash
cd firmware
pio run -e esp32c3
```

This compiles the firmware and produces:
- `.pio/build/esp32c3/firmware.bin`
- `.pio/build/esp32c3/bootloader.bin`
- `.pio/build/esp32c3/partitions.bin`

### Serial Flashing (Initial installation only)
```bash
pio run -e esp32c3 --target upload
pio device monitor -b 115200
```

#### Flashing Merged Binary directly with esptool (at offset 0x0)
```bash
esptool.py --chip esp32c3 --port /dev/ttyUSB0 --baud 460800 write_flash 0x0 .pio/build/esp32c3/firmware-merged.bin
```

#### Boot & Flashing Troubleshooting:
- **Flash Mode**: Always use `dio` mode (`--flash_mode dio`). Most ESP32-C3 modules (DevKitM-1, SuperMini, LuatOS, Xiao) do not support `qio`, and flashing `qio` will cause an immediate boot loop before `setup()`.
- **USB CDC (Native USB)**: For boards with native USB-C directly connected to ESP32-C3 internal USB pins (no CP2102/CH340 chip), `ARDUINO_USB_CDC_ON_BOOT=1` and `ARDUINO_USB_MODE=1` are enabled so serial output enumerates properly over USB.
- **Entering Download/Bootloader Mode manually**: If the board fails to flash or reboot, hold down the **BOOT** button (GPIO 9 pulled to GND), tap **RESET** (EN), then release **BOOT**. Run the upload command, then press **RESET** to boot.

### Over-The-Air (OTA) Updates
After initial flashing, subsequent updates do not require USB:
1. Open the receiver web interface at `http://wifi-hifi.local` (or receiver IP).
2. Navigate to the **System** tab.
3. Select the compiled `.pio/build/esp32c3/firmware.bin` file and click **Flash Firmware**.
4. The receiver will stream the binary to `app1`, verify CRC/hash, and reboot automatically.

---

## 4. First-Boot Wi-Fi Setup

If no Wi-Fi credentials have been saved:
1. The receiver automatically spawns an open Access Point: `WiFi-HiFi-Setup`.
2. Connect your phone or laptop to `WiFi-HiFi-Setup`.
3. Open `http://192.168.4.1` in your browser.
4. Enter your home Wi-Fi SSID and Password and click **Save & Connect**.
5. The device connects to your Wi-Fi network and announces itself as `wifi-hifi.local`.
