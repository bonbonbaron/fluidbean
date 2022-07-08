/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 * Copyright (C) 2021  E. "sykhro" Melucci
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

/* pipewire.c
 *
 * Audio driver for PipeWire.
 *
 */

#include "synth.h"
#include "adriver.h"
#include "settings.h"

#if PIPEWIRE_SUPPORT

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>

/* At the moment, only stereo is supported */
#define NUM_CHANNELS 2
static const int stride = sizeof(float) * NUM_CHANNELS;

typedef struct
{
    audioDriverT driver;
    audioFuncT userCallback;
    void *data;

    /* Used only with the user-provided callback */
    float *lbuf, *rbuf;

    int bufferPeriod;
    struct spaPodBuilder *builder;

    struct pwThreadLoop *pwLoop;
    struct pwStream *pwStream;
    struct pwStreamEvents *events;

} pipewireAudioDriverT;


/* Fast-path rendering routine with no user processing callbacks */
static void pipewireEventProcess(void *data)
{
    pipewireAudioDriverT *drv = data;
    struct pwBuffer *pwb;
    struct spaBuffer *buf;
    float *dest;

    pwb = pwStreamDequeueBuffer(drv->pwStream);

    if(!pwb)
    {
        FLUID_LOG(FLUID_WARN, "No buffers!");
        return;
    }

    buf = pwb->buffer;
    dest = buf->datas[0].data;

    if(!dest)
    {
        return;
    }

    synthWriteFloat(drv->data, drv->bufferPeriod, dest, 0, 2, dest, 1, 2);
    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = drv->bufferPeriod * stride;

    pwStreamQueueBuffer(drv->pwStream, pwb);
}

/* Rendering routine with support for user-defined audio manipulation */
static void pipewireEventProcess2(void *data)
{
    pipewireAudioDriverT *drv = data;
    struct pwBuffer *pwb;
    struct spaBuffer *buf;
    float *dest;
    float *channels[NUM_CHANNELS] = { drv->lbuf, drv->rbuf };
    int i;

    pwb = pwStreamDequeueBuffer(drv->pwStream);

    if(!pwb)
    {
        FLUID_LOG(FLUID_WARN, "No buffers!");
        return;
    }

    buf = pwb->buffer;
    dest = buf->datas[0].data;

    if(!dest)
    {
        return;
    }

    FLUID_MEMSET(drv->lbuf, 0, drv->bufferPeriod * sizeof(float));
    FLUID_MEMSET(drv->rbuf, 0, drv->bufferPeriod * sizeof(float));

    (*drv->userCallback)(drv->data, drv->bufferPeriod, 0, NULL, NUM_CHANNELS, channels);

    /* Interleave the floating point data */
    for(i = 0; i < drv->bufferPeriod; i++)
    {
        dest[i * 2] = drv->lbuf[i];
        dest[i * 2 + 1] = drv->rbuf[i];
    }

    buf->datas[0].chunk->offset = 0;
    buf->datas[0].chunk->stride = stride;
    buf->datas[0].chunk->size = drv->bufferPeriod * stride;

    pwStreamQueueBuffer(drv->pwStream, pwb);
}

audioDriverT *newFluidPipewireAudioDriver(FluidSettings *settings, synthT *synth)
{
    return newFluidPipewireAudioDriver2(settings, NULL, synth);
}

audioDriverT *
newFluidPipewireAudioDriver2(FluidSettings *settings, audioFuncT func, void *data)
{
    pipewireAudioDriverT *drv;
    int periodSize;
    int bufferLength;
    int res;
    int pwFlags;
    int realtimePrio = 0;
    double sampleRate;
    char *mediaRole = NULL;
    char *mediaType = NULL;
    char *mediaCategory = NULL;
    float *buffer = NULL;
    const struct spaPod *params[1];

    drv = FLUID_NEW(pipewireAudioDriverT);

    if(!drv)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(drv, 0, sizeof(*drv));

    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetint(settings, "audio.realtime-prio", &realtimePrio);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsDupstr(settings, "audio.pipewire.media-role", &mediaRole);
    settingsDupstr(settings, "audio.pipewire.media-type", &mediaType);
    settingsDupstr(settings, "audio.pipewire.media-category", &mediaCategory);

    drv->data = data;
    drv->userCallback = func;
    drv->bufferPeriod = periodSize;

    drv->events = FLUID_NEW(struct pwStreamEvents);

    if(!drv->events)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto driverCleanup;
    }

    FLUID_MEMSET(drv->events, 0, sizeof(*drv->events));
    drv->events->version = PW_VERSION_STREAM_EVENTS;
    drv->events->process = func ? pipewireEventProcess2 : pipewireEventProcess;

    drv->pwLoop = pwThreadLoopNew("pipewire", NULL);

    if(!drv->pwLoop)
    {
        FLUID_LOG(FLUID_ERR, "Failed to allocate PipeWire loop. Have you called pwInit() ?");
        goto driverCleanup;
    }

    drv->pwStream = pwStreamNewSimple(
                         pwThreadLoopGetLoop(drv->pwLoop),
                         "FluidSynth",
                         pwPropertiesNew(PW_KEY_MEDIA_TYPE, mediaType, PW_KEY_MEDIA_CATEGORY, mediaCategory, PW_KEY_MEDIA_ROLE, mediaRole, NULL),
                         drv->events,
                         drv);

    if(!drv->pwStream)
    {
        FLUID_LOG(FLUID_ERR, "Failed to allocate PipeWire stream");
        goto driverCleanup;
    }

    buffer = FLUID_ARRAY(float, NUM_CHANNELS * periodSize);

    if(!buffer)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto driverCleanup;
    }

    bufferLength = periodSize * sizeof(float) * NUM_CHANNELS;

    drv->builder = FLUID_NEW(struct spaPodBuilder);

    if(!drv->builder)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto driverCleanup;
    }

    FLUID_MEMSET(drv->builder, 0, sizeof(*drv->builder));
    drv->builder->data = buffer;
    drv->builder->size = bufferLength;

    if(func)
    {
        drv->lbuf = FLUID_ARRAY(float, periodSize);
        drv->rbuf = FLUID_ARRAY(float, periodSize);

        if(!drv->lbuf || !drv->rbuf)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            goto driverCleanup;
        }
    }

    params[0] = spaFormatAudioRawBuild(drv->builder,
                                           SPA_PARAM_EnumFormat,
                                           &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_F32,
                                                   .channels = 2,
                                                   .rate = sampleRate));

    pwFlags = PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS;
    pwFlags |= realtimePrio ? PW_STREAM_FLAG_RT_PROCESS : 0;
    res = pwStreamConnect(drv->pwStream, PW_DIRECTION_OUTPUT, PW_ID_ANY, pwFlags, params, 1);

    if(res < 0)
    {
        FLUID_LOG(FLUID_ERR, "PipeWire stream connection failed");
        goto driverCleanup;
    }

    res = pwThreadLoopStart(drv->pwLoop);

    if(res != 0)
    {
        FLUID_LOG(FLUID_ERR, "Failed starting PipeWire loop");
        goto driverCleanup;
    }

    FLUID_LOG(FLUID_INFO, "Using PipeWire audio driver");

    FLUID_FREE(mediaRole);
    FLUID_FREE(mediaType);
    FLUID_FREE(mediaCategory);

    return (audioDriverT *)drv;

driverCleanup:
    FLUID_FREE(mediaRole);
    FLUID_FREE(mediaType);
    FLUID_FREE(mediaCategory);

    deleteFluidPipewireAudioDriver((audioDriverT *)drv);
    return NULL;
}

void deleteFluidPipewireAudioDriver(audioDriverT *p)
{
    pipewireAudioDriverT *drv = (pipewireAudioDriverT *)p;
    returnIfFail(drv);

    if(drv->pwStream)
    {
        pwStreamDestroy(drv->pwStream);
    }

    if(drv->pwLoop)
    {
        pwThreadLoopDestroy(drv->pwLoop);
    }

    FLUID_FREE(drv->lbuf);
    FLUID_FREE(drv->rbuf);

    if(drv->builder)
    {
        FLUID_FREE(drv->builder->data);
    }

    FLUID_FREE(drv->builder);

    FLUID_FREE(drv->events);

    FLUID_FREE(drv);
}

void pipewireAudioDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "audio.pipewire.media-role", "Music", 0);
    settingsRegisterStr(settings, "audio.pipewire.media-type", "Audio", 0);
    settingsRegisterStr(settings, "audio.pipewire.media-category", "Playback", 0);
}

#endif
