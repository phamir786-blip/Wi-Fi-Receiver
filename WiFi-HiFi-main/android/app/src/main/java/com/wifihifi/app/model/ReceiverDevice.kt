package com.wifihifi.app.model

data class ReceiverDevice(
    val deviceName: String,
    val hostname: String,
    val ipAddress: String,
    val audioPort: Int = 50005,
    val httpPort: Int = 80,
    val sampleRates: List<Int> = listOf(44100),
    val channels: Int = 2,
    val bitDepths: List<Int> = listOf(16),
    val firmwareVersion: String = "1.0.0",
    val hardware: String = "ESP32-C3",
    val dac: String = "UDA1334A",
    val latencyMode: String = "BALANCED",
    val isStreaming: Boolean = false,
    val lastSeenMs: Long = System.currentTimeMillis()
)
