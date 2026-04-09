#pragma once

#ifdef __APPLE__
#include <OpenAL/al.h>
#else
#include <AL/al.h>
#endif

#include <filesystem>
#include <memory>

class AudioClip
{
public:
    AudioClip(const std::filesystem::path &path, ALuint buffer, ALenum format, ALsizei frequency);
    ~AudioClip();

    AudioClip(const AudioClip &) = delete;
    AudioClip &operator=(const AudioClip &) = delete;
    AudioClip(AudioClip &&) = delete;
    AudioClip &operator=(AudioClip &&) = delete;

    static std::shared_ptr<AudioClip> loadFromFile(const std::filesystem::path &path);
    static std::shared_ptr<AudioClip> loadMonoFromFile(const std::filesystem::path &path);
    static std::shared_ptr<AudioClip> loadFromWav(const std::filesystem::path &path);
    static std::shared_ptr<AudioClip> loadMonoFromWav(const std::filesystem::path &path);

    bool isValid() const { return buffer_ != 0; }
    ALuint getBuffer() const { return buffer_; }
    ALenum getFormat() const { return format_; }
    ALsizei getFrequency() const { return frequency_; }
    const std::filesystem::path &getPath() const { return path_; }

private:
    std::filesystem::path path_;
    ALuint buffer_ = 0;
    ALenum format_ = 0;
    ALsizei frequency_ = 0;
};
