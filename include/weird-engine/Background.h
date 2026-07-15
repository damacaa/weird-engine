#pragma once

#include <glm/glm.hpp>
#include <string>

namespace WeirdEngine
{
    enum class BackgroundType {
        Solid,
        Grid,
        Sky,
        Custom
    };

    struct BackgroundParams {
        BackgroundType type = BackgroundType::Grid;
        
        glm::vec4 primaryColor{0.7f, 0.7f, 0.71f, 1.0f};
        glm::vec4 secondaryColor{0.55f, 0.55f, 0.58f, 1.0f};
        float scale = 1.0f;
        float intensity = 1.0f;
        bool isDirty = true;

        std::string customShaderCode = ""; 
    };
}
