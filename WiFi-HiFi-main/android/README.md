# WiFi-HiFi Android 16+ Sender Application

This directory contains the production-grade Android sender and controller application for the **WiFi-HiFi** ecosystem.

---

## 1. Overview & Architecture

The Android app captures system audio output (Spotify, YouTube Music, podcasts, games, browser) using Android's official `AudioPlaybackCapture` API and streams raw, uncompressed 16-bit 44.1 kHz Stereo PCM audio over low-latency UDP to the ESP32-C3 WiFi-HiFi receiver.

```
+-------------------------------------------------------------------+
|                         Android 16+ Phone                         |
|                                                                   |
| [ Media Player / Game / App ]                                     |
|           |                                                       |
|           v (AudioPlaybackCapture API)                            |
| [ AudioCaptureManager ]                                           |
|           |                                                       |
|           v (Continuous PCM 44.1kHz 16-bit Stereo)                |
| [ PcmPacketizer ]                                                 |
|    - Prepends 28-byte WiFi-HiFi binary header                     |
|    - Assigns sequence numbers, frame position, timestamp          |
|    - 10ms frame chunks (1764 bytes payload + 28 bytes header)     |
|           |                                                       |
|           v (UDP DatagramSocket)                                  |
| [ UdpAudioSender ] --------(Wi-Fi LAN)--------> [ ESP32-C3 ]      |
+-------------------------------------------------------------------+
```

---

## 2. Requirements & Permissions

- **Operating System**: Android 16+ (API Level 36+).
- **Foreground Service**: Runs `AudioStreamService` with `foregroundServiceType="mediaProjection"` so playback is uninterrupted when the phone is locked.
- **Power & Network**: Holds `PARTIAL_WAKE_LOCK` and `WIFI_MODE_FULL_HIGH_PERF` to maintain low Wi-Fi jitter.
- **MediaProjection**: Prompts user consent once before streaming begins.
- **Audio Capture Restrictions**: Respects Android system security policy: applications explicitly marking their playback as non-capturable (`ALLOW_CAPTURE_BY_NONE` or DRM/protected media) cannot be captured; WiFi-HiFi displays a clear descriptive status banner instead of failing silently.

---

## 3. Features

- **Automatic Receiver Discovery**: Discovers ESP32-C3 receivers on the LAN via mDNS (`_wifihifi._udp.`) and UDP broadcast on port 50006 (`WFHF_DISCOVER`).
- **One-Tap Streaming**: Tap the detected receiver name and press **Start Lossless Streaming**.
- **Remote Receiver Control**: Adjust hardware output volume and mute directly over HTTP REST API (`/api/audio/volume` and `/api/audio/mute`).
- **Jitter Buffer Latency Control**: Switch receiver buffering profiles in real time between **Low Latency** (35ms), **Balanced** (70ms), and **Stable** (120ms).
- **Live Stream Telemetry**: Real-time display of transmission bitrate, packets sent, receiver buffer fullness percentage, and packet loss statistics.

---

## 4. How to Build

### Using Android Studio
1. Open the `/android` directory in **Android Studio Meerkat / Ladybug** or newer.
2. Ensure JDK 17 is configured in **Project Structure > SDK Location > Gradle Settings**.
3. Sync Gradle and click **Run 'app'** or **Build > Build Bundle(s) / APK(s) > Build APK(s)**.

### Using Command Line
```bash
cd android
./gradlew assembleDebug
```
The resulting debug APK will be generated at:
`android/app/build/outputs/apk/debug/app-debug.apk`

To build the release APK:
```bash
./gradlew assembleRelease
```
