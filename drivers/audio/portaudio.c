/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
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

/* portaudio.c
 *
 * Drivers for the PortAudio API : www.portaudio.com
 * Implementation files for PortAudio on each platform have to be added
 *
 * Stephane Letz  (letz@grame.fr)  Grame
 * 12/20/01 Adapdation for new audio drivers
 *
 * Josh Green <jgreen@users.sourceforge.net>
 * 2009-01-28 Overhauled for PortAudio 19 API and current Synth API (was broken)
 */

#include "synth.h"
#include "settings.h"
#include "adriver.h"

#if PORTAUDIO_SUPPORT

#include <portaudio.h>


/** portaudioDriverT
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
    audioDriverT driver;
    Synthesizer *synth;
    audioCallbackT read;
    PaStream *stream;
} portaudioDriverT;

static int
portaudioRun(const void *input, void *output, unsigned long frameCount,
                    const PaStreamCallbackTimeInfo *timeInfo,
                    PaStreamCallbackFlags statusFlags, void *userData);

#define PORTAUDIO_DEFAULT_DEVICE "PortAudio Default"

/**
 * Checks if deviceNum is a valid device and returns the name of the portaudio device.
 * A device is valid if it is an output device with at least 2 channels.
 *
 * @param deviceNum index of the portaudio device to check.
 * @param namePtr if deviceNum is valid, set to a unique device name, ignored otherwise
 *
 * The name returned is unique for each numDevice index, so this
 * name is useful to identify any available host audio device.
 * This name is convenient for audio.portaudio.device setting.
 *
 * The format of the name is: deviceIndex:hostApiName:hostDeviceName
 *
 *   example: 5:MME:SB PCI
 *
 *   5: is the portaudio device index.
 *   MME: is the host API name.
 *   SB PCI: is the host device name.
 *
 * @return #OK if deviceNum is a valid output device, #FAILED otherwise.
 * When #OK, the name is returned in allocated memory. The caller must check
 * the name pointer for a valid memory allocation and should free the memory.
 */
static int portaudioGetDeviceName(int deviceNum, char **namePtr)
{
    const PaDeviceInfo *deviceInfo =  Pa_GetDeviceInfo(deviceNum);

    if(deviceInfo->maxOutputChannels >= 2)
    {
        const PaHostApiInfo *hostInfo = Pa_GetHostApiInfo(deviceInfo->hostApi);
        /* The size of the buffer name for the following format:
           deviceIndex:hostApiName:hostDeviceName.
        */
        int i =  deviceNum;
        size_t size = 0;

        do
        {
            size++;
            i = i / 10 ;
        }
        while(i);		/*  index size */

        /* host API size +  host device size + 2 separators + zero termination */
        size += STRLEN(hostInfo->name) + STRLEN(deviceInfo->name) + 3u;
        *namePtr = MALLOC(size);

        if(*namePtr)
        {
            /* the name is filled if allocation is successful */
            SPRINTF(*namePtr, "%d:%s:%s", deviceNum,
                          hostInfo->name, deviceInfo->name);
        }

        return OK; /* deviceNum is a valid device */
    }
    else
    {
        return FAILED;    /* deviceNum is an invalid device */
    }
}

/**
 * Initializes "audio.portaudio.device" setting with an options list of unique device names
 * of available sound card devices.
 * @param settings pointer to settings.
 */
void
portaudioDriverSettings(Settings *settings)
{
    int numDevices;
    PaError err;
    int i;

    settingsRegisterStr(settings, "audio.portaudio.device", PORTAUDIO_DEFAULT_DEVICE, 0);
    settingsAddOption(settings, "audio.portaudio.device", PORTAUDIO_DEFAULT_DEVICE);

    err = Pa_Initialize();

    if(err != paNoError)
    {
        LOG(ERR, "Error initializing PortAudio driver: %s",
                  Pa_GetErrorText(err));
        return;
    }

    numDevices = Pa_GetDeviceCount();

    if(numDevices < 0)
    {
        LOG(ERR, "PortAudio returned unexpected device count %d", numDevices);
    }
    else
    {
        for(i = 0; i < numDevices; i++)
        {
            char *name;

            if(portaudioGetDeviceName(i, &name) == OK)
            {
                /* the device i is a valid output device */
                if(name)
                {
                    /* registers this name in the option list */
                    settingsAddOption(settings, "audio.portaudio.device", name);
                    FREE(name);
                }
                else
                {
                    LOG(ERR, "Out of memory");
                    break;
                }
            }
        }
    }

    /* done with PortAudio for now, may get reopened later */
    err = Pa_Terminate();

    if(err != paNoError)
    {
        printf("PortAudio termination error: %s\n", Pa_GetErrorText(err));
    }
}

/**
 * Creates the portaudio driver and opens the portaudio device
 * indicated by audio.portaudio.device setting.
 *
 * @param settings pointer to settings
 * @param synth the synthesizer instance
 * @return pointer to the driver on success, NULL otherwise.
 */
audioDriverT *
newPortaudioDriver(Settings *settings, Synthesizer *synth)
{
    portaudioDriverT *dev = NULL;
    PaStreamParameters outputParams;
    char *device = NULL; /* the portaudio device name to work with */
    double sampleRate;  /* intended sample rate */
    int periodSize;     /* intended buffer size */
    PaError err;

    dev = NEW(portaudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    err = Pa_Initialize();

    if(err != paNoError)
    {
        LOG(ERR, "Error initializing PortAudio driver: %s",
                  Pa_GetErrorText(err));
        FREE(dev);
        return NULL;
    }

    MEMSET(dev, 0, sizeof(portaudioDriverT));

    dev->synth = synth;

    /* gets audio parameters from the settings */
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsDupstr(settings, "audio.portaudio.device", &device);   /* ++ alloc device name */

    memset(&outputParams, 0, sizeof(outputParams));
    outputParams.channelCount = 2; /* For stereo output */
    outputParams.suggestedLatency = (PaTime)periodSize / sampleRate;

    /* Locate the device if specified */
    if(STRCMP(device, PORTAUDIO_DEFAULT_DEVICE) != 0)
    {
        /* The intended device is not the default device name, so we search
        a device among available devices */
        int numDevices;
        int i;

        numDevices = Pa_GetDeviceCount();

        if(numDevices < 0)
        {
            LOG(ERR, "PortAudio returned unexpected device count %d", numDevices);
            goto errorRecovery;
        }

        for(i = 0; i < numDevices; i++)
        {
            char *name;

            if(portaudioGetDeviceName(i, &name) == OK)
            {
                /* the device i is a valid output device */
                if(name)
                {
                    /* We see if the name corresponds to audio.portaudio.device */
                    char found = (STRCMP(device, name) == 0);
                    FREE(name);

                    if(found)
                    {
                        /* the device index is found */
                        outputParams.device = i;
                        /* The search is finished */
                        break;
                    }
                }
                else
                {
                    LOG(ERR, "Out of memory");
                    goto errorRecovery;
                }
            }
        }

        if(i == numDevices)
        {
            LOG(ERR, "PortAudio device '%s' was not found", device);
            goto errorRecovery;
        }
    }
    else
    {
        /* the default device will be used */
        outputParams.device = Pa_GetDefaultOutputDevice();
    }

    /* The device is found. We set the sample format and the audio rendering
       function suited to this format.
    */
    if(settingsStrEqual(settings, "audio.sample-format", "16bits"))
    {
        outputParams.sampleFormat = paInt16;
        dev->read = synthWriteS16;
    }
    else if(settingsStrEqual(settings, "audio.sample-format", "float"))
    {
        outputParams.sampleFormat = paFloat32;
        dev->read = synthWriteFloat;
    }
    else
    {
        LOG(ERR, "Unknown sample format");
        goto errorRecovery;
    }

    /* PortAudio section */

    /* Open an audio I/O stream. */
    err = Pa_OpenStream(&dev->stream,
                        NULL,              /* Input parameters */
                        &outputParams,     /* Output parameters */
                        sampleRate,
                        periodSize,
                        paNoFlag,
                        portaudioRun, /* callback */
                        dev);

    if(err != paNoError)
    {
        LOG(ERR, "Error opening PortAudio stream: %s",
                  Pa_GetErrorText(err));
        goto errorRecovery;
    }

    err = Pa_StartStream(dev->stream);  /* starts the I/O stream */

    if(err != paNoError)
    {
        LOG(ERR, "Error starting PortAudio stream: %s",
                  Pa_GetErrorText(err));
        goto errorRecovery;
    }

    if(device)
    {
        FREE(device);    /* -- free device name */
    }

    return (audioDriverT *)dev;

errorRecovery:

    if(device)
    {
        FREE(device);    /* -- free device name */
    }

    deletePortaudioDriver((audioDriverT *)dev);
    return NULL;
}

/* PortAudio callback
 * portaudioRun
 */
static int
portaudioRun(const void *input, void *output, unsigned long frameCount,
                    const PaStreamCallbackTimeInfo *timeInfo,
                    PaStreamCallbackFlags statusFlags, void *userData)
{
    portaudioDriverT *dev = (portaudioDriverT *)userData;
    /* it's as simple as that: */
    dev->read(dev->synth, frameCount, output, 0, 2, output, 1, 2);
    return 0;
}

/*
 * deletePortaudioDriver
 */
void
deletePortaudioDriver(audioDriverT *p)
{
    portaudioDriverT *dev = (portaudioDriverT *)p;
    PaError err;
    returnIfFail(dev != NULL);

    /* PortAudio section */
    if(dev->stream)
    {
        Pa_CloseStream(dev->stream);
    }

    err = Pa_Terminate();

    if(err != paNoError)
    {
        printf("PortAudio termination error: %s\n", Pa_GetErrorText(err));
    }

    FREE(dev);
}

#endif /* PORTAUDIO_SUPPORT */
