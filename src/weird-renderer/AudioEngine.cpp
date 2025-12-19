#include "weird-renderer/AudioEngine.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#include "weird-engine/Input.h"

#define CHANNELS    2
#define SAMPLE_RATE 44100

#ifndef M_PI
#define M_PI 3.14159265359f
#endif // !M_PI


namespace WeirdEngine {
    namespace WeirdRenderer {
        AudioEngine::AudioEngine() {
        }

        AudioEngine::~AudioEngine() {

        }

        bool AudioEngine::init() {
            ma_result result;
            ma_engine_config engineConfig = ma_engine_config_init();
            engineConfig.noDevice = MA_TRUE;
            engineConfig.channels = CHANNELS;
            engineConfig.sampleRate = SAMPLE_RATE;

            result = ma_engine_init(&engineConfig, &g_engine);
            if (result != MA_SUCCESS) {
                std::cerr << "Failed to initialize audio engine" << std::endl;
                return false;
            }

            // Initialize a persistent noise generator for friction
            ma_noise_config noiseConfig = ma_noise_config_init(
                ma_format_f32,
                CHANNELS,
                ma_noise_type_pink,
                0, // seed
                1.0f // amplitude
            );

            ma_result noiseResult = ma_noise_init(&noiseConfig, nullptr, &g_noise);
            if (noiseResult != MA_SUCCESS) {
                std::cerr << "Failed to initialize noise generator" << std::endl;
                return false;
            }

            return true;
        }

        void AudioEngine::close()
        {
            ma_noise_uninit(&g_noise, nullptr);
            ma_sound_uninit(&g_sound);
            ma_engine_uninit(&g_engine);
        }

        void AudioEngine::loadSound(const char *filePath) {
            ma_result result = ma_sound_init_from_file(&g_engine, filePath, 0, NULL, NULL, &g_sound);
            if (result != MA_SUCCESS) {
                ma_engine_uninit(&g_engine);
                throw std::runtime_error("Failed to initialize sound");
            }

            ma_sound_set_looping(&g_sound, MA_TRUE);
            ma_sound_set_volume(&g_sound, 0.0);
            ma_sound_start(&g_sound);
        }

        ma_uint32 AudioEngine::getSampleRate() const {
            return ma_engine_get_sample_rate(&g_engine);
        }

        ma_uint8 AudioEngine::getChannels() const {
            return ma_engine_get_channels(&g_engine);
        }

        // ------------------- Procedural Controls -------------------

        void AudioEngine::setFrictionLevel(float level)
        {
            // Normalize
            constexpr float MAX_FRICTION = 1.0f;
            const float normalizedFriction = (std::min)(level / MAX_FRICTION, 1.0f);

            // Remove min audible and compensate
            constexpr float MIN_AUDIBLE = 0.01f;
            constexpr float MIN_COMPENSATION = 1.0f / (1.0f - MIN_AUDIBLE);
            const float adjustedAmplitude = (std::max)(normalizedFriction - MIN_AUDIBLE, 0.0f) * MIN_COMPENSATION;

            // Use a square root curve to boost the volume of low values significantly
            // 0.5f = Strong boost (square root)
            // 0.75f = Medium boost
            // 1.0f = No boost (linear)
            constexpr float LOW_END_BOOST_EXPONENT = 0.5f;
            constexpr float MAX_AMPLITUDE = 0.75f;
            const float initialAmplitude = MAX_AMPLITUDE * pow(adjustedAmplitude, LOW_END_BOOST_EXPONENT);

            // Compression will tame the louder signal once it crosses the threshold, preserving top-end
            constexpr float COMPRESSION_THRESHOLD = 0.3f;
            constexpr float COMPRESSION_RATIO = 4.0f;
            if (initialAmplitude > COMPRESSION_THRESHOLD)
            {
                float overshoot = initialAmplitude - COMPRESSION_THRESHOLD;
                float compressedOvershoot = overshoot / COMPRESSION_RATIO;
                float finalAmplitude = COMPRESSION_THRESHOLD + compressedOvershoot;
                frictionLevel = finalAmplitude;
            } else
            {
                frictionLevel = initialAmplitude;
            }
        }


        void AudioEngine::triggerCollision(float freq, float amp, float decaySec) {
            collisionFreq = freq;
            collisionAmp = amp;
            collisionDecay = decaySec;
            collisionTime = 0.0f;
            collisionPhase = 0.0f;
        }

        // ------------------- Audio Mixing Callback -------------------

        void AudioEngine::data_callback(void *pUserData, SDL_AudioStream *stream, int additional_amount,
                                        int total_amount) {
            AudioEngine *audio = static_cast<AudioEngine *>(pUserData);
            if (!audio) return;

            ma_uint32 bytesToWrite = (ma_uint32) total_amount;
            ma_uint32 framesToWrite = bytesToWrite / ma_get_bytes_per_frame(ma_format_f32, CHANNELS);

            std::vector<float> mix(framesToWrite * CHANNELS, 0.0f);
            std::vector<float> temp(framesToWrite * CHANNELS);

            // --- 1. Mix background music from miniaudio engine ---
            ma_engine_read_pcm_frames(&audio->g_engine, mix.data(), framesToWrite, NULL);

            // --- 2. Friction noise (continuous, amplitude-modulated) ---
            audio->smoothedFriction += (audio->frictionLevel - audio->smoothedFriction) * 0.05f;
            if (audio->smoothedFriction > 0.0001f) {
                ma_noise_read_pcm_frames(&audio->g_noise, temp.data(), framesToWrite, NULL);
                for (ma_uint32 i = 0; i < framesToWrite * CHANNELS; ++i) {
                    mix[i] += temp[i] * audio->smoothedFriction * 0.4f; // scale down a bit
                }
            }

            // --- 3. Collision tone (decaying sine) ---
            if (audio->collisionAmp > 0.0001f) {
                const float phaseInc = 2.0f * M_PI * audio->collisionFreq / SAMPLE_RATE;
                for (ma_uint32 i = 0; i < framesToWrite; ++i) {
                    float env = audio->collisionAmp * expf(-audio->collisionTime / audio->collisionDecay);
                    float sample = env * sinf(audio->collisionPhase);
                    audio->collisionPhase += phaseInc;
                    if (audio->collisionPhase >= 2.0f * M_PI) audio->collisionPhase -= 2.0f * M_PI;
                    audio->collisionTime += 1.0f / SAMPLE_RATE;

                    for (int ch = 0; ch < CHANNELS; ++ch)
                        mix[i * CHANNELS + ch] += sample;
                }

                // auto stop when amplitude decays enough
                if (audio->collisionAmp * expf(-audio->collisionTime / audio->collisionDecay) < 0.0001f)
                    audio->collisionAmp = 0.0f;
            }

            // --- 4. Submit mixed result to SDL ---
            SDL_PutAudioStreamData(stream, mix.data(),
                                   framesToWrite * ma_get_bytes_per_frame(ma_format_f32, CHANNELS));
        }
    } // namespace WeirdRenderer
} // namespace WeirdEngine
