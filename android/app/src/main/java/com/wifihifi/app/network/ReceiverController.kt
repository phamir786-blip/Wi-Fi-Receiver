package com.wifihifi.app.network

import android.util.Log
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

/**
 * Communicates with the ESP32-C3 receiver's HTTP REST API.
 */
class ReceiverController {
    companion object {
        private const val TAG = "ReceiverController"
        private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()
    }

    private val client = OkHttpClient.Builder()
        .connectTimeout(3, TimeUnit.SECONDS)
        .readTimeout(3, TimeUnit.SECONDS)
        .build()

    suspend fun fetchStatus(ip: String): JSONObject? = withContext(Dispatchers.IO) {
        val request = Request.Builder()
            .url("http://$ip/api/status")
            .get()
            .build()

        try {
            client.newCall(request).execute().use { response ->
                if (response.isSuccessful) {
                    val body = response.body?.string() ?: return@withContext null
                    return@withContext JSONObject(body)
                }
            }
        } catch (e: Exception) {
            Log.w(TAG, "Failed to fetch status from $ip: ${e.message}")
        }
        null
    }

    suspend fun setVolume(ip: String, volume: Int): Boolean = withContext(Dispatchers.IO) {
        val json = JSONObject().apply { put("volume", volume) }.toString()
        postJson(ip, "/api/audio/volume", json)
    }

    suspend fun setMute(ip: String, mute: Boolean): Boolean = withContext(Dispatchers.IO) {
        val json = JSONObject().apply { put("mute", mute) }.toString()
        postJson(ip, "/api/audio/mute", json)
    }

    suspend fun setLatencyMode(ip: String, mode: String): Boolean = withContext(Dispatchers.IO) {
        val json = JSONObject().apply { put("mode", mode) }.toString()
        postJson(ip, "/api/audio/latency", json)
    }

    suspend fun rebootReceiver(ip: String): Boolean = withContext(Dispatchers.IO) {
        postJson(ip, "/api/system/reboot", "{}")
    }

    suspend fun uploadFirmwareOta(ip: String, fileName: String, fileBytes: ByteArray): Result<String> = withContext(Dispatchers.IO) {
        try {
            val body = okhttp3.MultipartBody.Builder()
                .setType(okhttp3.MultipartBody.FORM)
                .addFormDataPart(
                    "update",
                    fileName,
                    fileBytes.toRequestBody("application/octet-stream".toMediaType())
                )
                .build()

            val request = Request.Builder()
                .url("http://$ip/api/ota")
                .post(body)
                .build()

            val otaClient = client.newBuilder()
                .connectTimeout(15, TimeUnit.SECONDS)
                .writeTimeout(90, TimeUnit.SECONDS)
                .readTimeout(90, TimeUnit.SECONDS)
                .build()

            otaClient.newCall(request).execute().use { response ->
                val respStr = response.body?.string() ?: ""
                if (response.isSuccessful) {
                    Result.success(respStr.ifBlank { "OTA update pushed successfully! SuperMini rebooting." })
                } else {
                    Result.failure(Exception("OTA failed with status ${response.code}: $respStr"))
                }
            }
        } catch (e: Exception) {
            Result.failure(e)
        }
    }

    private fun postJson(ip: String, endpoint: String, json: String): Boolean {
        val request = Request.Builder()
            .url("http://$ip$endpoint")
            .post(json.toRequestBody(JSON_MEDIA_TYPE))
            .build()

        return try {
            client.newCall(request).execute().use { response ->
                response.isSuccessful
            }
        } catch (e: Exception) {
            Log.w(TAG, "POST $endpoint failed to $ip: ${e.message}")
            false
        }
    }
}
