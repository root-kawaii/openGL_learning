#pragma once
#ifdef __APPLE__
#include <OpenAL/al.h>
#include <OpenAL/alc.h>
#else
#include <AL/al.h>
#include <AL/alc.h>
#endif
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>
#include "model.h"
#include <filesystem>

class AudioManager
{
public:
    AudioManager();
    ~AudioManager();

    void playSource();
    void playSource(char *filename);
    void cleanUp();
    void loopAudio();

    ALCdevice *device;
    ALCcontext *context;
    ALuint buffer;
    ALuint source;
    ALenum format;
    ALsizei freq;
    ALint state;

private:
};