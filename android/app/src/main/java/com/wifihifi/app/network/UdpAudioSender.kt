package com.wifihifi.app.network

import android.util.Log
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.net.InetAddress
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicLong

/**
 * High-performance UDP packet transmitter for WiFi-HiFi audio streaming.
 */
class UdpAudioSender(
    private val targetIp: String,
    private val targetPort: Int = 50005
) {
    companion object {
        private const val TAG = "UdpAudioSender"
    }

    private var socket: DatagramSocket? = null
    private var inetAddress: InetAddress? = null
    private val preallocatedPacketArray = ByteArray(2048)
    private var datagramPacket: DatagramPacket? = null

    val totalPacketsSent = AtomicLong(0L)
    val totalBytesSent = AtomicLong(0L)

    fun open(): Boolean {
        return try {
            inetAddress = InetAddress.getByName(targetIp)
            socket = DatagramSocket().apply {
                sendBufferSize = 128 * 1024
                trafficClass = 0x10 // IPTOS_LOWDELAY
            }
            datagramPacket = DatagramPacket(preallocatedPacketArray, 0, inetAddress, targetPort)
            Log.i(TAG, "Opened UDP audio socket targeting $targetIp:$targetPort")
            true
        } catch (e: Exception) {
            Log.e(TAG, "Failed to open UDP socket to $targetIp:$targetPort", e)
            false
        }
    }

    fun sendPacket(buffer: ByteBuffer, length: Int) {
        val sock = socket ?: return
        val packet = datagramPacket ?: return

        try {
            buffer.position(0)
            buffer.get(preallocatedPacketArray, 0, length)

            packet.length = length
            sock.send(packet)

            totalPacketsSent.incrementAndGet()
            totalBytesSent.addAndGet(length.toLong())
        } catch (e: Exception) {
            Log.w(TAG, "Error sending UDP packet: ${e.message}")
        }
    }

    fun close() {
        try {
            socket?.close()
        } catch (e: Exception) {
            Log.w(TAG, "Error closing UDP socket", e)
        } finally {
            socket = null
            datagramPacket = null
        }
        Log.i(TAG, "UDP audio socket closed")
    }
}
