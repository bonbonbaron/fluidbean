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

/* aufile.c
 *
 * Audio driver, outputs the audio to a file (non real-time)
 *
 */

#include "sys.h"
#include "adriver.h"
#include "settings.h"


#if AUFILE_SUPPORT

/** fileAudioDriverT
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
    audioDriverT driver;
    void *data;
    fileRendererT *renderer;
    int periodSize;
    double sampleRate;
    timerT *timer;
    unsigned int samples;
} fileAudioDriverT;


static int fileAudioRun(void *d, unsigned int msec);

/**************************************************************
 *
 *        'file' audio driver
 *
 */

audioDriverT *
newFileAudioDriver(Settings *settings,
                            Synthesizer *synth)
{
    fileAudioDriverT *dev;
    int msec;

    dev = NEW(fileAudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(fileAudioDriverT));

    settingsGetint(settings, "audio.period-size", &dev->periodSize);
    settingsGetnum(settings, "synth.sample-rate", &dev->sampleRate);

    dev->data = synth;
    dev->samples = 0;

    dev->renderer = newFileRenderer(synth);

    if(dev->renderer == NULL)
    {
        goto errorRecovery;
    }

    msec = (int)(0.5 + dev->periodSize / dev->sampleRate * 1000.0);
    dev->timer = newTimer(msec, fileAudioRun, (void *) dev, TRUE, FALSE, TRUE);

    if(dev->timer == NULL)
    {
        LOG(PANIC, "Couldn't create the audio thread.");
        goto errorRecovery;
    }

    return (audioDriverT *) dev;

errorRecovery:
    deleteFileAudioDriver((audioDriverT *) dev);
    return NULL;
}

void deleteFileAudioDriver(audioDriverT *p)
{
    fileAudioDriverT *dev = (fileAudioDriverT *) p;
    returnIfFail(dev != NULL);

    deleteTimer(dev->timer);
    deleteFileRenderer(dev->renderer);

    FREE(dev);
}

static int fileAudioRun(void *d, unsigned int clockTime)
{
    fileAudioDriverT *dev = (fileAudioDriverT *) d;
    unsigned int sampleTime;

    sampleTime = (unsigned int)(dev->samples / dev->sampleRate * 1000.0);

    if(sampleTime > clockTime)
    {
        return 1;
    }

    dev->samples += dev->periodSize;

    return fileRendererProcessBlock(dev->renderer) == OK ? 1 : 0;
}

#endif /* AUFILE_SUPPORT */
