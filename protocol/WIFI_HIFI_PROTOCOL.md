# WiFi-HiFi Binary Protocol Specification

**Document Version:** 1.0.0  
**Protocol Version:** 0x01  
**Project:** WiFi-HiFi (anaya2025/WiFi-HiFi)  
**Target:** Lossless Wi-Fi PCM Audio Streaming from Android 16+ to ESP32-C3 (UDA1334A I2S DAC)

---

## 1. Architectural Overview

The WiFi-HiFi protocol provides low-latency, deterministic, uncompressed PCM audio streaming over a local Wi-Fi 802.11 b/g/n network. 

```
+------------------------------------+
| Android 16+ Sender (Phone)         |
| - AudioPlaybackCapture (PCM 16-bit)|
| - Resampler (44.1 kHz Stereo)      |
| - WiFi-HiFi Packetizer             |
+-----------------+------------------+
                  | UDP Port 50005 (Audio)
                  | UDP Port 50006 (Discovery)
                  | TCP Port 80    (HTTP / REST API)
                  v
+-----------------+------------------+
| ESP32-C3 Receiver                  |
| - UDP Packet Validator             |
| - Sequence & Jitter Ring Buffer    |
| - Real-time I2S Output Task        |
| - NXP UDA1334A Stereo DAC          |
+------------------------------------+
```

The system relies on three network channels:
1. **Audio Data Channel (UDP 50005)**: Unidirectional, high-throughput, low-latency binary PCM packet stream.
2. **Discovery Channel (UDP 50006 & mDNS)**: Broadcast discovery requests and unicast service announcements.
3. **Control & Web API (TCP 80)**: RESTful JSON API for volume, mute, latency modes, Wi-Fi configuration, and OTA firmware updates.

---

## 2. Audio Data Packet Layout (UDP Port 50005)

Every audio packet consists of a **28-byte fixed-size binary header** in Little-Endian byte order, immediately followed by the raw interleaved PCM payload.

### 2.1 Header Binary Layout

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       'W'     |      'F'      |      'H'      |      'F'      |  Magic (0x46484657)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Proto Version |  Packet Type  |           Stream ID           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                        Sequence Number                        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
+                    Timestamp / Frame Position                 +
|                            (uint64_t)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                          Sample Rate                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Channels    |Bits per Sample| Latency Mode  |   Reserved    |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Payload Length        |           Checksum            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                   Raw PCM Audio Payload ...                   |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

### 2.2 Field Definitions

| Offset (Bytes) | Field Name | Type | Description |
| :--- | :--- | :--- | :--- |
| 0–3 | `magic` | `uint32_t` / `char[4]` | Must equal `"WFHF"` (`0x57, 0x46, 0x48, 0x46`). Packets with invalid magic are immediately discarded. |
| 4 | `version` | `uint8_t` | Protocol version (`0x01`). |
| 5 | `packet_type` | `uint8_t` | `0x01`: Audio PCM Data<br>`0x02`: Stream Start / Sync<br>`0x03`: Stream Stop<br>`0x04`: Ping / Keep-Alive |
| 6–7 | `stream_id` | `uint16_t` | Random identifier generated when Android begins streaming. Allows receiver to detect when a new stream starts or when a sender reconnected. |
| 8–11 | `sequence` | `uint32_t` | Monotonically increasing sequence number (0, 1, 2, ...). Used for detecting packet loss, out-of-order delivery, and duplicates. |
| 12–19 | `frame_position`| `uint64_t` | Total cumulative audio frames transmitted since stream inception (1 frame = 1 sample per channel). Used for drift detection and jitter buffer synchronization. |
| 20–23 | `sample_rate` | `uint32_t` | Audio sample rate in Hertz. Milestone 1: `44100`. Future: `48000`. |
| 24 | `channels` | `uint8_t` | Number of audio channels. Must be `2` (Stereo, Left and Right). |
| 25 | `bits_per_sample`| `uint8_t` | Audio resolution. Must be `16` (16-bit Signed Integer). Future: `24`. |
| 26 | `latency_mode` | `uint8_t` | `0x00`: LOW LATENCY (~30ms buffer target)<br>`0x01`: BALANCED (~100ms buffer target)<br>`0x02`: STABLE (~250ms buffer target) |
| 27 | `reserved` | `uint8_t` | Reserved for alignment and future extensions. Must be `0x00`. |
| 28–29 | `payload_length`| `uint16_t` | Number of bytes of audio PCM in this packet. Typically `1764` bytes (10ms at 44.1kHz 16-bit Stereo) or `1024` bytes. |
| 30–31 | `checksum` | `uint16_t` | CRC-16 (CCITT) or standard Internet 16-bit ones-complement sum over the header and payload. Set to `0x0000` if verification is disabled by sender. |

### 2.3 Audio Payload Format
- **Encoding**: Linear Signed PCM (`int16_t`), Little-Endian.
- **Channel Interleaving**: Left Channel Sample (`int16_t`), Right Channel Sample (`int16_t`).
- **Frame Size**: 2 channels × 2 bytes = 4 bytes per audio frame.
- **Packet Duration**: 
  - For 1764 bytes payload: $1764 / 4 = 441$ audio frames. At 44,100 Hz, $441 / 44100 = 10.0$ milliseconds of audio per packet.
  - For 882 bytes payload: $882 / 4 = 220.5$ frames (or 880 bytes / 220 frames ≈ 4.98ms).

---

## 3. Discovery Protocol (UDP Port 50006)

Both mDNS and UDP Broadcast discovery are supported simultaneously. This ensures immediate discovery even on home Wi-Fi routers that block multicast DNS or IGMP snooping.

### 3.1 UDP Broadcast Discovery

#### Discovery Request (Android -> 255.255.255.255:50006)
JSON payload over UDP:
```json
{
  "magic": "WFHF_DISCOVER",
  "client": "Android-16",
  "version": 1
}
```

#### Discovery Response (ESP32 -> Sender Unicast:50006)
JSON payload over UDP:
```json
{
  "magic": "WFHF_BEACON",
  "version": 1,
  "device_name": "WiFi-HiFi",
  "hostname": "wifi-hifi.local",
  "ip": "192.168.1.145",
  "audio_port": 50005,
  "http_port": 80,
  "supported_sample_rates": [44100, 48000],
  "supported_channels": [2],
  "supported_bit_depths": [16],
  "firmware_version": "1.0.0",
  "hardware": "ESP32-C3",
  "dac": "UDA1334A",
  "latency_mode": "BALANCED",
  "streaming": false
}
```

### 3.2 mDNS Service Discovery
The receiver registers:
- Service type: `_wifihifi._udp.local.` (Port 50005)
- Web interface: `_http._tcp.local.` (Port 80)
- Hostname: `wifi-hifi.local`
- TXT Records:
  - `model=ESP32-C3`
  - `dac=UDA1334A`
  - `proto=1.0`
  - `rates=44100,48000`

---

## 4. Jitter Buffer Architecture & Latency Modes

The ESP32-C3 real-time audio pipeline requires a deterministic jitter buffer to accommodate packet delivery delay variance over Wi-Fi without blocking I2S output.

### 4.1 Ring Buffer Sizing
Total internal pre-allocated ring buffer: **32,768 bytes** (~185 milliseconds of 16-bit 44.1kHz Stereo audio).

### 4.2 Latency Modes

| Mode | ID | Target Buffer Level | Minimum Play Threshold | Wi-Fi Environment |
| :--- | :--- | :--- | :--- | :--- |
| **LOW LATENCY** | `0` | 5,292 bytes (~30 ms) | 3,528 bytes (~20 ms) | Dedicated 5GHz / low-traffic 2.4GHz Wi-Fi |
| **BALANCED** | `1` | 17,640 bytes (~100 ms) | 12,348 bytes (~70 ms) | Normal residential Wi-Fi network (Default) |
| **STABLE** | `2` | 28,224 bytes (~160 ms) | 21,168 bytes (~120 ms) | Congested 2.4GHz Wi-Fi / weak signal |

### 4.3 Sequence & Underrun Handling
1. **Initial Pre-roll**: When audio starts (`packet_type = 0x02` or stream idle), the receiver buffers incoming packets until the Minimum Play Threshold is satisfied before starting I2S DMA transmission.
2. **Packet Gap**: If a sequence number gap of 1 packet occurs, the receiver pauses briefly to allow out-of-order arrival. If absent by read time, it synthesizes smooth zero-crossing concealment or brief silence frames without desyncing the sample clock.
3. **Underrun Recovery**: On true buffer underrun (buffer empty), the I2S task feeds silence to the UDA1334A to avoid clicks, enters pre-roll state, and resumes once the buffer refills.
4. **Overrun Avoidance**: If buffer fullness exceeds 90%, the receiver skips or fast-tracks older unread audio frames to catch up to the live stream.

---

## 5. Control & REST API (HTTP Port 80)

All REST endpoints return `Content-Type: application/json`.

- `GET /api/status`: Complete system status, streaming state, buffer fill, volume, Wi-Fi.
- `GET /api/audio/status`: Current audio format, sample rate, bit depth, channels, DAC status.
- `GET /api/audio/stats`: Real-time packet statistics (received, lost, drop rate, underruns, overruns).
- `POST /api/audio/volume`: Set volume (`{"volume": 0-100}`).
- `POST /api/audio/mute`: Set mute (`{"mute": true|false}`).
- `POST /api/audio/latency`: Set latency mode (`{"mode": "LOW_LATENCY"|"BALANCED"|"STABLE"}`).
- `GET /api/network/status`: Wi-Fi SSID, RSSI, IP, MAC address, mDNS status.
- `POST /api/wifi/config`: Configure Wi-Fi station credentials (`{"ssid": "...", "password": "..."}`).
- `GET /api/device/info`: Firmware version, hardware chip model, free heap, uptime.
- `POST /api/system/reboot`: Safely restart the ESP32.
- `POST /api/system/reset`: Reset Wi-Fi credentials and return to AP provisioning mode.
- `POST /api/ota`: Multipart firmware binary upload (`firmware.bin`) for Over-The-Air updates.
