package com.wifihifi.app.network

import android.content.Context
import android.net.nsd.NsdManager
import android.net.nsd.NsdServiceInfo
import android.net.wifi.WifiManager
import android.util.Log
import com.wifihifi.app.model.ReceiverDevice
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import okhttp3.OkHttpClient
import okhttp3.Request
import org.json.JSONObject
import java.net.*
import java.text.SimpleDateFormat
import java.util.*
import java.util.concurrent.TimeUnit

data class DiscoveryStats(
    val isScanning: Boolean = false,
    val packetsSent: Int = 0,
    val lastScanTimeMs: Long = 0L,
    val localIp: String = "Detecting...",
    val broadcastTargets: List<String> = emptyList(),
    val logs: List<String> = emptyList(),
    val isSubnetScanning: Boolean = false,
    val subnetScanProgress: Float = 0f
)

/**
 * Dual mDNS, directed UDP Broadcast, and direct HTTP/UDP probe discovery
 * for WiFi-HiFi ESP32-C3 receivers.
 */
class ReceiverDiscovery(
    private val context: Context,
    private val onDeviceFound: (ReceiverDevice) -> Unit
) {
    companion object {
        private const val TAG = "ReceiverDiscovery"
        private const val DISCOVERY_PORT = 50006
        private const val SERVICE_TYPE = "_wifihifi._udp."
    }

    private val nsdManager = context.getSystemService(Context.NSD_SERVICE) as? NsdManager
    private val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE) as? WifiManager
    private var multicastLock: WifiManager.MulticastLock? = null

    private val _stats = MutableStateFlow(DiscoveryStats())
    val stats: StateFlow<DiscoveryStats> = _stats.asStateFlow()

    private var discoveryJob: Job? = null
    private var isDiscovering = false
    private var udpSocket: DatagramSocket? = null

    private val httpClient = OkHttpClient.Builder()
        .connectTimeout(2, TimeUnit.SECONDS)
        .readTimeout(2, TimeUnit.SECONDS)
        .build()

    private val timeFormat = SimpleDateFormat("HH:mm:ss", Locale.getDefault())

    private fun addLog(message: String) {
        val timestamp = timeFormat.format(Date())
        val logEntry = "[$timestamp] $message"
        Log.i(TAG, logEntry)
        val currentLogs = _stats.value.logs.takeLast(25).toMutableList()
        currentLogs.add(logEntry)
        _stats.value = _stats.value.copy(logs = currentLogs)
    }

    fun startDiscovery() {
        if (isDiscovering) return
        isDiscovering = true

        acquireMulticastLock()

        val netInfo = detectLocalNetwork()
        _stats.value = _stats.value.copy(
            isScanning = true,
            localIp = netInfo.first,
            broadcastTargets = netInfo.second.map { it.hostAddress ?: "" }
        )

        addLog("Discovery started on interface IP: ${netInfo.first}")
        addLog("Broadcast targets: ${_stats.value.broadcastTargets.joinToString()}")

        discoveryJob = CoroutineScope(Dispatchers.IO).launch {
            launch { runUdpBroadcastDiscovery(netInfo.second) }
            launch { startMdnsDiscovery() }
        }
    }

    fun stopDiscovery() {
        isDiscovering = false
        discoveryJob?.cancel()
        discoveryJob = null

        try {
            udpSocket?.close()
        } catch (e: Exception) {
            // Ignored
        } finally {
            udpSocket = null
        }

        stopMdnsDiscovery()
        releaseMulticastLock()
        _stats.value = _stats.value.copy(isScanning = false)
        addLog("Discovery stopped")
    }

    private fun detectLocalNetwork(): Pair<String, List<InetAddress>> {
        var localIp = "Unknown"
        val targets = mutableListOf<InetAddress>()

        try {
            val interfaces = NetworkInterface.getNetworkInterfaces()
            while (interfaces.hasMoreElements()) {
                val networkInterface = interfaces.nextElement()
                if (networkInterface.isLoopback || !networkInterface.isUp) continue

                for (interfaceAddress in networkInterface.interfaceAddresses) {
                    val addr = interfaceAddress.address
                    if (addr is Inet4Address && !addr.isLoopbackAddress) {
                        localIp = addr.hostAddress ?: localIp
                        val broadcast = interfaceAddress.broadcast
                        if (broadcast != null && !targets.contains(broadcast)) {
                            targets.add(broadcast)
                        }
                    }
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error detecting network: ${e.message}")
        }

        try {
            val globalBcast = InetAddress.getByName("255.255.255.255")
            if (!targets.contains(globalBcast)) {
                targets.add(globalBcast)
            }
        } catch (e: Exception) {
            // Ignored
        }

        return Pair(localIp, targets)
    }

    private fun acquireMulticastLock() {
        try {
            multicastLock = wifiManager?.createMulticastLock("WiFiHiFiMulticastLock")?.apply {
                setReferenceCounted(false)
                acquire()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Could not acquire MulticastLock: ${e.message}")
        }
    }

    private fun releaseMulticastLock() {
        try {
            if (multicastLock?.isHeld == true) {
                multicastLock?.release()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Error releasing MulticastLock: ${e.message}")
        }
        multicastLock = null
    }

    private suspend fun runUdpBroadcastDiscovery(targets: List<InetAddress>) = withContext(Dispatchers.IO) {
        val queryJson = JSONObject().apply {
            put("magic", "WFHF_DISCOVER")
            put("client", "Android-16")
            put("version", 1)
        }.toString().toByteArray()

        try {
            udpSocket = DatagramSocket().apply {
                broadcast = true
                soTimeout = 2000
            }

            val recvBuffer = ByteArray(2048)
            val recvPacket = DatagramPacket(recvBuffer, recvBuffer.size)

            while (isDiscovering && isActive) {
                try {
                    // Send discovery ping to each target broadcast address
                    var sentCount = _stats.value.packetsSent
                    for (target in targets) {
                        val sendPacket = DatagramPacket(queryJson, queryJson.size, target, DISCOVERY_PORT)
                        udpSocket?.send(sendPacket)
                        sentCount++
                    }

                    _stats.value = _stats.value.copy(
                        packetsSent = sentCount,
                        lastScanTimeMs = System.currentTimeMillis()
                    )
                    addLog("Transmitted UDP discovery pings to ${targets.size} targets (Total sent: $sentCount)")

                    // Listen for responses
                    val startTime = System.currentTimeMillis()
                    while (System.currentTimeMillis() - startTime < 3000 && isDiscovering) {
                        try {
                            udpSocket?.receive(recvPacket)
                            val respStr = String(recvPacket.data, 0, recvPacket.length)
                            val senderIp = recvPacket.address.hostAddress ?: ""
                            addLog("Received UDP response from $senderIp (${recvPacket.length} bytes)")
                            parseDiscoveryJson(respStr, senderIp)
                        } catch (e: SocketTimeoutException) {
                            break
                        } catch (e: Exception) {
                            Log.w(TAG, "Receive error: ${e.message}")
                            break
                        }
                    }
                } catch (e: Exception) {
                    addLog("Broadcast cycle exception: ${e.message}")
                }
                delay(3500)
            }
        } catch (e: Exception) {
            addLog("UDP discovery socket fatal error: ${e.message}")
        }
    }

    /**
     * Direct probe for a specific IP (e.g. 192.168.4.1 or user-configured IP).
     * Tests both HTTP REST API (/api/status & /api/settings) and UDP discovery.
     */
    suspend fun probeDirectIp(ip: String): Result<ReceiverDevice> = withContext(Dispatchers.IO) {
        val trimmedIp = ip.trim()
        addLog("Probing direct IP: $trimmedIp ...")

        // 1. Send direct UDP packet to target
        try {
            val queryJson = JSONObject().apply {
                put("magic", "WFHF_DISCOVER")
                put("client", "Android-16")
                put("version", 1)
            }.toString().toByteArray()

            val targetAddr = InetAddress.getByName(trimmedIp)
            val sendPacket = DatagramPacket(queryJson, queryJson.size, targetAddr, DISCOVERY_PORT)
            val socket = DatagramSocket()
            socket.soTimeout = 1500
            socket.send(sendPacket)

            val recvBuf = ByteArray(2048)
            val recvPkt = DatagramPacket(recvBuf, recvBuf.size)
            try {
                socket.receive(recvPkt)
                val respStr = String(recvPkt.data, 0, recvPkt.length)
                parseDiscoveryJson(respStr, trimmedIp)
                addLog("Direct UDP probe success from $trimmedIp")
            } catch (e: Exception) {
                // Timeout, continue to HTTP fallback
            } finally {
                socket.close()
            }
        } catch (e: Exception) {
            Log.w(TAG, "Direct UDP probe failed for $trimmedIp: ${e.message}")
        }

        // 2. HTTP Probe to /api/status & /api/settings
        try {
            val statusUrl = "http://$trimmedIp/api/status"
            val req = Request.Builder().url(statusUrl).get().build()
            httpClient.newCall(req).execute().use { resp ->
                if (resp.isSuccessful) {
                    val body = resp.body?.string() ?: "{}"
                    val json = JSONObject(body)

                    // Try fetching /api/settings for device name if available
                    var deviceName = "WiFi-HiFi ($trimmedIp)"
                    var hardware = "ESP32-C3"
                    var dac = "UDA1334A"
                    var latencyMode = json.optString("latency_mode", "BALANCED")

                    try {
                        val settingsReq = Request.Builder().url("http://$trimmedIp/api/settings").get().build()
                        httpClient.newCall(settingsReq).execute().use { sResp ->
                            if (sResp.isSuccessful) {
                                val sJson = JSONObject(sResp.body?.string() ?: "{}")
                                deviceName = sJson.optString("device_name", deviceName)
                                hardware = sJson.optString("hardware", hardware)
                                dac = sJson.optString("dac", dac)
                            }
                        }
                    } catch (e: Exception) {
                        // Use default info
                    }

                    val device = ReceiverDevice(
                        deviceName = deviceName,
                        hostname = "wifi-hifi.local",
                        ipAddress = trimmedIp,
                        audioPort = 50005,
                        httpPort = 80,
                        sampleRates = listOf(44100, 48000),
                        channels = json.optInt("channels", 2),
                        bitDepths = listOf(16),
                        hardware = hardware,
                        dac = dac,
                        latencyMode = latencyMode,
                        isStreaming = json.optBoolean("streaming", false)
                    )

                    onDeviceFound(device)
                    addLog("Direct HTTP probe success! Found $deviceName at $trimmedIp")
                    return@withContext Result.success(device)
                }
            }
        } catch (e: Exception) {
            addLog("Direct HTTP probe failed for $trimmedIp: ${e.message}")
        }

        Result.failure(Exception("Could not connect to WiFi-HiFi at $trimmedIp. Ensure device is powered and on the same Wi-Fi."))
    }

    /**
     * Sweeps local subnet (e.g. 192.168.1.1 to 192.168.1.254) for ESP32 receivers.
     */
    suspend fun scanSubnet(onDeviceFoundCallback: ((ReceiverDevice) -> Unit)? = null) = withContext(Dispatchers.IO) {
        val (localIp, _) = detectLocalNetwork()
        val parts = localIp.split(".")
        if (parts.size != 4) {
            addLog("Cannot determine subnet from local IP: $localIp")
            return@withContext
        }

        val subnetPrefix = "${parts[0]}.${parts[1]}.${parts[2]}."
        addLog("Starting fast subnet scan on ${subnetPrefix}1-254 ...")
        _stats.value = _stats.value.copy(isSubnetScanning = true, subnetScanProgress = 0f)

        val fastClient = httpClient.newBuilder()
            .connectTimeout(500, TimeUnit.MILLISECONDS)
            .readTimeout(500, TimeUnit.MILLISECONDS)
            .build()

        val foundDevices = mutableListOf<ReceiverDevice>()
        val totalHosts = 254
        var scannedHosts = 0

        // Chunk into batches of 25 concurrent checks
        val hostList = (1..254).toList()
        for (batch in hostList.chunked(25)) {
            if (!_stats.value.isSubnetScanning) break

            batch.map { hostNum ->
                async {
                    val targetIp = "$subnetPrefix$hostNum"
                    try {
                        val req = Request.Builder().url("http://$targetIp/api/status").get().build()
                        fastClient.newCall(req).execute().use { resp ->
                            if (resp.isSuccessful) {
                                val body = resp.body?.string() ?: ""
                                if (body.contains("sample_rate") || body.contains("streaming")) {
                                    val json = JSONObject(body)
                                    val dev = ReceiverDevice(
                                        deviceName = "WiFi-HiFi ($targetIp)",
                                        hostname = "wifi-hifi.local",
                                        ipAddress = targetIp,
                                        audioPort = 50005,
                                        httpPort = 80,
                                        latencyMode = json.optString("latency_mode", "BALANCED")
                                    )
                                    synchronized(foundDevices) {
                                        foundDevices.add(dev)
                                    }
                                    onDeviceFound(dev)
                                    onDeviceFoundCallback?.invoke(dev)
                                    addLog("Subnet scan MATCH: Found ESP32 receiver at $targetIp!")
                                }
                            }
                        }
                    } catch (e: Exception) {
                        // Normal timeout on unused IPs
                    }
                }
            }.awaitAll()

            scannedHosts += batch.size
            _stats.value = _stats.value.copy(
                subnetScanProgress = scannedHosts.toFloat() / totalHosts
            )
        }

        _stats.value = _stats.value.copy(isSubnetScanning = false, subnetScanProgress = 1.0f)
        addLog("Subnet scan finished. Found ${foundDevices.size} receiver(s).")
    }

    private fun parseDiscoveryJson(jsonStr: String, sourceIp: String) {
        try {
            val json = JSONObject(jsonStr)
            if (json.optString("magic") == "WFHF_BEACON") {
                val ip = if (json.has("ip") && json.getString("ip").isNotEmpty()) {
                    json.getString("ip")
                } else {
                    sourceIp
                }

                val device = ReceiverDevice(
                    deviceName = json.optString("device_name", "WiFi-HiFi"),
                    hostname = json.optString("hostname", "wifi-hifi.local"),
                    ipAddress = ip,
                    audioPort = json.optInt("audio_port", 50005),
                    httpPort = json.optInt("http_port", 80),
                    firmwareVersion = json.optString("firmware_version", "1.0.0"),
                    hardware = json.optString("hardware", "ESP32-C3"),
                    dac = json.optString("dac", "UDA1334A"),
                    latencyMode = json.optString("latency_mode", "BALANCED"),
                    isStreaming = json.optBoolean("streaming", false)
                )

                addLog("Beacon verified: ${device.deviceName} at ${device.ipAddress}")
                onDeviceFound(device)
            }
        } catch (e: Exception) {
            Log.w(TAG, "Malformed discovery JSON from $sourceIp: ${e.message}")
        }
    }

    // mDNS Fallback
    private var nsdListener: NsdManager.DiscoveryListener? = null

    private fun startMdnsDiscovery() {
        val listener = object : NsdManager.DiscoveryListener {
            override fun onDiscoveryStarted(regType: String) {
                addLog("mDNS Discovery started for $SERVICE_TYPE")
            }

            override fun onServiceFound(service: NsdServiceInfo) {
                addLog("mDNS Service candidate found: ${service.serviceName}")
                if (service.serviceType.contains("wifihifi")) {
                    nsdManager?.resolveService(service, object : NsdManager.ResolveListener {
                        override fun onResolveFailed(serviceInfo: NsdServiceInfo, errorCode: Int) {
                            Log.w(TAG, "mDNS Resolve failed: $errorCode")
                        }

                        override fun onServiceResolved(serviceInfo: NsdServiceInfo) {
                            val host = serviceInfo.host?.hostAddress ?: return
                            val device = ReceiverDevice(
                                deviceName = serviceInfo.serviceName ?: "WiFi-HiFi",
                                hostname = "${serviceInfo.serviceName}.local",
                                ipAddress = host,
                                audioPort = serviceInfo.port
                            )
                            addLog("mDNS Resolved: ${device.deviceName} at ${device.ipAddress}:${device.audioPort}")
                            onDeviceFound(device)
                        }
                    })
                }
            }

            override fun onServiceLost(service: NsdServiceInfo) {}
            override fun onDiscoveryStopped(serviceType: String) {}
            override fun onStartDiscoveryFailed(serviceType: String, errorCode: Int) {
                addLog("mDNS Start failed: code $errorCode")
            }
            override fun onStopDiscoveryFailed(serviceType: String, errorCode: Int) {}
        }

        nsdListener = listener
        try {
            nsdManager?.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, listener)
        } catch (e: Exception) {
            addLog("mDNS discovery initialization error: ${e.message}")
        }
    }

    private fun stopMdnsDiscovery() {
        nsdListener?.let {
            try {
                nsdManager?.stopServiceDiscovery(it)
            } catch (e: Exception) {
                // Ignore
            }
        }
        nsdListener = null
    }
}
