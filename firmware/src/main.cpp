/**
 * @file main.cpp
 * @brief WiFi-HiFi ESP32-C3 Firmware Entry Point
 * 
 * Hardware:
 *   - MCU: Espressif ESP32-C3 RISC-V (160MHz, 4MB Flash)
 *   - DAC: NXP UDA1334A Stereo I2S Audio DAC
 * 
 * Audio Pipeline:
 *   Android 16+ Phone -> Wi-Fi -> UDP 50005 -> Packet Validation ->
 *   Jitter Buffer -> FreeRTOS I2S Task -> UDA1334A -> Amp -> Speakers
 */

#include <Arduino.h>
#include <esp_log.h>

#include "config.h"
#include "protocol_defs.h"
#include "settings.h"
#include "pcm_buffer.h"
#include "audio_output.h"
#include "pcm_receiver.h"
#include "wifi_manager.h"
#include "discovery.h"
#include "web_server.h"
#include "system_manager.h"

static const char* TAG = "[MAIN]";

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\n==================================================");
    Serial.printf("  WiFi-HiFi ESP32-C3 Audio Receiver v%s\n", WIFI_HIFI_FIRMWARE_VERSION);
    Serial.println("  Target DAC: NXP UDA1334A (I2S Standard Mode)");
    Serial.printf("  I2S Pins: BCLK=GPIO%d, WS=GPIO%d, DOUT=GPIO%d\n", 
                  I2S_BCLK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    Serial.println("==================================================");

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "  WiFi-HiFi ESP32-C3 Audio Receiver v%s", WIFI_HIFI_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "  Target DAC: NXP UDA1334A (I2S Standard Mode)");
    ESP_LOGI(TAG, "  I2S Pins: BCLK=GPIO%d, WS=GPIO%d, DOUT=GPIO%d", 
             I2S_BCLK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);
    ESP_LOGI(TAG, "==================================================");

    // 1. Initialize NVS persistent settings
    if (!g_settings.begin()) {
        ESP_LOGE(TAG, "[SETTINGS] Failed to initialize NVS!");
    }

    // 2. Initialize System Supervisor & watchdog
    g_system_manager.init();

    // 3. Initialize PCM Ring/Jitter Buffer
    if (!g_pcm_buffer.init(PCM_RING_BUFFER_SIZE)) {
        ESP_LOGE(TAG, "[AUDIO] Failed to allocate PCM ring buffer!");
    }

    // 4. Restore Latency Mode and Volume preferences
    WfhfLatencyMode savedLatency = g_settings.getLatencyMode();
    g_pcm_buffer.setLatencyMode(savedLatency);

    uint8_t savedVol = g_settings.getVolume();
    bool savedMute = g_settings.getMute();

    // 5. Initialize I2S Audio Output Driver (NXP UDA1334A)
    if (!g_audio_output.init(AUDIO_DEFAULT_SAMPLE_RATE, AUDIO_DEFAULT_BITS_PER_SAMPLE, AUDIO_DEFAULT_CHANNELS)) {
        ESP_LOGE(TAG, "[I2S] Failed to initialize I2S interface!");
    }
    g_audio_output.setVolume(savedVol);
    g_audio_output.setMute(savedMute);

    // 6. Initialize Wi-Fi (Station or Fallback AP)
    g_wifi_manager.init();

    // 7. Initialize UDP PCM Receiver (Port 50005)
    if (!g_pcm_receiver.init(NETWORK_AUDIO_UDP_PORT)) {
        ESP_LOGE(TAG, "[PCM] Failed to start UDP receiver!");
    }

    // 8. Initialize mDNS and UDP Discovery Responder (Port 50006)
    g_discovery.begin();

    // 9. Initialize Web Server and REST API (Port 80)
    g_web_server.begin(NETWORK_HTTP_PORT);

    ESP_LOGI(TAG, "[SYSTEM] WiFi-HiFi Receiver startup complete. Ready for audio stream.");
}

void loop() {
    // Non-blocking background management loops
    g_wifi_manager.update();
    g_discovery.update();
    g_system_manager.update();

    delay(10);
}
