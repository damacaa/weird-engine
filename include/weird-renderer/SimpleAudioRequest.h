#ifndef WEIRDSAMPLES_AUDIOREQUEST_H
#define WEIRDSAMPLES_AUDIOREQUEST_H

#include "weird-engine/vec.h"

namespace WeirdEngine
{
    namespace WeirdRenderer
    {
        struct SimpleAudioRequest
        {
            float volume;
            float frequency;
            bool spatial;
            vec3 position;
        };
    }
}

#endif //WEIRDSAMPLES_AUDIOREQUEST_H