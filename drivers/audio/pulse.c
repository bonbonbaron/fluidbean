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

/* pulse.c
 *
 * Audio driver for PulseAudio.
 *
 */

#include "synth.h"
#include "adriver.h"
#include "settings.h"

#if PULSE_SUPPORT

#include <pulse/simple.h>
#include <pulse/error.h>

/** pulseAudioDriverT
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
    audioDriverT driver;
    paSimple *paHandle;
    audioFuncT callback;
    void *data;
    int bufferSize;
    threadT *thread;
    int cont;

    float *left;
    float *right;
    float *buf;
} pulseAudioDriverT;


static threadReturnT pulseAudioRun(void *d);
static threadReturnT pulseAudioRun2(void *d);


void pulseAudioDriverSettings(Settings *settings)
{
    settingsRegisterStr(settings, "audio.pulseaudio.server", "default", 0);
    settingsRegisterStr(settings, "audio.pulseaudio.device", "default", 0);
    settingsRegisterStr(settings, "audio.pulseaudio.media-role", "music", 0);
    settingsRegisterInt(settings, "audio.pulseaudio.adjust-latency", 1, 0, 1,
                                HINT_TOGGLED);
}


audioDriverT *
newPulseAudioDriver(Settings *settings,
                             Synthesizer *synth)
{
    return newPulseAudioDriver2(settings, NULL, synth);
}

audioDriverT *
newPulseAudioDriver2(Settings *settings,
                              audioFuncT func, void *data)
{
    pulseAudioDriverT *dev;
    paSampleSpec samplespec;
    paBufferAttr bufattr;
    double sampleRate;
    int periodSize, periodBytes, adjustLatency;
    char *server = NULL;
    char *device = NULL;
    char *mediaRole = NULL;
    int realtimePrio = 0;
    int err;
    float *left = NULL,
           *right = NULL,
            *buf = NULL;

    dev = NEW(pulseAudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(pulseAudioDriverT));

    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsDupstr(settings, "audio.pulseaudio.server", &server);  /* ++ alloc server string */
    settingsDupstr(settings, "audio.pulseaudio.device", &device);  /* ++ alloc device string */
    settingsDupstr(settings, "audio.pulseaudio.media-role", &mediaRole);  /* ++ alloc media-role string */
    settingsGetint(settings, "audio.realtime-prio", &realtimePrio);
    settingsGetint(settings, "audio.pulseaudio.adjust-latency", &adjustLatency);

    if(mediaRole != NULL)
    {
        if(STRCMP(mediaRole, "") != 0)
        {
            gSetenv("PULSE_PROP_media.role", mediaRole, TRUE);
        }

        FREE(mediaRole);       /* -- free mediaRole string */
    }

    if(server && STRCMP(server, "default") == 0)
    {
        FREE(server);         /* -- free server string */
        server = NULL;
    }

    if(device && STRCMP(device, "default") == 0)
    {
        FREE(device);         /* -- free device string */
        device = NULL;
    }

    dev->data = data;
    dev->callback = func;
    dev->cont = 1;
    dev->bufferSize = periodSize;

    samplespec.format = PA_SAMPLE_FLOAT32NE;
    samplespec.channels = 2;
    samplespec.rate = sampleRate;

    periodBytes = periodSize * sizeof(float) * 2;
    bufattr.maxlength = adjustLatency ? -1 : periodBytes;
    bufattr.tlength = periodBytes;
    bufattr.minreq = -1;
    bufattr.prebuf = -1;    /* Just initialize to same value as tlength */
    bufattr.fragsize = -1;  /* Not used */

    dev->paHandle = paSimpleNew(server, "Synth", PA_STREAM_PLAYBACK,
                                   device, "Synth output", &samplespec,
                                   NULL, /* paChannelMap */
                                   &bufattr,
                                   &err);

    if(!dev->paHandle)
    {
        LOG(ERR, "Failed to create PulseAudio connection");
        goto errorRecovery;
    }

    LOG(INFO, "Using PulseAudio driver");

    if(func != NULL)
    {
        left = ARRAY(float, periodSize);
        right = ARRAY(float, periodSize);

        if(left == NULL || right == NULL)
        {
            LOG(ERR, "Out of memory.");
            goto errorRecovery;
        }
    }

    buf = ARRAY(float, periodSize * 2);
    if(buf == NULL)
    {
        LOG(ERR, "Out of memory.");
        goto errorRecovery;
    }

    dev->left = left;
    dev->right = right;
    dev->buf = buf;

    /* Create the audio thread */
    dev->thread = newThread("pulse-audio", func ? pulseAudioRun2 : pulseAudioRun,
                                   dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    FREE(server);    /* -- free server string */
    FREE(device);    /* -- free device string */

    return (audioDriverT *) dev;

errorRecovery:
    FREE(server);    /* -- free server string */
    FREE(device);    /* -- free device string */
    FREE(left);
    FREE(right);
    FREE(buf);

    deletePulseAudioDriver((audioDriverT *) dev);
    return NULL;
}

void deletePulseAudioDriver(audioDriverT *p)
{
    pulseAudioDriverT *dev = (pulseAudioDriverT *) p;
    returnIfFail(dev != NULL);

    dev->cont = 0;

    if(dev->thread)
    {
        threadJoin(dev->thread);
        deleteThread(dev->thread);
    }

    if(dev->paHandle)
    {
        paSimpleFree(dev->paHandle);
    }

    FREE(dev->left);
    FREE(dev->right);
    FREE(dev->buf);

    FREE(dev);
}

/* Thread without audio callback, more efficient */
static threadReturnT
pulseAudioRun(void *d)
{
    pulseAudioDriverT *dev = (pulseAudioDriverT *) d;
    float *buf = dev->buf;
    int bufferSize;
    int err;

    bufferSize = dev->bufferSize;

    while(dev->cont)
    {
        synthWriteFloat(dev->data, bufferSize, buf, 0, 2, buf, 1, 2);

        if(paSimpleWrite(dev->paHandle, buf,
                           bufferSize * sizeof(float) * 2, &err) < 0)
        {
            LOG(ERR, "Error writing to PulseAudio connection.");
            break;
        }
    }	/* while (dev->cont) */

    return THREAD_RETURN_VALUE;
}

static threadReturnT
pulseAudioRun2(void *d)
{
    pulseAudioDriverT *dev = (pulseAudioDriverT *) d;
    Synthesizer *synth = (Synthesizer *)(dev->data);
    float *left = dev->left,
           *right = dev->right,
            *buf = dev->buf;
    float *handle[2];
    int bufferSize;
    int err;
    int i;

    bufferSize = dev->bufferSize;

    handle[0] = left;
    handle[1] = right;

    while(dev->cont)
    {
        MEMSET(left, 0, bufferSize * sizeof(float));
        MEMSET(right, 0, bufferSize * sizeof(float));

        (*dev->callback)(synth, bufferSize, 0, NULL, 2, handle);

        /* Interleave the floating point data */
        for(i = 0; i < bufferSize; i++)
        {
            buf[i * 2] = left[i];
            buf[i * 2 + 1] = right[i];
        }

        if(paSimpleWrite(dev->paHandle, buf,
                           bufferSize * sizeof(float) * 2, &err) < 0)
        {
            LOG(ERR, "Error writing to PulseAudio connection.");
            break;
        }
    }	/* while (dev->cont) */

    return THREAD_RETURN_VALUE;
}

#endif /* PULSE_SUPPORT */

