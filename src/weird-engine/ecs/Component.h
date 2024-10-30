#pragma once
#include "Entity.h"


// Component base class
class Component {
public:
    virtual ~Component() = default;
    Entity Owner;
};

