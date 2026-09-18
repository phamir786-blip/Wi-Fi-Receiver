# WiFi-HiFi: Lossless Wireless Audio Streaming Ecosystem

[![ESP32-C3 Firmware CI](https://github.com/anaya2025/WiFi-HiFi/actions/workflows/firmware.yml/badge.svg)](https://github.com/anaya2025/WiFi-HiFi/actions/workflows/firmware.yml)
[![Android App CI](https://github.com/anaya2025/WiFi-HiFi/actions/workflows/android.yml/badge.svg)](https://github.com/anaya2025/WiFi-HiFi/actions/workflows/android.yml)

**WiFi-HiFi** is a unified, production-grade wireless audio transmission product. It captures bit-perfect, uncompressed linear PCM audio from an Android 16+ smartphone and streams it over Wi-Fi with ultra-low jitter to a dedicated Espressif ESP32-C3 receiver wired to an NXP UDA1334A I2S digital-to-analog converter.

---

## 1. System Architecture

```
+-------------------------------------------------------------------------+
|                           Android 16+ Device                            |
|                                                                         |
|  [ Music App / Game / Podcast ]                                         |
|                 |                                                       |
|                 v (AudioPlaybackCapture API)                            |
|       [ AudioCaptureManager ]                                           |
|                 |                                                       |
|                 v (44.1 kHz, 16-bit, 2-ch Stereo Linear PCM)            |
|        [ PcmPacketizer ]                                                |
|                 | (10ms Chunks + 28-byte Header, Magic 0x46484657)      |
|                 v                                                       |
|        [ UdpAudioSender ]                                               |
+-----------------|-------------------------------------------------------+
                  |
             Wi-Fi (802.11 b/g/n UDP Port 50005)
                  |
+-----------------v-------------------------------------------------------+
|                       ESP32-C3 Receiver Module                          |
|                                                                         |
|        [ PcmReceiver ] (UDP Socket on Port 50005)                       |
|                 |                                                       |
|                 v (Header Validation, Seq Tracking, Jitter Filter)      |
|         [ PcmBuffer ] (Thread-Safe Ring Buffer, 48 KB)                  |
|                 |                                                       |
|                 v (FreeRTOS High-Priority Audio Task)                   |
|        [ AudioOutput ] (I2S DMA Driver + Digital Volume / Soft Mute)    |
+-----------------|-------------------------------------------------------+
                  | (I2S Standard Mode: BCLK=GPIO4, WS=GPIO5, DOUT=GPIO6)
+-----------------v-------------------------------------------------------+
|                    NXP UDA1334A Stereo I2S DAC                          |
|                                                                         |
|   (Derives internal audio master clock via on-chip PLL)                 |
+-----------------|-------------------------------------------------------+
                  | 3.5mm Stereo Line-Out / RCA
+-----------------v-------------------------------------------------------+
|                 Audio Amplifier / Powered Monitors                      |
+-------------------------------------------------------------------------+
```

---

## 2. Repository Structure

```
anaya2025/WiFi-HiFi/
├── .github/
│   └── workflows/
│       ├── firmware.yml         # GitHub Actions for PlatformIO ESP32-C3 build
│       └── android.yml          # GitHub Actions for Android Gradle APK build
├── android/                     # Android 16+ Sender Application (Kotlin + Compose)
│   ├── app/
│   │   ├── build.gradle
│   │   ├── proguard-rules.pro
│   │   └── src/main/
│   │       ├── AndroidManifest.xml
│   │       ├── java/com/wifihifi/app/
│   │       │   ├── WiFiHiFiApp.kt
│   │       │   ├── MainActivity.kt
│   │       │   ├── audio/       # AudioCaptureManager, PcmPacketizer, AudioResampler
│   │       │   ├── network/     # UdpAudioSender, ReceiverDiscovery, ReceiverController
│   │       │   ├── service/     # AudioStreamService (Foreground Service)
│   │       │   ├── protocol/    # WiFiHiFiProtocol definitions
│   │       │   ├── model/       # ReceiverDevice, StreamState, AudioStats
│   │       │   └── ui/          # MainScreen, SettingsScreen, theme
│   │       └── res/             # Strings, colors, themes, adaptive icons
│   ├── build.gradle
│   ├── settings.gradle
│   └── README.md
├── firmware/                    # ESP32-C3 Receiver Firmware (C++ / PlatformIO)
│   ├── include/
│   │   ├── config.h             # Hardware pins, buffer limits, task priorities
│   │   └── protocol_defs.h      # Shared binary packet layout & struct headers
│   ├── src/
│   │   ├── main.cpp             # Startup orchestration & loop
│   │   ├── audio_output.cpp/.h  # I2S driver for NXP UDA1334A DAC
│   │   ├── pcm_buffer.cpp/.h    # FreeRTOS ring buffer with jitter management
│   │   ├── pcm_receiver.cpp/.h  # UDP socket listener & packet validator
│   │   ├── wifi_manager.cpp/.h  # Wi-Fi Station & Fallback Provisioning AP
│   │   ├── discovery.cpp/.h     # mDNS & UDP broadcast discovery responder
│   │   ├── settings.cpp/.h      # NVS persistent configuration storage
│   │   ├── ota_manager.cpp/.h   # Dual-partition failsafe firmware update
│   │   ├── system_manager.cpp/.h# Watchdog, uptime & restart supervisor
│   │   ├── web_server.cpp/.h    # Asynchronous Web Server & JSON REST API
│   │   └── web_assets.h         # Mobile-first embedded Web UI
│   ├── platformio.ini           # PlatformIO environment configuration
│   ├── partitions.csv           # 4MB Flash Dual OTA partition table
│   └── README.md
├── protocol/
│   └── WIFI_HIFI_PROTOCOL.md    # Network audio & discovery protocol specification
├── metadata.json                # Project identity and environment metadata
└── README.md
```

---

## 3. Hardware Requirements & Wiring

### Bill of Materials
1. **Microcontroller**: Espressif ESP32-C3 RISC-V development board (4MB Flash, No PSRAM).
2. **Audio DAC**: NXP UDA1334A I2S Stereo Audio DAC breakout board (e.g. Adafruit I2S Stereo DAC).
3. **Power Supply**: 5V USB-C power supply (recommended 1A+ filtered).

### Exact NXP UDA1334A Wiring to ESP32-C3

| ESP32-C3 Pin | UDA1334A Pin | Function | Notes |
| :--- | :--- | :--- | :--- |
| **3V3** or **5V** | **VIN** | Power | Breakout includes onboard 3.3V LDO |
| **GND** | **GND** | Ground | Common system ground |
| **GPIO 4** | **BCLK** | Bit Clock | Continuous clock for I2S framing |
| **GPIO 5** | **WSEL / LRCK** | Word Select | Frame synchronization clock (44.1 kHz) |
| **GPIO 6** | **DIN / DATA** | Data In | Serial PCM audio samples |
| *(Floating)* | **MCLK** | Master Clock | Not needed (UDA1334A PLL generates MCLK) |

---

## 4. Binary Network Audio Protocol Summary

- **Transport**: UDP Unicast to port `50005`.
- **Packet Structure**: 28-byte binary header followed by 10ms of 16-bit stereo PCM audio (1764 bytes payload = 1792 bytes total datagram).
- **Byte Order**: Little-Endian.

```
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                       Magic ('WFHF')                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Version (1)  | PktType (1)   |           Stream ID           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                    Frame Position (64-bit)                    +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                     Sample Rate (44100 Hz)                    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  Channels (2) |  Bits/Sam (16)| Latency Mode  |   Reserved    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Payload Length (1764)   |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                  Audio Payload (1764 bytes)                   +
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 5. Building & Deploying

### ESP32-C3 Firmware
```bash
cd firmware
pio run -e esp32c3
pio run -e esp32c3 --target upload
```

### Android Application
```bash
cd android
./gradlew assembleDebug
```
Install the generated APK onto your Android 16+ phone:
```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

---

## 6. First-Boot & Provisioning

1. Power on the ESP32-C3 receiver.
2. If Wi-Fi is unconfigured, the receiver starts an Access Point named **WiFi-HiFi-Setup**.
3. Connect your phone or laptop to `WiFi-HiFi-Setup` and open `http://192.168.4.1`.
4. Enter your local Wi-Fi SSID and password and tap **Save & Connect**.
5. Once connected to your home Wi-Fi, the receiver broadcasts its presence on the network.
6. Open the **WiFi-HiFi** app on Android: the receiver appears automatically under **Target Receiver**.
7. Tap **Start Lossless Streaming** and grant the standard Android audio capture permission.
8. Enjoy low-latency, lossless audio through your amplifier!

---

## 7. Over-The-Air (OTA) Updates

The firmware uses an active-passive dual partition table (`app0` and `app1`, 1.5MB each):
1. Navigate to `http://wifi-hifi.local` in your browser.
2. Open the **System** tab.
3. Select `firmware.bin` and upload.
4. The receiver verifies partition boundaries, writes the new slot, updates boot flags, and restarts smoothly without risks of bricking.
