#pragma once
#include "Entity.h"


namespace WeirdEngine 
{
    // Component base class
    class Component {
    public:
        virtual ~Component() = default;
        Entity Owner;
    };
}

