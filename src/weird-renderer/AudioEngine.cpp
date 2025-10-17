#include "weird-renderer/AudioEngine.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>

#define MA_NO_DEVICE_IO
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio/miniaudio.h>

#define CHANNELS    2
#define SAMPLE_RATE 44100

namespace WeirdEngine {
namespace WeirdRenderer {

AudioEngine::AudioEngine() {}

AudioEngine::~AudioEngine() {
    ma_noise_uninit(&g_noise, nullptr);
    ma_sound_uninit(&g_sound);
    ma_engine_uninit(&g_engine);
}

bool AudioEngine::init() {
    ma_result result;
    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.noDevice   = MA_TRUE;
    engineConfig.channels   = CHANNELS;
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
        ma_noise_type_white,
        0,          // seed
        1.0f        // amplitude
    );

    ma_result noiseResult = ma_noise_init(&noiseConfig, nullptr, &g_noise);
    if (noiseResult != MA_SUCCESS) {
        std::cerr << "Failed to initialize noise generator" << std::endl;
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
    const float MAX_FRICTION = 0.5f;
    const float MIN_AUDIBLE = 0.05f;
    const float MAX_AMPLITUDE = 0.3f;

    level = std::clamp(level, 0.0f, MAX_FRICTION);

    static int curveMode = 6;



    switch (curveMode) {

        case 0: {
            frictionLevel = std::min(0.5f * level, 0.1f);
            break;
        }

        case 1: {
            float normalizedFriction = std::min(level / MAX_FRICTION, 1.0f);
            float amplitude = MIN_AUDIBLE + (MAX_AMPLITUDE - MIN_AUDIBLE) * pow(normalizedFriction, 2.0f);
            frictionLevel = amplitude;
            break;
        }

        case 2: {
            float normalizedFriction = std::min(level / MAX_FRICTION, 1.0f);
            float amplitude = MIN_AUDIBLE + (MAX_AMPLITUDE - MIN_AUDIBLE) * pow(normalizedFriction, 3.0f);
            frictionLevel = amplitude;
            break;
        }

        case 3: {
            float normalizedFriction = std::min(level / MAX_FRICTION, 1.0f);
            if (normalizedFriction > 0) {
                float amplitude = MIN_AUDIBLE * pow((MAX_AMPLITUDE / MIN_AUDIBLE), normalizedFriction);
                frictionLevel = amplitude;
            } else {
                frictionLevel = 0.0f;
            }
            break;
        }

        case 4: {
            const float minFrictionLog = 0.01f;
            if (level > minFrictionLog) {
                float normalizedFriction = std::min(level / MAX_FRICTION, 1.0f);
                float logFriction = log(normalizedFriction * (MAX_FRICTION - minFrictionLog) + minFrictionLog);
                float logMin = log(minFrictionLog);
                float logMax = log(MAX_FRICTION);

                float range = logMax - logMin;
                if (range > 0) {
                    float normalizedLog = (logFriction - logMin) / range;
                    float amplitude = MIN_AUDIBLE + (MAX_AMPLITUDE - MIN_AUDIBLE) * normalizedLog;
                    frictionLevel = amplitude;
                } else {
                    frictionLevel = MIN_AUDIBLE;
                }
            } else {
                frictionLevel = 0.0f;
            }
            break;
        }

        case 5: {
            float threshold = 0.4f;
            float ratio = 4.0f;

            float normalizedFriction = std::min(level / MAX_FRICTION, 1.0f);
            float initialAmplitude = MAX_AMPLITUDE * pow(normalizedFriction, 2.0f);

            if (initialAmplitude > threshold) {
                float overshoot = initialAmplitude - threshold;
                float compressedOvershoot = overshoot / ratio;
                float finalAmplitude = threshold + compressedOvershoot;
                frictionLevel = finalAmplitude;
            } else {
                frictionLevel = initialAmplitude;
            }
            break;
        }

        case 6: {
            float threshold = 0.25f;
            float ratio = 4.0f;

            float lowEndBoostExponent = 0.75f;
            float normalizedFriction = std::min(level / MAX_FRICTION, 1.0f);
            float initialAmplitude = (MAX_AMPLITUDE - MIN_AUDIBLE) * pow(normalizedFriction, lowEndBoostExponent);

            if (initialAmplitude > threshold) {
                float overshoot = initialAmplitude - threshold;
                float compressedOvershoot = overshoot / ratio;
                float finalAmplitude = threshold + compressedOvershoot;
                frictionLevel = finalAmplitude;
            } else {
                frictionLevel = initialAmplitude;
            }
            break;
        }

        default: {
            curveMode = 0;
            break;
        }
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

void AudioEngine::data_callback(void* pUserData, SDL_AudioStream* stream, int additional_amount, int total_amount) {
    AudioEngine* audio = static_cast<AudioEngine*>(pUserData);
    if (!audio) return;

    ma_uint32 bytesToWrite = (ma_uint32)total_amount;
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
