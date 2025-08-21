#include "audio_manager.h"

namespace fs = std::filesystem;

bool loadWavFile(const char *filename, ALuint &buffer, ALenum &format, ALsizei &freq);

AudioManager::~AudioManager()
{
}

AudioManager::AudioManager()
{
    device = alcOpenDevice(nullptr); // nullptr for default device
    if (!device)
    {
        // Handle error
    }

    context = alcCreateContext(device, nullptr);
    if (!context)
    {
        // Handle error
    }
    alcMakeContextCurrent(context);
    ALfloat listenerPos[] = {0.0f, 0.0f, 0.0f};
    ALfloat listenerVel[] = {0.0f, 0.0f, 0.0f};
    ALfloat listenerOri[] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f}; // Forward and Up vectors

    alListenerfv(AL_POSITION, listenerPos);
    alListenerfv(AL_VELOCITY, listenerVel);
    alListenerfv(AL_ORIENTATION, listenerOri);

    alGenBuffers(1, &buffer);

    loadWavFile(fs::path("assets/audio_2.wav").c_str(), buffer, format, freq);

    alGenSources(1, &source);

    alSourcei(source, AL_BUFFER, buffer);

    alSourcef(source, AL_PITCH, 1.0f);
    alSourcef(source, AL_GAIN, 1.0f);
    alSource3f(source, AL_POSITION, 5.0f, 0.0f, 0.0f); // Position in 3D space
    alSourcei(source, AL_LOOPING, AL_FALSE);
}

void AudioManager::playSource()
{
    alSourcePlay(source);
}

void AudioManager::playSource(char *filename)
{
    loadWavFile(filename, buffer, format, freq);

    alGenSources(1, &source);

    alSourcei(source, AL_BUFFER, buffer);

    alSourcef(source, AL_PITCH, 1.0f);
    alSourcef(source, AL_GAIN, 1.0f);
    alSource3f(source, AL_POSITION, 5.0f, 0.0f, 0.0f); // Position in 3D space
    alSourcei(source, AL_LOOPING, AL_FALSE);
    alSourcePlay(source);
}

void AudioManager::loopAudio()
{
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING)
    {
        // Still playing
    }
    else if (state == AL_STOPPED)
    {
        playSource();
        // Finished
    }
}

void AudioManager::cleanUp()
{
    alDeleteSources(1, &source);
    alDeleteBuffers(1, &buffer);

    alcDestroyContext(context);
    alcCloseDevice(device);
}

// Very basic WAV loader (uncompressed PCM only)
bool loadWavFile(const char *filename, ALuint &buffer, ALenum &format, ALsizei &freq)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file)
        return false;

    char riff[4];
    file.read(riff, 4); // "RIFF"
    file.ignore(4);     // file size
    file.read(riff, 4); // "WAVE"

    char chunkId[4];
    file.read(chunkId, 4); // "fmt "
    uint32_t chunkSize;
    file.read(reinterpret_cast<char *>(&chunkSize), 4);

    uint16_t audioFormat, channels, blockAlign, bitsPerSample;
    uint32_t sampleRate, byteRate;

    file.read(reinterpret_cast<char *>(&audioFormat), 2);
    file.read(reinterpret_cast<char *>(&channels), 2);
    file.read(reinterpret_cast<char *>(&sampleRate), 4);
    file.read(reinterpret_cast<char *>(&byteRate), 4);
    file.read(reinterpret_cast<char *>(&blockAlign), 2);
    file.read(reinterpret_cast<char *>(&bitsPerSample), 2);

    // Skip any extra fmt bytes
    if (chunkSize > 16)
        file.ignore(chunkSize - 16);

    // Find "data" chunk
    char dataId[4];
    uint32_t dataSize = 0;
    while (true)
    {
        file.read(dataId, 4);
        file.read(reinterpret_cast<char *>(&dataSize), 4);
        if (std::strncmp(dataId, "data", 4) == 0)
            break;
        file.ignore(dataSize);
    }

    std::vector<char> data(dataSize);
    file.read(data.data(), dataSize);

    // Determine format
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

    freq = sampleRate;

    alBufferData(buffer, format, data.data(), dataSize, freq);

    return true;
}