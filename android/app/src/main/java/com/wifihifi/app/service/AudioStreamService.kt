package com.wifihifi.app.service

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.media.projection.MediaProjection
import android.net.wifi.WifiManager
import android.os.Binder
import android.os.IBinder
import android.os.PowerManager
import android.os.Build
import android.util.Log
import androidx.core.app.NotificationCompat
import com.wifihifi.app.MainActivity
import com.wifihifi.app.R
import com.wifihifi.app.WiFiHiFiApp
import com.wifihifi.app.audio.AudioCaptureManager
import com.wifihifi.app.audio.PcmPacketizer
import com.wifihifi.app.model.AudioStats
import com.wifihifi.app.model.ReceiverDevice
import com.wifihifi.app.model.StreamState
import com.wifihifi.app.model.StreamStatus
import com.wifihifi.app.network.ReceiverController
import com.wifihifi.app.network.UdpAudioSender
import com.wifihifi.app.protocol.WiFiHiFiProtocol
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/**
 * Android Foreground Service maintaining continuous AudioPlaybackCapture and UDP audio transmission.
 */
class AudioStreamService : Service() {

    companion object {
        private const val TAG = "AudioStreamService"
        private const val NOTIFICATION_ID = 1001

        const val ACTION_START_STREAM = "com.wifihifi.app.ACTION_START"
        const val ACTION_STOP_STREAM = "com.wifihifi.app.ACTION_STOP"
        const val EXTRA_PROJECTION_RESULT_CODE = "projection_result_code"
        const val EXTRA_PROJECTION_DATA = "projection_data"
        const val EXTRA_RECEIVER_IP = "receiver_ip"
        const val EXTRA_RECEIVER_PORT = "receiver_port"
        const val EXTRA_RECEIVER_NAME = "receiver_name"
        const val EXTRA_RECEIVER_HOSTNAME = "receiver_hostname"
        const val EXTRA_RECEIVER_LATENCY = "receiver_latency"
    }

    private val binder = LocalBinder()
    private val serviceScope = CoroutineScope(Dispatchers.Default + Job())

    private var wakeLock: PowerManager.WakeLock? = null
    private var wifiLock: WifiManager.WifiLock? = null

    private var udpSender: UdpAudioSender? = null
    private var packetizer: PcmPacketizer? = null
    private var captureManager: AudioCaptureManager? = null
    private val controller = ReceiverController()

    private val _streamState = MutableStateFlow(StreamState())
    val streamState: StateFlow<StreamState> = _streamState.asStateFlow()

    private val _audioStats = MutableStateFlow(AudioStats())
    val audioStats: StateFlow<AudioStats> = _audioStats.asStateFlow()

    private var telemetryJob: Job? = null

    inner class LocalBinder : Binder() {
        fun getService(): AudioStreamService = this@AudioStreamService
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onCreate() {
        super.onCreate()
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP_STREAM -> stopStreaming()
            ACTION_START_STREAM -> startStreamingFromIntent(intent)
        }
        return START_NOT_STICKY
    }
    
    private fun startStreamingFromIntent(intent: Intent) {
        if (_streamState.value.status != StreamStatus.IDLE) return

        val resultCode = intent.getIntExtra(EXTRA_PROJECTION_RESULT_CODE, -1)
        val projectionData = intent.getParcelableExtra<Intent>(EXTRA_PROJECTION_DATA)
        val ip = intent.getStringExtra(EXTRA_RECEIVER_IP)
        if (resultCode != android.app.Activity.RESULT_OK || projectionData == null || ip.isNullOrBlank()) {
            Log.e(TAG, "Invalid streaming start intent")
            stopSelf()
            return
        }

        val receiver = ReceiverDevice(
            deviceName = intent.getStringExtra(EXTRA_RECEIVER_NAME) ?: "WiFi-HiFi",
            hostname = intent.getStringExtra(EXTRA_RECEIVER_HOSTNAME) ?: "wifi-hifi.local",
            ipAddress = ip,
            audioPort = intent.getIntExtra(EXTRA_RECEIVER_PORT, WiFiHiFiProtocol.DEFAULT_AUDIO_PORT),
            latencyMode = intent.getStringExtra(EXTRA_RECEIVER_LATENCY) ?: "BALANCED"
        )

        try {
            startForeground(NOTIFICATION_ID, createNotification("Connecting to ${receiver.deviceName}..."),
                android.content.pm.ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION)
            val mpm = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as android.media.projection.MediaProjectionManager
            val projection = mpm.getMediaProjection(resultCode, projectionData)
                ?: throw IllegalStateException("MediaProjection could not be created")
            startStreamingInternal(projection, receiver)
        } catch (e: Exception) {
            Log.e(TAG, "MediaProjection initialization failed", e)
            _streamState.value = StreamState(status = StreamStatus.ERROR, errorMessage = "Capture initialization failed: ${e.message}")
            stopForeground(STOP_FOREGROUND_REMOVE)
            stopSelf()
        }
    }

    fun startStreaming(mediaProjection: MediaProjection, receiver: ReceiverDevice) {
        if (_streamState.value.status != StreamStatus.IDLE) return
        startStreamingInternal(mediaProjection, receiver)
    }

    private fun startStreamingInternal(mediaProjection: MediaProjection, receiver: ReceiverDevice) {
        _streamState.value = StreamState(
            status = StreamStatus.CONNECTING,
            connectedReceiver = receiver
        )
        acquireLocks()

        serviceScope.launch {
            // 1. Initialize UDP Audio Sender
            val sender = UdpAudioSender(receiver.ipAddress, receiver.audioPort)
            if (!sender.open()) {
                _streamState.value = _streamState.value.copy(
                    status = StreamStatus.ERROR,
                    errorMessage = "Failed to open UDP socket to ${receiver.ipAddress}:${receiver.audioPort}"
                )
                stopForeground(STOP_FOREGROUND_REMOVE)
                return@launch
            }
            udpSender = sender

            // 2. Initialize Packetizer
            val pkt = PcmPacketizer { buffer, length ->
                sender.sendPacket(buffer, length)
            }
            val modeByte = when (receiver.latencyMode) {
                "LOW_LATENCY" -> WiFiHiFiProtocol.LATENCY_LOW
                "STABLE" -> WiFiHiFiProtocol.LATENCY_STABLE
                else -> WiFiHiFiProtocol.LATENCY_BALANCED
            }
            pkt.startNewStream(modeByte)
            packetizer = pkt

            // 3. Initialize Audio Capture
            val capture = AudioCaptureManager(mediaProjection, pkt) { errMsg ->
                _streamState.value = _streamState.value.copy(
                    status = StreamStatus.ERROR,
                    errorMessage = errMsg
                )
                stopStreaming()
            }

            if (!capture.startCapture()) {
                stopStreaming()
                return@launch
            }
            captureManager = capture

            _streamState.value = _streamState.value.copy(
                status = StreamStatus.STREAMING,
                connectedReceiver = receiver
            )

            updateNotification("Streaming PCM 44.1kHz Stereo -> ${receiver.deviceName}")
            startTelemetryMonitoring(receiver.ipAddress)
        }
    }

    fun stopStreaming() {
        telemetryJob?.cancel()
        telemetryJob = null

        packetizer?.sendStreamStop()

        captureManager?.stopCapture()
        captureManager = null

        udpSender?.close()
        udpSender = null

        packetizer = null

        _streamState.value = _streamState.value.copy(
            status = StreamStatus.IDLE,
            connectedReceiver = null
        )

        releaseLocks()
        stopForeground(STOP_FOREGROUND_REMOVE)
        stopSelf()
    }

    fun setVolume(volume: Int) {
        val receiver = _streamState.value.connectedReceiver ?: return
        serviceScope.launch {
            controller.setVolume(receiver.ipAddress, volume)
            _streamState.value = _streamState.value.copy(volume = volume)
        }
    }

    fun setMute(mute: Boolean) {
        val receiver = _streamState.value.connectedReceiver ?: return
        serviceScope.launch {
            controller.setMute(receiver.ipAddress, mute)
            _streamState.value = _streamState.value.copy(isMuted = mute)
        }
    }

    fun setLatencyMode(mode: String) {
        val receiver = _streamState.value.connectedReceiver ?: return
        serviceScope.launch {
            controller.setLatencyMode(receiver.ipAddress, mode)
            val modeByte = when (mode) {
                "LOW_LATENCY" -> WiFiHiFiProtocol.LATENCY_LOW
                "STABLE" -> WiFiHiFiProtocol.LATENCY_STABLE
                else -> WiFiHiFiProtocol.LATENCY_BALANCED
            }
            packetizer?.setLatencyMode(modeByte)
            _streamState.value = _streamState.value.copy(latencyMode = mode)
        }
    }

    private fun startTelemetryMonitoring(receiverIp: String) {
        telemetryJob = serviceScope.launch {
            var lastBytes = 0L
            var lastTime = System.currentTimeMillis()

            while (isActive && _streamState.value.status == StreamStatus.STREAMING) {
                delay(1000)

                val currentBytes = udpSender?.totalBytesSent?.get() ?: 0L
                val currentTime = System.currentTimeMillis()
                val deltaBytes = currentBytes - lastBytes
                val deltaTimeSec = (currentTime - lastTime) / 1000.0

                val kbps = if (deltaTimeSec > 0) (deltaBytes * 8) / (deltaTimeSec * 1000) else 0.0

                lastBytes = currentBytes
                lastTime = currentTime

                // Poll receiver stats from REST API
                val statusJson = controller.fetchStatus(receiverIp)
                val lossPct = statusJson?.optDouble("packet_loss_pct", 0.0) ?: 0.0
                val bufPct = statusJson?.optDouble("buffer_fill_pct", 0.0) ?: 0.0
                val underruns = statusJson?.optInt("buffer_underruns", 0) ?: 0
                val overruns = statusJson?.optInt("buffer_overruns", 0) ?: 0

                _audioStats.value = AudioStats(
                    packetsSent = udpSender?.totalPacketsSent?.get() ?: 0L,
                    bytesSent = currentBytes,
                    bitrateKbps = kbps,
                    receiverLossPct = lossPct,
                    receiverBufferPct = bufPct,
                    receiverUnderruns = underruns,
                    receiverOverruns = overruns
                )
            }
        }
    }

    private fun createNotification(contentText: String): Notification {
        val pendingIntent = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_IMMUTABLE
        )

        val stopIntent = PendingIntent.getService(
            this,
            1,
            Intent(this, AudioStreamService::class.java).apply { action = ACTION_STOP_STREAM },
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, WiFiHiFiApp.NOTIFICATION_CHANNEL_ID)
            .setContentTitle("WiFi-HiFi Active")
            .setContentText(contentText)
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .addAction(R.drawable.ic_launcher_foreground, "Stop", stopIntent)
            .build()
    }

    private fun updateNotification(contentText: String) {
        val manager = getSystemService(Context.NOTIFICATION_SERVICE) as android.app.NotificationManager
        manager.notify(NOTIFICATION_ID, createNotification(contentText))
    }

    private fun acquireLocks() {
        try {
            val pm = getSystemService(Context.POWER_SERVICE) as? PowerManager
            if (wakeLock == null) {
                wakeLock = pm?.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "WiFiHiFi:AudioWakeLock")?.apply {
                    setReferenceCounted(false)
                    acquire(10 * 60 * 60 * 1000L) // 10 hours
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not acquire WakeLock: ${e.message}")
        }

        try {
            val wm = applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
            if (wifiLock == null) {
                wifiLock = wm?.createWifiLock(WifiManager.WIFI_MODE_FULL_HIGH_PERF, "WiFiHiFi:WifiLock")?.apply {
                    setReferenceCounted(false)
                    acquire()
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not acquire WifiLock: ${e.message}")
        }
    }

    private fun releaseLocks() {
        try {
            if (wakeLock?.isHeld == true) wakeLock?.release()
        } catch (e: Exception) {
            Log.w(TAG, "Error releasing wakeLock: ${e.message}")
        }
        try {
            if (wifiLock?.isHeld == true) wifiLock?.release()
        } catch (e: Exception) {
            Log.w(TAG, "Error releasing wifiLock: ${e.message}")
        }
        wakeLock = null
        wifiLock = null
    }

    override fun onDestroy() {
        stopStreaming()
        serviceScope.cancel()
        releaseLocks()
        super.onDestroy()
    }
}
