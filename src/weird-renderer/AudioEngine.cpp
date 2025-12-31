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

WeirdEngine::WeirdRenderer::AudioEngine* g_instance;

namespace WeirdEngine {
    namespace WeirdRenderer {
        AudioEngine::AudioEngine() {
            g_instance = this;
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

        // Standard Reference: A4 = 440Hz
        const float A4_FREQ = 440.0f;
        const int A4_MIDI = 69;

        // Helper: Convert Frequency to MIDI Note Number (e.g., 60.0 is Middle C)
        float freqToMidi(float freq)
        {
            return 69.0f + 12.0f * std::log2(freq / 440.0f);
        }

        // Helper: Convert MIDI Note Number back to Frequency
        float midiToFreq(float midiNote)
        {
            return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
        }

        /**
         * rounds the input frequency to the nearest note in the C Major Pentatonic scale.
         * Pentatonic scales are famous because almost any combination of notes
         * within them sounds "pleasant" together.
         */
        float getPleasantFrequency(float inputFreq)
        {
            // 1. Convert input frequency to a continuous MIDI note value
            float continuousNote = freqToMidi(inputFreq);

            // 2. Round to nearest integer (nearest semitone)
            int roundedNote = static_cast<int>(std::round(continuousNote));

            // 3. Define a "Safe" Scale (C Major Pentatonic: C, D, E, G, A)
            // Notes relative to C: 0, 2, 4, 7, 9
            // This removes notes that create high tension (like F and B)
            std::vector<int> allowedIntervals = {0, 2, 4, 7, 9};

            // Find the note relative to C (MIDI % 12)
            // We strictly want notes where (note % 12) matches our allowed intervals
            // If the current note isn't allowed, find the closest one that is.

            int closestNote = roundedNote;
            int minDistance = 100;

            // Search neighboring notes to find the closest allowed note
            for (int offset = -2; offset <= 2; ++offset)
            {
                int candidate = roundedNote + offset;
                int interval = candidate % 12; // Modulo 12 gets the note name (C, C#, etc.)

                // Handle negative modulo results if freq is very low
                if (interval < 0) interval += 12;

                // Check if this interval is in our allowed list
                for (int allowed : allowedIntervals)
                {
                    if (interval == allowed)
                    {
                        // If this allowed note is closer to original, pick it
                        if (std::abs(candidate - continuousNote) < minDistance)
                        {
                            minDistance = std::abs(candidate - continuousNote);
                            closestNote = candidate;
                        }
                    }
                }
            }

            // 4. Convert back to frequency
            return midiToFreq(static_cast<float>(closestNote));
        }

        void AudioEngine::listen(Scene& scene)
        {
            if (Input::GetKeyDown(Input::C))
                playSineSound(getPleasantFrequency(200.0f), 1.0f, 0.1f);

            float frictionValue = scene.getFrictionSound();
            setFrictionLevel(frictionValue);

            auto& audioQueue = scene.getAudioQueue();

            static int buffered = 0;
            static SimpleAudioRequest buffered_request{0.0f, 0.0f, true, vec3(0.0f)};
            static float nextTime = 0.0f;
            const static float MIN_TIME = 0.2f;
            const static float MAX_TIME = 1.0f;

            float time = SDL_GetTicks() / 1000.0f;

            if (activeVoices.size() > 0 && (time < nextTime))
            {

                SimpleAudioRequest aux{0,0,true, vec3(0.0f)};
                while (audioQueue.pop(aux))
                {
                    buffered++;
                    buffered_request.volume += aux.volume;
                    buffered_request.frequency = (std::max)(aux.frequency, buffered_request.frequency);
                    buffered_request.position += buffered_request.position;
                }

                return;
            }

            if (buffered)
            {
                SimpleAudioRequest aux{0,0,true, vec3(0.0f)};
                while (audioQueue.pop(aux))
                {
                    buffered++;
                    buffered_request.volume += aux.volume;
                    buffered_request.frequency = (std::max)(aux.frequency, buffered_request.frequency);
                    buffered_request.position += buffered_request.position;
                }

                float invBufferedAmount = 1.0f / static_cast<float>(buffered + 1);
                // buffered_request.volume *= invBufferedAmount;
                buffered_request.volume = (std::min)(buffered_request.volume, 1.0f);
                // buffered_request.frequency *= invBufferedAmount;
                buffered_request.position *= invBufferedAmount;
                audioQueue.push(buffered_request);
                // std::cout << "Playing: " << static_cast<int>(buffered_request.volume * 100) << "% -> " << buffered_request.frequency << "Hz" << std::endl;

                buffered = 0;
                buffered_request.volume = 0.0f;
                buffered_request.frequency = 0.0f;
                buffered_request.position = vec3(0.0f);
            }

            // std::cout << activeVoices.size() << std::endl;


            auto cameraPosition = scene.getCamera().position;

            // Process queue
            SimpleAudioRequest request{};
            while (audioQueue.pop(request))
            {
                float frequency = getPleasantFrequency(request.frequency);
                float amplitude = request.volume;
                if (request.spatial)
                {
                    // 1. Calculate Distances
                    // We need squared distance for spreading, and linear distance for absorption
                    float distSq = glm::distance2(request.position, cameraPosition);
                    float dist = std::sqrt(distSq);

                    // --- PHYSICS CONSTANTS (Tune these to your game scale) ---
                    // Controls how fast sound dies in general (Inverse Square Law)
                    const float FALLOFF_GEOMETRIC = 0.001f;

                    // Controls how much air "eats" sound.
                    // Real world is approx 0.000002, but in games use higher values (e.g. 0.00005)
                    // to exaggerate the effect so players notice it.
                    const float FALLOFF_AIR_ABSORPTION = 0.00005f;

                    // 2. Geometric Spreading (Inverse Distance Model)
                    // Energy dissipates over area. Affects all frequencies equally.
                    // Using (1.0 + ...) prevents division by zero at close range.
                    float geometricFactor = 1.0f / (1.0f + (FALLOFF_GEOMETRIC * distSq));

                    // 3. Atmospheric Absorption (Exponential Decay)
                    // Formula: e^(-coefficient * frequency * distance)
                    // Higher Freq = Faster Decay. Further Dist = More Decay.
                    float absorptionFactor = std::exp(-FALLOFF_AIR_ABSORPTION * frequency * dist);

                    // Combine factors
                    amplitude = request.volume * geometricFactor * absorptionFactor;
                }

                // Optimization: Don't play sounds that are effectively silent
                if (amplitude > 0.01f)
                {
                    float randLength = static_cast<float>(std::rand() % 2) + 1.0f;

                    float decay = 0.15f * randLength;
                    nextTime = time + (MIN_TIME * randLength);
                    playSineSound(frequency, amplitude, decay);
                    return;
                }
            }
        }

        // ------------------- Procedural Controls -------------------

        void AudioEngine::setFrictionLevel(float level)
        {
            // Normalize
            constexpr float MAX_FRICTION = 1.0f;
            const float normalizedFriction = (std::min)(level / MAX_FRICTION, 1.0f);

            // Remove min audible and compensate
            constexpr float MIN_AUDIBLE = 0.001f;
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


        void AudioEngine::playSineSound(float freq, float amp, float decaySec) {
            // Lock the mutex so the audio thread doesn't read while we write
            std::lock_guard<std::mutex> lock(voiceMutex);

            // Optional: Limit max polyphony to prevent CPU overload
            if (activeVoices.size() > 4) return;

            CollisionVoice newVoice;
            newVoice.frequency = freq;
            newVoice.amplitude = amp;
            newVoice.decay = decaySec;
            newVoice.time = 0.0f;
            newVoice.phase = 0.0f;
            newVoice.finished = false;

            activeVoices.push_back(newVoice);
        }

        AudioVisualData AudioEngine::getAudioVisuals()
        {
            std::lock_guard<std::mutex> lock(g_instance->visualMutex);
            return g_instance->visualSnapshot;
        }

        // ------------------- Updated Callback -------------------

        void AudioEngine::data_callback(void *pUserData, SDL_AudioStream *stream, int additional_amount, int total_amount) {
            AudioEngine *audio = static_cast<AudioEngine *>(pUserData);
            if (!audio) return;

            ma_uint32 bytesToWrite = (ma_uint32) total_amount;
            ma_uint32 framesToWrite = bytesToWrite / ma_get_bytes_per_frame(ma_format_f32, CHANNELS);

            // Initialize mix buffer with silence
            std::vector<float> mix(framesToWrite * CHANNELS, 0.0f);
            std::vector<float> temp(framesToWrite * CHANNELS);

            // --- 1. Mix background music (MiniAudio) ---
            ma_engine_read_pcm_frames(&audio->g_engine, mix.data(), framesToWrite, NULL);

            // --- 2. Friction noise ---
            audio->smoothedFriction += (audio->frictionLevel - audio->smoothedFriction) * 0.05f;
            if (audio->smoothedFriction > 0.0001f) {
                ma_noise_read_pcm_frames(&audio->g_noise, temp.data(), framesToWrite, NULL);
                for (ma_uint32 i = 0; i < framesToWrite * CHANNELS; ++i) {
                    mix[i] += temp[i] * audio->smoothedFriction * 0.4f;
                }
            }

            // --- 3. Collision Tones (Polyphonic) ---

            // Lock mutex to safely access the vector
            // Lock mutex to safely access the vector
            {
                std::lock_guard<std::mutex> lock(audio->voiceMutex);

                // Define a short attack time (e.g., 10ms) to prevent popping
                const float ATTACK_TIME = 0.01f;

                for (auto& voice : audio->activeVoices) {
                    if (voice.finished) continue;

                    const float phaseInc = 2.0f * M_PI * voice.frequency / SAMPLE_RATE;

                    for (ma_uint32 i = 0; i < framesToWrite; ++i) {
                        // 1. Calculate the standard decay envelope
                        float env = voice.amplitude * expf(-voice.time / voice.decay);

                        // 2. APPLY ATTACK RAMP (The Fix)
                        // If time is less than ATTACK_TIME, scale from 0.0 to 1.0
                        if (voice.time < ATTACK_TIME) {
                            env *= (voice.time / ATTACK_TIME);
                        }

                        float sample = env * sinf(voice.phase);

                        voice.phase += phaseInc;
                        if (voice.phase >= 2.0f * M_PI) voice.phase -= 2.0f * M_PI;
                        voice.time += 1.0f / SAMPLE_RATE;

                        // Add to both channels
                        for (int ch = 0; ch < CHANNELS; ++ch) {
                            mix[i * CHANNELS + ch] += sample;
                        }
                    }

                    // Check if voice is effectively silent
                    if (voice.amplitude * expf(-voice.time / voice.decay) < 0.001f) {
                        voice.finished = true;
                    }
                }

                // Cleanup finished voices to keep vector small
                // Using remove_if idiom
                audio->activeVoices.erase(
                    std::remove_if(audio->activeVoices.begin(), audio->activeVoices.end(),
                        [](const CollisionVoice& v) { return v.finished; }),
                    audio->activeVoices.end()
                );
            }

            // --- 4. HARD LIMITER / CLIPPING PROTECTION ---
            // Because we are adding multiple sounds, volume might exceed 1.0.
            // We apply a "soft clip" using tanh() or simple clamping to prevent distortion.
            for (size_t i = 0; i < mix.size(); ++i) {
                // Soft clipping (smoothly limits to -1.0 to 1.0 range)
                mix[i] = std::tanh(mix[i]);
            }

            // --- NEW: CALCULATE VISUAL DATA ---
            // Do this AFTER mixing but BEFORE SDL_PutAudioStreamData
            if (audio) {
                float sumSquares = 0.0f;

                // 1. Calculate RMS (Volume)
                // We iterate with a stride to save CPU (checking every 4th sample is enough for visuals)
                for (size_t i = 0; i < mix.size(); i += 4) {
                    sumSquares += mix[i] * mix[i];
                }
                float rms = std::sqrt(sumSquares / (mix.size() / 4));

                // 2. Update the shared snapshot
                {
                    // Try_lock allows us to skip an update if the render thread is currently reading.
                    // This prevents the audio thread from stuttering.
                    if (audio->visualMutex.try_lock()) {
                        audio->visualSnapshot.currentVolume = rms;
                        audio->visualSnapshot.currentFriction = audio->smoothedFriction;

                        // Grab a small chunk of the wave for visuals (e.g., last 128 samples)
                        size_t captureSize = std::min((size_t)128, mix.size());
                        audio->visualSnapshot.waveform.resize(captureSize);
                        std::copy(mix.end() - captureSize, mix.end(), audio->visualSnapshot.waveform.begin());

                        audio->visualMutex.unlock();
                    }
                }
            }

            // --- 5. Submit to SDL ---
            SDL_PutAudioStreamData(stream, mix.data(),
                                   framesToWrite * ma_get_bytes_per_frame(ma_format_f32, CHANNELS));
        }
    }  // namespace WeirdRenderer
} // namespace WeirdEngine
