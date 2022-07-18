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


/* oss.c
 *
 * Drivers for the Open (?) Sound System
 */

#include "synth.h"
#include "midi.h"
#include "adriver.h"
#include "mdriver.h"
#include "settings.h"

#if OSS_SUPPORT

#if defined(HAVE_SYS_SOUNDCARD_H)
#include <sys/soundcard.h>
#elif defined(HAVE_LINUX_SOUNDCARD_H)
#include <linux/soundcard.h>
#else
#include <machine/soundcard.h>
#endif

#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/poll.h>

#define BUFFER_LENGTH 512

// Build issue on some systems (OSS 4.0)?
#if !defined(SOUND_PCM_WRITE_CHANNELS) && defined(SNDCTL_DSP_CHANNELS)
#define SOUND_PCM_WRITE_CHANNELS        SNDCTL_DSP_CHANNELS
#endif

/** ossAudioDriverT
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct
{
    audioDriverT driver;
    Synthesizer *synth;
    audioCallbackT read;
    void *buffer;
    threadT *thread;
    int cont;
    int dspfd;
    int bufferSize;
    int bufferByteSize;
    int bigendian;
    int formats;
    int format;
    int caps;
    audioFuncT callback;
    void *data;
    float *buffers[2];
} ossAudioDriverT;


/* local utilities */
static int ossSetQueueSize(ossAudioDriverT *dev, int ss, int ch, int qs, int bs);
static threadReturnT ossAudioRun(void *d);
static threadReturnT ossAudioRun2(void *d);


typedef struct
{
    midiDriverT driver;
    int fd;
    threadT *thread;
    int status;
    unsigned char buffer[BUFFER_LENGTH];
    midiParserT *parser;
} ossMidiDriverT;

static threadReturnT ossMidiRun(void *d);


void
ossAudioDriverSettings(Settings *settings)
{
    settingsRegisterStr(settings, "audio.oss.device", "/dev/dsp", 0);
}

/*
 * newOssAudioDriver
 */
audioDriverT *
newOssAudioDriver(Settings *settings, Synthesizer *synth)
{
    ossAudioDriverT *dev = NULL;
    int channels, sr, sampleSize = 0, ossFormat;
    struct stat devstat;
    int queuesize;
    double sampleRate;
    int periods, periodSize;
    int realtimePrio = 0;
    char *devname = NULL;
    int format;

    dev = NEW(ossAudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(ossAudioDriverT));

    settingsGetint(settings, "audio.periods", &periods);
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.realtime-prio", &realtimePrio);

    dev->dspfd = -1;
    dev->synth = synth;
    dev->callback = NULL;
    dev->data = NULL;
    dev->cont = 1;
    dev->bufferSize = (int) periodSize;
    queuesize = (int)(periods * periodSize);

    if(settingsStrEqual(settings, "audio.sample-format", "16bits"))
    {
        sampleSize = 16;
        ossFormat = AFMT_S16_LE;
        dev->read = synthWriteS16;
        dev->bufferByteSize = dev->bufferSize * 4;

    }
    else if(settingsStrEqual(settings, "audio.sample-format", "float"))
    {
        sampleSize = 32;
        ossFormat = -1;
        dev->read = synthWriteFloat;
        dev->bufferByteSize = dev->bufferSize * 8;

    }
    else
    {
        LOG(ERR, "Unknown sample format");
        goto errorRecovery;
    }

    dev->buffer = MALLOC(dev->bufferByteSize);

    if(dev->buffer == NULL)
    {
        LOG(ERR, "Out of memory");
        goto errorRecovery;
    }

    if(settingsDupstr(settings, "audio.oss.device", &devname) != OK || !devname)            /* ++ alloc device name */
    {
        devname = STRDUP("/dev/dsp");

        if(devname == NULL)
        {
            LOG(ERR, "Out of memory");
            goto errorRecovery;
        }
    }

    dev->dspfd = open(devname, O_WRONLY, 0);

    if(dev->dspfd == -1)
    {
        LOG(ERR, "Device <%s> could not be opened for writing: %s",
                  devname, gStrerror(errno));
        goto errorRecovery;
    }

    if(fstat(dev->dspfd, &devstat) == -1)
    {
        LOG(ERR, "fstat failed on device <%s>: %s", devname, gStrerror(errno));
        goto errorRecovery;
    }

    if((devstat.stMode & S_IFCHR) != S_IFCHR)
    {
        LOG(ERR, "Device <%s> is not a device file", devname);
        goto errorRecovery;
    }

    if(ossSetQueueSize(dev, sampleSize, 2, queuesize, periodSize) < 0)
    {
        LOG(ERR, "Can't set device buffer size");
        goto errorRecovery;
    }

    format = ossFormat;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SETFMT, &ossFormat) < 0)
    {
        LOG(ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    if(ossFormat != format)
    {
        LOG(ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    channels = 2;

    if(ioctl(dev->dspfd, SOUND_PCM_WRITE_CHANNELS, &channels) < 0)
    {
        LOG(ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    if(channels != 2)
    {
        LOG(ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    sr = sampleRate;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SPEED, &sr) < 0)
    {
        LOG(ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    if((sr < 0.95 * sampleRate) ||
            (sr > 1.05 * sampleRate))
    {
        LOG(ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    /* Create the audio thread */
    dev->thread = newThread("oss-audio", ossAudioRun, dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    if(devname)
    {
        FREE(devname);    /* -- free device name */
    }

    return (audioDriverT *) dev;

errorRecovery:

    if(devname)
    {
        FREE(devname);    /* -- free device name */
    }

    deleteOssAudioDriver((audioDriverT *) dev);
    return NULL;
}

audioDriverT *
newOssAudioDriver2(Settings *settings, audioFuncT func, void *data)
{
    ossAudioDriverT *dev = NULL;
    int channels, sr;
    struct stat devstat;
    int queuesize;
    double sampleRate;
    int periods, periodSize;
    char *devname = NULL;
    int realtimePrio = 0;
    int format;

    dev = NEW(ossAudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(ossAudioDriverT));

    settingsGetint(settings, "audio.periods", &periods);
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.realtime-prio", &realtimePrio);

    dev->dspfd = -1;
    dev->synth = NULL;
    dev->read = NULL;
    dev->callback = func;
    dev->data = data;
    dev->cont = 1;
    dev->bufferSize = (int) periodSize;
    queuesize = (int)(periods * periodSize);
    dev->bufferByteSize = dev->bufferSize * 2 * 2; /* 2 channels * 16 bits audio */


    if(settingsDupstr(settings, "audio.oss.device", &devname) != OK || !devname)
    {
        devname = STRDUP("/dev/dsp");

        if(!devname)
        {
            LOG(ERR, "Out of memory");
            goto errorRecovery;
        }
    }

    dev->dspfd = open(devname, O_WRONLY, 0);

    if(dev->dspfd == -1)
    {
        LOG(ERR, "Device <%s> could not be opened for writing: %s",
                  devname, gStrerror(errno));
        goto errorRecovery;
    }

    if(fstat(dev->dspfd, &devstat) == -1)
    {
        LOG(ERR, "fstat failed on device <%s>: %s", devname, gStrerror(errno));
        goto errorRecovery;
    }

    if((devstat.stMode & S_IFCHR) != S_IFCHR)
    {
        LOG(ERR, "Device <%s> is not a device file", devname);
        goto errorRecovery;
    }

    if(ossSetQueueSize(dev, 16, 2, queuesize, periodSize) < 0)
    {
        LOG(ERR, "Can't set device buffer size");
        goto errorRecovery;
    }

    format = AFMT_S16_LE;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SETFMT, &format) < 0)
    {
        LOG(ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    if(format != AFMT_S16_LE)
    {
        LOG(ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    channels = 2;

    if(ioctl(dev->dspfd, SOUND_PCM_WRITE_CHANNELS, &channels) < 0)
    {
        LOG(ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    if(channels != 2)
    {
        LOG(ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    sr = sampleRate;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SPEED, &sr) < 0)
    {
        LOG(ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    if((sr < 0.95 * sampleRate) ||
            (sr > 1.05 * sampleRate))
    {
        LOG(ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    /* allocate the buffers. */
    dev->buffer = MALLOC(dev->bufferByteSize);
    dev->buffers[0] = ARRAY(float, dev->bufferSize);
    dev->buffers[1] = ARRAY(float, dev->bufferSize);

    if((dev->buffer == NULL) || (dev->buffers[0] == NULL) || (dev->buffers[1] == NULL))
    {
        LOG(ERR, "Out of memory");
        goto errorRecovery;
    }

    /* Create the audio thread */
    dev->thread = newThread("oss-audio", ossAudioRun2, dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    if(devname)
    {
        FREE(devname);    /* -- free device name */
    }

    return (audioDriverT *) dev;

errorRecovery:

    if(devname)
    {
        FREE(devname);    /* -- free device name */
    }

    deleteOssAudioDriver((audioDriverT *) dev);
    return NULL;
}

/*
 * deleteOssAudioDriver
 */
void
deleteOssAudioDriver(audioDriverT *p)
{
    ossAudioDriverT *dev = (ossAudioDriverT *) p;
    returnIfFail(dev != NULL);

    dev->cont = 0;

    if(dev->thread)
    {
        threadJoin(dev->thread);
        deleteThread(dev->thread);
    }

    if(dev->dspfd >= 0)
    {
        close(dev->dspfd);
    }

    FREE(dev->buffer);
    FREE(dev->buffers[0]);
    FREE(dev->buffers[1]);
    FREE(dev);
}

/**
 *  ossSetQueueSize
 *
 *  Set the internal buffersize of the output device.
 *
 *  @param ss Sample size in bits
 *  @param ch Number of channels
 *  @param qs The queue size in frames
 *  @param bs The synthesis buffer size in frames
 */
int
ossSetQueueSize(ossAudioDriverT *dev, int ss, int ch, int qs, int bs)
{
    unsigned int fragmentSize;
    unsigned int fragSizePower;
    unsigned int fragments;
    unsigned int fragmentsPower;

    fragmentSize = (unsigned int)(bs * ch * ss / 8);

    fragSizePower = 0;

    while(0 < fragmentSize)
    {
        fragmentSize = (fragmentSize >> 1);
        fragSizePower++;
    }

    fragSizePower--;

    fragments = (unsigned int)(qs / bs);

    if(fragments < 2)
    {
        fragments = 2;
    }

    /* make sure fragments is a power of 2 */
    fragmentsPower = 0;

    while(0 < fragments)
    {
        fragments = (fragments >> 1);
        fragmentsPower++;
    }

    fragmentsPower--;

    fragments = (1 << fragmentsPower);
    fragments = (fragments << 16) + fragSizePower;

    return ioctl(dev->dspfd, SNDCTL_DSP_SETFRAGMENT, &fragments);
}

/*
 * ossAudioRun
 */
threadReturnT
ossAudioRun(void *d)
{
    ossAudioDriverT *dev = (ossAudioDriverT *) d;
    Synthesizer *synth = dev->synth;
    void *buffer = dev->buffer;
    int len = dev->bufferSize;

    /* it's as simple as that: */
    while(dev->cont)
    {
        dev->read(synth, len, buffer, 0, 2, buffer, 1, 2);

        if(write(dev->dspfd, buffer, dev->bufferByteSize) < 0)
        {
            LOG(ERR, "Error writing to OSS sound device: %s",
                      gStrerror(errno));
            break;
        }
    }

    LOG(DBG, "Audio thread finished");

    return THREAD_RETURN_VALUE;
}


/*
 * ossAudioRun
 */
threadReturnT
ossAudioRun2(void *d)
{
    ossAudioDriverT *dev = (ossAudioDriverT *) d;
    short *buffer = (short *) dev->buffer;
    float *left = dev->buffers[0];
    float *right = dev->buffers[1];
    int bufferSize = dev->bufferSize;
    int ditherIndex = 0;

    LOG(DBG, "Audio thread running");

    /* it's as simple as that: */
    while(dev->cont)
    {
        MEMSET(left, 0, bufferSize * sizeof(float));
        MEMSET(right, 0, bufferSize * sizeof(float));

        (*dev->callback)(dev->data, bufferSize, 0, NULL, 2, dev->buffers);

        synthDitherS16(&ditherIndex, bufferSize, left, right,
                               buffer, 0, 2, buffer, 1, 2);

        if(write(dev->dspfd, buffer, dev->bufferByteSize) < 0)
        {
            LOG(ERR, "Error writing to OSS sound device: %s",
                      gStrerror(errno));
            break;
        }
    }

    LOG(DBG, "Audio thread finished");

    return THREAD_RETURN_VALUE;
}


void ossMidiDriverSettings(Settings *settings)
{
    settingsRegisterStr(settings, "midi.oss.device", "/dev/midi", 0);
}

/*
 * newOssMidiDriver
 */
midiDriverT *
newOssMidiDriver(Settings *settings,
                          handleMidiEventFuncT handler, void *data)
{
    ossMidiDriverT *dev;
    int realtimePrio = 0;
    char *device = NULL;

    /* not much use doing anything */
    if(handler == NULL)
    {
        LOG(ERR, "Invalid argument");
        return NULL;
    }

    /* allocate the device */
    dev = NEW(ossMidiDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(ossMidiDriverT));
    dev->fd = -1;

    dev->driver.handler = handler;
    dev->driver.data = data;

    /* allocate one event to store the input data */
    dev->parser = newMidiParser();

    if(dev->parser == NULL)
    {
        LOG(ERR, "Out of memory");
        goto errorRecovery;
    }

    /* get the device name. if none is specified, use the default device. */
    settingsDupstr(settings, "midi.oss.device", &device);  /* ++ alloc device name */

    if(device == NULL)
    {
        device = STRDUP("/dev/midi");

        if(!device)
        {
            LOG(ERR, "Out of memory");
            goto errorRecovery;
        }
    }

    settingsGetint(settings, "midi.realtime-prio", &realtimePrio);

    /* open the default hardware device. only use midi in. */
    dev->fd = open(device, O_RDONLY, 0);

    if(dev->fd < 0)
    {
        perror(device);
        goto errorRecovery;
    }

    if(fcntl(dev->fd, F_SETFL, O_NONBLOCK) == -1)
    {
        LOG(ERR, "Failed to set OSS MIDI device to non-blocking: %s",
                  gStrerror(errno));
        goto errorRecovery;
    }

    dev->status = MIDI_READY;

    /* create MIDI thread */
    dev->thread = newThread("oss-midi", ossMidiRun, dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    if(device)
    {
        FREE(device);    /* ++ free device */
    }

    return (midiDriverT *) dev;

errorRecovery:

    if(device)
    {
        FREE(device);    /* ++ free device */
    }

    deleteOssMidiDriver((midiDriverT *) dev);
    return NULL;
}

/*
 * deleteOssMidiDriver
 */
void
deleteOssMidiDriver(midiDriverT *p)
{
    ossMidiDriverT *dev = (ossMidiDriverT *) p;
    returnIfFail(dev != NULL);

    /* cancel the thread and wait for it before cleaning up */
    dev->status = MIDI_DONE;

    if(dev->thread)
    {
        threadJoin(dev->thread);
        deleteThread(dev->thread);
    }

    if(dev->fd >= 0)
    {
        close(dev->fd);
    }

    deleteMidiParser(dev->parser);
    FREE(dev);
}

/*
 * ossMidiRun
 */
threadReturnT
ossMidiRun(void *d)
{
    ossMidiDriverT *dev = (ossMidiDriverT *) d;
    midiEventT *evt;
    struct pollfd fds;
    int n, i;

    /* go into a loop until someone tells us to stop */
    dev->status = MIDI_LISTENING;

    fds.fd = dev->fd;
    fds.events = POLLIN;
    fds.revents = 0;

    while(dev->status == MIDI_LISTENING)
    {

        n = poll(&fds, 1, 100);

        if(n == 0)
        {
            continue;
        }

        if(n < 0)
        {
            LOG(ERR, "Error waiting for MIDI input: %s", gStrerror(errno));
            break;
        }

        /* read new data */
        n = read(dev->fd, dev->buffer, BUFFER_LENGTH);

        if(n == -EAGAIN)
        {
            continue;
        }

        if(n < 0)
        {
            perror("read");
            LOG(ERR, "Failed to read the midi input");
            break;
        }

        /* let the parser convert the data into events */
        for(i = 0; i < n; i++)
        {
            evt = midiParserParse(dev->parser, dev->buffer[i]);

            if(evt != NULL)
            {
                /* send the event to the next link in the chain */
                (*dev->driver.handler)(dev->driver.data, evt);
            }
        }
    }

    return THREAD_RETURN_VALUE;
}

#endif /*#if OSS_SUPPORT */
