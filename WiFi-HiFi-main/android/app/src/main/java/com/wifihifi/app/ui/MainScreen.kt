package com.wifihifi.app.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.core.*
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.scale
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.wifihifi.app.model.*
import com.wifihifi.app.network.DiscoveryStats
import com.wifihifi.app.ui.theme.*

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    streamState: StreamState,
    audioStats: AudioStats,
    discoveryStats: DiscoveryStats,
    discoveredReceivers: List<ReceiverDevice>,
    selectedReceiver: ReceiverDevice?,
    onSelectReceiver: (ReceiverDevice) -> Unit,
    onStartStreaming: () -> Unit,
    onStopStreaming: () -> Unit,
    onVolumeChanged: (Int) -> Unit,
    onMuteToggled: (Boolean) -> Unit,
    onLatencyModeChanged: (String) -> Unit,
    onRefreshDiscovery: () -> Unit,
    onProbeDirectIp: (String) -> Unit,
    onScanSubnet: () -> Unit,
    onOpenSettings: () -> Unit
) {
    val scrollState = rememberScrollState()
    var showDirectIpDialog by remember { mutableStateOf(false) }
    var showLogs by remember { mutableStateOf(false) }
    var directIpInput by remember { mutableStateOf("") }

    // Pulsing animation for scanning radar
    val infiniteTransition = rememberInfiniteTransition(label = "pulse")
    val pulseScale by infiniteTransition.animateFloat(
        initialValue = 0.85f,
        targetValue = 1.15f,
        animationSpec = infiniteRepeatable(
            animation = tween(1200, easing = FastOutSlowInEasing),
            repeatMode = RepeatMode.Reverse
        ),
        label = "pulseScale"
    )

    Scaffold(
        topBar = {
            TopAppBar(
                title = {
                    Column {
                        Text(
                            text = "WiFi-HiFi",
                            color = HiFiGold,
                            fontWeight = FontWeight.Bold,
                            fontSize = 20.sp
                        )
                        Text(
                            text = "Android 16+ Lossless PCM Sender",
                            color = HiFiTextMuted,
                            fontSize = 11.sp
                        )
                    }
                },
                actions = {
                    IconButton(onClick = onRefreshDiscovery) {
                        Icon(Icons.Default.Refresh, contentDescription = "Refresh", tint = HiFiGold)
                    }
                    IconButton(onClick = onOpenSettings) {
                        Icon(Icons.Default.Settings, contentDescription = "Settings", tint = HiFiText)
                    }
                },
                colors = TopAppBarDefaults.topAppBarColors(containerColor = HiFiBackground)
            )
        },
        containerColor = HiFiBackground
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .verticalScroll(scrollState)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Error Banner
            AnimatedVisibility(visible = streamState.errorMessage != null) {
                Card(
                    colors = CardDefaults.cardColors(containerColor = Color(0x33EF4444)),
                    modifier = Modifier.fillMaxWidth().border(1.dp, HiFiDanger, RoundedCornerShape(12.dp))
                ) {
                    Row(
                        modifier = Modifier.padding(14.dp),
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Icon(Icons.Default.Warning, contentDescription = null, tint = HiFiDanger)
                        Spacer(modifier = Modifier.width(10.dp))
                        Text(
                            text = streamState.errorMessage ?: "",
                            color = HiFiText,
                            fontSize = 13.sp
                        )
                    }
                }
            }

            // 1. ACTIVE RECEIVER SELECTOR & DISCOVERY RADAR
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    // Header with live scanner status
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Box(
                                modifier = Modifier
                                    .size(8.dp)
                                    .scale(if (discoveryStats.isScanning) pulseScale else 1f)
                                    .background(if (discoveryStats.isScanning) HiFiSuccess else HiFiTextMuted, CircleShape)
                            )
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("TARGET RECEIVER", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                        }

                        Text(
                            text = if (discoveredReceivers.isEmpty()) {
                                if (discoveryStats.isScanning) "Searching..." else "Idle"
                            } else {
                                "${discoveredReceivers.size} found"
                            },
                            color = if (discoveredReceivers.isNotEmpty()) HiFiSuccess else HiFiTextMuted,
                            fontSize = 12.sp,
                            fontWeight = FontWeight.SemiBold
                        )
                    }

                    Spacer(modifier = Modifier.height(10.dp))

                    // Live Network Status Strip
                    Row(
                        modifier = Modifier
                            .fillMaxWidth()
                            .clip(RoundedCornerShape(8.dp))
                            .background(HiFiCard)
                            .padding(horizontal = 10.dp, vertical = 6.dp),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text(
                            text = "My IP: ${discoveryStats.localIp}",
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            color = HiFiTextMuted
                        )
                        Text(
                            text = "UDP Pings: ${discoveryStats.packetsSent}",
                            fontSize = 11.sp,
                            fontFamily = FontFamily.Monospace,
                            color = HiFiGold
                        )
                    }

                    // Subnet Sweep Progress Bar
                    if (discoveryStats.isSubnetScanning) {
                        Spacer(modifier = Modifier.height(10.dp))
                        Column {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween
                            ) {
                                Text("Sweeping Subnet /24 ...", fontSize = 11.sp, color = HiFiGold)
                                Text("${(discoveryStats.subnetScanProgress * 100).toInt()}%", fontSize = 11.sp, color = HiFiGold)
                            }
                            Spacer(modifier = Modifier.height(4.dp))
                            LinearProgressIndicator(
                                progress = discoveryStats.subnetScanProgress,
                                modifier = Modifier.fillMaxWidth().height(4.dp).clip(RoundedCornerShape(2.dp)),
                                color = HiFiGold,
                                trackColor = HiFiCard,
                            )
                        }
                    }

                    Spacer(modifier = Modifier.height(12.dp))

                    // Receiver List or Active Search Card
                    if (discoveredReceivers.isEmpty()) {
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(8.dp))
                                .background(Color(0x1A000000))
                                .border(1.dp, HiFiBorder, RoundedCornerShape(8.dp))
                                .padding(14.dp),
                            horizontalAlignment = Alignment.CenterHorizontally
                        ) {
                            Icon(
                                imageVector = Icons.Default.WifiTethering,
                                contentDescription = null,
                                tint = HiFiGold,
                                modifier = Modifier.size(32.dp).scale(pulseScale)
                            )
                            Spacer(modifier = Modifier.height(8.dp))
                            Text(
                                text = "Transmitting discovery pings on UDP 50006 & mDNS",
                                color = HiFiText,
                                fontSize = 13.sp,
                                fontWeight = FontWeight.Medium
                            )
                            Spacer(modifier = Modifier.height(4.dp))
                            Text(
                                text = "Ensure your ESP32-C3 receiver is powered on and on the same Wi-Fi. If using AP mode, connect to 'WiFi-HiFi-Setup' and use 192.168.4.1.",
                                color = HiFiTextMuted,
                                fontSize = 11.sp,
                                lineHeight = 16.sp
                            )
                        }
                    } else {
                        discoveredReceivers.forEach { receiver ->
                            val isSelected = (selectedReceiver?.ipAddress == receiver.ipAddress)
                            Row(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .clip(RoundedCornerShape(8.dp))
                                    .background(if (isSelected) HiFiCard else Color.Transparent)
                                    .border(
                                        1.dp,
                                        if (isSelected) HiFiGold else HiFiBorder,
                                        RoundedCornerShape(8.dp)
                                    )
                                    .clickable { onSelectReceiver(receiver) }
                                    .padding(12.dp),
                                verticalAlignment = Alignment.CenterVertically,
                                horizontalArrangement = Arrangement.SpaceBetween
                            ) {
                                Row(verticalAlignment = Alignment.CenterVertically) {
                                    Box(
                                        modifier = Modifier
                                            .size(8.dp)
                                            .background(HiFiSuccess, CircleShape)
                                    )
                                    Spacer(modifier = Modifier.width(10.dp))
                                    Column {
                                        Text(receiver.deviceName, fontWeight = FontWeight.SemiBold, color = HiFiText)
                                        Text(
                                            "${receiver.ipAddress} • ${receiver.hardware} (${receiver.dac})",
                                            fontSize = 12.sp,
                                            color = HiFiTextMuted
                                        )
                                    }
                                }
                                if (isSelected) {
                                    Icon(Icons.Default.CheckCircle, contentDescription = null, tint = HiFiGold)
                                }
                            }
                            Spacer(modifier = Modifier.height(8.dp))
                        }
                    }

                    Spacer(modifier = Modifier.height(12.dp))

                    // Direct Action Buttons: Manual IP Connect & Subnet Sweep
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        OutlinedButton(
                            onClick = { showDirectIpDialog = true },
                            modifier = Modifier.weight(1f).height(40.dp),
                            shape = RoundedCornerShape(8.dp),
                            colors = ButtonDefaults.outlinedButtonColors(contentColor = HiFiGold),
                            border = androidx.compose.foundation.BorderStroke(1.dp, HiFiGold)
                        ) {
                            Icon(Icons.Default.AddLink, contentDescription = null, modifier = Modifier.size(16.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Direct IP", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                        }

                        OutlinedButton(
                            onClick = onScanSubnet,
                            enabled = !discoveryStats.isSubnetScanning,
                            modifier = Modifier.weight(1f).height(40.dp),
                            shape = RoundedCornerShape(8.dp),
                            colors = ButtonDefaults.outlinedButtonColors(contentColor = HiFiText),
                            border = androidx.compose.foundation.BorderStroke(1.dp, HiFiBorder)
                        ) {
                            Icon(Icons.Default.Radar, contentDescription = null, modifier = Modifier.size(16.dp))
                            Spacer(modifier = Modifier.width(6.dp))
                            Text("Sweep Subnet", fontSize = 12.sp, fontWeight = FontWeight.SemiBold)
                        }

                        IconButton(
                            onClick = { showLogs = !showLogs },
                            modifier = Modifier.size(40.dp).clip(RoundedCornerShape(8.dp)).background(HiFiCard)
                        ) {
                            Icon(
                                Icons.Default.Terminal,
                                contentDescription = "Logs",
                                tint = if (showLogs) HiFiGold else HiFiTextMuted,
                                modifier = Modifier.size(18.dp)
                            )
                        }
                    }

                    // Live Discovery Event Log View
                    AnimatedVisibility(visible = showLogs) {
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .padding(top = 10.dp)
                                .clip(RoundedCornerShape(8.dp))
                                .background(Color(0xFF090B10))
                                .border(1.dp, HiFiBorder, RoundedCornerShape(8.dp))
                                .padding(10.dp)
                        ) {
                            Row(
                                modifier = Modifier.fillMaxWidth(),
                                horizontalArrangement = Arrangement.SpaceBetween,
                                verticalAlignment = Alignment.CenterVertically
                            ) {
                                Text("LIVE DISCOVERY EVENT LOGS", fontSize = 10.sp, color = HiFiGold, fontWeight = FontWeight.Bold)
                                Text("${discoveryStats.logs.size} entries", fontSize = 10.sp, color = HiFiTextMuted)
                            }
                            Spacer(modifier = Modifier.height(6.dp))
                            val recentLogs = discoveryStats.logs.takeLast(8)
                            if (recentLogs.isEmpty()) {
                                Text("Awaiting network activity...", fontSize = 11.sp, color = HiFiTextMuted, fontFamily = FontFamily.Monospace)
                            } else {
                                recentLogs.forEach { logLine ->
                                    Text(
                                        text = logLine,
                                        fontSize = 10.sp,
                                        fontFamily = FontFamily.Monospace,
                                        color = if (logLine.contains("MATCH") || logLine.contains("Beacon") || logLine.contains("success")) HiFiSuccess else Color(0xFFC0C7D8),
                                        lineHeight = 14.sp
                                    )
                                }
                            }
                        }
                    }
                }
            }

            // 2. STREAM CONTROLS
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("STREAM CONTROLLER", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                        val (badgeText, badgeBg, badgeFg) = when (streamState.status) {
                            StreamStatus.STREAMING -> Triple("STREAMING", Color(0x3310B981), HiFiSuccess)
                            StreamStatus.CONNECTING -> Triple("CONNECTING", Color(0x33F59E0B), Color(0xFFF59E0B))
                            StreamStatus.ERROR -> Triple("ERROR", Color(0x33EF4444), HiFiDanger)
                            else -> Triple("IDLE", Color(0x338E98AC), HiFiTextMuted)
                        }
                        Box(
                            modifier = Modifier
                                .clip(RoundedCornerShape(50))
                                .background(badgeBg)
                                .padding(horizontal = 10.dp, vertical = 4.dp)
                        ) {
                            Text(badgeText, color = badgeFg, fontSize = 11.sp, fontWeight = FontWeight.Bold)
                        }
                    }

                    Spacer(modifier = Modifier.height(16.dp))

                    if (streamState.status == StreamStatus.STREAMING) {
                        Button(
                            onClick = onStopStreaming,
                            modifier = Modifier.fillMaxWidth().height(48.dp),
                            colors = ButtonDefaults.buttonColors(containerColor = HiFiDanger),
                            shape = RoundedCornerShape(8.dp)
                        ) {
                            Icon(Icons.Default.Stop, contentDescription = null)
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Stop Streaming", fontWeight = FontWeight.Bold)
                        }
                    } else {
                        Button(
                            onClick = onStartStreaming,
                            enabled = selectedReceiver != null && streamState.status != StreamStatus.CONNECTING,
                            modifier = Modifier.fillMaxWidth().height(48.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = HiFiGold,
                                contentColor = Color.Black
                            ),
                            shape = RoundedCornerShape(8.dp)
                        ) {
                            Icon(Icons.Default.PlayArrow, contentDescription = null)
                            Spacer(modifier = Modifier.width(8.dp))
                            Text(
                                if (selectedReceiver == null) "Select Receiver First" else "Start Lossless Streaming",
                                fontWeight = FontWeight.Bold
                            )
                        }
                    }
                }
            }

            // 3. AUDIO FORMAT & TELEMETRY
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("AUDIO SPECIFICATION & METRICS", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(12.dp))

                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        MetricItem("Format", "Linear PCM 16-bit")
                        MetricItem("Sample Rate", "44,100 Hz")
                    }
                    Spacer(modifier = Modifier.height(10.dp))
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        MetricItem("Channels", "2 (Stereo L/R)")
                        MetricItem("Bitrate", "%.1f kbps".format(audioStats.bitrateKbps))
                    }
                    Spacer(modifier = Modifier.height(10.dp))
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        MetricItem("Packets Sent", audioStats.packetsSent.toString())
                        MetricItem("Receiver Packet Loss", "%.2f%%".format(audioStats.receiverLossPct))
                    }
                    Spacer(modifier = Modifier.height(10.dp))
                    Row(modifier = Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceBetween) {
                        MetricItem("Buffer Fill", "%.1f%%".format(audioStats.receiverBufferPct))
                        MetricItem("Underruns", audioStats.receiverUnderruns.toString())
                    }
                }
            }

            // 4. VOLUME & MUTE
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.SpaceBetween,
                        verticalAlignment = Alignment.CenterVertically
                    ) {
                        Text("RECEIVER VOLUME", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                        Text("${streamState.volume}%", fontWeight = FontWeight.Bold, color = HiFiText)
                    }

                    Spacer(modifier = Modifier.height(8.dp))

                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Slider(
                            value = streamState.volume.toFloat(),
                            onValueChange = { onVolumeChanged(it.toInt()) },
                            valueRange = 0f..100f,
                            modifier = Modifier.weight(1f),
                            colors = SliderDefaults.colors(thumbColor = HiFiGold, activeTrackColor = HiFiGold)
                        )
                        Spacer(modifier = Modifier.width(12.dp))
                        IconButton(
                            onClick = { onMuteToggled(!streamState.isMuted) }
                        ) {
                            Icon(
                                if (streamState.isMuted) Icons.Default.VolumeOff else Icons.Default.VolumeUp,
                                contentDescription = "Mute",
                                tint = if (streamState.isMuted) HiFiDanger else HiFiText
                            )
                        }
                    }
                }
            }

            // 5. LATENCY PROFILE
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("JITTER BUFFER LATENCY MODE", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(10.dp))

                    Row(
                        modifier = Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(8.dp)
                    ) {
                        LatencyButton("Low (35ms)", "LOW_LATENCY", streamState.latencyMode == "LOW_LATENCY") {
                            onLatencyModeChanged("LOW_LATENCY")
                        }
                        LatencyButton("Balanced", "BALANCED", streamState.latencyMode == "BALANCED") {
                            onLatencyModeChanged("BALANCED")
                        }
                        LatencyButton("Stable", "STABLE", streamState.latencyMode == "STABLE") {
                            onLatencyModeChanged("STABLE")
                        }
                    }
                }
            }
        }
    }

    // Direct IP Connection Dialog
    if (showDirectIpDialog) {
        AlertDialog(
            onDismissRequest = { showDirectIpDialog = false },
            title = {
                Text("Direct IP Connect", color = HiFiGold, fontWeight = FontWeight.Bold)
            },
            text = {
                Column {
                    Text(
                        "Enter the IP address of your ESP32-C3 receiver. This bypasses router multicast/mDNS blocking and directly queries HTTP & UDP.",
                        fontSize = 12.sp,
                        color = HiFiTextMuted,
                        lineHeight = 16.sp
                    )
                    Spacer(modifier = Modifier.height(12.dp))

                    OutlinedTextField(
                        value = directIpInput,
                        onValueChange = { directIpInput = it },
                        placeholder = { Text("e.g. 192.168.1.150") },
                        label = { Text("Receiver IP") },
                        singleLine = true,
                        keyboardOptions = KeyboardOptions(
                            keyboardType = KeyboardType.Decimal,
                            imeAction = ImeAction.Done
                        ),
                        keyboardActions = KeyboardActions(
                            onDone = {
                                if (directIpInput.isNotBlank()) {
                                    onProbeDirectIp(directIpInput)
                                    showDirectIpDialog = false
                                }
                            }
                        ),
                        colors = OutlinedTextFieldDefaults.colors(
                            focusedBorderColor = HiFiGold,
                            cursorColor = HiFiGold
                        ),
                        modifier = Modifier.fillMaxWidth()
                    )

                    Spacer(modifier = Modifier.height(10.dp))
                    Text("Quick Presets:", fontSize = 11.sp, color = HiFiTextMuted)
                    Spacer(modifier = Modifier.height(4.dp))
                    Row(horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        SuggestionChip(
                            onClick = { directIpInput = "192.168.4.1" },
                            label = { Text("192.168.4.1 (AP Mode)", fontSize = 11.sp) }
                        )
                    }
                }
            },
            confirmButton = {
                Button(
                    onClick = {
                        if (directIpInput.isNotBlank()) {
                            onProbeDirectIp(directIpInput)
                            showDirectIpDialog = false
                        }
                    },
                    colors = ButtonDefaults.buttonColors(containerColor = HiFiGold, contentColor = Color.Black)
                ) {
                    Text("Probe & Connect")
                }
            },
            dismissButton = {
                TextButton(onClick = { showDirectIpDialog = false }) {
                    Text("Cancel", color = HiFiTextMuted)
                }
            },
            containerColor = HiFiSurface
        )
    }
}

@Composable
private fun MetricItem(label: String, value: String) {
    Column {
        Text(label, fontSize = 11.sp, color = HiFiTextMuted)
        Text(value, fontSize = 14.sp, fontWeight = FontWeight.SemiBold, color = HiFiText)
    }
}

@Composable
private fun RowScope.LatencyButton(label: String, mode: String, isSelected: Boolean, onClick: () -> Unit) {
    Button(
        onClick = onClick,
        modifier = Modifier.weight(1f),
        colors = ButtonDefaults.buttonColors(
            containerColor = if (isSelected) HiFiGold else HiFiCard,
            contentColor = if (isSelected) Color.Black else HiFiText
        ),
        shape = RoundedCornerShape(8.dp)
    ) {
        Text(label, fontSize = 11.sp, fontWeight = FontWeight.SemiBold)
    }
}
