#pragma once

#include <SDL3/SDL.h>
#include <miniaudio/miniaudio.h>

namespace WeirdEngine {
    namespace WeirdRenderer {
        class AudioEngine {
        public:
            AudioEngine();
            ~AudioEngine();

            bool init();
            void loadSound(const char* filePath);
            static void data_callback(void* pUserData, SDL_AudioStream* stream, int additional_amount, int total_amount);

            ma_uint32 getSampleRate() const;
            ma_uint8 getChannels() const;

        private:
            ma_engine g_engine;
            ma_sound g_sound;
        };
    }
}