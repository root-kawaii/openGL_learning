#include "audio_listener_3d.h"

#ifdef __APPLE__
#include <OpenAL/al.h>
#else
#include <AL/al.h>
#endif

#include <glm/geometric.hpp>

void AudioListener3D::setTransform(const glm::vec3 &position,
                                   const glm::vec3 &forward,
                                   const glm::vec3 &velocity,
                                   const glm::vec3 &up)
{
    position_ = position;
    velocity_ = velocity;

    if (glm::length(forward) > 1e-4f)
        forward_ = glm::normalize(forward);

    if (glm::length(up) > 1e-4f)
        up_ = glm::normalize(up);

    if (glm::length(glm::cross(forward_, up_)) < 1e-4f)
        up_ = glm::vec3(0.0f, 1.0f, 0.0f);
}

void AudioListener3D::apply() const
{
    ALfloat position[3] = {position_.x, position_.y, position_.z};
    ALfloat velocity[3] = {velocity_.x, velocity_.y, velocity_.z};
    ALfloat orientation[6] = {
        forward_.x, forward_.y, forward_.z,
        up_.x, up_.y, up_.z};

    alListenerfv(AL_POSITION, position);
    alListenerfv(AL_VELOCITY, velocity);
    alListenerfv(AL_ORIENTATION, orientation);
}
