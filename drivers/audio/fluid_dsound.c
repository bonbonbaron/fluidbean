/* FluidSynth - A Software Synthesizer
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



#include "synth.h"
#include "adriver.h"
#include "settings.h"


#if DSOUND_SUPPORT


#include <mmsystem.h>
#include <dsound.h>
#include <mmreg.h>

/* Those two includes are required on Windows 9x/ME */
#include <ks.h>
#include <ksmedia.h>

static DWORD WINAPI dsoundAudioRun(LPVOID lpParameter);
static int dsoundWriteProcessedChannels(synthT *data, int len,
                               int channelsCount,
                               void *channelsOut[], int channelsOff[],
                               int channelsIncr[]);

static char *win32_error(HRESULT hr);

/**
* The driver handle multiple channels.
* Actually the number maximum of channels is limited to  2 * DSOUND_MAX_STEREO_CHANNELS.
* The only reason of this limitation is because we dont know how to define the mapping
* of speakers for stereo output number above DSOUND_MAX_STEREO_CHANNELS.
*/
/* Maximum number of stereo outputs */
#define DSOUND_MAX_STEREO_CHANNELS 4
/* Speakers mapping */
static const DWORD channelMaskSpeakers[DSOUND_MAX_STEREO_CHANNELS] =
{
    /* 1 stereo output */
    SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT,
  
    /* 2 stereo outputs */
    SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,

    /* 3 stereo outputs */
    SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
    SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT,

    /* 4 stereo outputs */
    SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT |
    SPEAKER_FRONT_CENTER | SPEAKER_LOW_FREQUENCY |
    SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT |
    SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT
};

typedef struct
{
    audioDriverT driver;
    LPDIRECTSOUND directSound;      /* dsound instance */
    LPDIRECTSOUNDBUFFER primBuffer; /* dsound buffer*/
    LPDIRECTSOUNDBUFFER secBuffer;  /* dsound buffer */

    HANDLE thread;        /* driver task */
    DWORD threadID;
    void *synth; /* fluidsynth instance, or user pointer if custom processing function is defined */
    audioFuncT func;
    /* callback called by the task for audio rendering in dsound buffers */
    audioChannelsCallbackT write;
    HANDLE quitEv;       /* Event object to request the audio task to stop */
    float **drybuf;

    int   bytesPerSecond; /* number of bytes per second */
    DWORD bufferByteSize; /* size of one buffer in bytes */
    DWORD queueByteSize;  /* total size of all buffers in bytes */
    DWORD frameSize;       /* frame size in bytes */
    int channelsCount; /* number of channels in audio stream */
} dsoundAudioDriverT;

typedef struct
{
    LPGUID devGUID;
    char *devname;
} dsoundDevselT;

/* enumeration callback to add "device name" option on setting "audio.dsound.device" */
BOOL CALLBACK
dsoundEnumCallback(LPGUID guid, LPCTSTR description, LPCTSTR module, LPVOID context)
{
    FluidSettings *settings = (FluidSettings *) context;
    settingsAddOption(settings, "audio.dsound.device", (const char *)description);

    return TRUE;
}

/* enumeration callback to look if a certain device exists and return its GUID.
   @context, (dsoundDevselT *) context->devname provide the device name to look for.
             (dsoundDevselT *) context->devGUID pointer to return device GUID.
   @return TRUE to continue enumeration, FALSE otherwise.
*/
BOOL CALLBACK
dsoundEnumCallback2(LPGUID guid, LPCTSTR description, LPCTSTR module, LPVOID context)
{
    dsoundDevselT *devsel = (dsoundDevselT *) context;
    FLUID_LOG(FLUID_DBG, "Testing audio device: %s", description);

    if(FLUID_STRCASECMP(devsel->devname, description) == 0)
    {
        /* The device exists, return a copy of its GUID */
        devsel->devGUID = FLUID_NEW(GUID);

        if(devsel->devGUID)
        {
            /* return GUID */
            memcpy(devsel->devGUID, guid, sizeof(GUID));
            FLUID_LOG(FLUID_DBG, "Selected audio device GUID: %p", devsel->devGUID);
            return FALSE;
        }
    }

    return TRUE;
}

/*
   - register setting "audio.dsound.device".
   - add list of dsound device name as option of "audio.dsound.device" setting.
*/
void dsoundAudioDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "audio.dsound.device", "default", 0);
    settingsAddOption(settings, "audio.dsound.device", "default");
    DirectSoundEnumerate((LPDSENUMCALLBACK) dsoundEnumCallback, settings);
}


/*
 * newFluidDsoundAudioDriver
 * The driver handle the case of multiple stereo buffers provided by fluidsynth
 * mixer.
 * Each stereo buffers (left, right) are written to respective channels pair
 * of the audio device card.
 * For example, if the number of internal mixer buffer is 2, the audio device
 * must have at least 4 channels:
 * - buffer 0 (left, right) will be written to channel pair (0, 1).
 * - buffer 1 (left, right) will be written to channel pair (2, 3).
 *
 * @param setting. The settings the driver looks for:
 *  "synth.sample-rate", the sample rate.
 *  "audio.periods", the number of buffers and
 *  "audio.period-size", the size of each buffer.
 *  "audio.sample-format",the sample format, 16bits or float.

 * @param synth, fluidsynth synth instance to associate to the driver.
 *
 * Note: The number of internal mixer buffer is indicated by synth->audioChannels.
 * If the audio device cannot handle the format or do not have enough channels,
 * the driver fails and return NULL.
*/
audioDriverT *
newFluidDsoundAudioDriver(FluidSettings *settings, synthT *synth)
{
    return newFluidDsoundAudioDriver2(settings, NULL, synth);
}

audioDriverT *
newFluidDsoundAudioDriver2(FluidSettings *settings, audioFuncT func, void *data)
{
    HRESULT hr;
    DSBUFFERDESC desc;
    dsoundAudioDriverT *dev = NULL;
    DSCAPS caps;
    char *buf1;
    DWORD bytes1;
    double sampleRate;
    int periods, periodSize;
    int audioChannels;
    int i;
    dsoundDevselT devsel;
    WAVEFORMATEXTENSIBLE format;

    /* create and clear the driver data */
    dev = FLUID_NEW(dsoundAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(dsoundAudioDriverT));
    dev->synth = data;
    dev->func = func;

    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.periods", &periods);
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetint(settings, "synth.audio-channels", &audioChannels);

    /* Clear format structure*/
    ZeroMemory(&format, sizeof(WAVEFORMATEXTENSIBLE));

    /* Set this early so that if buffer allocation failed we can free the memory */
    dev->channelsCount = audioChannels * 2;

    /* check the format */
    if(!func)
    {
        if(settingsStrEqual(settings, "audio.sample-format", "float"))
        {
            FLUID_LOG(FLUID_DBG, "Selected 32 bit sample format");
            dev->write = synthWriteFloatChannels;
            /* sample container size in bits: 32 bits */
            format.Format.wBitsPerSample = 8 * sizeof(float);
            format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
            format.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        }
        else if(settingsStrEqual(settings, "audio.sample-format", "16bits"))
        {
            FLUID_LOG(FLUID_DBG, "Selected 16 bit sample format");
            dev->write = synthWriteS16_channels;
            /* sample container size in bits: 16bits */
            format.Format.wBitsPerSample = 8 * sizeof(short);
            format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
            format.Format.wFormatTag = WAVE_FORMAT_PCM;
        }
        else
        {
            FLUID_LOG(FLUID_ERR, "Unhandled sample format");
            goto errorRecovery;
        }
    }
    else
    {
        FLUID_LOG(FLUID_DBG, "Selected 32 bit sample format");
        dev->write = dsoundWriteProcessedChannels;
        /* sample container size in bits: 32 bits */
        format.Format.wBitsPerSample = 8 * sizeof(float);
        format.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        format.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        dev->drybuf = FLUID_ARRAY(float*, audioChannels * 2);
        if(dev->drybuf == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            goto errorRecovery;
        }
        FLUID_MEMSET(dev->drybuf, 0, sizeof(float*) * audioChannels * 2);
        for(i = 0; i < audioChannels * 2; ++i)
        {
            dev->drybuf[i] = FLUID_ARRAY(float, periods * periodSize);
            if(dev->drybuf[i] == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Out of memory");
                goto errorRecovery;
            }
        }
    }

    /* Finish to initialize the format structure */
    /* number of channels in a frame */
    format.Format.nChannels = audioChannels * 2;

    if(audioChannels > DSOUND_MAX_STEREO_CHANNELS)
    {
        FLUID_LOG(FLUID_ERR, "Channels number %d exceed internal limit %d",
                  format.Format.nChannels, DSOUND_MAX_STEREO_CHANNELS * 2);
        goto errorRecovery;
    }

    /* size of frame in bytes */
    format.Format.nBlockAlign = format.Format.nChannels * format.Format.wBitsPerSample / 8;
    format.Format.nSamplesPerSec = (DWORD) sampleRate;
    format.Format.nAvgBytesPerSec = format.Format.nBlockAlign * format.Format.nSamplesPerSec;

    /* extension */
    if(format.Format.nChannels > 2)
    {
        format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        format.Format.cbSize = 22;
        format.Samples.wValidBitsPerSample = format.Format.wBitsPerSample;

        /* CreateSoundBuffer accepts only format.dwChannelMask compatible with
           format.Format.nChannels
        */
        format.dwChannelMask = channelMaskSpeakers[audioChannels - 1];
    }

    /* Finish to initialize dev structure */
    dev->frameSize = format.Format.nBlockAlign;
    dev->bufferByteSize = periodSize * dev->frameSize;
    dev->queueByteSize = periods * dev->bufferByteSize;
    dev->bytesPerSecond = format.Format.nAvgBytesPerSec;

    devsel.devGUID = NULL;

    /* get the selected device name. if none is specified, use NULL for the default device. */
    if(settingsDupstr(settings, "audio.dsound.device", &devsel.devname) == FLUID_OK /* ++ alloc device name */
            && devsel.devname && strlen(devsel.devname) > 0)
    {
        /* look for the GUID of the selected device */
        DirectSoundEnumerate((LPDSENUMCALLBACK) dsoundEnumCallback2, (void *)&devsel);
    }

    if(devsel.devname)
    {
        FLUID_FREE(devsel.devname);    /* -- free device name */
    }

    /* open DirectSound */
    hr = DirectSoundCreate(devsel.devGUID, &dev->directSound, NULL);

    if(devsel.devGUID)
    {
        FLUID_FREE(devsel.devGUID);    /* -- free device GUID */
    }

    if(hr != DS_OK)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create the DirectSound object: '%s'", win32_error(hr));
        goto errorRecovery;
    }

    hr = IDirectSound_SetCooperativeLevel(dev->directSound, GetDesktopWindow(), DSSCL_PRIORITY);

    if(hr != DS_OK)
    {
        FLUID_LOG(FLUID_ERR, "Failed to set the cooperative level: '%s'", win32_error(hr));
        goto errorRecovery;
    }

    caps.dwSize = sizeof(caps);
    hr = IDirectSound_GetCaps(dev->directSound, &caps);

    if(hr != DS_OK)
    {
        FLUID_LOG(FLUID_ERR, "Failed to query the device capacities: '%s'", win32_error(hr));
        goto errorRecovery;
    }

    /* create primary buffer */

    ZeroMemory(&desc, sizeof(DSBUFFERDESC));
    desc.dwSize = sizeof(DSBUFFERDESC);
    desc.dwFlags = DSBCAPS_PRIMARYBUFFER;

    if(caps.dwFreeHwMixingStreamingBuffers > 0)
    {
        desc.dwFlags |= DSBCAPS_LOCHARDWARE;
    }

    hr = IDirectSound_CreateSoundBuffer(dev->directSound, &desc, &dev->primBuffer, NULL);

    if(hr != DS_OK)
    {
        FLUID_LOG(FLUID_ERR, "Failed to allocate the primary buffer: '%s'", win32_error(hr));
        goto errorRecovery;
    }

    /* set the primary sound buffer to this format. if it fails, just
       print a warning. */
    hr = IDirectSoundBuffer_SetFormat(dev->primBuffer, (WAVEFORMATEX *)&format);

    if(hr != DS_OK)
    {
        FLUID_LOG(FLUID_WARN, "Can't set format of primary sound buffer: '%s'", win32_error(hr));
    }

    /* initialize the buffer description */

    ZeroMemory(&desc, sizeof(DSBUFFERDESC));
    desc.dwSize = sizeof(DSBUFFERDESC);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_GETCURRENTPOSITION2;

    /* CreateSoundBuffer accepts only format.dwChannelMask compatible with
       format.Format.nChannels
    */
    desc.lpwfxFormat = (WAVEFORMATEX *)&format;
    desc.dwBufferBytes = dev->queueByteSize;

    if(caps.dwFreeHwMixingStreamingBuffers > 0)
    {
        desc.dwFlags |= DSBCAPS_LOCHARDWARE;
    }

    /* create the secondary sound buffer */

    hr = IDirectSound_CreateSoundBuffer(dev->directSound, &desc, &dev->secBuffer, NULL);

    if(hr != DS_OK)
    {
        FLUID_LOG(FLUID_ERR, "dsound: Can't create sound buffer: '%s'", win32_error(hr));
        goto errorRecovery;
    }


    /* Lock and get dsound buffer */
    hr = IDirectSoundBuffer_Lock(dev->secBuffer, 0, 0, (void *) &buf1, &bytes1, 0, 0, DSBLOCK_ENTIREBUFFER);

    if((hr != DS_OK) || (buf1 == NULL))
    {
        FLUID_LOG(FLUID_PANIC, "Failed to lock the audio buffer: '%s'", win32_error(hr));
        goto errorRecovery;
    }

    /* fill the buffer with silence */
    memset(buf1, 0, bytes1);

    /* Unlock dsound buffer */
    IDirectSoundBuffer_Unlock(dev->secBuffer, buf1, bytes1, 0, 0);

    /* Create object to signal thread exit */
    dev->quitEv = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(dev->quitEv == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create quit Event: '%s'", getWindowsError());
        goto errorRecovery;
    }

    /* start the audio thread */
    dev->thread = CreateThread(NULL, 0, dsoundAudioRun, (LPVOID) dev, 0, &dev->threadID);

    if(dev->thread == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create DSound audio thread: '%s'", getWindowsError());
        goto errorRecovery;
    }

    return (audioDriverT *) dev;

errorRecovery:
    deleteFluidDsoundAudioDriver((audioDriverT *) dev);
    return NULL;
}


void deleteFluidDsoundAudioDriver(audioDriverT *d)
{
    int i;

    dsoundAudioDriverT *dev = (dsoundAudioDriverT *) d;
    returnIfFail(dev != NULL);

    /* First tell dsound to stop playing its buffer now.
       Doing this before stopping audio thread avoid dsound playing transient
       glitches between the time audio task is stopped and dsound will be released.
       These glitches are particularly audible when a reverb is connected
       on output.
    */
    if(dev->secBuffer != NULL)
    {
        IDirectSoundBuffer_Stop(dev->secBuffer);
    }

    /* request the audio task to stop and wait till the audio thread exits */
    if(dev->thread != NULL)
    {
        /* tell the audio thread to stop its loop */
        SetEvent(dev->quitEv);

        if(WaitForSingleObject(dev->thread, 2000) != WAIT_OBJECT_0)
        {
            /* on error kill the thread mercilessly */
            FLUID_LOG(FLUID_DBG, "Couldn't join the audio thread. killing it.");
            TerminateThread(dev->thread, 0);
        }

        /* Release the thread object */
        CloseHandle(dev->thread);
    }

    /* Release the event object */
    if(dev->quitEv != NULL)
    {
        CloseHandle(dev->quitEv);
    }

    /* Finish to release all the dsound allocated resources */

    if(dev->secBuffer != NULL)
    {
        IDirectSoundBuffer_Release(dev->secBuffer);
    }

    if(dev->primBuffer != NULL)
    {
        IDirectSoundBuffer_Release(dev->primBuffer);
    }

    if(dev->directSound != NULL)
    {
        IDirectSound_Release(dev->directSound);
    }

    if(dev->drybuf != NULL)
    {
        for(i = 0; i < dev->channelsCount; ++i)
        {
            FLUID_FREE(dev->drybuf[i]);
        }
    }

    FLUID_FREE(dev->drybuf);

    FLUID_FREE(dev);
}

static DWORD WINAPI dsoundAudioRun(LPVOID lpParameter)
{
    dsoundAudioDriverT *dev = (dsoundAudioDriverT *) lpParameter;
    short *buf1, *buf2;
    DWORD bytes1, bytes2;
    DWORD curPosition, frames, playPosition, writePosition, bytes;
    HRESULT res;
    int     ms;

    /* pointers table on output first sample channels */
    void *channelsOut[DSOUND_MAX_STEREO_CHANNELS * 2];
    int channelsOff[DSOUND_MAX_STEREO_CHANNELS * 2];
    int channelsIncr[DSOUND_MAX_STEREO_CHANNELS * 2];
    int i;

    /* initialize write callback constant parameters:
       dsound expects interleaved channels in a unique buffer.
       For example 4 channels (c1, c2, c3, c4) and n samples:
       { s1:c1, s1:c2, s1:c3, s1:c4,  s2:c1, s2:c2, s2:c3, s2:c4,...
         sn:c1, sn:c2, sn:c3, sn:c4 }.

       So, channelsOff[], channnelIncr[] tables should initialized like this:
         channelsOff[0] = 0    channelsIncr[0] = 4
         channelsOff[1] = 1    channelsIncr[1] = 4
         channelsOff[2] = 2    channelsIncr[2] = 4
         channelsOff[3] = 3    channelsIncr[3] = 4

       channelsOut[], table will be initialized later, just before calling
       the write callback function.
         channelsOut[0] = address of dsound buffer
         channelsOut[1] = address of dsound buffer
         channelsOut[2] = address of dsound buffer
         channelsOut[3] = address of dsound buffer
    */
    for(i = 0; i < dev->channelsCount; i++)
    {
        channelsOff[i] = i;
        channelsIncr[i] = dev->channelsCount;
    }

    curPosition = 0;

    /* boost the priority of the audio thread */
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

    IDirectSoundBuffer_Play(dev->secBuffer, 0, 0, DSBPLAY_LOOPING);

    for(;;)
    {
        IDirectSoundBuffer_GetCurrentPosition(dev->secBuffer, &playPosition, &writePosition);

        if(curPosition <= playPosition)
        {
            bytes = playPosition - curPosition;
        }
        else if((playPosition < curPosition) && (writePosition <= curPosition))
        {
            bytes = dev->queueByteSize + playPosition - curPosition;
        }
        else
        {
            bytes = 0;
        }

        if(bytes >= dev->bufferByteSize)
        {

            /* Lock */
            res = IDirectSoundBuffer_Lock(dev->secBuffer, curPosition, bytes, (void *) &buf1, &bytes1, (void *) &buf2, &bytes2, 0);

            if((res != DS_OK) || (buf1 == NULL))
            {
                FLUID_LOG(FLUID_PANIC, "Failed to lock the audio buffer. System lockup might follow. Exiting.");
                ExitProcess((UINT)-1);
            }

            /* fill the first part of the buffer */
            if(bytes1 > 0)
            {
                frames = bytes1 / dev->frameSize;
                /* Before calling write function, finish to initialize
                   channelsOut[] table parameter:
                   dsound expects interleaved channels in a unique buffer.
                   So, channelsOut[] table must be initialized with the address
                   of the same buffer (buf1).
                */
                i = dev->channelsCount;

                do
                {
                    channelsOut[--i] = buf1;
                }
                while(i);

                /* calling write function */
                if(dev->func == NULL)
                {
                    dev->write(dev->synth, frames, dev->channelsCount,
                            channelsOut, channelsOff, channelsIncr);
                }
                else
                {
                    dev->write((synthT*)dev, frames, dev->channelsCount,
                            channelsOut, channelsOff, channelsIncr);
                }
                curPosition += frames * dev->frameSize;
            }

            /* fill the second part of the buffer */
            if((buf2 != NULL) && (bytes2 > 0))
            {
                frames = bytes2 / dev->frameSize;
                /* Before calling write function, finish to initialize
                   channelsOut[] table parameter:
                   dsound expects interleaved channels in a unique buffer.
                   So, channelsOut[] table must be initialized with the address
                   of the same buffer (buf2).
                */
                i = dev->channelsCount;

                do
                {
                    channelsOut[--i] = buf2;
                }
                while(i);

                /* calling write function */
                if(dev->func == NULL)
                {
                    dev->write(dev->synth, frames, dev->channelsCount,
                            channelsOut, channelsOff, channelsIncr);
                }
                else
                {
                    dev->write((synthT*)dev, frames, dev->channelsCount,
                            channelsOut, channelsOff, channelsIncr);
                }
                curPosition += frames * dev->frameSize;
            }

            /* Unlock */
            IDirectSoundBuffer_Unlock(dev->secBuffer, buf1, bytes1, buf2, bytes2);

            if(curPosition >= dev->queueByteSize)
            {
                curPosition -= dev->queueByteSize;
            }

            /* 1 ms of wait */
            ms = 1;
        }
        else
        {
            /* Calculate how many milliseconds to sleep (minus 1 for safety) */
            ms = (dev->bufferByteSize - bytes) * 1000 / dev->bytesPerSecond - 1;

            if(ms < 1)
            {
                ms = 1;
            }
        }

        /* Wait quit event or timeout */
        if(WaitForSingleObject(dev->quitEv, ms) == WAIT_OBJECT_0)
        {
            break;
        }
    }

    return 0;
}

static int dsoundWriteProcessedChannels(synthT *data, int len,
                               int channelsCount,
                               void *channelsOut[], int channelsOff[],
                               int channelsIncr[])
{
    int i, ch;
    int ret;
    dsoundAudioDriverT *drv = (dsoundAudioDriverT*) data;
    float *optr[DSOUND_MAX_STEREO_CHANNELS * 2];
    for(ch = 0; ch < drv->channelsCount; ++ch)
    {
        FLUID_MEMSET(drv->drybuf[ch], 0, len * sizeof(float));
        optr[ch] = (float*)channelsOut[ch] + channelsOff[ch];
    }
    ret = drv->func(drv->synth, len, 0, NULL, drv->channelsCount, drv->drybuf);
    for(ch = 0; ch < drv->channelsCount; ++ch)
    {
        for(i = 0; i < len; ++i)
        {
            *optr[ch] = drv->drybuf[ch][i];
            optr[ch] += channelsIncr[ch];
        }
    }
    return ret;
}


static char *win32_error(HRESULT hr)
{
    char *s = "Don't know why";

    switch(hr)
    {
    case E_NOINTERFACE:
        s = "No such interface";
        break;

    case DSERR_GENERIC:
        s = "Generic error";
        break;

    case DSERR_ALLOCATED:
        s = "Required resources already allocated";
        break;

    case DSERR_BADFORMAT:
        s = "The format is not supported";
        break;

    case DSERR_INVALIDPARAM:
        s = "Invalid parameter";
        break;

    case DSERR_NOAGGREGATION:
        s = "No aggregation";
        break;

    case DSERR_OUTOFMEMORY:
        s = "Out of memory";
        break;

    case DSERR_UNINITIALIZED:
        s = "Uninitialized";
        break;

    case DSERR_UNSUPPORTED:
        s = "Function not supported";
        break;
    }

    return s;
}

#endif /* DSOUND_SUPPORT */
