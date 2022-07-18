/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

/* opensles.c
 *
 * Audio driver for OpenSLES.
 *
 */

#include "adriver.h"

#if OPENSLES_SUPPORT

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

static const int NUM_CHANNELS = 2;

/** openslesAudioDriverT
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
    audioDriverT driver;
    SLObjectItf engine;
    SLObjectItf outputMixObject;
    SLObjectItf audioPlayer;
    SLPlayItf audioPlayerInterface;
    SLAndroidSimpleBufferQueueItf playerBufferQueueInterface;

    void *synth;
    int periodFrames;

    int isSampleFormatFloat;

    /* used only by callback mode */
    short *slesBufferShort;
    float *slesBufferFloat;

    int cont;

    double sampleRate;
} openslesAudioDriverT;


static void openslesCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext);
static void processBuffer(openslesAudioDriverT *dev);

void openslesAudioDriverSettings(Settings *settings)
{
}


/*
 * newOpenslesAudioDriver
 */
audioDriverT *
newOpenslesAudioDriver(Settings *settings, Synthesizer *synth)
{
    SLresult result;
    openslesAudioDriverT *dev;
    double sampleRate;
    int periodSize;
    int realtimePrio = 0;
    int isSampleFormatFloat;
    SLEngineItf engineInterface;

    dev = NEW(openslesAudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(*dev));

    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.realtime-prio", &realtimePrio);
    isSampleFormatFloat = settingsStrEqual(settings, "audio.sample-format", "float");

    dev->synth = synth;
    dev->isSampleFormatFloat = isSampleFormatFloat;
    dev->periodFrames = periodSize;
    dev->sampleRate = sampleRate;
    dev->cont = 1;

    result = slCreateEngine(&(dev->engine), 0, NULL, 0, NULL, NULL);

    if(!dev->engine)
    {
        LOG(ERR, "Failed to create the OpenSL ES engine, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->engine)->Realize(dev->engine, SL_BOOLEAN_FALSE);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to realize the OpenSL ES engine, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->engine)->GetInterface(dev->engine, SL_IID_ENGINE, &engineInterface);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to retrieve the OpenSL ES engine interface, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*engineInterface)->CreateOutputMix(engineInterface, &dev->outputMixObject, 0, 0, 0);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to create the OpenSL ES output mix object, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->outputMixObject)->Realize(dev->outputMixObject, SL_BOOLEAN_FALSE);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to realize the OpenSL ES output mix object, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    {
        SLDataLocator_AndroidSimpleBufferQueue locBufferQueue =
        {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            2 /* number of buffers */
        };
        SLAndroidDataFormat_PCM_EX formatPcm =
        {
            SL_ANDROID_DATAFORMAT_PCM_EX,
            NUM_CHANNELS,
            ((SLuint32) sampleRate) * 1000,
            isSampleFormatFloat ? SL_PCMSAMPLEFORMAT_FIXED_32 : SL_PCMSAMPLEFORMAT_FIXED_16,
            isSampleFormatFloat ? SL_PCMSAMPLEFORMAT_FIXED_32 : SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,
            SL_BYTEORDER_LITTLEENDIAN,
            isSampleFormatFloat ? SL_ANDROID_PCM_REPRESENTATION_FLOAT : SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT
        };
        SLDataSource audioSrc =
        {
            &locBufferQueue,
            &formatPcm
        };

        SLDataLocator_OutputMix locOutmix =
        {
            SL_DATALOCATOR_OUTPUTMIX,
            dev->outputMixObject
        };
        SLDataSink audioSink = {&locOutmix, NULL};

        const SLInterfaceID ids1[] = {SL_IID_ANDROIDSIMPLEBUFFERQUEUE};
        const SLboolean req1[] = {SL_BOOLEAN_TRUE};
        result = (*engineInterface)->CreateAudioPlayer(engineInterface,
                &(dev->audioPlayer), &audioSrc, &audioSink, 1, ids1, req1);
    }

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to create the OpenSL ES audio player object, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->audioPlayer)->Realize(dev->audioPlayer, SL_BOOLEAN_FALSE);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to realize the OpenSL ES audio player object, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->audioPlayer)->GetInterface(dev->audioPlayer,
             SL_IID_PLAY, &(dev->audioPlayerInterface));

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to retrieve the OpenSL ES audio player interface, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->audioPlayer)->GetInterface(dev->audioPlayer,
             SL_IID_ANDROIDSIMPLEBUFFERQUEUE, &(dev->playerBufferQueueInterface));

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to retrieve the OpenSL ES buffer queue interface, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    if(dev->isSampleFormatFloat)
    {
        dev->slesBufferFloat = ARRAY(float, dev->periodFrames * NUM_CHANNELS);
    }
    else
    {
        dev->slesBufferShort = ARRAY(short, dev->periodFrames * NUM_CHANNELS);
    }

    if(dev->slesBufferFloat == NULL && dev->slesBufferShort == NULL)
    {
        LOG(ERR, "Out of memory.");
        goto errorRecovery;
    }

    result = (*dev->playerBufferQueueInterface)->RegisterCallback(dev->playerBufferQueueInterface, openslesCallback, dev);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to register the openslesCallback, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    if(dev->isSampleFormatFloat)
    {
        result = (*dev->playerBufferQueueInterface)->Enqueue(dev->playerBufferQueueInterface, dev->slesBufferFloat, dev->periodFrames * NUM_CHANNELS * sizeof(float));
    }
    else
    {
        result = (*dev->playerBufferQueueInterface)->Enqueue(dev->playerBufferQueueInterface, dev->slesBufferShort, dev->periodFrames * NUM_CHANNELS * sizeof(short));
    }

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to add a buffer to the queue, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->audioPlayerInterface)->SetCallbackEventsMask(dev->audioPlayerInterface, SL_PLAYEVENT_HEADATEND);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to set OpenSL ES audio player callback events, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    result = (*dev->audioPlayerInterface)->SetPlayState(dev->audioPlayerInterface, SL_PLAYSTATE_PLAYING);

    if(result != SL_RESULT_SUCCESS)
    {
        LOG(ERR, "Failed to set OpenSL ES audio player play state to playing, error code 0x%lx", (unsigned long)result);
        goto errorRecovery;
    }

    LOG(INFO, "Using OpenSLES driver.");

    return (audioDriverT *) dev;

errorRecovery:

    deleteOpenslesAudioDriver((audioDriverT *) dev);
    return NULL;
}

void deleteOpenslesAudioDriver(audioDriverT *p)
{
    openslesAudioDriverT *dev = (openslesAudioDriverT *) p;

    returnIfFail(dev != NULL);

    dev->cont = 0;

    if(dev->audioPlayer)
    {
        (*dev->audioPlayer)->Destroy(dev->audioPlayer);
    }

    if(dev->outputMixObject)
    {
        (*dev->outputMixObject)->Destroy(dev->outputMixObject);
    }

    if(dev->engine)
    {
        (*dev->engine)->Destroy(dev->engine);
    }

    if(dev->slesBufferFloat)
    {
        FREE(dev->slesBufferFloat);
    }

    if(dev->slesBufferShort)
    {
        FREE(dev->slesBufferShort);
    }

    FREE(dev);
}

void openslesCallback(SLAndroidSimpleBufferQueueItf caller, void *pContext)
{
    openslesAudioDriverT *dev = (openslesAudioDriverT *) pContext;
    SLresult result;

    processBuffer(dev);

    if(dev->isSampleFormatFloat)
    {
        result = (*caller)->Enqueue(
                     dev->playerBufferQueueInterface, dev->slesBufferFloat, dev->periodFrames * sizeof(float) * NUM_CHANNELS);
    }
    else
    {
        result = (*caller)->Enqueue(
                     dev->playerBufferQueueInterface, dev->slesBufferShort, dev->periodFrames * sizeof(short) * NUM_CHANNELS);
    }

    /*
    if (result != SL_RESULT_SUCCESS)
    {
      // Do not simply break at just one single insufficient buffer. Go on.
    }
    */
}

void processBuffer(openslesAudioDriverT *dev)
{
    short *outShort = dev->slesBufferShort;
    float *outFloat = dev->slesBufferFloat;
    int periodFrames = dev->periodFrames;

    if(dev->isSampleFormatFloat)
    {
        synthWriteFloat(dev->synth, periodFrames, outFloat, 0, 2, outFloat, 1, 2);
    }
    else
    {
        synthWriteS16(dev->synth, periodFrames, outShort, 0, 2, outShort, 1, 2);
    }
}

#endif
