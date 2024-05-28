#pragma once

// Component base class
class Component {
public:
    virtual ~Component() = default;
    Entity Owner;
};

