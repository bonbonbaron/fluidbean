/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 * Copyright (C) 2018  Carlo Bramini
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "synth.h"
#include "adriver.h"
#include "settings.h"

#if SDL2_SUPPORT

#include "SDL.h"

typedef struct
{
    audioDriverT driver;

    Synthesizer *synth;
    audioCallbackT writePtr;

    SDL_AudioDeviceID devid;

    int frameSize;

} sdl2_audioDriverT;


static void
SDLAudioCallback(void *data, void *stream, int len)
{
    sdl2_audioDriverT *dev = (sdl2_audioDriverT *)data;

    len /= dev->frameSize;

    dev->writePtr(dev->synth, len, stream, 0, 2, stream, 1, 2);
}

void sdl2_audioDriverSettings(Settings *settings)
{
    int n, nDevs;

    settingsRegisterStr(settings, "audio.sdl2.device", "default", 0);
    settingsAddOption(settings, "audio.sdl2.device", "default");

    if(!SDL_WasInit(SDL_INIT_AUDIO))
    {
#if VERSION_CHECK(SYNTH_VERSION_MAJOR, SYNTH_VERSION_MINOR, SYNTH_VERSION_MICRO) < VERSION_CHECK(2,2,0)
        LOG(WARN, "SDL2 not initialized, SDL2 audio driver won't be usable");
#endif
        return;
    }

    nDevs = SDL_GetNumAudioDevices(0);

    for(n = 0; n < nDevs; n++)
    {
        const char *devName = SDL_GetAudioDeviceName(n, 0);

        if(devName != NULL)
        {
            LOG(DBG, "SDL2 driver testing audio device: %s", devName);
            settingsAddOption(settings, "audio.sdl2.device", devName);
        }
    }
}


/*
 * newSdl2_audioDriver
 */
audioDriverT *
newSdl2_audioDriver(Settings *settings, Synthesizer *synth)
{
    sdl2_audioDriverT *dev = NULL;
    audioCallbackT writePtr;
    double sampleRate;
    int periodSize, sampleSize;
    SDL_AudioSpec aspec, rspec;
    char *device;
    const char *devName;

    /* Check if SDL library has been started */
    if(!SDL_WasInit(SDL_INIT_AUDIO))
    {
        LOG(ERR, "Failed to create SDL2 audio driver, because the audio subsystem of SDL2 is not initialized.");
        return NULL;
    }

    /* Retrieve the settings */
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.period-size", &periodSize);

    /* Lower values do not seem to give good results */
    if(periodSize < 1024)
    {
        periodSize = 1024;
    }
    else
    {
        /* According to documentation, it MUST be a power of two */
        if((periodSize & (periodSize - 1)) != 0)
        {
            LOG(ERR, "\"audio.period-size\" must be a power of 2 for SDL2");
            return NULL;
        }
    }
    /* Clear the format buffer */
    MEMSET(&aspec, 0, sizeof(aspec));

    /* Setup mixing frequency */
    aspec.freq = (int)sampleRate;

    /* Check the format */
    if(settingsStrEqual(settings, "audio.sample-format", "float"))
    {
        LOG(DBG, "Selected 32 bit sample format");

        sampleSize = sizeof(float);
        writePtr   = synthWriteFloat;

        aspec.format = AUDIO_F32SYS;
    }
    else if(settingsStrEqual(settings, "audio.sample-format", "16bits"))
    {
        LOG(DBG, "Selected 16 bit sample format");

        sampleSize = sizeof(short);
        writePtr   = synthWriteS16;

        aspec.format = AUDIO_S16SYS;
    }
    else
    {
        LOG(ERR, "Unhandled sample format");
        return NULL;
    }

    /* Compile the format buffer */
    aspec.channels   = 2;
    aspec.samples    = aspec.channels * ((periodSize + 7) & ~7);
    aspec.callback   = (SDL_AudioCallback)SDLAudioCallback;

    /* Set default device to use */
    device   = NULL;
    devName = NULL;

    /* get the selected device name. if none is specified, use default device. */
    if(settingsDupstr(settings, "audio.sdl2.device", &device) == OK
            && device != NULL && device[0] != '\0')
    {
        int n, nDevs = SDL_GetNumAudioDevices(0);

        for(n = 0; n < nDevs; n++)
        {
            devName = SDL_GetAudioDeviceName(n, 0);

            if(STRCASECMP(devName, device) == 0)
            {
                LOG(DBG, "Selected audio device GUID: %s", devName);
                break;
            }
        }

        if(n >= nDevs)
        {
            LOG(DBG, "Audio device %s, using \"default\"", device);
            devName = NULL;
        }
    }

    if(device != NULL)
    {
        FREE(device);
    }

    do
    {
        /* create and clear the driver data */
        dev = NEW(sdl2_audioDriverT);

        if(dev == NULL)
        {
            LOG(ERR, "Out of memory");
            break;
        }

        MEMSET(dev, 0, sizeof(sdl2_audioDriverT));

        /* set device pointer to userdata */
        aspec.userdata = dev;

        /* Save copy of synth */
        dev->synth = synth;

        /* Save copy of other variables */
        dev->writePtr = writePtr;
        dev->frameSize = sampleSize * aspec.channels;

        /* Open audio device */
        dev->devid = SDL_OpenAudioDevice(devName, 0, &aspec, &rspec, 0);

        if(!dev->devid)
        {
            LOG(ERR, "Failed to open audio device");
            break;
        }

        /* Start to play */
        SDL_PauseAudioDevice(dev->devid, 0);

        return (audioDriverT *) dev;
    }
    while(0);

    deleteSdl2_audioDriver(&dev->driver);
    return NULL;
}


void deleteSdl2_audioDriver(audioDriverT *d)
{
    sdl2_audioDriverT *dev = (sdl2_audioDriverT *) d;

    if(dev != NULL)
    {
        if(dev->devid)
        {
            /* Stop audio and close */
            SDL_PauseAudioDevice(dev->devid, 1);
            SDL_CloseAudioDevice(dev->devid);
        }

        FREE(dev);
    }
}

#endif /* SDL2_SUPPORT */
