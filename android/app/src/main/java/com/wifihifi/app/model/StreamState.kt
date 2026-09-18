package com.wifihifi.app.model

enum class StreamStatus {
    IDLE,
    DISCOVERING,
    CONNECTING,
    STREAMING,
    ERROR
}

data class StreamState(
    val status: StreamStatus = StreamStatus.IDLE,
    val connectedReceiver: ReceiverDevice? = null,
    val errorMessage: String? = null,
    val isMuted: Boolean = false,
    val volume: Int = 85,
    val latencyMode: String = "BALANCED"
)
