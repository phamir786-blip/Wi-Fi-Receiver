/**
 * @file discovery.cpp
 * @brief mDNS and UDP Broadcast discovery responder for WiFi-HiFi.
 */

#include "discovery.h"
#include "wifi_manager.h"
#include "settings.h"
#include "pcm_receiver.h"
#include "audio_output.h"
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <esp_log.h>

static const char* TAG = "[DISCOVERY]";

DiscoveryService g_discovery;

DiscoveryService::DiscoveryService()
    : running_(false),
      lastBeaconMs_(0) {}

DiscoveryService::~DiscoveryService() {
    stop();
}

bool DiscoveryService::begin() {
    String hostname = g_settings.getHostname();
    String devName = g_settings.getDeviceName();

    // 1. Initialize mDNS
    if (!MDNS.begin(hostname.c_str())) {
        ESP_LOGE(TAG, "Error starting mDNS responder for %s.local", hostname.c_str());
    } else {
        MDNS.addService("wifihifi", "udp", NETWORK_AUDIO_UDP_PORT);
        MDNS.addServiceTxt("wifihifi", "udp", "model", "ESP32-C3");
        MDNS.addServiceTxt("wifihifi", "udp", "dac", "UDA1334A");
        MDNS.addServiceTxt("wifihifi", "udp", "proto", "1.0");
        MDNS.addServiceTxt("wifihifi", "udp", "rates", "44100,48000");

        MDNS.addService("http", "tcp", NETWORK_HTTP_PORT);
        MDNS.setInstanceName(devName.c_str());

        ESP_LOGI(TAG, "mDNS active: %s.local (_wifihifi._udp: %u, _http._tcp: %u)",
                 hostname.c_str(), NETWORK_AUDIO_UDP_PORT, NETWORK_HTTP_PORT);
    }

    // 2. Initialize UDP Discovery Socket on port 50006
    if (udp_.begin(NETWORK_DISCOVERY_UDP_PORT)) {
        running_ = true;
        ESP_LOGI(TAG, "UDP Discovery responder active on port %u", NETWORK_DISCOVERY_UDP_PORT);
        return true;
    } else {
        ESP_LOGE(TAG, "Failed to bind UDP discovery port %u", NETWORK_DISCOVERY_UDP_PORT);
        return false;
    }
}

void DiscoveryService::update() {
    if (!running_) return;

    // Check for incoming discovery requests
    int packetSize = udp_.parsePacket();
    if (packetSize > 0) {
        handleUdpPacket();
    }
}

void DiscoveryService::handleUdpPacket() {
    char buffer[256];
    int len = udp_.read(buffer, sizeof(buffer) - 1);
    if (len <= 0) return;
    buffer[len] = '\0';

    // Check for discovery query magic
    if (strstr(buffer, "WFHF_DISCOVER") != nullptr || strstr(buffer, "DISCOVER") != nullptr) {
        IPAddress remoteIp = udp_.remoteIP();
        uint16_t remotePort = udp_.remotePort();
        ESP_LOGI(TAG, "Discovery query from %s:%u", remoteIp.toString().c_str(), remotePort);
        sendDiscoveryResponse(remoteIp, remotePort);
    }
}

void DiscoveryService::sendDiscoveryResponse(IPAddress remoteIp, uint16_t remotePort) {
    JsonDocument doc;
    doc["magic"] = "WFHF_BEACON";
    doc["version"] = WFHF_PROTOCOL_VERSION;
    doc["device_name"] = g_settings.getDeviceName();
    doc["hostname"] = g_settings.getHostname() + ".local";
    doc["ip"] = g_wifi_manager.getIpAddress().toString();
    doc["audio_port"] = NETWORK_AUDIO_UDP_PORT;
    doc["http_port"] = NETWORK_HTTP_PORT;

    JsonArray rates = doc["supported_sample_rates"].to<JsonArray>();
    rates.add(44100);
    rates.add(48000);

    JsonArray channels = doc["supported_channels"].to<JsonArray>();
    channels.add(2);

    JsonArray bitDepths = doc["supported_bit_depths"].to<JsonArray>();
    bitDepths.add(16);

    doc["firmware_version"] = WIFI_HIFI_FIRMWARE_VERSION;
    doc["hardware"] = "ESP32-C3";
    doc["dac"] = "UDA1334A";
    doc["latency_mode"] = (g_settings.getLatencyMode() == WFHF_LATENCY_LOW) ? "LOW_LATENCY" :
                          (g_settings.getLatencyMode() == WFHF_LATENCY_STABLE) ? "STABLE" : "BALANCED";
    doc["streaming"] = g_pcm_receiver.isStreaming();

    String response;
    serializeJson(doc, response);

    udp_.beginPacket(remoteIp, remotePort);
    udp_.write((const uint8_t*)response.c_str(), response.length());
    udp_.endPacket();
}

void DiscoveryService::stop() {
    running_ = false;
    udp_.stop();
}
