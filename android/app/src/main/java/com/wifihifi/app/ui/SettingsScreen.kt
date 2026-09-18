package com.wifihifi.app.ui

import android.net.Uri
import android.widget.Toast
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.filled.ArrowBack
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.wifihifi.app.model.ReceiverDevice
import com.wifihifi.app.network.ReceiverController
import com.wifihifi.app.ui.theme.*
import kotlinx.coroutines.launch

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    selectedReceiver: ReceiverDevice?,
    onBack: () -> Unit
) {
    val scrollState = rememberScrollState()
    val context = LocalContext.current
    val coroutineScope = rememberCoroutineScope()
    val receiverController = remember { ReceiverController() }

    var isOtaUploading by remember { mutableStateOf(false) }
    var otaStatusMessage by remember { mutableStateOf<String?>(null) }
    var selectedFileUri by remember { mutableStateOf<Uri?>(null) }

    // Launcher for picking .bin firmware file
    val filePickerLauncher = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.GetContent()
    ) { uri: Uri? ->
        if (uri != null) {
            selectedFileUri = uri
            if (selectedReceiver == null) {
                Toast.makeText(context, "Please select an active receiver first!", Toast.LENGTH_SHORT).show()
                return@rememberLauncherForActivityResult
            }

            coroutineScope.launch {
                isOtaUploading = true
                otaStatusMessage = "Reading firmware file..."
                try {
                    val bytes = context.contentResolver.openInputStream(uri)?.use { it.readBytes() }
                    if (bytes == null || bytes.isEmpty()) {
                        otaStatusMessage = "Error: File is empty or unreadable"
                        isOtaUploading = false
                        return@launch
                    }

                    otaStatusMessage = "Pushing ${bytes.size / 1024} KB to ${selectedReceiver.ipAddress}..."
                    val result = receiverController.uploadFirmwareOta(
                        selectedReceiver.ipAddress,
                        "firmware.bin",
                        bytes
                    )

                    result.onSuccess { msg ->
                        otaStatusMessage = "Success: $msg"
                        Toast.makeText(context, "Firmware updated! Receiver rebooting.", Toast.LENGTH_LONG).show()
                    }.onFailure { err ->
                        otaStatusMessage = "Failed: ${err.message}"
                        Toast.makeText(context, "OTA Error: ${err.message}", Toast.LENGTH_LONG).show()
                    }
                } catch (e: Exception) {
                    otaStatusMessage = "Upload Exception: ${e.message}"
                } finally {
                    isOtaUploading = false
                }
            }
        }
    }

    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings & SuperMini Config", color = HiFiGold, fontSize = 18.sp, fontWeight = FontWeight.Bold) },
                navigationIcon = {
                    IconButton(onClick = onBack) {
                        Icon(Icons.AutoMirrored.Filled.ArrowBack, contentDescription = "Back", tint = HiFiText)
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
            // 1. ACTIVE RECEIVER & SUPERMINI HARDWARE
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("ACTIVE RECEIVER", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(10.dp))
                    Text(selectedReceiver?.deviceName ?: "No receiver connected", color = HiFiText, fontWeight = FontWeight.SemiBold)
                    Text("IP: ${selectedReceiver?.ipAddress ?: "N/A"}", color = HiFiTextMuted, fontSize = 13.sp)
                    Text("Audio Port: ${selectedReceiver?.audioPort ?: 50005} (UDP)", color = HiFiTextMuted, fontSize = 13.sp)
                    Text("Hardware: ${selectedReceiver?.hardware ?: "ESP32-C3 SuperMini"} / ${selectedReceiver?.dac ?: "UDA1334A"}", color = HiFiTextMuted, fontSize = 13.sp)
                }
            }

            // 2. OVER-THE-AIR (OTA) PUSH TO ESP32-C3 SUPERMINI
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("PUSH FIRMWARE TO ESP32-C3 (OTA)", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(8.dp))
                    Text(
                        "Upload and flash compiled firmware.bin directly to your ESP32-C3 SuperMini over Wi-Fi (HTTP /api/ota). No USB cable required after initial flash!",
                        color = HiFiTextMuted,
                        fontSize = 12.sp
                    )
                    Spacer(modifier = Modifier.height(14.dp))

                    Button(
                        onClick = { filePickerLauncher.launch("*/*") },
                        enabled = !isOtaUploading && selectedReceiver != null,
                        colors = ButtonDefaults.buttonColors(containerColor = HiFiGold),
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        if (isOtaUploading) {
                            CircularProgressIndicator(modifier = Modifier.size(16.dp), color = HiFiBackground, strokeWidth = 2.dp)
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Pushing Firmware...", color = HiFiBackground, fontWeight = FontWeight.Bold)
                        } else {
                            Icon(Icons.Default.CloudUpload, contentDescription = null, tint = HiFiBackground)
                            Spacer(modifier = Modifier.width(8.dp))
                            Text("Select & Push firmware.bin", color = HiFiBackground, fontWeight = FontWeight.Bold)
                        }
                    }

                    if (otaStatusMessage != null) {
                        Spacer(modifier = Modifier.height(10.dp))
                        Text(
                            text = otaStatusMessage ?: "",
                            color = if (otaStatusMessage?.startsWith("Success") == true) HiFiSuccess else HiFiTextMuted,
                            fontSize = 12.sp
                        )
                    }
                }
            }

            // 3. ESP32-C3 SUPERMINI PINOUT REFERENCE
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("ESP32-C3 SUPERMINI I2S WIRING", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(10.dp))
                    Text("• GPIO 4 -> UDA1334A BCLK (Bit Clock)", color = HiFiText, fontSize = 13.sp)
                    Text("• GPIO 5 -> UDA1334A WSEL / LRCK (Word Select)", color = HiFiText, fontSize = 13.sp)
                    Text("• GPIO 6 -> UDA1334A DIN / DATA (Audio Data)", color = HiFiText, fontSize = 13.sp)
                    Text("• 5V / 3V3 -> UDA1334A VIN (Power)", color = HiFiText, fontSize = 13.sp)
                    Text("• GND -> UDA1334A GND (Ground)", color = HiFiText, fontSize = 13.sp)
                    Text("• MCLK -> Unconnected (UDA1334A internal PLL)", color = HiFiTextMuted, fontSize = 12.sp)
                }
            }

            // 4. PROTOCOL SPECIFICATION
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("PROTOCOL SPECIFICATION", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(10.dp))
                    Text("Protocol Name: WiFi-HiFi Binary Protocol", color = HiFiText, fontSize = 13.sp)
                    Text("Magic: 'WFHF' (0x46484657)", color = HiFiText, fontSize = 13.sp)
                    Text("Header Size: 28 Bytes (Little Endian)", color = HiFiText, fontSize = 13.sp)
                    Text("Transport: UDP Datagram (Port 50005)", color = HiFiText, fontSize = 13.sp)
                    Text("Sample Rate: 44,100 Hz (CD Quality)", color = HiFiText, fontSize = 13.sp)
                    Text("Bit Depth: 16-bit Signed Linear PCM", color = HiFiText, fontSize = 13.sp)
                    Text("Channels: 2 (Stereo)", color = HiFiText, fontSize = 13.sp)
                }
            }

            // 5. ABOUT
            Card(
                colors = CardDefaults.cardColors(containerColor = HiFiSurface),
                modifier = Modifier.fillMaxWidth().border(1.dp, HiFiBorder, RoundedCornerShape(12.dp))
            ) {
                Column(modifier = Modifier.padding(16.dp)) {
                    Text("ABOUT WIFI-HIFI", color = HiFiGold, fontSize = 12.sp, fontWeight = FontWeight.Bold)
                    Spacer(modifier = Modifier.height(10.dp))
                    Text("Application: WiFi-HiFi Sender", color = HiFiText, fontSize = 13.sp)
                    Text("Target OS: Android 16+ (API 36)", color = HiFiText, fontSize = 13.sp)
                    Text("Audio Source: AudioPlaybackCapture (Media & Games)", color = HiFiText, fontSize = 13.sp)
                    Text("Target Board: ESP32-C3 SuperMini + UDA1334A DAC", color = HiFiText, fontSize = 13.sp)
                    Text("Repository: anaya2025/WiFi-HiFi", color = HiFiText, fontSize = 13.sp)
                }
            }
        }
    }
}
