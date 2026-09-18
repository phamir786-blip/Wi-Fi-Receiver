/**
 * @file main.cpp
 * @brief WiFi-HiFi ESP32-C3 Firmware Entry Point
 *
 * Wi-Fi is initialized before the audio stack so the provisioning AP is
 * established as early as possible and cannot be blocked by another subsystem.
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

    ESP_LOGI(TAG, "WiFi-HiFi ESP32-C3 Audio Receiver v%s", WIFI_HIFI_FIRMWARE_VERSION);

    // 1. Initialize persistent settings.
    if (!g_settings.begin()) {
        ESP_LOGE(TAG, "[SETTINGS] Failed to initialize NVS!");
    }

    // 2. Start Wi-Fi BEFORE the audio stack. The provisioning AP must not
    // depend on I2S, PCM buffers, discovery, or the web UI.
    ESP_LOGI(TAG, "[BOOT] Starting Wi-Fi first...");
    if (!g_wifi_manager.init()) {
        ESP_LOGE(TAG, "[WIFI] Initial Wi-Fi initialization reported failure!");
    }

    // 3. Initialize system supervisor.
    g_system_manager.init();

    // 4. Initialize PCM ring/jitter buffer.
    if (!g_pcm_buffer.init(PCM_RING_BUFFER_SIZE)) {
        ESP_LOGE(TAG, "[AUDIO] Failed to allocate PCM ring buffer!");
    }

    // 5. Restore latency/volume/mute preferences.
    g_pcm_buffer.setLatencyMode(g_settings.getLatencyMode());
    g_audio_output.setVolume(g_settings.getVolume());
    g_audio_output.setMute(g_settings.getMute());

    // 6. Initialize I2S audio output.
    if (!g_audio_output.init(AUDIO_DEFAULT_SAMPLE_RATE,
                             AUDIO_DEFAULT_BITS_PER_SAMPLE,
                             AUDIO_DEFAULT_CHANNELS)) {
        ESP_LOGE(TAG, "[I2S] Failed to initialize I2S interface!");
    }
    g_audio_output.setVolume(g_settings.getVolume());
    g_audio_output.setMute(g_settings.getMute());

    // 7. Initialize UDP PCM receiver.
    if (!g_pcm_receiver.init(NETWORK_AUDIO_UDP_PORT)) {
        ESP_LOGE(TAG, "[PCM] Failed to start UDP receiver!");
    }

    // 8. Initialize discovery.
    if (!g_discovery.begin()) {
        ESP_LOGE(TAG, "[DISCOVERY] Discovery initialization failed!");
    }

    // 9. Initialize HTTP server.
    if (!g_web_server.begin(NETWORK_HTTP_PORT)) {
        ESP_LOGE(TAG, "[WEB] Web server initialization failed!");
    }

    ESP_LOGI(TAG, "[SYSTEM] Startup complete.");
    ESP_LOGI(TAG, "[SYSTEM] AP target: SSID='%s' IP=%s",
             AP_FALLBACK_SSID, AP_FALLBACK_IP.toString().c_str());
}

void loop() {
    g_wifi_manager.update();
    g_discovery.update();
    g_system_manager.update();
    delay(10);
}
