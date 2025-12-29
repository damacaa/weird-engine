#pragma once

#include <mutex>
#include <vector>

#include <SDL3/SDL.h>
#include <miniaudio/miniaudio.h>

#include "weird-engine/Scene.h"

namespace WeirdEngine {
    namespace WeirdRenderer {

        struct CollisionVoice {
            float frequency;
            float amplitude;
            float decay;
            float time = 0.0f;
            float phase = 0.0f;
            bool finished = false;
        };

        class AudioEngine {
        public:
            AudioEngine();
            ~AudioEngine();

            bool init();
            void close();
            void loadSound(const char* filePath);
            static void data_callback(void* pUserData, SDL_AudioStream* stream, int additional_amount, int total_amount);

            ma_uint32 getSampleRate() const;
            ma_uint8 getChannels() const;

            void listen(Scene& scene);

            // --- Procedural control ---
            void setFrictionLevel(float level);      // 0..1, continuous
            void triggerCollision(float freq, float amp, float decaySec = 0.3f);

        private:
            ma_engine g_engine;
            ma_sound  g_sound;  // background music

            // Procedural state
            ma_noise  g_noise;
            float     frictionLevel = 0.0f; // modulated each frame
            float     smoothedFriction = 0.0f;

            // Collision tone
            float     collisionFreq = 0.0f;
            float     collisionAmp = 0.0f;
            float     collisionPhase = 0.0f;
            float     collisionDecay = 0.0f;
            float     collisionTime = 0.0f;

            // 2. Replace the single float variables with a vector of voices
            std::vector<CollisionVoice> activeVoices;
            std::mutex voiceMutex; // Essential for thread safety
        };

    } // namespace WeirdRenderer
} // namespace WeirdEngine
