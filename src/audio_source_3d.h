#pragma once

#ifdef __APPLE__
#include <OpenAL/al.h>
#else
#include <AL/al.h>
#endif

#include <glm/glm.hpp>

#include <memory>

class AudioClip;

struct AudioAttenuationSettings
{
    float referenceDistance = 3.0f;
    float maxDistance = 45.0f;
    float rolloffFactor = 1.0f;
};

struct AudioDirectionalCone
{
    float innerAngleDegrees = 360.0f;
    float outerAngleDegrees = 360.0f;
    float outerGain = 0.0f;
};

class AudioSource3D
{
public:
    AudioSource3D();
    ~AudioSource3D();

    AudioSource3D(const AudioSource3D &) = delete;
    AudioSource3D &operator=(const AudioSource3D &) = delete;
    AudioSource3D(AudioSource3D &&) = delete;
    AudioSource3D &operator=(AudioSource3D &&) = delete;

    bool isValid() const { return source_ != 0; }

    void setClip(const std::shared_ptr<AudioClip> &clip);
    void setPosition(const glm::vec3 &position);
    void setVelocity(const glm::vec3 &velocity);
    void setDirection(const glm::vec3 &direction);
    void setGain(float gain);
    void setPitch(float pitch);
    void setLooping(bool looping);
    void setRelativeToListener(bool relative);
    void setAttenuation(const AudioAttenuationSettings &attenuation);
    void setDirectionalCone(const AudioDirectionalCone &cone);

    void play();
    void stop();
    void pause();
    void rewind();

    bool isPlaying() const;
    bool isStopped() const;

    void setAutoDestroy(bool autoDestroy) { autoDestroy_ = autoDestroy; }
    bool shouldAutoDestroy() const { return autoDestroy_; }

    const glm::vec3 &getPosition() const { return position_; }
    const glm::vec3 &getDirection() const { return direction_; }

private:
    ALuint source_ = 0;
    std::shared_ptr<AudioClip> clip_;
    glm::vec3 position_ = glm::vec3(0.0f);
    glm::vec3 velocity_ = glm::vec3(0.0f);
    glm::vec3 direction_ = glm::vec3(0.0f, 0.0f, -1.0f);
    float gain_ = 1.0f;
    float pitch_ = 1.0f;
    bool looping_ = false;
    bool relativeToListener_ = false;
    bool autoDestroy_ = false;
    AudioAttenuationSettings attenuation_;
    AudioDirectionalCone cone_;
};
