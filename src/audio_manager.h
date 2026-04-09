#pragma once

#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <glm/glm.hpp>

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "audio_clip.h"
#include "audio_listener_3d.h"
#include "audio_source_3d.h"

class AudioManager
{
public:
    AudioManager();
    ~AudioManager();

    void playSource();
    void playSource(char *filename);
    void cleanUp();
    void loopAudio();
    bool initialize();
    void update(float deltaTime = 0.0f);
    void updateListener(const glm::vec3 &position,
                        const glm::vec3 &forward,
                        const glm::vec3 &velocity = glm::vec3(0.0f),
                        const glm::vec3 &up = glm::vec3(0.0f, 1.0f, 0.0f));
    void setMasterVolume(float volume);

    std::shared_ptr<AudioClip> loadClip(const std::filesystem::path &path);
    std::shared_ptr<AudioClip> loadClipMono(const std::filesystem::path &path);
    std::shared_ptr<AudioSource3D> createSource(const std::shared_ptr<AudioClip> &clip = nullptr);
    std::shared_ptr<AudioSource3D> createSpatialSource(const std::shared_ptr<AudioClip> &clip,
                                                       const glm::vec3 &position,
                                                       const AudioAttenuationSettings &attenuation = AudioAttenuationSettings());
    std::shared_ptr<AudioSource3D> createDirectionalSource(const std::shared_ptr<AudioClip> &clip,
                                                           const glm::vec3 &position,
                                                           const glm::vec3 &direction,
                                                           const AudioAttenuationSettings &attenuation = AudioAttenuationSettings(),
                                                           const AudioDirectionalCone &cone = AudioDirectionalCone());
    bool isInitialized() const { return initialized_; }

    ALCdevice *getDevice() const { return device_; }
    ALCcontext *getContext() const { return context_; }

private:
    void pruneStoppedTransientSources();

    ALCdevice *device_ = nullptr;
    ALCcontext *context_ = nullptr;
    bool initialized_ = false;
    float masterVolume_ = 1.0f;
    AudioListener3D listener_;
    std::unordered_map<std::string, std::weak_ptr<AudioClip>> clipCache_;
    std::vector<std::shared_ptr<AudioSource3D>> sources_;
    std::shared_ptr<AudioClip> defaultClip_;
    std::shared_ptr<AudioSource3D> defaultSource_;
};
