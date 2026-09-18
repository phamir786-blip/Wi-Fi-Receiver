/**
 * @file web_server.cpp
 * @brief Implementation of REST API and embedded web interface.
 */

#include "web_server.h"
#include "web_assets.h"
#include "settings.h"
#include "wifi_manager.h"
#include "audio_output.h"
#include "pcm_buffer.h"
#include "pcm_receiver.h"
#include "ota_manager.h"
#include "system_manager.h"
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "[WEB]";

WebServerManager g_web_server;

WebServerManager::WebServerManager()
    : server_(NETWORK_HTTP_PORT),
      running_(false) {}

WebServerManager::~WebServerManager() {
    stop();
}

bool WebServerManager::begin(uint16_t port) {
    registerRoutes();
    registerApiRoutes();
    server_.begin();
    running_ = true;
    ESP_LOGI(TAG, "HTTP Web Server and REST API active on port %u", port);
    return true;
}

void WebServerManager::stop() {
    if (running_) {
        server_.end();
        running_ = false;
    }
}

void WebServerManager::registerRoutes() {
    // Serve embedded Hi-Fi Web UI
    server_.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "text/html; charset=utf-8", INDEX_HTML);
    });

    // Captive Portal Detection redirects for first-boot provisioning
    server_.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
    server_.on("/fwlink", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->redirect("/");
    });
}

void WebServerManager::registerApiRoutes() {
    // 1. GET /api/status - Complete real-time snapshot
    server_.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        wfhf_audio_stats_t stats;
        g_pcm_receiver.getStats(&stats);

        JsonDocument doc;
        doc["streaming"] = stats.is_streaming;
        doc["sender"] = stats.connected_sender_ip;
        doc["sample_rate"] = stats.sample_rate;
        doc["channels"] = stats.channels;
        doc["bits_per_sample"] = stats.bits_per_sample;
        doc["latency_mode"] = (stats.latency_mode == WFHF_LATENCY_LOW) ? "LOW_LATENCY" :
                              (stats.latency_mode == WFHF_LATENCY_STABLE) ? "STABLE" : "BALANCED";
        doc["buffer_bytes"] = stats.current_buffer_bytes;
        doc["buffer_capacity"] = stats.buffer_capacity_bytes;
        doc["buffer_fill_pct"] = stats.buffer_fill_pct;
        
        doc["volume"] = g_audio_output.getVolume();
        doc["muted"] = g_audio_output.isMuted();

        doc["ssid"] = g_wifi_manager.getSsid();
        doc["rssi"] = g_wifi_manager.getRssi();
        doc["ip"] = g_wifi_manager.getIpAddress().toString();
        doc["mac"] = g_wifi_manager.getMacAddress();

        doc["packets_received"] = stats.packets_received;
        doc["packets_lost"] = stats.packets_lost;
        doc["packet_loss_pct"] = stats.packet_loss_pct;
        doc["packets_out_of_order"] = stats.packets_out_of_order;
        doc["packets_duplicate"] = stats.packets_duplicate;
        doc["buffer_underruns"] = stats.buffer_underruns;
        doc["buffer_overruns"] = stats.buffer_overruns;

        doc["free_heap"] = g_system_manager.getFreeHeap();
        doc["uptime_s"] = g_system_manager.getUptimeSeconds();
        doc["version"] = WIFI_HIFI_FIRMWARE_VERSION;

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // 2. GET /api/audio/status
    server_.on("/api/audio/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["sample_rate"] = g_audio_output.getSampleRate();
        doc["channels"] = g_audio_output.getChannels();
        doc["bits_per_sample"] = g_audio_output.getBitsPerSample();
        doc["volume"] = g_audio_output.getVolume();
        doc["muted"] = g_audio_output.isMuted();
        doc["dac"] = "UDA1334A";
        doc["i2s_bclk"] = I2S_BCLK_PIN;
        doc["i2s_ws"] = I2S_WS_PIN;
        doc["i2s_dout"] = I2S_DOUT_PIN;

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // 3. GET /api/audio/stats
    server_.on("/api/audio/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
        wfhf_audio_stats_t stats;
        g_pcm_receiver.getStats(&stats);

        JsonDocument doc;
        doc["packets_received"] = stats.packets_received;
        doc["packets_lost"] = stats.packets_lost;
        doc["packet_loss_pct"] = stats.packet_loss_pct;
        doc["packets_out_of_order"] = stats.packets_out_of_order;
        doc["packets_duplicate"] = stats.packets_duplicate;
        doc["packets_malformed"] = stats.packets_malformed;
        doc["buffer_underruns"] = stats.buffer_underruns;
        doc["buffer_overruns"] = stats.buffer_overruns;
        doc["buffer_bytes"] = stats.current_buffer_bytes;
        doc["buffer_capacity"] = stats.buffer_capacity_bytes;
        doc["buffer_fill_pct"] = stats.buffer_fill_pct;

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // 4. POST /api/audio/volume
    server_.on("/api/audio/volume", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (!err && doc.containsKey("volume")) {
                int vol = doc["volume"];
                if (vol >= 0 && vol <= 100) {
                    g_audio_output.setVolume((uint8_t)vol);
                    g_settings.setVolume((uint8_t)vol);
                    request->send(200, "application/json", "{\"status\":\"ok\"}");
                    return;
                }
            }
            request->send(400, "application/json", "{\"error\":\"Invalid volume value (0-100)\"}");
        });

    // 5. POST /api/audio/mute
    server_.on("/api/audio/mute", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (!err && doc.containsKey("mute")) {
                bool mute = doc["mute"];
                g_audio_output.setMute(mute);
                g_settings.setMute(mute);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid mute boolean\"}");
        });

    // 6. POST /api/audio/latency
    server_.on("/api/audio/latency", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (!err && doc.containsKey("mode")) {
                String modeStr = doc["mode"];
                WfhfLatencyMode mode = WFHF_LATENCY_BALANCED;
                if (modeStr == "LOW_LATENCY") mode = WFHF_LATENCY_LOW;
                else if (modeStr == "STABLE") mode = WFHF_LATENCY_STABLE;

                g_pcm_buffer.setLatencyMode(mode);
                g_settings.setLatencyMode(mode);
                request->send(200, "application/json", "{\"status\":\"ok\"}");
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Invalid latency mode\"}");
        });

    // 7. GET /api/network/status
    server_.on("/api/network/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["state"] = g_wifi_manager.isApMode() ? "AP_PROVISIONING" :
                       g_wifi_manager.isConnected() ? "CONNECTED" : "DISCONNECTED";
        doc["ssid"] = g_wifi_manager.getSsid();
        doc["rssi"] = g_wifi_manager.getRssi();
        doc["ip"] = g_wifi_manager.getIpAddress().toString();
        doc["mac"] = g_wifi_manager.getMacAddress();
        doc["hostname"] = g_wifi_manager.getHostname() + ".local";

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // 8. GET /api/device/info
    server_.on("/api/device/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_name"] = g_settings.getDeviceName();
        doc["hardware"] = "ESP32-C3 RISC-V";
        doc["dac"] = "NXP UDA1334A";
        doc["firmware_version"] = WIFI_HIFI_FIRMWARE_VERSION;
        doc["free_heap"] = g_system_manager.getFreeHeap();
        doc["uptime_seconds"] = g_system_manager.getUptimeSeconds();
        doc["mac"] = g_wifi_manager.getMacAddress();

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // 9. GET /api/settings
    server_.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
        JsonDocument doc;
        doc["device_name"] = g_settings.getDeviceName();
        doc["hostname"] = g_settings.getHostname();
        doc["volume"] = g_settings.getVolume();
        doc["muted"] = g_settings.getMute();
        doc["latency_mode"] = (g_settings.getLatencyMode() == WFHF_LATENCY_LOW) ? "LOW_LATENCY" :
                              (g_settings.getLatencyMode() == WFHF_LATENCY_STABLE) ? "STABLE" : "BALANCED";
        doc["wifi_configured"] = g_settings.hasWifiCredentials();
        doc["wifi_ssid"] = g_settings.getWifiSsid();
        // Password intentionally omitted for security

        String out;
        serializeJson(doc, out);
        request->send(200, "application/json", out);
    });

    // 10. POST /api/wifi/config
    server_.on("/api/wifi/config", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError err = deserializeJson(doc, data, len);
            if (!err && doc.containsKey("ssid") && doc.containsKey("password")) {
                String ssid = doc["ssid"];
                String pass = doc["password"];
                g_settings.setWifiCredentials(ssid, pass);
                request->send(200, "application/json", "{\"status\":\"saved\",\"message\":\"Reconnecting...\"}");
                g_wifi_manager.connectStation(ssid, pass);
                return;
            }
            request->send(400, "application/json", "{\"error\":\"Missing ssid or password\"}");
        });

    // 11. POST /api/system/reboot
    server_.on("/api/system/reboot", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"rebooting\"}");
        g_system_manager.requestReboot(500);
    });

    // 12. POST /api/system/reset
    server_.on("/api/system/reset", HTTP_POST, [](AsyncWebServerRequest *request) {
        request->send(200, "application/json", "{\"status\":\"factory_resetting\"}");
        g_system_manager.requestFactoryReset();
    });

    // 13. POST /api/ota - Firmware binary upload
    server_.on("/api/ota", HTTP_POST, [](AsyncWebServerRequest *request) {
        bool ok = !Update.hasError();
        AsyncWebServerResponse *response = request->beginResponse(ok ? 200 : 500, "text/plain", 
                                                                  ok ? "OTA SUCCESS" : g_ota_manager.getLastError());
        response->addHeader("Connection", "close");
        request->send(response);
        if (ok) {
            g_system_manager.requestReboot(1000);
        }
    }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!index) {
            g_ota_manager.beginUpdate(request->contentLength());
        }
        if (len) {
            g_ota_manager.writeChunk(data, len);
        }
        if (final) {
            g_ota_manager.finishUpdate();
        }
    });
}
