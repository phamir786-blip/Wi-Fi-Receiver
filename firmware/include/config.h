/**
 * @file config.h
 * @brief Centralized Hardware & System Configuration for WiFi-HiFi ESP32-C3
 * 
 * Target: ESP32-C3 RISC-V (No PSRAM)
 * DAC: NXP UDA1334A (I2S Stereo Audio DAC)
 */

#pragma once

#include <stdint.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// =============================================================================
// FIRMWARE & PRODUCT IDENTIFICATION
// =============================================================================
#ifndef WIFI_HIFI_FIRMWARE_VERSION
#define WIFI_HIFI_FIRMWARE_VERSION "1.0.0"
#endif

#ifndef WIFI_HIFI_DEVICE_NAME
#define WIFI_HIFI_DEVICE_NAME "WiFi-HiFi"
#endif

#ifndef WIFI_HIFI_HOSTNAME
#define WIFI_HIFI_HOSTNAME "wifi-hifi"
#endif

// =============================================================================
// HARDWARE I2S PIN CONFIGURATION (ESP32-C3 -> NXP UDA1334A)
// Centralized pin definitions - modify here if custom board wiring differs.
// =============================================================================
#define I2S_PORT_NUM            (I2S_NUM_0)
#define I2S_BCLK_PIN            (4)   // Bit Clock (BCLK) -> UDA1334A BCLK
#define I2S_WS_PIN              (5)   // Word Select / LRCK -> UDA1334A WSEL
#define I2S_DOUT_PIN            (6)   // Data Out (Serial Data) -> UDA1334A DIN
// Note: UDA1334A has an internal PLL that derives master clock (MCLK) from WSEL/BCLK.
// Dedicated MCLK pin is not required on the ESP32-C3.

// =============================================================================
// ONBOARD STATUS LED (ESP32-C3 Super Mini Blue LED on GPIO 8)
// Active LOW: LOW = LED ON, HIGH = LED OFF
// =============================================================================
#define STATUS_LED_PIN          (8)
#define STATUS_LED_ACTIVE       (LOW)
#define STATUS_LED_INACTIVE     (HIGH)

// =============================================================================
// AUDIO SPECIFICATION (MILESTONE 1)
// =============================================================================
#define AUDIO_DEFAULT_SAMPLE_RATE    44100
#define AUDIO_DEFAULT_CHANNELS       2
#define AUDIO_DEFAULT_BITS_PER_SAMPLE 16
#define AUDIO_BYTES_PER_SAMPLE       2
#define AUDIO_BYTES_PER_FRAME        (AUDIO_DEFAULT_CHANNELS * AUDIO_BYTES_PER_SAMPLE) // 4 bytes

// =============================================================================
// NETWORK PORTS
// =============================================================================
#define NETWORK_AUDIO_UDP_PORT       50005
#define NETWORK_DISCOVERY_UDP_PORT   50006
#define NETWORK_HTTP_PORT            80

// =============================================================================
// JITTER BUFFER SIZING (Internal SRAM, Static Pre-allocation)
// ESP32-C3 has ~384KB usable SRAM. We allocate 32KB for uncompressed PCM.
// 32KB = 8,192 frames = ~185.7 ms of 44.1kHz 16-bit Stereo PCM.
// =============================================================================
#define PCM_RING_BUFFER_SIZE         (32 * 1024)

// Latency Mode Watermarks (Bytes of buffered PCM audio)
// LOW LATENCY: ~35 ms buffer
#define LATENCY_LOW_THRESHOLD_BYTES       (6 * 1024)   // ~35 ms
// BALANCED (Default): ~100 ms buffer
#define LATENCY_BALANCED_THRESHOLD_BYTES  (16 * 1024)  // ~93 ms
// STABLE: ~160 ms buffer
#define LATENCY_STABLE_THRESHOLD_BYTES    (26 * 1024)  // ~151 ms

// =============================================================================
// FREERTOS REAL-TIME TASK SETTINGS (ESP32-C3 Single-Core RISC-V)
// Balanced priorities prevent starving FreeRTOS IDLE0 (which feeds the Task Watchdog)
// and allow Wi-Fi/lwIP system events (priority ~18-20) to run cleanly.
// =============================================================================
#define TASK_AUDIO_PRIORITY          (5)
#define TASK_AUDIO_STACK_SIZE        (4096)

#define TASK_UDP_PRIORITY            (4)
#define TASK_UDP_STACK_SIZE          (4096)

#define TASK_SYSTEM_PRIORITY         (2)
#define TASK_SYSTEM_STACK_SIZE       (3072)

// =============================================================================
// FIRST-BOOT WI-FI AP FALLBACK
// =============================================================================
#define AP_FALLBACK_SSID             "WiFi-HiFi-Setup"
#define AP_FALLBACK_PASSWORD         "" // Open for easy first-time connection
#define AP_FALLBACK_IP               IPAddress(192, 168, 4, 1)
#define AP_FALLBACK_GATEWAY          IPAddress(192, 168, 4, 1)
#define AP_FALLBACK_SUBNET           IPAddress(255, 255, 255, 0)
