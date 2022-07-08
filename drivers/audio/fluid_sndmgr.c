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


/* sndmgr.c
 *
 * Driver for MacOS Classic
 */

#if SNDMAN_SUPPORT

#include "synth.h"
#include "adriver.h"
#include "settings.h"

#include <Sound.h>

typedef struct
{
    audioDriverT driver;
    SndDoubleBufferHeader2 *doubleHeader;
    SndDoubleBackUPP doubleCallbackProc;
    SndChannelPtr channel;
    int callbackIsAudioFunc;
    void *data;
    audioFuncT callback;
    float *convbuffers[2];
    int bufferByteSize;
    int bufferFrameSize;
} sndmgrAudioDriverT;

void  pascal sndmgrCallback(SndChannelPtr chan, SndDoubleBufferPtr doubleBuffer);
Fixed sndmgrDoubleToFix(long double theLD);

/*
 * generic new : returns error
 */
int
startFluidSndmgrAudioDriver(FluidSettings *settings,
                                sndmgrAudioDriverT *dev,
                                int bufferSize)
{
    int i;
    SndDoubleBufferHeader2 *doubleHeader = NULL;
    SndDoubleBufferPtr doubleBuffer = NULL;
    OSErr err;
    SndChannelPtr channel = NULL;
    double sampleRate;

    settingsGetnum(settings, "synth.sample-rate", &sampleRate);

    dev->doubleCallbackProc = NewSndDoubleBackProc(sndmgrCallback);

    /* the channel */
    FLUID_LOG(FLUID_DBG, "FLUID-SndManager@2");
    err = SndNewChannel(&channel, sampledSynth, initStereo, NULL);

    if((err != noErr) || (channel == NULL))
    {
        FLUID_LOG(FLUID_ERR, "Failed to allocate a sound channel (error %i)", err);
        return err;
    }

    /* the double buffer struct */
    FLUID_LOG(FLUID_DBG, "FLUID-SndManager@3");
    doubleHeader = FLUID_NEW(SndDoubleBufferHeader2);

    if(doubleHeader == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        return -1;
    }

    doubleHeader->dbhBufferPtr[0] = NULL;
    doubleHeader->dbhBufferPtr[1] = NULL;
    doubleHeader->dbhNumChannels = 2;
    doubleHeader->dbhSampleSize = 16;
    doubleHeader->dbhCompressionID = 0;
    doubleHeader->dbhPacketSize = 0;
    doubleHeader->dbhSampleRate = sndmgrDoubleToFix((long double) sampleRate);
    doubleHeader->dbhDoubleBack = dev->doubleCallbackProc;
    doubleHeader->dbhFormat = 0;

    /* prepare dev */
    FLUID_LOG(FLUID_DBG, "FLUID-SndManager@4");
    dev->doubleHeader = doubleHeader;
    dev->channel = channel;
    dev->bufferFrameSize = bufferSize;
    dev->bufferByteSize = bufferSize * 2 * 2;

    /* the 2 doublebuffers */
    FLUID_LOG(FLUID_DBG, "FLUID-SndManager@5");

    for(i = 0; i < 2; i++)
    {
        doubleBuffer = (SndDoubleBufferPtr) FLUID_MALLOC(sizeof(SndDoubleBuffer)
                       + dev->bufferByteSize);

        if(doubleBuffer == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            return -1;
        }

        doubleBuffer->dbNumFrames = 0;
        doubleBuffer->dbFlags = 0;
        doubleBuffer->dbUserInfo[0] = (long) dev;
        doubleHeader->dbhBufferPtr[i] = doubleBuffer;
        CallSndDoubleBackProc(doubleHeader->dbhDoubleBack, channel, doubleBuffer);
    }

    /* start */
    FLUID_LOG(FLUID_DBG, "FLUID-SndManager@6");

    err = SndPlayDoubleBuffer(channel, (SndDoubleBufferHeader *)doubleHeader);

    if(err != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to start the sound driver (error %i)", err);
        return err;
    }

    FLUID_LOG(FLUID_DBG, "FLUID-SndManager@7");
    return 0;
}

/*
 * newFluidSndmgrAudioDriver
 * This implementation used the 16bit format.
 */
audioDriverT *
newFluidSndmgrAudioDriver(FluidSettings *settings, synthT *synth)
{
    sndmgrAudioDriverT *dev = NULL;
    int periodSize, periods, bufferSize;

    /* check the format */
    if(!settingsStrEqual(settings, "audio.sample-format", "16bits"))
    {
        FLUID_LOG(FLUID_ERR, "Unhandled sample format");
        return NULL;
    }

    /* compute buffer size */
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetint(settings, "audio.periods", &periods);
    bufferSize = periodSize * periods;

    /* allocated dev */
    dev = FLUID_NEW(sndmgrAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(sndmgrAudioDriverT));

    dev->callbackIsAudioFunc = false;
    dev->data = (void *)synth;
    dev->callback = NULL;

    if(startFluidSndmgrAudioDriver(settings, dev, bufferSize) != 0)
    {
        deleteFluidSndmgrAudioDriver((audioDriverT *)dev);
        return NULL;
    }

    return (audioDriverT *)dev;
}

/*
 * newFluidSndmgrAudioDriver2
 *
 * This implementation used the audioFunc float format, with
 * conversion from float to 16bits in the driver.
 */
audioDriverT *
newFluidSndmgrAudioDriver2(FluidSettings *settings, audioFuncT func, void *data)
{
    sndmgrAudioDriverT *dev = NULL;
    int periodSize, periods, bufferSize;

    /* compute buffer size */
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetint(settings, "audio.periods", &periods);
    bufferSize = periodSize * periods;

    /* allocated dev */
    dev = FLUID_NEW(sndmgrAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(sndmgrAudioDriverT));

    /* allocate the conversion buffers */
    dev->convbuffers[0] = FLUID_ARRAY(float, bufferSize);
    dev->convbuffers[1] = FLUID_ARRAY(float, bufferSize);

    if((dev->convbuffers[0] == NULL) || (dev->convbuffers[1] == NULL))
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        goto errorRecovery;
    }

    dev->callbackIsAudioFunc = true;
    dev->data = data;
    dev->callback = func;

    if(startFluidSndmgrAudioDriver(settings, dev, bufferSize) != 0)
    {
        goto errorRecovery;
    }

    return (audioDriverT *)dev;

errorRecovery:
    deleteFluidSndmgrAudioDriver((audioDriverT *)dev);
    return NULL;
}

/*
 * deleteFluidSndmgrAudioDriver
 */
void deleteFluidSndmgrAudioDriver(audioDriverT *p)
{
    sndmgrAudioDriverT *dev = (sndmgrAudioDriverT *) p;
    returnIfFail(dev != NULL);

    if(dev->channel != NULL)
    {
        SndDisposeChannel(dev->channel, 1);
    }

    if(dev->doubleCallbackProc != NULL)
    {
        DisposeRoutineDescriptor(dev->doubleCallbackProc);
    }

    if(dev->doubleHeader != NULL)
    {
        FLUID_FREE(dev->doubleHeader->dbhBufferPtr[0]);
        FLUID_FREE(dev->doubleHeader->dbhBufferPtr[1]);
        FLUID_FREE(dev->doubleHeader);
    }

    FLUID_FREE(dev->convbuffers[0]);
    FLUID_FREE(dev->convbuffers[1]);
    FLUID_FREE(dev);
}

/*
 * sndmgrCallback
 *
 */
void  pascal sndmgrCallback(SndChannelPtr chan, SndDoubleBufferPtr doubleBuffer)
{
    sndmgrAudioDriverT *dev;
    signed short *buf;
    float *left;
    float *right;
    float v;
    int i, k, bufferSize;

    dev = (sndmgrAudioDriverT *) doubleBuffer->dbUserInfo[0];
    buf = (signed short *)doubleBuffer->dbSoundData;
    bufferSize = dev->bufferFrameSize;

    if(dev->callbackIsAudioFunc)
    {
        /* float API : conversion to signed short */
        left = dev->convbuffers[0];
        right = dev->convbuffers[1];

        FLUID_MEMSET(left, 0, bufferSize * sizeof(float));
        FLUID_MEMSET(right, 0, bufferSize * sizeof(float));

        (*dev->callback)(dev->data, bufferSize, 0, NULL, 2, dev->convbuffers);

        for(i = 0, k = 0; i < bufferSize; i++)
        {
            v = 32767.0f * left[i];
            clip(v, -32768.0f, 32767.0f);
            buf[k++] = (signed short) v;

            v = 32767.0f * right[i];
            clip(v, -32768.0f, 32767.0f);
            buf[k++] = (signed short) v;
        }

    }
    else
    {
        /* let the synth do the conversion */
        synthWriteS16((synthT *)dev->data, bufferSize, buf, 0, 2, buf, 1, 2);
    }

    doubleBuffer->dbFlags = doubleBuffer->dbFlags | dbBufferReady;
    doubleBuffer->dbNumFrames = bufferSize;
}

/*
 * sndmgrDoubleToFix
 *
 * A Fixed number is of the type 12345.67890.  It is 32 bits in size with the
 * high order bits representing the significant value (that before the point)
 * and the lower 16 bits representing the fractional part of the number.
 * The Sound Manager further complicates matters by using Fixed numbers, but
 * needing to represent numbers larger than what the Fixed is capable of.
 * To do this the Sound Manager treats the sign bit as having the value 32768
 * which will cause any number greater or equal to 32768 to look like it is
 * negative.
 * This routine is designed to "do the right thing" and convert any long double
 * into the Fixed number it represents.
 * long double is the input type because AIFF files use extended80 numbers and
 * there are routines that will convert from an extended80 to a long double.
 * A long double has far greater precision than a Fixed, so any number whose
 * significant or fraction is larger than 65535 will not convert correctly.
 */
#define _MAX_VALUE     65535
#define _BITS_PER_BYTE 8
Fixed sndmgrDoubleToFix(long double theLD)
{
    unsigned long	theResult = 0;
    unsigned short theSignificant = 0, theFraction = 0;

    if(theLD < _MAX_VALUE)
    {
        theSignificant = theLD;
        theFraction = theLD - theSignificant;

        if(theFraction > _MAX_VALUE)
        {
            /* Won't be able to convert */
            theSignificant = 0;
            theFraction = 0;
        }
    }

    theResult |= theSignificant;
    theResult = theResult << (sizeof(unsigned short) * _BITS_PER_BYTE);
    theResult |= theFraction;
    return theResult;
}

#endif
