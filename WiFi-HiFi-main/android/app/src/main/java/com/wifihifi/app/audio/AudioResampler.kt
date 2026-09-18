package com.wifihifi.app.audio

/**
 * High-performance, zero-allocation linear interpolation PCM resampler for 16-bit Stereo.
 */
class AudioResampler(
    val inSampleRate: Int,
    val outSampleRate: Int = 44100
) {
    private val ratio = inSampleRate.toDouble() / outSampleRate.toDouble()

    /**
     * Resamples 16-bit interleaved stereo PCM.
     * @param inPcm Input short array (interleaved L, R, L, R)
     * @param inShorts Number of valid shorts in inPcm
     * @param outPcm Pre-allocated destination short array
     * @return Number of output shorts written to outPcm
     */
    fun resampleStereo(inPcm: ShortArray, inShorts: Int, outPcm: ShortArray): Int {
        if (inSampleRate == outSampleRate) {
            val count = minOf(inShorts, outPcm.size)
            System.arraycopy(inPcm, 0, outPcm, 0, count)
            return count
        }

        val inFrames = inShorts / 2
        val maxOutFrames = outPcm.size / 2
        var outFrameIdx = 0

        var inPosition = 0.0

        while (outFrameIdx < maxOutFrames) {
            val inIdx = inPosition.toInt()
            if (inIdx >= inFrames - 1) break

            val frac = inPosition - inIdx
            val left1 = inPcm[inIdx * 2].toInt()
            val left2 = inPcm[(inIdx + 1) * 2].toInt()
            val right1 = inPcm[inIdx * 2 + 1].toInt()
            val right2 = inPcm[(inIdx + 1) * 2 + 1].toInt()

            val leftOut = (left1 + frac * (left2 - left1)).toInt()
            val rightOut = (right1 + frac * (right2 - right1)).toInt()

            outPcm[outFrameIdx * 2] = leftOut.coerceIn(Short.MIN_VALUE.toInt(), Short.MAX_VALUE.toInt()).toShort()
            outPcm[outFrameIdx * 2 + 1] = rightOut.coerceIn(Short.MIN_VALUE.toInt(), Short.MAX_VALUE.toInt()).toShort()

            outFrameIdx++
            inPosition += ratio
        }

        return outFrameIdx * 2
    }
}
