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
    synthT *synth;
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
ossAudioDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "audio.oss.device", "/dev/dsp", 0);
}

/*
 * newFluidOssAudioDriver
 */
audioDriverT *
newFluidOssAudioDriver(FluidSettings *settings, synthT *synth)
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

    dev = FLUID_NEW(ossAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(ossAudioDriverT));

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
        FLUID_LOG(FLUID_ERR, "Unknown sample format");
        goto errorRecovery;
    }

    dev->buffer = FLUID_MALLOC(dev->bufferByteSize);

    if(dev->buffer == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto errorRecovery;
    }

    if(settingsDupstr(settings, "audio.oss.device", &devname) != FLUID_OK || !devname)            /* ++ alloc device name */
    {
        devname = FLUID_STRDUP("/dev/dsp");

        if(devname == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            goto errorRecovery;
        }
    }

    dev->dspfd = open(devname, O_WRONLY, 0);

    if(dev->dspfd == -1)
    {
        FLUID_LOG(FLUID_ERR, "Device <%s> could not be opened for writing: %s",
                  devname, gStrerror(errno));
        goto errorRecovery;
    }

    if(fstat(dev->dspfd, &devstat) == -1)
    {
        FLUID_LOG(FLUID_ERR, "fstat failed on device <%s>: %s", devname, gStrerror(errno));
        goto errorRecovery;
    }

    if((devstat.stMode & S_IFCHR) != S_IFCHR)
    {
        FLUID_LOG(FLUID_ERR, "Device <%s> is not a device file", devname);
        goto errorRecovery;
    }

    if(ossSetQueueSize(dev, sampleSize, 2, queuesize, periodSize) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set device buffer size");
        goto errorRecovery;
    }

    format = ossFormat;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SETFMT, &ossFormat) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    if(ossFormat != format)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    channels = 2;

    if(ioctl(dev->dspfd, SOUND_PCM_WRITE_CHANNELS, &channels) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    if(channels != 2)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    sr = sampleRate;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SPEED, &sr) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    if((sr < 0.95 * sampleRate) ||
            (sr > 1.05 * sampleRate))
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    /* Create the audio thread */
    dev->thread = newFluidThread("oss-audio", ossAudioRun, dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    if(devname)
    {
        FLUID_FREE(devname);    /* -- free device name */
    }

    return (audioDriverT *) dev;

errorRecovery:

    if(devname)
    {
        FLUID_FREE(devname);    /* -- free device name */
    }

    deleteFluidOssAudioDriver((audioDriverT *) dev);
    return NULL;
}

audioDriverT *
newFluidOssAudioDriver2(FluidSettings *settings, audioFuncT func, void *data)
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

    dev = FLUID_NEW(ossAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(ossAudioDriverT));

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


    if(settingsDupstr(settings, "audio.oss.device", &devname) != FLUID_OK || !devname)
    {
        devname = FLUID_STRDUP("/dev/dsp");

        if(!devname)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            goto errorRecovery;
        }
    }

    dev->dspfd = open(devname, O_WRONLY, 0);

    if(dev->dspfd == -1)
    {
        FLUID_LOG(FLUID_ERR, "Device <%s> could not be opened for writing: %s",
                  devname, gStrerror(errno));
        goto errorRecovery;
    }

    if(fstat(dev->dspfd, &devstat) == -1)
    {
        FLUID_LOG(FLUID_ERR, "fstat failed on device <%s>: %s", devname, gStrerror(errno));
        goto errorRecovery;
    }

    if((devstat.stMode & S_IFCHR) != S_IFCHR)
    {
        FLUID_LOG(FLUID_ERR, "Device <%s> is not a device file", devname);
        goto errorRecovery;
    }

    if(ossSetQueueSize(dev, 16, 2, queuesize, periodSize) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set device buffer size");
        goto errorRecovery;
    }

    format = AFMT_S16_LE;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SETFMT, &format) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    if(format != AFMT_S16_LE)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample format");
        goto errorRecovery;
    }

    channels = 2;

    if(ioctl(dev->dspfd, SOUND_PCM_WRITE_CHANNELS, &channels) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    if(channels != 2)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the number of channels");
        goto errorRecovery;
    }

    sr = sampleRate;

    if(ioctl(dev->dspfd, SNDCTL_DSP_SPEED, &sr) < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    if((sr < 0.95 * sampleRate) ||
            (sr > 1.05 * sampleRate))
    {
        FLUID_LOG(FLUID_ERR, "Can't set the sample rate");
        goto errorRecovery;
    }

    /* allocate the buffers. */
    dev->buffer = FLUID_MALLOC(dev->bufferByteSize);
    dev->buffers[0] = FLUID_ARRAY(float, dev->bufferSize);
    dev->buffers[1] = FLUID_ARRAY(float, dev->bufferSize);

    if((dev->buffer == NULL) || (dev->buffers[0] == NULL) || (dev->buffers[1] == NULL))
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto errorRecovery;
    }

    /* Create the audio thread */
    dev->thread = newFluidThread("oss-audio", ossAudioRun2, dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    if(devname)
    {
        FLUID_FREE(devname);    /* -- free device name */
    }

    return (audioDriverT *) dev;

errorRecovery:

    if(devname)
    {
        FLUID_FREE(devname);    /* -- free device name */
    }

    deleteFluidOssAudioDriver((audioDriverT *) dev);
    return NULL;
}

/*
 * deleteFluidOssAudioDriver
 */
void
deleteFluidOssAudioDriver(audioDriverT *p)
{
    ossAudioDriverT *dev = (ossAudioDriverT *) p;
    returnIfFail(dev != NULL);

    dev->cont = 0;

    if(dev->thread)
    {
        threadJoin(dev->thread);
        deleteFluidThread(dev->thread);
    }

    if(dev->dspfd >= 0)
    {
        close(dev->dspfd);
    }

    FLUID_FREE(dev->buffer);
    FLUID_FREE(dev->buffers[0]);
    FLUID_FREE(dev->buffers[1]);
    FLUID_FREE(dev);
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
    synthT *synth = dev->synth;
    void *buffer = dev->buffer;
    int len = dev->bufferSize;

    /* it's as simple as that: */
    while(dev->cont)
    {
        dev->read(synth, len, buffer, 0, 2, buffer, 1, 2);

        if(write(dev->dspfd, buffer, dev->bufferByteSize) < 0)
        {
            FLUID_LOG(FLUID_ERR, "Error writing to OSS sound device: %s",
                      gStrerror(errno));
            break;
        }
    }

    FLUID_LOG(FLUID_DBG, "Audio thread finished");

    return FLUID_THREAD_RETURN_VALUE;
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

    FLUID_LOG(FLUID_DBG, "Audio thread running");

    /* it's as simple as that: */
    while(dev->cont)
    {
        FLUID_MEMSET(left, 0, bufferSize * sizeof(float));
        FLUID_MEMSET(right, 0, bufferSize * sizeof(float));

        (*dev->callback)(dev->data, bufferSize, 0, NULL, 2, dev->buffers);

        synthDitherS16(&ditherIndex, bufferSize, left, right,
                               buffer, 0, 2, buffer, 1, 2);

        if(write(dev->dspfd, buffer, dev->bufferByteSize) < 0)
        {
            FLUID_LOG(FLUID_ERR, "Error writing to OSS sound device: %s",
                      gStrerror(errno));
            break;
        }
    }

    FLUID_LOG(FLUID_DBG, "Audio thread finished");

    return FLUID_THREAD_RETURN_VALUE;
}


void ossMidiDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "midi.oss.device", "/dev/midi", 0);
}

/*
 * newFluidOssMidiDriver
 */
midiDriverT *
newFluidOssMidiDriver(FluidSettings *settings,
                          handleMidiEventFuncT handler, void *data)
{
    ossMidiDriverT *dev;
    int realtimePrio = 0;
    char *device = NULL;

    /* not much use doing anything */
    if(handler == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Invalid argument");
        return NULL;
    }

    /* allocate the device */
    dev = FLUID_NEW(ossMidiDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(ossMidiDriverT));
    dev->fd = -1;

    dev->driver.handler = handler;
    dev->driver.data = data;

    /* allocate one event to store the input data */
    dev->parser = newFluidMidiParser();

    if(dev->parser == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto errorRecovery;
    }

    /* get the device name. if none is specified, use the default device. */
    settingsDupstr(settings, "midi.oss.device", &device);  /* ++ alloc device name */

    if(device == NULL)
    {
        device = FLUID_STRDUP("/dev/midi");

        if(!device)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
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
        FLUID_LOG(FLUID_ERR, "Failed to set OSS MIDI device to non-blocking: %s",
                  gStrerror(errno));
        goto errorRecovery;
    }

    dev->status = FLUID_MIDI_READY;

    /* create MIDI thread */
    dev->thread = newFluidThread("oss-midi", ossMidiRun, dev, realtimePrio, FALSE);

    if(!dev->thread)
    {
        goto errorRecovery;
    }

    if(device)
    {
        FLUID_FREE(device);    /* ++ free device */
    }

    return (midiDriverT *) dev;

errorRecovery:

    if(device)
    {
        FLUID_FREE(device);    /* ++ free device */
    }

    deleteFluidOssMidiDriver((midiDriverT *) dev);
    return NULL;
}

/*
 * deleteFluidOssMidiDriver
 */
void
deleteFluidOssMidiDriver(midiDriverT *p)
{
    ossMidiDriverT *dev = (ossMidiDriverT *) p;
    returnIfFail(dev != NULL);

    /* cancel the thread and wait for it before cleaning up */
    dev->status = FLUID_MIDI_DONE;

    if(dev->thread)
    {
        threadJoin(dev->thread);
        deleteFluidThread(dev->thread);
    }

    if(dev->fd >= 0)
    {
        close(dev->fd);
    }

    deleteFluidMidiParser(dev->parser);
    FLUID_FREE(dev);
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
    dev->status = FLUID_MIDI_LISTENING;

    fds.fd = dev->fd;
    fds.events = POLLIN;
    fds.revents = 0;

    while(dev->status == FLUID_MIDI_LISTENING)
    {

        n = poll(&fds, 1, 100);

        if(n == 0)
        {
            continue;
        }

        if(n < 0)
        {
            FLUID_LOG(FLUID_ERR, "Error waiting for MIDI input: %s", gStrerror(errno));
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
            FLUID_LOG(FLUID_ERR, "Failed to read the midi input");
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

    return FLUID_THREAD_RETURN_VALUE;
}

#endif /*#if OSS_SUPPORT */
