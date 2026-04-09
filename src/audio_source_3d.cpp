#include "audio_source_3d.h"

#include "audio_clip.h"

#include <glm/geometric.hpp>

AudioSource3D::AudioSource3D()
{
    alGenSources(1, &source_);
    if (source_ == 0)
        return;

    setPitch(1.0f);
    setGain(1.0f);
    setPosition(glm::vec3(0.0f));
    setVelocity(glm::vec3(0.0f));
    setDirection(glm::vec3(0.0f, 0.0f, -1.0f));
    setLooping(false);
    setRelativeToListener(false);
    setAttenuation(attenuation_);
    setDirectionalCone(cone_);
}

AudioSource3D::~AudioSource3D()
{
    if (source_ != 0)
        alDeleteSources(1, &source_);
}

void AudioSource3D::setClip(const std::shared_ptr<AudioClip> &clip)
{
    clip_ = clip;
    if (source_ == 0)
        return;

    alSourcei(source_, AL_BUFFER, clip_ ? static_cast<ALint>(clip_->getBuffer()) : 0);
}

void AudioSource3D::setPosition(const glm::vec3 &position)
{
    position_ = position;
    if (source_ != 0)
        alSource3f(source_, AL_POSITION, position_.x, position_.y, position_.z);
}

void AudioSource3D::setVelocity(const glm::vec3 &velocity)
{
    velocity_ = velocity;
    if (source_ != 0)
        alSource3f(source_, AL_VELOCITY, velocity_.x, velocity_.y, velocity_.z);
}

void AudioSource3D::setDirection(const glm::vec3 &direction)
{
    if (glm::length(direction) > 1e-4f)
        direction_ = glm::normalize(direction);
    else
        direction_ = glm::vec3(0.0f, 0.0f, -1.0f);

    if (source_ != 0)
        alSource3f(source_, AL_DIRECTION, direction_.x, direction_.y, direction_.z);
}

void AudioSource3D::setGain(float gain)
{
    gain_ = gain;
    if (source_ != 0)
        alSourcef(source_, AL_GAIN, gain_);
}

void AudioSource3D::setPitch(float pitch)
{
    pitch_ = pitch;
    if (source_ != 0)
        alSourcef(source_, AL_PITCH, pitch_);
}

void AudioSource3D::setLooping(bool looping)
{
    looping_ = looping;
    if (source_ != 0)
        alSourcei(source_, AL_LOOPING, looping_ ? AL_TRUE : AL_FALSE);
}

void AudioSource3D::setRelativeToListener(bool relative)
{
    relativeToListener_ = relative;
    if (source_ != 0)
        alSourcei(source_, AL_SOURCE_RELATIVE, relativeToListener_ ? AL_TRUE : AL_FALSE);
}

void AudioSource3D::setAttenuation(const AudioAttenuationSettings &attenuation)
{
    attenuation_ = attenuation;
    if (source_ == 0)
        return;

    alSourcef(source_, AL_REFERENCE_DISTANCE, attenuation_.referenceDistance);
    alSourcef(source_, AL_MAX_DISTANCE, attenuation_.maxDistance);
    alSourcef(source_, AL_ROLLOFF_FACTOR, attenuation_.rolloffFactor);
}

void AudioSource3D::setDirectionalCone(const AudioDirectionalCone &cone)
{
    cone_ = cone;
    if (source_ == 0)
        return;

    alSourcef(source_, AL_CONE_INNER_ANGLE, cone_.innerAngleDegrees);
    alSourcef(source_, AL_CONE_OUTER_ANGLE, cone_.outerAngleDegrees);
    alSourcef(source_, AL_CONE_OUTER_GAIN, cone_.outerGain);
}

void AudioSource3D::play()
{
    if (source_ != 0)
        alSourcePlay(source_);
}

void AudioSource3D::stop()
{
    if (source_ != 0)
        alSourceStop(source_);
}

void AudioSource3D::pause()
{
    if (source_ != 0)
        alSourcePause(source_);
}

void AudioSource3D::rewind()
{
    if (source_ != 0)
        alSourceRewind(source_);
}

bool AudioSource3D::isPlaying() const
{
    if (source_ == 0)
        return false;

    ALint state = 0;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

bool AudioSource3D::isStopped() const
{
    if (source_ == 0)
        return true;

    ALint state = 0;
    alGetSourcei(source_, AL_SOURCE_STATE, &state);
    return state == AL_STOPPED || state == AL_INITIAL;
}
