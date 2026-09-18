/**
 * @file audio_output.cpp
 * @brief I2S audio output implementation targeting NXP UDA1334A DAC.
 */

#include "audio_output.h"
#include "pcm_buffer.h"
#include "pcm_receiver.h"
#include <esp_log.h>
#include <driver/i2s.h>
#include <string.h>

static const char* TAG = "[I2S]";

AudioOutput g_audio_output;

AudioOutput::AudioOutput()
    : sampleRate_(AUDIO_DEFAULT_SAMPLE_RATE),
      bitsPerSample_(AUDIO_DEFAULT_BITS_PER_SAMPLE),
      channels_(AUDIO_DEFAULT_CHANNELS),
      volume_(100),
      muted_(false),
      running_(false),
      taskHandle_(nullptr),
      volumeScale_(256) {
    memset(dmaScratchBuffer_, 0, sizeof(dmaScratchBuffer_));
}

AudioOutput::~AudioOutput() {
    stop();
}

bool AudioOutput::init(uint32_t sampleRate, uint8_t bitsPerSample, uint8_t channels) {
    sampleRate_ = sampleRate;
    bitsPerSample_ = bitsPerSample;
    channels_ = channels;

    ESP_LOGI(TAG, "Initializing I2S for NXP UDA1334A: %u Hz, %d-bit, %d channels",
             (unsigned int)sampleRate_, (int)bitsPerSample_, (int)channels_);
    ESP_LOGI(TAG, "I2S Pins: BCLK=GPIO%d, WS/LRCK=GPIO%d, DOUT=GPIO%d",
             I2S_BCLK_PIN, I2S_WS_PIN, I2S_DOUT_PIN);

    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = sampleRate_,
        .bits_per_sample = (i2s_bits_per_sample_t)bitsPerSample_,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 6,
        .dma_buf_len = 256,
        .use_apll = false,
        .tx_desc_auto_clear = true,
        .fixed_mclk = 0
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_WS_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE
    };

    esp_err_t err = i2s_driver_install(I2S_PORT_NUM, &i2s_config, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_driver_install failed: %d", err);
        return false;
    }

    err = i2s_set_pin(I2S_PORT_NUM, &pin_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2s_set_pin failed: %d", err);
        i2s_driver_uninstall(I2S_PORT_NUM);
        return false;
    }

    i2s_zero_dma_buffer(I2S_PORT_NUM);

    running_ = true;
    BaseType_t res = xTaskCreatePinnedToCore(
        audioTaskTrampoline,
        "wfhf_audio_out",
        TASK_AUDIO_STACK_SIZE,
        this,
        TASK_AUDIO_PRIORITY,
        &taskHandle_,
        0 // ESP32-C3 is single core RISC-V
    );

    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio output task");
        i2s_driver_uninstall(I2S_PORT_NUM);
        running_ = false;
        return false;
    }

    ESP_LOGI(TAG, "I2S Audio Output task running at priority %d", TASK_AUDIO_PRIORITY);
    return true;
}

void AudioOutput::setVolume(uint8_t volume) {
    if (volume > 100) volume = 100;
    volume_ = volume;
    // Map 0-100 to 0-256 for fast bit shift scaling
    volumeScale_ = (uint16_t)((volume * 256) / 100);
    ESP_LOGI(TAG, "Volume changed to %d%% (scale=%d)", volume_, volumeScale_);
}

void AudioOutput::setMute(bool mute) {
    muted_ = mute;
    ESP_LOGI(TAG, "Mute state changed to %s", muted_ ? "MUTED" : "UNMUTED");
}

void AudioOutput::stop() {
    running_ = false;
    if (taskHandle_) {
        vTaskDelete(taskHandle_);
        taskHandle_ = nullptr;
    }
    i2s_driver_uninstall(I2S_PORT_NUM);
}

void AudioOutput::audioTaskTrampoline(void* arg) {
    AudioOutput* instance = static_cast<AudioOutput*>(arg);
    instance->audioTaskLoop();
}

void AudioOutput::audioTaskLoop() {
    size_t bytesWritten = 0;
    const size_t sampleCount = DMA_CHUNK_BYTES / sizeof(int16_t);

    while (running_) {
        // When not actively receiving audio stream, yield CPU to FreeRTOS IDLE task
        // to prevent Task Watchdog Timer triggers on single-core ESP32-C3
        if (!g_pcm_receiver.isStreaming()) {
            memset(dmaScratchBuffer_, 0, DMA_CHUNK_BYTES);
            i2s_write(I2S_PORT_NUM, dmaScratchBuffer_, DMA_CHUNK_BYTES, &bytesWritten, pdMS_TO_TICKS(50));
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Read directly from jitter buffer into pre-allocated scratch buffer
        g_pcm_buffer.read((uint8_t*)dmaScratchBuffer_, DMA_CHUNK_BYTES);

        // Apply digital attenuation or mute in fast integer math
        if (muted_ || volumeScale_ == 0) {
            memset(dmaScratchBuffer_, 0, DMA_CHUNK_BYTES);
        } else if (volumeScale_ < 256) {
            for (size_t i = 0; i < sampleCount; i++) {
                int32_t val = (int32_t)dmaScratchBuffer_[i];
                dmaScratchBuffer_[i] = (int16_t)((val * volumeScale_) >> 8);
            }
        }

        // Send to I2S DMA
        i2s_write(I2S_PORT_NUM, dmaScratchBuffer_, DMA_CHUNK_BYTES, &bytesWritten, portMAX_DELAY);
    }

    vTaskDelete(NULL);
}
