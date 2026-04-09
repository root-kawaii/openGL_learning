#include "audio_clip.h"

#include <sndfile.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>

namespace
{
bool bufferInterleavedSamples(const std::filesystem::path &path,
                              ALuint buffer,
                              ALenum &format,
                              ALsizei &frequency,
                              bool forceMono)
{
    SF_INFO info{};
    SNDFILE *sndFile = sf_open(path.string().c_str(), SFM_READ, &info);
    if (!sndFile)
    {
        std::cerr << "[Audio] Failed to open audio file: " << path << " (" << sf_strerror(nullptr) << ")" << std::endl;
        return false;
    }

    if (info.frames <= 0 || info.channels <= 0)
    {
        sf_close(sndFile);
        return false;
    }

    std::vector<short> interleavedSamples(static_cast<size_t>(info.frames) * static_cast<size_t>(info.channels));
    const sf_count_t samplesRead = sf_readf_short(sndFile, interleavedSamples.data(), info.frames);
    sf_close(sndFile);
    if (samplesRead <= 0)
        return false;

    const int channels = forceMono ? 1 : info.channels;
    std::vector<short> finalSamples;

    if (forceMono && info.channels > 1)
    {
        finalSamples.resize(static_cast<size_t>(samplesRead));
        for (sf_count_t frame = 0; frame < samplesRead; ++frame)
        {
            int mixed = 0;
            for (int channel = 0; channel < info.channels; ++channel)
                mixed += interleavedSamples[static_cast<size_t>(frame) * static_cast<size_t>(info.channels) + static_cast<size_t>(channel)];
            finalSamples[static_cast<size_t>(frame)] = static_cast<short>(mixed / info.channels);
        }
    }
    else
    {
        finalSamples.assign(
            interleavedSamples.begin(),
            interleavedSamples.begin() + static_cast<std::ptrdiff_t>(samplesRead * info.channels));
    }

    if (channels == 1)
        format = AL_FORMAT_MONO16;
    else if (channels == 2)
        format = AL_FORMAT_STEREO16;
    else
        return false;

    frequency = static_cast<ALsizei>(info.samplerate);
    alBufferData(buffer,
                 format,
                 finalSamples.data(),
                 static_cast<ALsizei>(finalSamples.size() * sizeof(short)),
                 frequency);
    return true;
}

bool loadWavIntoBuffer(const std::filesystem::path &path,
                       ALuint buffer,
                       ALenum &format,
                       ALsizei &frequency,
                       bool forceMono)
{
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
    {
        std::cerr << "[Audio] Failed to open WAV file: " << path << std::endl;
        return false;
    }

    char riff[4] = {};
    file.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0)
        return false;

    file.ignore(4);
    file.read(riff, 4);
    if (std::strncmp(riff, "WAVE", 4) != 0)
        return false;

    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    bool fmtFound = false;
    bool dataFound = false;
    std::vector<char> sampleData;

    while (file && (!fmtFound || !dataFound))
    {
        char chunkId[4] = {};
        uint32_t chunkSize = 0;
        file.read(chunkId, 4);
        file.read(reinterpret_cast<char *>(&chunkSize), 4);
        if (!file)
            break;

        if (std::strncmp(chunkId, "fmt ", 4) == 0)
        {
            uint16_t audioFormat = 0;
            uint16_t blockAlign = 0;
            uint32_t byteRate = 0;
            file.read(reinterpret_cast<char *>(&audioFormat), 2);
            file.read(reinterpret_cast<char *>(&channels), 2);
            file.read(reinterpret_cast<char *>(&sampleRate), 4);
            file.read(reinterpret_cast<char *>(&byteRate), 4);
            file.read(reinterpret_cast<char *>(&blockAlign), 2);
            file.read(reinterpret_cast<char *>(&bitsPerSample), 2);

            if (chunkSize > 16)
                file.ignore(chunkSize - 16);

            if (audioFormat != 1)
            {
                std::cerr << "[Audio] Unsupported WAV format in " << path << std::endl;
                return false;
            }
            fmtFound = true;
        }
        else if (std::strncmp(chunkId, "data", 4) == 0)
        {
            sampleData.resize(chunkSize);
            file.read(sampleData.data(), static_cast<std::streamsize>(chunkSize));
            dataFound = true;
        }
        else
        {
            file.ignore(chunkSize);
        }
    }

    if (!fmtFound || !dataFound)
    {
        std::cerr << "[Audio] Incomplete WAV data in " << path << std::endl;
        return false;
    }

    if (forceMono && channels == 2)
    {
        if (bitsPerSample == 16)
        {
            std::vector<int16_t> monoSamples(sampleData.size() / 4);
            const int16_t *stereoSamples = reinterpret_cast<const int16_t *>(sampleData.data());
            for (size_t i = 0; i < monoSamples.size(); ++i)
            {
                const int left = stereoSamples[i * 2];
                const int right = stereoSamples[i * 2 + 1];
                monoSamples[i] = static_cast<int16_t>((left + right) / 2);
            }

            format = AL_FORMAT_MONO16;
            frequency = static_cast<ALsizei>(sampleRate);
            alBufferData(buffer,
                         format,
                         monoSamples.data(),
                         static_cast<ALsizei>(monoSamples.size() * sizeof(int16_t)),
                         frequency);
            return true;
        }

        if (bitsPerSample == 8)
        {
            std::vector<uint8_t> monoSamples(sampleData.size() / 2);
            const uint8_t *stereoSamples = reinterpret_cast<const uint8_t *>(sampleData.data());
            for (size_t i = 0; i < monoSamples.size(); ++i)
            {
                const int left = stereoSamples[i * 2];
                const int right = stereoSamples[i * 2 + 1];
                monoSamples[i] = static_cast<uint8_t>((left + right) / 2);
            }

            format = AL_FORMAT_MONO8;
            frequency = static_cast<ALsizei>(sampleRate);
            alBufferData(buffer,
                         format,
                         monoSamples.data(),
                         static_cast<ALsizei>(monoSamples.size()),
                         frequency);
            return true;
        }
    }

    if (channels == 1 && bitsPerSample == 8)
        format = AL_FORMAT_MONO8;
    else if (channels == 1 && bitsPerSample == 16)
        format = AL_FORMAT_MONO16;
    else if (channels == 2 && bitsPerSample == 8)
        format = AL_FORMAT_STEREO8;
    else if (channels == 2 && bitsPerSample == 16)
        format = AL_FORMAT_STEREO16;
    else
        return false;

    frequency = static_cast<ALsizei>(sampleRate);
    alBufferData(buffer, format, sampleData.data(), static_cast<ALsizei>(sampleData.size()), frequency);
    return true;
}
} // namespace

AudioClip::AudioClip(const std::filesystem::path &path, ALuint buffer, ALenum format, ALsizei frequency)
    : path_(path), buffer_(buffer), format_(format), frequency_(frequency)
{
}

AudioClip::~AudioClip()
{
    if (buffer_ != 0)
        alDeleteBuffers(1, &buffer_);
}

std::shared_ptr<AudioClip> AudioClip::loadFromWav(const std::filesystem::path &path)
{
    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    if (buffer == 0)
        return nullptr;

    ALenum format = 0;
    ALsizei frequency = 0;
    if (!loadWavIntoBuffer(path, buffer, format, frequency, false))
    {
        alDeleteBuffers(1, &buffer);
        return nullptr;
    }

    return std::make_shared<AudioClip>(path, buffer, format, frequency);
}

std::shared_ptr<AudioClip> AudioClip::loadFromFile(const std::filesystem::path &path)
{
    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    if (buffer == 0)
        return nullptr;

    ALenum format = 0;
    ALsizei frequency = 0;
    if (!bufferInterleavedSamples(path, buffer, format, frequency, false))
    {
        alDeleteBuffers(1, &buffer);
        return nullptr;
    }

    return std::make_shared<AudioClip>(path, buffer, format, frequency);
}

std::shared_ptr<AudioClip> AudioClip::loadMonoFromWav(const std::filesystem::path &path)
{
    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    if (buffer == 0)
        return nullptr;

    ALenum format = 0;
    ALsizei frequency = 0;
    if (!loadWavIntoBuffer(path, buffer, format, frequency, true))
    {
        alDeleteBuffers(1, &buffer);
        return nullptr;
    }

    return std::make_shared<AudioClip>(path, buffer, format, frequency);
}

std::shared_ptr<AudioClip> AudioClip::loadMonoFromFile(const std::filesystem::path &path)
{
    ALuint buffer = 0;
    alGenBuffers(1, &buffer);
    if (buffer == 0)
        return nullptr;

    ALenum format = 0;
    ALsizei frequency = 0;
    if (!bufferInterleavedSamples(path, buffer, format, frequency, true))
    {
        alDeleteBuffers(1, &buffer);
        return nullptr;
    }

    return std::make_shared<AudioClip>(path, buffer, format, frequency);
}
