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

/* alsa.c
 *
 * Driver for the Advanced Linux Sound Architecture
 *
 */

#include "synth.h"
#include "midi.h"
#include "adriver.h"
#include "mdriver.h"

#if ALSA_SUPPORT

#define ALSA_PCM_NEW_HW_PARAMS_API
#include <alsa/asoundlib.h>
#include <sys/poll.h>

#include "lash.h"

#define FLUID_ALSA_DEFAULT_MIDI_DEVICE  "default"
#define FLUID_ALSA_DEFAULT_SEQ_DEVICE   "default"

#define BUFFER_LENGTH 512

/** alsaAudioDriverT
 *
 * This structure should not be accessed directly. Use audio port
 * functions instead.
 */
typedef struct {
  audioDriverT driver;
  sndPcmT *pcm;
  audioFuncT callback;
  void *data;
  int bufferSize;
  threadT *thread;
  int cont;
} alsaAudioDriverT;

static threadReturnT alsaAudioRunFloat(void *d);
static threadReturnT alsaAudioRunS16(void *d);

typedef struct {
  char *name;
  sndPcmFormatT format;
  sndPcmAccessT access;
  threadFuncT run;
} alsaFormatsT;

//MB get rid of float
static const alsaFormatsT alsaFormats[] = {
  {
    "s16, rw, interleaved",
    SND_PCM_FORMAT_S16,
    SND_PCM_ACCESS_RW_INTERLEAVED,
    alsaAudioRunS16
  },
  {
    "float, rw, non interleaved",
    SND_PCM_FORMAT_FLOAT,
    SND_PCM_ACCESS_RW_NONINTERLEAVED,
    alsaAudioRunFloat
  },
  { NULL, 0, 0, NULL }
};



/*
 * alsaRawmidiDriverT
 *
 */
typedef struct
{
  midiDriverT driver;
  sndRawmidiT *rawmidiIn;
  struct pollfd *pfd;
  int npfd;
  threadT *thread;
  atomicIntT shouldQuit;
  unsigned char buffer[BUFFER_LENGTH];
  midiParserT *parser;
} alsaRawmidiDriverT;


static threadReturnT alsaMidiRun(void *d);


/*
 * alsaSeqDriverT
 *
 */
typedef struct {
  midiDriverT driver;
  sndSeqT *seqHandle;
  struct pollfd *pfd;
  int npfd;
  threadT *thread;
  atomicIntT shouldQuit;
  int portCount;
  int autoconnInputs;
  sndSeqAddrT autoconnDest;
} alsaSeqDriverT;

static threadReturnT alsaSeqRun(void *d);

/* Alsa audio driver */

void alsaAudioDriverSettings(FluidSettings *settings) {
  settingsRegisterStr(settings, "audio.alsa.device", "default", 0);
}


audioDriverT * newFluidAlsaAudioDriver(FluidSettings *settings, synthT *synth) {
  return newFluidAlsaAudioDriver2(settings, NULL, synth);
}

audioDriverT * newFluidAlsaAudioDriver2(FluidSettings *settings, audioFuncT func, void *data) {
  alsaAudioDriverT *dev;
  double sampleRate;
  int periods, periodSize;
  char *device = NULL;
  int realtimePrio = 0;
  int i, err, dir = 0;
  sndPcmHwParamsT *hwparams;
  sndPcmSwParamsT *swparams = NULL;
  sndPcmUframesT uframes;
  unsigned int tmp;

  dev = FLUID_NEW(alsaAudioDriverT);

  if(dev == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  FLUID_MEMSET(dev, 0, sizeof(alsaAudioDriverT));

  settingsGetint(settings, "audio.periods", &periods);
  settingsGetint(settings, "audio.period-size", &periodSize);
  settingsGetnum(settings, "synth.sample-rate", &sampleRate);
  settingsDupstr(settings, "audio.alsa.device", &device);   /* ++ dup device name */
  settingsGetint(settings, "audio.realtime-prio", &realtimePrio);

  dev->data = data;
  dev->callback = func;
  dev->cont = 1;
  dev->bufferSize = periodSize;

  /* Open the PCM device */
  if((err = sndPcmOpen(&dev->pcm, device ? device : "default",
               SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) != 0)
  {
    if(err == -EBUSY)
    {
      FLUID_LOG(FLUID_ERR, "The \"%s\" audio device is used by another application",
            device ? device : "default");
      goto errorRecovery;
    }
    else
    {
      FLUID_LOG(FLUID_ERR, "Failed to open the \"%s\" audio device",
            device ? device : "default");
      goto errorRecovery;
    }
  }

  sndPcmHwParamsAlloca(&hwparams);
  sndPcmSwParamsAlloca(&swparams);

  /* Set hardware parameters. We continue trying access methods and
     sample formats until we have one that works. For example, if
     memory mapped access fails we try regular IO methods. (not
     finished, yet). */

  for(i = 0; alsaFormats[i].name != NULL; i++) {
    sndPcmHwParamsAny(dev->pcm, hwparams);

    if(sndPcmHwParamsSetAccess(dev->pcm, hwparams, alsaFormats[i].access) < 0) {
      FLUID_LOG(FLUID_DBG, "sndPcmHwParamsSetAccess() failed with audio format '%s'", alsaFormats[i].name);
      continue;
    }

    if(sndPcmHwParamsSetFormat(dev->pcm, hwparams, alsaFormats[i].format) < 0) {
      FLUID_LOG(FLUID_DBG, "sndPcmHwParamsSetFormat() failed with audio format '%s'", alsaFormats[i].name);
      continue;
    }

    if((err = sndPcmHwParamsSetChannels(dev->pcm, hwparams, 2)) < 0) {
      FLUID_LOG(FLUID_ERR, "Failed to set the channels: %s",
            sndStrerror(err));
      goto errorRecovery;
    }

    tmp = (unsigned int) sampleRate;

    if((err = sndPcmHwParamsSetRateNear(dev->pcm, hwparams, &tmp, NULL)) < 0) {
      FLUID_LOG(FLUID_ERR, "Failed to set the sample rate: %s",
            sndStrerror(err));
      goto errorRecovery;
    }

    if(tmp != sampleRate) {
      /* There's currently no way to change the sampling rate of the
      synthesizer after it's been created. */
      FLUID_LOG(FLUID_WARN, "Requested sample rate of %u, got %u instead, "
            "synthesizer likely out of tune!", (unsigned int) sampleRate, tmp);
    }

    uframes = periodSize;

    if(sndPcmHwParamsSetPeriodSizeNear(dev->pcm, hwparams, &uframes, &dir) < 0)
    {
      FLUID_LOG(FLUID_ERR, "Failed to set the period size");
      goto errorRecovery;
    }

    if(uframes != (unsigned long) periodSize)
    {
      FLUID_LOG(FLUID_WARN, "Requested a period size of %d, got %d instead",
            periodSize, (int) uframes);
      dev->bufferSize = (int) uframes;
      periodSize = uframes;	/* period size is used below, so set it to the real value */
    }

    tmp = periods;

    if(sndPcmHwParamsSetPeriodsNear(dev->pcm, hwparams, &tmp, &dir) < 0)
    {
      FLUID_LOG(FLUID_ERR, "Failed to set the number of periods");
      goto errorRecovery;
    }

    if(tmp != (unsigned int) periods)
    {
      FLUID_LOG(FLUID_WARN, "Requested %d periods, got %d instead",
            periods, (int) tmp);
    }

    if(sndPcmHwParams(dev->pcm, hwparams) < 0)
    {
      FLUID_LOG(FLUID_WARN, "Audio device hardware configuration failed");
      continue;
    }

    break;
  }

  if(alsaFormats[i].name == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Failed to find an audio format supported by alsa");
    goto errorRecovery;
  }

  /* Set the software params */
  sndPcmSwParamsCurrent(dev->pcm, swparams);

  if(sndPcmSwParamsSetStartThreshold(dev->pcm, swparams, periodSize) != 0)
  {
    FLUID_LOG(FLUID_ERR, "Failed to set start threshold.");
  }

  if(sndPcmSwParamsSetAvailMin(dev->pcm, swparams, periodSize) != 0)
  {
    FLUID_LOG(FLUID_ERR, "Software setup for minimum available frames failed.");
  }

  if(sndPcmSwParams(dev->pcm, swparams) != 0)
  {
    FLUID_LOG(FLUID_ERR, "Software setup failed.");
  }

  if(sndPcmNonblock(dev->pcm, 0) != 0)
  {
    FLUID_LOG(FLUID_ERR, "Failed to set the audio device to blocking mode");
    goto errorRecovery;
  }

  /* Create the audio thread */
  dev->thread = newFluidThread("alsa-audio", alsaFormats[i].run, dev, realtimePrio, FALSE);

  if(!dev->thread)
  {
    FLUID_LOG(FLUID_ERR, "Failed to start the alsa-audio thread.");
    goto errorRecovery;
  }

  if(device)
  {
    FLUID_FREE(device);  /* -- free device name */
  }

  return (audioDriverT *) dev;

errorRecovery:

  if(device)
  {
    FLUID_FREE(device);  /* -- free device name */
  }

  deleteFluidAlsaAudioDriver((audioDriverT *) dev);
  return NULL;
}

void deleteFluidAlsaAudioDriver(audioDriverT *p)
{
  alsaAudioDriverT *dev = (alsaAudioDriverT *) p;
  returnIfFail(dev != NULL);

  dev->cont = 0;

  if(dev->thread)
  {
    threadJoin(dev->thread);
    deleteFluidThread(dev->thread);
  }

  if(dev->pcm)
  {
    sndPcmClose(dev->pcm);
  }

  FLUID_FREE(dev);
}

/* handle error after an ALSA write call */
static int alsaHandleWriteError(sndPcmT *pcm, int errval)
{
  switch(errval)
  {
  case -EAGAIN:
    sndPcmWait(pcm, 1);
    break;
// on some BSD variants ESTRPIPE is defined as EPIPE.
// not sure why, maybe because this version of alsa doesn't support
// suspending pcm streams. anyway, since EPIPE seems to be more
// likely than ESTRPIPE, so ifdef it out in case.
#if ESTRPIPE == EPIPE
#warning "ESTRPIPE defined as EPIPE. This may cause trouble with ALSA playback."
#else

  case -ESTRPIPE:
    if(sndPcmResume(pcm) != 0)
    {
      FLUID_LOG(FLUID_ERR, "Failed to resume the audio device");
      return FLUID_FAILED;
    }

#endif

  /* fall through ... */
  /* ... since the stream got resumed, but still has to be prepared */
  case -EPIPE:
  case -EBADFD:
    if(sndPcmPrepare(pcm) != 0)
    {
      FLUID_LOG(FLUID_ERR, "Failed to prepare the audio device");
      return FLUID_FAILED;
    }

    break;

  default:
    FLUID_LOG(FLUID_ERR, "The audio device error: %s", sndStrerror(errval));
    return FLUID_FAILED;
  }

  return FLUID_OK;
}

static threadReturnT alsaAudioRunFloat(void *d)
{
  alsaAudioDriverT *dev = (alsaAudioDriverT *) d;
  synthT *synth = (synthT *)(dev->data);
  float *left;
  float *right;
  float *handle[2];
  int n, bufferSize, offset;

  bufferSize = dev->bufferSize;

  left = FLUID_ARRAY(float, bufferSize);
  right = FLUID_ARRAY(float, bufferSize);

  if((left == NULL) || (right == NULL))
  {
    FLUID_LOG(FLUID_ERR, "Out of memory.");
    goto errorRecovery;
  }

  if(sndPcmPrepare(dev->pcm) != 0)
  {
    FLUID_LOG(FLUID_ERR, "Failed to prepare the audio device");
    goto errorRecovery;
  }

  /* use separate loops depending on if callback supplied or not (overkill?) */
  if(dev->callback) {
    while(dev->cont) {
      FLUID_MEMSET(left, 0, bufferSize * sizeof(*left));
      FLUID_MEMSET(right, 0, bufferSize * sizeof(*right));

      handle[0] = left;
      handle[1] = right;

      (*dev->callback)(synth, bufferSize, 0, NULL, 2, handle);

      offset = 0;

      while(offset < bufferSize) {
        handle[0] = left + offset;
        handle[1] = right + offset;

        n = sndPcmWriten(dev->pcm, (void *)handle, bufferSize - offset);

        if(n < 0)	/* error occurred? */ {
          if(alsaHandleWriteError(dev->pcm, n) != FLUID_OK) {
            goto errorRecovery;
          }
        }
        else
        {
          offset += n;  /* no error occurred */
        }
      }	/* while (offset < bufferSize) */
    }	/* while (dev->cont) */
  }
  else	/* no user audio callback (faster) */
  {
    while(dev->cont) {
      synthWriteFloat(dev->data, bufferSize, left, 0, 1, right, 0, 1);

      offset = 0;

      while(offset < bufferSize) {
        handle[0] = left + offset;
        handle[1] = right + offset;

        n = sndPcmWriten(dev->pcm, (void *)handle, bufferSize - offset);

        if(n < 0)	/* error occurred? */
        {
          if(alsaHandleWriteError(dev->pcm, n) != FLUID_OK)
          {
            goto errorRecovery;
          }
        }
        else
        {
          offset += n;  /* no error occurred */
        }
      }	/* while (offset < bufferSize) */
    }	/* while (dev->cont) */
  }

errorRecovery:

  FLUID_FREE(left);
  FLUID_FREE(right);

  return FLUID_THREAD_RETURN_VALUE;
}

static threadReturnT alsaAudioRunS16(void *d) {
  alsaAudioDriverT *dev = (alsaAudioDriverT *) d;
  float *left;
  float *right;
  short *buf;
  float *handle[2];
  int n, bufferSize, offset;

  bufferSize = dev->bufferSize;

  left = FLUID_ARRAY(float, bufferSize);
  right = FLUID_ARRAY(float, bufferSize);
  buf = FLUID_ARRAY(short, 2 * bufferSize);

  if((left == NULL) || (right == NULL) || (buf == NULL))
  {
    FLUID_LOG(FLUID_ERR, "Out of memory.");
    goto errorRecovery;
  }

  handle[0] = left;
  handle[1] = right;

  if(sndPcmPrepare(dev->pcm) != 0)
  {
    FLUID_LOG(FLUID_ERR, "Failed to prepare the audio device");
    goto errorRecovery;
  }

  /* use separate loops depending on if callback supplied or not */
  if(dev->callback)
  {
    int ditherIndex = 0;

    while(dev->cont)
    {
      FLUID_MEMSET(left, 0, bufferSize * sizeof(*left));
      FLUID_MEMSET(right, 0, bufferSize * sizeof(*right));
      
      (*dev->callback)(dev->data, bufferSize, 0, NULL, 2, handle);

      /* convert floating point data to 16 bit (with dithering) */
      synthDitherS16(&ditherIndex, bufferSize, left, right, buf, 0, 2, buf, 1, 2);
      offset = 0;

      while(offset < bufferSize) {
        n = sndPcmWritei(dev->pcm, (void *)(buf + 2 * offset), bufferSize - offset);

        if(n < 0)	{
          if(alsaHandleWriteError(dev->pcm, n) != FLUID_OK)
            goto errorRecovery;
        }
        else
          offset += n;  /* no error occurred */
      }	/* while (offset < bufferSize) */
    }	/* while (dev->cont) */
  }
  else {
    synthT *synth = (synthT *)(dev->data);

    while(dev->cont) {
      synthWriteS16(synth, bufferSize, buf, 0, 2, buf, 1, 2);
      offset = 0;
      while(offset < bufferSize) {
        n = sndPcmWritei(dev->pcm, (void *)(buf + 2 * offset),
                   bufferSize - offset);
        offset += n;  /* no error occurred */
        if(n < 0)	
          if(alsaHandleWriteError(dev->pcm, n) != FLUID_OK)
            goto errorRecovery;
      }	
    }	/* while (dev->cont) */
  }

errorRecovery:

  FLUID_FREE(left);
  FLUID_FREE(right);
  FLUID_FREE(buf);

  return FLUID_THREAD_RETURN_VALUE;
}


/**************************************************************
 *
 *    Alsa MIDI driver
 *
 */


void alsaRawmidiDriverSettings(FluidSettings *settings) {
  settingsRegisterStr(settings, "midi.alsa.device", "default", 0);
}

/*
 * newFluidAlsaRawmidiDriver
 */
midiDriverT *
newFluidAlsaRawmidiDriver(FluidSettings *settings,
                handleMidiEventFuncT handler,
                void *data)
{
  int i, err;
  alsaRawmidiDriverT *dev;
  int realtimePrio = 0;
  int count;
  struct pollfd *pfd = NULL;
  char *device = NULL;

  /* not much use doing anything */
  if(handler == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Invalid argument");
    return NULL;
  }

  /* allocate the device */
  dev = FLUID_NEW(alsaRawmidiDriverT);

  if(dev == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  FLUID_MEMSET(dev, 0, sizeof(alsaRawmidiDriverT));

  dev->driver.handler = handler;
  dev->driver.data = data;

  /* allocate one event to store the input data */
  dev->parser = newFluidMidiParser();

  if(dev->parser == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    goto errorRecovery;
  }

  settingsGetint(settings, "midi.realtime-prio", &realtimePrio);

  /* get the device name. if none is specified, use the default device. */
  settingsDupstr(settings, "midi.alsa.device", &device);     /* ++ alloc device name */

  /* open the hardware device. only use midi in. */
  if((err = sndRawmidiOpen(&dev->rawmidiIn, NULL, device ? device : "default",
                 SND_RAWMIDI_NONBLOCK)) < 0)
  {
    FLUID_LOG(FLUID_ERR, "Error opening ALSA raw MIDI port: %s", sndStrerror(err));
    goto errorRecovery;
  }

  sndRawmidiNonblock(dev->rawmidiIn, 1);

  /* get # of MIDI file descriptors */
  count = sndRawmidiPollDescriptorsCount(dev->rawmidiIn);

  if(count > 0)  		/* make sure there are some */
  {
    pfd = FLUID_MALLOC(sizeof(struct pollfd) * count);
    dev->pfd = FLUID_MALLOC(sizeof(struct pollfd) * count);
    /* grab file descriptor POLL info structures */
    count = sndRawmidiPollDescriptors(dev->rawmidiIn, pfd, count);
  }

  /* copy the input FDs */
  for(i = 0; i < count; i++)  		/* loop over file descriptors */
  {
    if(pfd[i].events & POLLIN)  /* use only the input FDs */
    {
      dev->pfd[dev->npfd].fd = pfd[i].fd;
      dev->pfd[dev->npfd].events = POLLIN;
      dev->pfd[dev->npfd].revents = 0;
      dev->npfd++;
    }
  }

  FLUID_FREE(pfd);

  atomicIntSet(&dev->shouldQuit, 0);

  /* create the MIDI thread */
  dev->thread = newFluidThread("alsa-midi-raw", alsaMidiRun, dev, realtimePrio, FALSE);

  if(!dev->thread)
  {
    goto errorRecovery;
  }

  if(device)
  {
    FLUID_FREE(device);  /* -- free device name */
  }

  return (midiDriverT *) dev;

errorRecovery:

  if(device)
  {
    FLUID_FREE(device);  /* -- free device name */
  }

  deleteFluidAlsaRawmidiDriver((midiDriverT *) dev);
  return NULL;

}

/*
 * deleteFluidAlsaRawmidiDriver
 */
void
deleteFluidAlsaRawmidiDriver(midiDriverT *p)
{
  alsaRawmidiDriverT *dev = (alsaRawmidiDriverT *) p;
  returnIfFail(dev != NULL);

  /* cancel the thread and wait for it before cleaning up */
  atomicIntSet(&dev->shouldQuit, 1);

  if(dev->thread)
  {
    threadJoin(dev->thread);
    deleteFluidThread(dev->thread);
  }

  if(dev->rawmidiIn)
  {
    sndRawmidiClose(dev->rawmidiIn);
  }

  if(dev->parser != NULL)
  {
    deleteFluidMidiParser(dev->parser);
  }

  FLUID_FREE(dev);
}

/*
 * alsaMidiRun
 */
threadReturnT
alsaMidiRun(void *d)
{
  midiEventT *evt;
  alsaRawmidiDriverT *dev = (alsaRawmidiDriverT *) d;
  int n, i;

  /* go into a loop until someone tells us to stop */
  while(!atomicIntGet(&dev->shouldQuit))
  {

    /* is there something to read? */
    n = poll(dev->pfd, dev->npfd, 100); /* use a 100 milliseconds timeout */

    if(n < 0)
    {
      perror("poll");
    }
    else if(n > 0)
    {

      /* read new data */
      n = sndRawmidiRead(dev->rawmidiIn, dev->buffer, BUFFER_LENGTH);

      if((n < 0) && (n != -EAGAIN))
      {
        FLUID_LOG(FLUID_ERR, "Failed to read the midi input");
        atomicIntSet(&dev->shouldQuit, 1);
      }

      /* let the parser convert the data into events */
      for(i = 0; i < n; i++)
      {
        evt = midiParserParse(dev->parser, dev->buffer[i]);

        if(evt != NULL)
        {
          (*dev->driver.handler)(dev->driver.data, evt);
        }
      }
    }
  }

  return FLUID_THREAD_RETURN_VALUE;
}

/**************************************************************
 *
 *    Alsa sequencer
 *
 */


void alsaSeqDriverSettings(FluidSettings *settings)
{
  settingsRegisterStr(settings, "midi.alsaSeq.device", "default", 0);
  settingsRegisterStr(settings, "midi.alsaSeq.id", "pid", 0);
}


static char *alsaSeqFullId(char *id, char *buf, int len)
{
  if(id != NULL)
  {
    if(FLUID_STRCMP(id, "pid") == 0)
    {
      FLUID_SNPRINTF(buf, len, "FLUID Synth (%d)", getpid());
    }
    else
    {
      FLUID_SNPRINTF(buf, len, "FLUID Synth (%s)", id);
    }
  }
  else
  {
    FLUID_SNPRINTF(buf, len, "FLUID Synth");
  }

  return buf;
}

static char *alsaSeqFullName(char *id, int port, char *buf, int len)
{
  if(id != NULL)
  {
    if(FLUID_STRCMP(id, "pid") == 0)
    {
      FLUID_SNPRINTF(buf, len, "Synth input port (%d:%d)", getpid(), port);
    }
    else
    {
      FLUID_SNPRINTF(buf, len, "Synth input port (%s:%d)", id, port);
    }
  }
  else
  {
    FLUID_SNPRINTF(buf, len, "Synth input port");
  }

  return buf;
}

// Connect a single portInfo to autoconnectDest if it has right type/capabilities
static void alsaSeqAutoconnectPortInfo(alsaSeqDriverT *dev, sndSeqPortInfoT *pinfo)
{
  sndSeqPortSubscribeT *subs;
  sndSeqT *seq = dev->seqHandle;
  const unsigned int neededType = SND_SEQ_PORT_TYPE_MIDI_GENERIC;
  const unsigned int neededCap = SND_SEQ_PORT_CAP_READ | SND_SEQ_PORT_CAP_SUBS_READ;
  const sndSeqAddrT *sender = sndSeqPortInfoGetAddr(pinfo);
  const char *pname = sndSeqPortInfoGetName(pinfo);

  if((sndSeqPortInfoGetType(pinfo) & neededType) != neededType)
  {
    return;
  }

  if((sndSeqPortInfoGetCapability(pinfo) & neededCap) != neededCap)
  {
    return;
  }

  sndSeqPortSubscribeAlloca(&subs);
  sndSeqPortSubscribeSetSender(subs, sender);
  sndSeqPortSubscribeSetDest(subs, &dev->autoconnDest);

  if(sndSeqGetPortSubscription(seq, subs) == 0)
  {
    FLUID_LOG(FLUID_WARN, "Connection %s is already subscribed", pname);
    return;
  }

  if(sndSeqSubscribePort(seq, subs) < 0)
  {
    FLUID_LOG(FLUID_ERR, "Connection of %s failed (%s)", pname, sndStrerror(errno));
    return;
  }

  FLUID_LOG(FLUID_INFO, "Connection of %s succeeded", pname);
}

// Autoconnect a single client port (by id) to autoconnectDest if it has right type/capabilities
static void alsaSeqAutoconnectPort(alsaSeqDriverT *dev, int clientId, int portId)
{
  int err;
  sndSeqT *seq = dev->seqHandle;
  sndSeqPortInfoT *pinfo;

  sndSeqPortInfoAlloca(&pinfo);

  if((err = sndSeqGetAnyPortInfo(seq, clientId, portId, pinfo)) < 0)
  {
    FLUID_LOG(FLUID_ERR, "sndSeqGetAnyPortInfo() failed: %s", sndStrerror(err));
    return;
  }

  alsaSeqAutoconnectPortInfo(dev, pinfo);
}

// Connect available ALSA MIDI inputs to the provided portInfo
static void alsaSeqAutoconnect(alsaSeqDriverT *dev, const sndSeqPortInfoT *destPinfo)
{
  int err;
  sndSeqT *seq = dev->seqHandle;
  sndSeqClientInfoT *cinfo;
  sndSeqPortInfoT *pinfo;

  // subscribe to future new clients/ports showing up
  if((err = sndSeqConnectFrom(seq, sndSeqPortInfoGetPort(destPinfo),
                   SND_SEQ_CLIENT_SYSTEM, SND_SEQ_PORT_SYSTEM_ANNOUNCE)) < 0)
  {
    FLUID_LOG(FLUID_ERR, "sndSeqConnectFrom() failed: %s", sndStrerror(err));
  }

  sndSeqClientInfoAlloca(&cinfo);
  sndSeqPortInfoAlloca(&pinfo);

  dev->autoconnDest = *sndSeqPortInfoGetAddr(destPinfo);

  sndSeqClientInfoSetClient(cinfo, -1);

  while(sndSeqQueryNextClient(seq, cinfo) >= 0)
  {
    sndSeqPortInfoSetClient(pinfo, sndSeqClientInfoGetClient(cinfo));
    sndSeqPortInfoSetPort(pinfo, -1);

    while(sndSeqQueryNextPort(seq, pinfo) >= 0)
    {
      alsaSeqAutoconnectPortInfo(dev, pinfo);
    }
  }
}

/*
 * newFluidAlsaSeqDriver
 */
midiDriverT *
newFluidAlsaSeqDriver(FluidSettings *settings,
              handleMidiEventFuncT handler, void *data)
{
  int i, err;
  alsaSeqDriverT *dev;
  int realtimePrio = 0;
  int count;
  struct pollfd *pfd = NULL;
  char *device = NULL;
  char *id = NULL;
  char *portname = NULL;
  char fullId[64];
  char fullName[64];
  sndSeqPortInfoT *portInfo = NULL;
  int midiChannels;

  /* not much use doing anything */
  if(handler == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Invalid argument");
    return NULL;
  }

  /* allocate the device */
  dev = FLUID_NEW(alsaSeqDriverT);

  if(dev == NULL)
  {
    FLUID_LOG(FLUID_ERR, "Out of memory");
    return NULL;
  }

  FLUID_MEMSET(dev, 0, sizeof(alsaSeqDriverT));
  dev->driver.data = data;
  dev->driver.handler = handler;

  settingsGetint(settings, "midi.realtime-prio", &realtimePrio);

  /* get the device name. if none is specified, use the default device. */
  if(settingsDupstr(settings, "midi.alsaSeq.device", &device) != FLUID_OK)   /* ++ alloc device name */
  {
    goto errorRecovery;
  }

  if(settingsDupstr(settings, "midi.alsaSeq.id", &id) != FLUID_OK)   /* ++ alloc id string */
  {
    goto errorRecovery;
  }

  if(id == NULL)
  {
    id = FLUID_MALLOC(32);

    if(!id)
    {
      FLUID_LOG(FLUID_ERR, "Out of memory");
      goto errorRecovery;
    }

    sprintf(id, "%d", getpid());
  }

  /* get the midi portname */
  settingsDupstr(settings, "midi.portname", &portname);

  if(portname && FLUID_STRLEN(portname) == 0)
  {
    FLUID_FREE(portname);     /* -- free port name */
    portname = NULL;
  }

  /* open the sequencer INPUT only */
  err = sndSeqOpen(&dev->seqHandle, device ? device : "default", SND_SEQ_OPEN_INPUT, 0);

  if(err < 0)
  {
    FLUID_LOG(FLUID_ERR, "Error opening ALSA sequencer");
    goto errorRecovery;
  }

  sndSeqNonblock(dev->seqHandle, 1);

  /* get # of MIDI file descriptors */
  count = sndSeqPollDescriptorsCount(dev->seqHandle, POLLIN);

  if(count > 0)  		/* make sure there are some */
  {
    pfd = FLUID_MALLOC(sizeof(struct pollfd) * count);
    dev->pfd = FLUID_MALLOC(sizeof(struct pollfd) * count);
    /* grab file descriptor POLL info structures */
    count = sndSeqPollDescriptors(dev->seqHandle, pfd, count, POLLIN);
  }

  /* copy the input FDs */
  for(i = 0; i < count; i++)  		/* loop over file descriptors */
  {
    if(pfd[i].events & POLLIN)  /* use only the input FDs */
    {
      dev->pfd[dev->npfd].fd = pfd[i].fd;
      dev->pfd[dev->npfd].events = POLLIN;
      dev->pfd[dev->npfd].revents = 0;
      dev->npfd++;
    }
  }

  FLUID_FREE(pfd);

  /* set the client name */
  if(!portname)
  {
    sndSeqSetClientName(dev->seqHandle, alsaSeqFullId(id, fullId, 64));
  }
  else
  {
    sndSeqSetClientName(dev->seqHandle, portname);
  }


  /* create the ports */
  sndSeqPortInfoAlloca(&portInfo);
  FLUID_MEMSET(portInfo, 0, sndSeqPortInfoSizeof());

  settingsGetint(settings, "synth.midi-channels", &midiChannels);
  dev->portCount = midiChannels / 16;

  sndSeqPortInfoSetCapability(portInfo,
                   SND_SEQ_PORT_CAP_WRITE |
                   SND_SEQ_PORT_CAP_SUBS_WRITE);
  sndSeqPortInfoSetType(portInfo,
                 SND_SEQ_PORT_TYPE_MIDI_GM    |
                 SND_SEQ_PORT_TYPE_SYNTHESIZER  |
                 SND_SEQ_PORT_TYPE_APPLICATION  |
                 SND_SEQ_PORT_TYPE_MIDI_GENERIC);
  sndSeqPortInfoSetMidiChannels(portInfo, 16);
  sndSeqPortInfoSetPortSpecified(portInfo, 1);

  for(i = 0; i < dev->portCount; i++)
  {

    if(!portname)
    {
      sndSeqPortInfoSetName(portInfo, alsaSeqFullName(id, i, fullName, 64));
    }
    else
    {
      sndSeqPortInfoSetName(portInfo, portname);
    }

    sndSeqPortInfoSetPort(portInfo, i);

    err = sndSeqCreatePort(dev->seqHandle, portInfo);

    if(err  < 0)
    {
      FLUID_LOG(FLUID_ERR, "Error creating ALSA sequencer port");
      goto errorRecovery;
    }
  }

  settingsGetint(settings, "midi.autoconnect", &dev->autoconnInputs);

  if(dev->autoconnInputs)
  {
    alsaSeqAutoconnect(dev, portInfo);
  }

  /* tell the lash server our client id */
#ifdef HAVE_LASH
  {
    int enableLash = 0;
    settingsGetint(settings, "lash.enable", &enableLash);

    if(enableLash)
    {
      lashAlsaClientId(lashClient, sndSeqClientId(dev->seqHandle));
    }
  }
#endif /* HAVE_LASH */

  atomicIntSet(&dev->shouldQuit, 0);

  /* create the MIDI thread */
  dev->thread = newFluidThread("alsa-midi-seq", alsaSeqRun, dev, realtimePrio, FALSE);

  if(portname)
  {
    FLUID_FREE(portname);
  }

  if(id)
  {
    FLUID_FREE(id);
  }

  if(device)
  {
    FLUID_FREE(device);
  }

  return (midiDriverT *) dev;

errorRecovery:

  if(portname)
  {
    FLUID_FREE(portname);
  }

  if(id)
  {
    FLUID_FREE(id);
  }

  if(device)
  {
    FLUID_FREE(device);
  }

  deleteFluidAlsaSeqDriver((midiDriverT *) dev);

  return NULL;
}

/*
 * deleteFluidAlsaSeqDriver
 */
void
deleteFluidAlsaSeqDriver(midiDriverT *p)
{
  alsaSeqDriverT *dev = (alsaSeqDriverT *) p;
  returnIfFail(dev != NULL);

  /* cancel the thread and wait for it before cleaning up */
  atomicIntSet(&dev->shouldQuit, 1);

  if(dev->thread)
  {
    threadJoin(dev->thread);
    deleteFluidThread(dev->thread);
  }

  if(dev->seqHandle)
  {
    sndSeqClose(dev->seqHandle);
  }

  if(dev->pfd)
  {
    FLUID_FREE(dev->pfd);
  }

  FLUID_FREE(dev);
}

/*
 * alsaSeqRun
 */
threadReturnT
alsaSeqRun(void *d)
{
  int n, ev;
  sndSeqEventT *seqEv;
  midiEventT evt;
  alsaSeqDriverT *dev = (alsaSeqDriverT *) d;

  /* go into a loop until someone tells us to stop */
  while(!atomicIntGet(&dev->shouldQuit))
  {

    /* is there something to read? */
    n = poll(dev->pfd, dev->npfd, 100); /* use a 100 milliseconds timeout */

    if(n < 0)
    {
      perror("poll");
    }
    else if(n > 0)       /* check for pending events */
    {
      do
      {
        ev = sndSeqEventInput(dev->seqHandle, &seqEv);	/* read the events */

        if(ev == -EAGAIN)
        {
          break;
        }

        /* Negative value indicates an error, ignore interrupted system call
         * (-EPERM) and input event buffer overrun (-ENOSPC) */
        if(ev < 0)
        {
          /* FIXME - report buffer overrun? */
          if(ev != -EPERM && ev != -ENOSPC)
          {
            FLUID_LOG(FLUID_ERR, "Error while reading ALSA sequencer (code=%d)", ev);
            atomicIntSet(&dev->shouldQuit, 1);
          }

          break;
        }

        switch(seqEv->type)
        {
        case SND_SEQ_EVENT_NOTEON:
          evt.type = NOTE_ON;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.note.channel;
          evt.param1 = seqEv->data.note.note;
          evt.param2 = seqEv->data.note.velocity;
          break;

        case SND_SEQ_EVENT_NOTEOFF:
          evt.type = NOTE_OFF;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.note.channel;
          evt.param1 = seqEv->data.note.note;
          evt.param2 = seqEv->data.note.velocity;
          break;

        case SND_SEQ_EVENT_KEYPRESS:
          evt.type = KEY_PRESSURE;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.note.channel;
          evt.param1 = seqEv->data.note.note;
          evt.param2 = seqEv->data.note.velocity;
          break;

        case SND_SEQ_EVENT_CONTROLLER:
          evt.type = CONTROL_CHANGE;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.control.channel;
          evt.param1 = seqEv->data.control.param;
          evt.param2 = seqEv->data.control.value;
          break;

        case SND_SEQ_EVENT_PITCHBEND:
          evt.type = PITCH_BEND;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.control.channel;

          /* ALSA pitch bend is -8192 - 8191, we adjust it here */
          evt.param1 = seqEv->data.control.value + 8192;
          break;

        case SND_SEQ_EVENT_PGMCHANGE:
          evt.type = PROGRAM_CHANGE;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.control.channel;
          evt.param1 = seqEv->data.control.value;
          break;

        case SND_SEQ_EVENT_CHANPRESS:
          evt.type = CHANNEL_PRESSURE;
          evt.channel = seqEv->dest.port * 16 + seqEv->data.control.channel;
          evt.param1 = seqEv->data.control.value;
          break;

        case SND_SEQ_EVENT_SYSEX:
          if(seqEv->data.ext.len < 2)
          {
            continue;
          }

          midiEventSetSysex(&evt, (char *)(seqEv->data.ext.ptr) + 1,
                         seqEv->data.ext.len - 2, FALSE);
          break;

        case SND_SEQ_EVENT_PORT_START:
        {
          if(dev->autoconnInputs)
          {
            alsaSeqAutoconnectPort(dev, seqEv->data.addr.client, seqEv->data.addr.port);
          }
        }
        break;

        default:
          continue;		/* unhandled event, next loop iteration */
        }

        /* send the events to the next link in the chain */
        (*dev->driver.handler)(dev->driver.data, &evt);
      }
      while(ev > 0);
    }	/* if poll() > 0 */
  }	/* while (!dev->shouldQuit) */

  return FLUID_THREAD_RETURN_VALUE;
}

#endif /* #if ALSA_SUPPORT */
