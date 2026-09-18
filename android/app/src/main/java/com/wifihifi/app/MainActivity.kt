package com.wifihifi.app

import android.Manifest
import android.app.Activity
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.media.projection.MediaProjectionManager
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.util.Log
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.runtime.*
import androidx.core.content.ContextCompat
import com.wifihifi.app.model.AudioStats
import com.wifihifi.app.model.ReceiverDevice
import com.wifihifi.app.model.StreamState
import com.wifihifi.app.network.ReceiverDiscovery
import com.wifihifi.app.network.DiscoveryStats
import com.wifihifi.app.service.AudioStreamService
import com.wifihifi.app.ui.MainScreen
import com.wifihifi.app.ui.SettingsScreen
import com.wifihifi.app.ui.theme.WiFiHiFiTheme
import kotlinx.coroutines.flow.MutableStateFlow

class MainActivity : ComponentActivity() {

    companion object {
        private const val TAG = "MainActivity"
    }

    private val audioServiceState = mutableStateOf<AudioStreamService?>(null)
    private var isBound = false

    private val defaultStreamState = MutableStateFlow(StreamState())
    private val defaultAudioStats = MutableStateFlow(AudioStats())

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, service: IBinder?) {
            try {
                val binder = service as AudioStreamService.LocalBinder
                audioServiceState.value = binder.getService()
                isBound = true
                Log.i(TAG, "AudioStreamService bound successfully")
            } catch (e: Exception) {
                Log.e(TAG, "Error in onServiceConnected", e)
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            audioServiceState.value = null
            isBound = false
            Log.i(TAG, "AudioStreamService disconnected")
        }
    }

    private var mediaProjectionManager: MediaProjectionManager? = null
    private var pendingReceiver: ReceiverDevice? = null

    // Standard Runtime Permissions Launcher (Audio & Notifications)
    private val permissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestMultiplePermissions()
    ) { permissions ->
        val recordAudioGranted = permissions[Manifest.permission.RECORD_AUDIO] ?: false
        if (!recordAudioGranted) {
            Log.w(TAG, "RECORD_AUDIO permission was denied by user")
        }
    }

    // MediaProjection permission launcher
    private val projectionLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) { result ->
        if (result.resultCode == Activity.RESULT_OK && result.data != null) {
            try {
                val mpm = mediaProjectionManager ?: (getSystemService(Context.MEDIA_PROJECTION_SERVICE) as? MediaProjectionManager)
                val projection = mpm?.getMediaProjection(result.resultCode, result.data!!)
                val receiver = pendingReceiver
                val service = audioServiceState.value

                if (projection != null && receiver != null && service != null) {
                    service.startStreaming(projection, receiver)
                } else {
                    Toast.makeText(this, "Service not ready or capture failed", Toast.LENGTH_SHORT).show()
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error acquiring MediaProjection", e)
                Toast.makeText(this, "Capture initialization error: ${e.message}", Toast.LENGTH_LONG).show()
            }
        } else {
            Toast.makeText(this, "Audio capture permission denied", Toast.LENGTH_SHORT).show()
        }
    }

    private var discovery: ReceiverDiscovery? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        try {
            mediaProjectionManager = getSystemService(Context.MEDIA_PROJECTION_SERVICE) as? MediaProjectionManager
        } catch (e: Exception) {
            Log.w(TAG, "MediaProjectionManager unavailable: ${e.message}")
        }

        // Request runtime permissions required for Android 13/14/15/16
        requestRequiredPermissions()

        // Bind Foreground Service on-demand (do NOT call startService prematurely)
        try {
            val serviceIntent = Intent(this, AudioStreamService::class.java)
            bindService(serviceIntent, serviceConnection, Context.BIND_AUTO_CREATE)
        } catch (e: Exception) {
            Log.e(TAG, "Failed to bind AudioStreamService", e)
        }

        setContent {
            WiFiHiFiTheme {
                val service = audioServiceState.value
                val streamStateFlow = service?.streamState ?: defaultStreamState
                val audioStatsFlow = service?.audioStats ?: defaultAudioStats

                val streamState by streamStateFlow.collectAsState()
                val audioStats by audioStatsFlow.collectAsState()

                val discoveredReceivers = remember { mutableStateListOf<ReceiverDevice>() }
                var selectedReceiver by remember { mutableStateOf<ReceiverDevice?>(null) }
                var showSettings by remember { mutableStateOf(false) }

                var discoveryInstance by remember { mutableStateOf<ReceiverDiscovery?>(null) }
                val defaultDiscoveryStats = remember { MutableStateFlow(DiscoveryStats()) }
                val discoveryStatsFlow = discoveryInstance?.stats ?: defaultDiscoveryStats
                val discoveryStats by discoveryStatsFlow.collectAsState()
                val coroutineScope = rememberCoroutineScope()

                // Discovery initialization
                DisposableEffect(Unit) {
                    try {
                        val d = ReceiverDiscovery(this@MainActivity) { newDevice ->
                            val existingIdx = discoveredReceivers.indexOfFirst { it.ipAddress == newDevice.ipAddress }
                            if (existingIdx >= 0) {
                                discoveredReceivers[existingIdx] = newDevice
                            } else {
                                discoveredReceivers.add(newDevice)
                                if (selectedReceiver == null) {
                                    selectedReceiver = newDevice
                                }
                            }
                        }
                        discovery = d
                        discoveryInstance = d
                        d.startDiscovery()
                    } catch (e: Exception) {
                        Log.e(TAG, "Failed starting discovery", e)
                    }

                    onDispose {
                        try {
                            discovery?.stopDiscovery()
                            discovery = null
                            discoveryInstance = null
                        } catch (e: Exception) {
                            Log.w(TAG, "Error disposing discovery: ${e.message}")
                        }
                    }
                }

                if (showSettings) {
                    SettingsScreen(
                        selectedReceiver = selectedReceiver,
                        onBack = { showSettings = false }
                    )
                } else {
                    MainScreen(
                        streamState = streamState,
                        audioStats = audioStats,
                        discoveryStats = discoveryStats,
                        discoveredReceivers = discoveredReceivers,
                        selectedReceiver = selectedReceiver,
                        onSelectReceiver = { selectedReceiver = it },
                        onStartStreaming = {
                            val receiver = selectedReceiver
                            if (receiver != null) {
                                pendingReceiver = receiver
                                checkAndLaunchCapture()
                            } else {
                                Toast.makeText(this@MainActivity, "Please select a receiver first", Toast.LENGTH_SHORT).show()
                            }
                        },
                        onStopStreaming = {
                            audioServiceState.value?.stopStreaming()
                        },
                        onVolumeChanged = { vol ->
                            audioServiceState.value?.setVolume(vol)
                        },
                        onMuteToggled = { mute ->
                            audioServiceState.value?.setMute(mute)
                        },
                        onLatencyModeChanged = { mode ->
                            audioServiceState.value?.setLatencyMode(mode)
                        },
                        onRefreshDiscovery = {
                            discoveredReceivers.clear()
                            try {
                                discovery?.stopDiscovery()
                                discovery?.startDiscovery()
                            } catch (e: Exception) {
                                Log.w(TAG, "Error refreshing discovery: ${e.message}")
                            }
                        },
                        onProbeDirectIp = { ip ->
                            coroutineScope.launch {
                                Toast.makeText(this@MainActivity, "Probing $ip ...", Toast.LENGTH_SHORT).show()
                                val result = discovery?.probeDirectIp(ip)
                                if (result?.isSuccess == true) {
                                    val dev = result.getOrNull()
                                    if (dev != null) {
                                        selectedReceiver = dev
                                        Toast.makeText(this@MainActivity, "Connected to ${dev.deviceName}!", Toast.LENGTH_SHORT).show()
                                    }
                                } else {
                                    val errMsg = result?.exceptionOrNull()?.message ?: "Direct probe failed"
                                    Toast.makeText(this@MainActivity, errMsg, Toast.LENGTH_LONG).show()
                                }
                            }
                        },
                        onScanSubnet = {
                            coroutineScope.launch {
                                Toast.makeText(this@MainActivity, "Subnet sweep started...", Toast.LENGTH_SHORT).show()
                                discovery?.scanSubnet { foundDev ->
                                    if (selectedReceiver == null) {
                                        selectedReceiver = foundDev
                                    }
                                }
                            }
                        },
                        onOpenSettings = {
                            showSettings = true
                        }
                    )
                }
            }
        }
    }

    private fun requestRequiredPermissions() {
        val permissions = mutableListOf<String>()

        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            permissions.add(Manifest.permission.RECORD_AUDIO)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            if (ContextCompat.checkSelfPermission(this, Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED) {
                permissions.add(Manifest.permission.POST_NOTIFICATIONS)
            }
        }

        if (permissions.isNotEmpty()) {
            permissionLauncher.launch(permissions.toTypedArray())
        }
    }

    private fun checkAndLaunchCapture() {
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
            Toast.makeText(this, "Audio recording permission required for Hi-Fi streaming", Toast.LENGTH_SHORT).show()
            permissionLauncher.launch(arrayOf(Manifest.permission.RECORD_AUDIO))
            return
        }

        val mpm = mediaProjectionManager ?: (getSystemService(Context.MEDIA_PROJECTION_SERVICE) as? MediaProjectionManager)
        if (mpm != null) {
            try {
                projectionLauncher.launch(mpm.createScreenCaptureIntent())
            } catch (e: Exception) {
                Log.e(TAG, "Failed to launch screen capture intent", e)
                Toast.makeText(this, "Could not start audio capture: ${e.message}", Toast.LENGTH_LONG).show()
            }
        } else {
            Toast.makeText(this, "MediaProjection service unavailable on this device", Toast.LENGTH_LONG).show()
        }
    }

    override fun onDestroy() {
        if (isBound) {
            try {
                unbindService(serviceConnection)
            } catch (e: Exception) {
                Log.w(TAG, "Error unbinding service: ${e.message}")
            }
            isBound = false
        }
        audioServiceState.value = null
        super.onDestroy()
    }
}
