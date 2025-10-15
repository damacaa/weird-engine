#include "weird-renderer/AudioEngine.h"

#include <iostream>
#include <vector>

#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#define CHANNELS    2
#define SAMPLE_RATE 44100

namespace WeirdEngine {
    namespace WeirdRenderer {
        AudioEngine::AudioEngine() {}

        AudioEngine::~AudioEngine() {
            ma_sound_uninit(&g_sound);
            ma_engine_uninit(&g_engine);
        }

        bool AudioEngine::init() {
            ma_result result;
            ma_engine_config engineConfig;

            engineConfig = ma_engine_config_init();
            engineConfig.noDevice   = MA_TRUE;
            engineConfig.channels   = CHANNELS;
            engineConfig.sampleRate = SAMPLE_RATE;

            result = ma_engine_init(&engineConfig, &g_engine);
            if (result != MA_SUCCESS) {
                std::cerr << "Failed to initialize audio engine" << std::endl;
                return false;
            }

            return true;
        }

        void AudioEngine::loadSound(const char* filePath) {
            ma_result result = ma_sound_init_from_file(&g_engine, filePath, 0, NULL, NULL, &g_sound);
            if (result != MA_SUCCESS) {
                ma_engine_uninit(&g_engine);
                throw std::runtime_error("Failed to initialize sound");
            }

            ma_sound_set_looping(&g_sound, MA_TRUE);
            ma_sound_start(&g_sound);
        }

        ma_uint32 AudioEngine::getSampleRate() const {
            return ma_engine_get_sample_rate(&g_engine);
        }

        ma_uint8 AudioEngine::getChannels() const {
            return ma_engine_get_channels(&g_engine);
        }

        void AudioEngine::data_callback(void* pUserData, SDL_AudioStream* stream, int additional_amount, int total_amount)
        {

            // AudioEngine* audioEngine = static_cast<AudioEngine*>(pUserData);
            // if (!audioEngine) return;
            //
            // ma_uint32 framesToRead;
            // ma_uint32 bytesToRead = (ma_uint32)total_amount;
            //
            // framesToRead = bytesToRead / ma_get_bytes_per_frame(ma_format_f32, ma_engine_get_channels(&audioEngine->g_engine));
            //
            // float tempBuffer[512 * CHANNELS];
            //
            // if (framesToRead > 512) framesToRead = 512;
            //
            // ma_engine_read_pcm_frames(&audioEngine->g_engine, tempBuffer, framesToRead, NULL);
            //
            // SDL_PutAudioStreamData(stream, tempBuffer, framesToRead * ma_get_bytes_per_frame(ma_format_f32, ma_engine_get_channels(&audioEngine->g_engine)));


            static float phase = 0.0f;

            const float freq         = 40.0f;   // Sine frequency (Hz)
            const float amplitude    = 0.2f;     // Sine amplitude
            const float noise_amp    = 0.001f;    // Noise amplitude (lower = more subtle)
            const float phaseInc     = 2.0f * M_PI * freq / SAMPLE_RATE;

            ma_uint32 bytesToWrite   = (ma_uint32)total_amount;
            ma_uint32 framesToWrite  = bytesToWrite / ma_get_bytes_per_frame(ma_format_f32, CHANNELS);

            std::vector<float> tempBuffer(framesToWrite * CHANNELS);

            for (ma_uint32 i = 0; i < framesToWrite; ++i) {
                // Base sine sample
                float sineSample = amplitude * sinf(phase);

                // White noise sample (-1.0 to 1.0)
                float noiseSample = noise_amp * ((float)rand() / RAND_MAX * 2.0f - 1.0f);

                // Combine
                float sample = sineSample + noiseSample;

                // Update phase and wrap
                phase += phaseInc;
                if (phase >= 2.0f * M_PI) phase -= 2.0f * M_PI;

                // Stereo output
                for (int ch = 0; ch < CHANNELS; ++ch)
                    tempBuffer[i * CHANNELS + ch] = sample;
            }

            SDL_PutAudioStreamData(stream, tempBuffer.data(),
                framesToWrite * ma_get_bytes_per_frame(ma_format_f32, CHANNELS));
        }
    }
}