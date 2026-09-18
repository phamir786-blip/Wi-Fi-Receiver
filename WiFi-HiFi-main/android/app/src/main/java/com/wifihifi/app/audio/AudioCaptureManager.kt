package com.wifihifi.app.audio

import android.annotation.SuppressLint
import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioPlaybackCaptureConfiguration
import android.media.AudioRecord
import android.media.projection.MediaProjection
import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Manages real-time audio playback capture on Android 16+ using AudioPlaybackCapture API.
 */
class AudioCaptureManager(
    private val mediaProjection: MediaProjection,
    private val packetizer: PcmPacketizer,
    private val onError: (String) -> Unit
) {
    companion object {
        private const val TAG = "AudioCaptureMgr"
        private const val TARGET_SAMPLE_RATE = 44100
        private const val NATIVE_SAMPLE_RATE = 44100
        private const val CHANNELS = AudioFormat.CHANNEL_IN_STEREO
        private const val ENCODING = AudioFormat.ENCODING_PCM_16BIT
    }

    private var audioRecord: AudioRecord? = null
    private var isRecording = false
    private var captureThread: Thread? = null

    private val projectionCallback = object : MediaProjection.Callback() {
        override fun onStop() {
            Log.i(TAG, "MediaProjection stopped by system")
            stopCapture()
        }
    }

    @SuppressLint("MissingPermission")
    fun startCapture(): Boolean {
        try {
            // Register callback required on Android 14+
            try {
                mediaProjection.registerCallback(projectionCallback, android.os.Handler(android.os.Looper.getMainLooper()))
            } catch (e: Exception) {
                Log.w(TAG, "Could not register projection callback: ${e.message}")
            }

            // Build AudioPlaybackCaptureConfiguration for Media & Games
            val config = AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                .addMatchingUsage(AudioAttributes.USAGE_GAME)
                .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                .build()

            val minBufferSize = AudioRecord.getMinBufferSize(
                TARGET_SAMPLE_RATE,
                CHANNELS,
                ENCODING
            )

            if (minBufferSize <= 0) {
                onError("AudioRecord minBufferSize returned error: $minBufferSize. Check device audio hardware.")
                return false
            }

            // Buffer sized for 50ms of audio
            val bufferSize = maxOf(minBufferSize * 2, TARGET_SAMPLE_RATE * 4 / 20)

            val audioFormat = AudioFormat.Builder()
                .setEncoding(ENCODING)
                .setSampleRate(TARGET_SAMPLE_RATE)
                .setChannelMask(CHANNELS)
                .build()

            audioRecord = AudioRecord.Builder()
                .setAudioPlaybackCaptureConfig(config)
                .setAudioFormat(audioFormat)
                .setBufferSizeInBytes(bufferSize)
                .build()

            if (audioRecord?.state != AudioRecord.STATE_INITIALIZED) {
                onError("AudioRecord failed to initialize. Android policy may disallow capture on this source.")
                audioRecord?.release()
                audioRecord = null
                return false
            }

            audioRecord?.startRecording()
            isRecording = true

            // Dedicated high-priority capture thread
            captureThread = Thread({
                android.os.Process.setThreadPriority(android.os.Process.THREAD_PRIORITY_URGENT_AUDIO)
                captureLoop()
            }, "WiFiHiFi-AudioCapture").apply { start() }

            Log.i(TAG, "AudioPlaybackCapture active at $TARGET_SAMPLE_RATE Hz, 16-bit Stereo")
            return true

        } catch (e: SecurityException) {
            Log.e(TAG, "SecurityException starting AudioPlaybackCapture", e)
            onError("Permission denied for MediaProjection capture: ${e.message}")
            return false
        } catch (e: Exception) {
            Log.e(TAG, "Failed to start audio capture", e)
            onError("Audio capture initialization error: ${e.message}")
            return false
        }
    }

    private fun captureLoop() {
        val record = audioRecord ?: return
        val readBufferSize = 1764 // 10ms chunk
        val byteBuffer = ByteArray(readBufferSize)

        while (isRecording) {
            val bytesRead = record.read(byteBuffer, 0, byteBuffer.size, AudioRecord.READ_BLOCKING)

            if (bytesRead > 0) {
                // Pass directly to packetizer
                packetizer.feedPcm(byteBuffer, 0, bytesRead)
            } else if (bytesRead < 0) {
                when (bytesRead) {
                    AudioRecord.ERROR_INVALID_OPERATION -> {
                        onError("AudioRecord: Invalid operation. Capture stream may be blocked by active app.")
                        break
                    }
                    AudioRecord.ERROR_BAD_VALUE -> {
                        onError("AudioRecord: Bad parameter value.")
                        break
                    }
                    AudioRecord.ERROR_DEAD_OBJECT -> {
                        onError("AudioRecord: Audio service died or MediaProjection was revoked.")
                        break
                    }
                }
            }
        }
    }

    fun stopCapture() {
        isRecording = false
        captureThread?.interrupt()
        captureThread = null

        try {
            mediaProjection.unregisterCallback(projectionCallback)
        } catch (e: Exception) {
            // Ignore
        }

        try {
            audioRecord?.stop()
            audioRecord?.release()
        } catch (e: Exception) {
            Log.w(TAG, "Exception stopping AudioRecord", e)
        } finally {
            audioRecord = null
        }
        Log.i(TAG, "AudioPlaybackCapture stopped")
    }
}
