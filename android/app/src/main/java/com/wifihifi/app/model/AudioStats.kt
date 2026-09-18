package com.wifihifi.app.model

data class AudioStats(
    val packetsSent: Long = 0L,
    val bytesSent: Long = 0L,
    val bitrateKbps: Double = 0.0,
    val captureFps: Double = 0.0,
    val receiverLossPct: Double = 0.0,
    val receiverBufferPct: Double = 0.0,
    val receiverUnderruns: Int = 0,
    val receiverOverruns: Int = 0
)
