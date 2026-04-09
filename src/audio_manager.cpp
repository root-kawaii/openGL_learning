#include "audio_manager.h"

AudioManager::AudioManager()
{
    initialize();
}

AudioManager::~AudioManager()
{
    cleanUp();
}

bool AudioManager::initialize()
{
    if (initialized_)
        return true;

    device_ = alcOpenDevice(nullptr);
    if (!device_)
        return false;

    context_ = alcCreateContext(device_, nullptr);
    if (!context_)
    {
        alcCloseDevice(device_);
        device_ = nullptr;
        return false;
    }

    alcMakeContextCurrent(context_);
    alDistanceModel(AL_INVERSE_DISTANCE_CLAMPED);
    listener_.setTransform(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    listener_.apply();
    setMasterVolume(1.0f);

    initialized_ = true;
    return true;
}

void AudioManager::setMasterVolume(float volume)
{
    masterVolume_ = volume;
    if (initialized_)
        alListenerf(AL_GAIN, masterVolume_);
}

void AudioManager::updateListener(const glm::vec3 &position,
                                  const glm::vec3 &forward,
                                  const glm::vec3 &velocity,
                                  const glm::vec3 &up)
{
    if (!initialized_)
        return;

    listener_.setTransform(position, forward, velocity, up);
    listener_.apply();
}

std::shared_ptr<AudioClip> AudioManager::loadClip(const std::filesystem::path &path)
{
    if (!initialize())
        return nullptr;

    const auto normalized = std::filesystem::absolute(path).lexically_normal().string() + "|stereo";
    auto found = clipCache_.find(normalized);
    if (found != clipCache_.end())
    {
        if (auto cached = found->second.lock())
            return cached;
    }

    auto clip = AudioClip::loadFromFile(path);
    if (!clip)
        return nullptr;

    clipCache_[normalized] = clip;
    return clip;
}

std::shared_ptr<AudioClip> AudioManager::loadClipMono(const std::filesystem::path &path)
{
    if (!initialize())
        return nullptr;

    const auto normalized = std::filesystem::absolute(path).lexically_normal().string() + "|mono";
    auto found = clipCache_.find(normalized);
    if (found != clipCache_.end())
    {
        if (auto cached = found->second.lock())
            return cached;
    }

    auto clip = AudioClip::loadMonoFromFile(path);
    if (!clip)
        return nullptr;

    clipCache_[normalized] = clip;
    return clip;
}

std::shared_ptr<AudioSource3D> AudioManager::createSource(const std::shared_ptr<AudioClip> &clip)
{
    if (!initialize())
        return nullptr;

    auto source = std::make_shared<AudioSource3D>();
    if (!source->isValid())
        return nullptr;

    if (clip)
        source->setClip(clip);

    sources_.push_back(source);
    return source;
}

std::shared_ptr<AudioSource3D> AudioManager::createSpatialSource(const std::shared_ptr<AudioClip> &clip,
                                                                 const glm::vec3 &position,
                                                                 const AudioAttenuationSettings &attenuation)
{
    auto source = createSource(clip);
    if (!source)
        return nullptr;

    source->setPosition(position);
    source->setAttenuation(attenuation);
    return source;
}

std::shared_ptr<AudioSource3D> AudioManager::createDirectionalSource(const std::shared_ptr<AudioClip> &clip,
                                                                     const glm::vec3 &position,
                                                                     const glm::vec3 &direction,
                                                                     const AudioAttenuationSettings &attenuation,
                                                                     const AudioDirectionalCone &cone)
{
    auto source = createSpatialSource(clip, position, attenuation);
    if (!source)
        return nullptr;

    source->setDirection(direction);
    source->setDirectionalCone(cone);
    return source;
}

void AudioManager::playSource()
{
    if (!initialize())
        return;

    if (!defaultClip_)
        defaultClip_ = loadClip("assets/audio_2.wav");
    if (!defaultSource_)
        defaultSource_ = createSpatialSource(defaultClip_, glm::vec3(5.0f, 0.0f, 0.0f));

    if (!defaultSource_)
        return;

    defaultSource_->setLooping(false);
    defaultSource_->play();
}

void AudioManager::playSource(char *filename)
{
    if (!filename || !initialize())
        return;

    defaultClip_ = loadClip(filename);
    if (!defaultClip_)
        return;

    if (!defaultSource_)
        defaultSource_ = createSpatialSource(defaultClip_, glm::vec3(5.0f, 0.0f, 0.0f));
    else
        defaultSource_->setClip(defaultClip_);

    if (!defaultSource_)
        return;

    defaultSource_->setLooping(false);
    defaultSource_->play();
}

void AudioManager::loopAudio()
{
    if (defaultSource_ && defaultSource_->isStopped())
        defaultSource_->play();
}

void AudioManager::update(float)
{
    if (!initialized_)
        return;

    pruneStoppedTransientSources();
}

void AudioManager::pruneStoppedTransientSources()
{
    sources_.erase(
        std::remove_if(
            sources_.begin(),
            sources_.end(),
            [](const std::shared_ptr<AudioSource3D> &source)
            {
                return !source || (source->shouldAutoDestroy() && source->isStopped());
            }),
        sources_.end());
}

void AudioManager::cleanUp()
{
    if (!device_ && !context_ && !initialized_)
        return;

    defaultSource_.reset();
    sources_.clear();
    defaultClip_.reset();
    clipCache_.clear();

    if (context_)
    {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context_);
        context_ = nullptr;
    }

    if (device_)
    {
        alcCloseDevice(device_);
        device_ = nullptr;
    }

    initialized_ = false;
}
