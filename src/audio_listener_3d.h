#pragma once

#include <glm/glm.hpp>

class AudioListener3D
{
public:
    void setTransform(const glm::vec3 &position,
                      const glm::vec3 &forward,
                      const glm::vec3 &velocity = glm::vec3(0.0f),
                      const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f));
    void apply() const;

    const glm::vec3 &getPosition() const { return position_; }
    const glm::vec3 &getVelocity() const { return velocity_; }
    const glm::vec3 &getForward() const { return forward_; }
    const glm::vec3 &getUp() const { return up_; }

private:
    glm::vec3 position_ = glm::vec3(0.0f);
    glm::vec3 velocity_ = glm::vec3(0.0f);
    glm::vec3 forward_ = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up_ = glm::vec3(0.0f, 1.0f, 0.0f);
};
