/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 * Copyright (C) 2018  Carlo Bramini
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

#if WAVEOUT_SUPPORT

#include <mmsystem.h>
#include <mmreg.h>

/* Those two includes are required on Windows 9x/ME */
#include <ks.h>
#include <ksmedia.h>

/**
* The driver handle multiple channels.
* Actually the number maximum of channels is limited to  2 * WAVEOUT_MAX_STEREO_CHANNELS.
* The only reason of this limitation is because we dont know how to define the mapping
* of speakers for stereo output number above WAVEOUT_MAX_STEREO_CHANNELS.
*/
/* Maximum number of stereo outputs */
#define WAVEOUT_MAX_STEREO_CHANNELS 4

static char *waveoutError(MMRESULT hr);
static int waveoutWriteProcessedChannels(Synthesizer *data, int len,
                               int channelsCount,
                               void *channelsOut[], int channelsOff[],
                               int channelsIncr[]);

/* Speakers mapping */
static const DWORD channelMaskSpeakers[WAVEOUT_MAX_STEREO_CHANNELS] =
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

    void *synth;
    audioFuncT func;
    audioChannelsCallbackT writePtr;
    float **drybuf;

    HWAVEOUT hWaveOut;
    WAVEHDR *waveHeader;

    int periods; /* numbers of internal driver buffers */
    int numFrames;

    HANDLE hThread;
    DWORD  dwThread;

    int    nQuit;
    HANDLE hQuit;
    int channelsCount; /* number of channels in audio stream */

} waveoutAudioDriverT;


/* Thread for playing sample buffers */
static DWORD WINAPI waveoutSynthThread(void *data)
{
    waveoutAudioDriverT *dev;
    WAVEHDR                      *pWave;

    MSG msg;
    int code;
    /* pointers table on output first sample channels */
    void *channelsOut[WAVEOUT_MAX_STEREO_CHANNELS * 2];
    int channelsOff[WAVEOUT_MAX_STEREO_CHANNELS * 2];
    int channelsIncr[WAVEOUT_MAX_STEREO_CHANNELS * 2];
    int i;

    dev = (waveoutAudioDriverT *)data;

    /* initialize write callback constant parameters:
       MME expects interleaved channels in a unique buffer.
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

    /* Forces creation of message queue */
    PeekMessage(&msg, NULL, WM_USER, WM_USER, PM_NOREMOVE);

    for(;;)
    {
        code = GetMessage(&msg, NULL, 0, 0);

        if(code < 0)
        {
            LOG(ERR, "waveoutSynthThread: GetMessage() failed: '%s'", getWindowsError());
            break;
        }

        if(msg.message == WM_CLOSE)
        {
            break;
        }

        switch(msg.message)
        {
        case MM_WOM_DONE:
            pWave = (WAVEHDR *)msg.lParam;
            dev   = (waveoutAudioDriverT *)pWave->dwUser;

            if(dev->nQuit > 0)
            {
                /* Release the sample buffer */
                waveOutUnprepareHeader((HWAVEOUT)msg.wParam, pWave, sizeof(WAVEHDR));

                if(--dev->nQuit == 0)
                {
                    SetEvent(dev->hQuit);
                }
            }
            else
            {
                /* Before calling write function, finish to initialize
                   channelsOut[] table parameter:
                   MME expects interleaved channels in a unique buffer.
                   So, channelsOut[] table must be initialized with the address
                   of the same buffer (lpData).
                */
                i = dev->channelsCount;

                do
                {
                    channelsOut[--i] = pWave->lpData;
                }
                while(i);

                dev->writePtr(dev->func ? (Synthesizer*)dev : dev->synth, dev->numFrames, dev->channelsCount,
                               channelsOut, channelsOff, channelsIncr);

                waveOutWrite((HWAVEOUT)msg.wParam, pWave, sizeof(WAVEHDR));
            }

            break;
        }
    }

    return 0;
}

void waveoutAudioDriverSettings(Settings *settings)
{
    UINT n, nDevs = waveOutGetNumDevs();
#ifdef _UNICODE
    char devName[MAXPNAMELEN];
#endif

    settingsRegisterStr(settings, "audio.waveout.device", "default", 0);
    settingsAddOption(settings, "audio.waveout.device", "default");

    for(n = 0; n < nDevs; n++)
    {
        WAVEOUTCAPS caps;
        MMRESULT    res;

        res = waveOutGetDevCaps(n, &caps, sizeof(caps));

        if(res == MMSYSERR_NOERROR)
        {
#ifdef _UNICODE
            WideCharToMultiByte(CP_UTF8, 0, caps.szPname, -1, devName, MAXPNAMELEN, 0, 0);
            LOG(DBG, "Testing audio device: %s", devName);
            settingsAddOption(settings, "audio.waveout.device", devName);
#else
            LOG(DBG, "Testing audio device: %s", caps.szPname);
            settingsAddOption(settings, "audio.waveout.device", caps.szPname);
#endif
        }
    }
}


/*
 * newWaveoutAudioDriver
 * The driver handle the case of multiple stereo buffers provided by synth
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
 *  "synth.audio-channels", the number of internal mixer stereo buffer.
 *  "audio.periods",the number of buffers.
 *  "audio.period-size",the size of one buffer.
 *  "audio.sample-format",the sample format, 16bits or float.
 *
 * @param synth, synth synth instance to associate to the driver.
 *
 * Note: The number of internal mixer stereo buffer is indicated by "synth.audio-channels".
 * If the audio device cannot handle the format or do not have enough channels,
 * the driver fails and return NULL.
 */
audioDriverT *
newWaveoutAudioDriver(Settings *settings, Synthesizer *synth)
{
    return newWaveoutAudioDriver2(settings, NULL, synth);
}

audioDriverT *
newWaveoutAudioDriver2(Settings *settings, audioFuncT func, void *data)
{
    waveoutAudioDriverT *dev = NULL;
    audioChannelsCallbackT writePtr;
    double sampleRate;
    int frequency, sampleSize;
    int periods, periodSize;
    int audioChannels;
    LPSTR ptrBuffer;
    int lenBuffer;
    int device;
    int i;
    WAVEFORMATEXTENSIBLE wfx;
    char devName[MAXPNAMELEN];
    MMRESULT errCode;

    /* Retrieve the settings */
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.periods", &periods);
    settingsGetint(settings, "audio.period-size", &periodSize);
    settingsGetint(settings, "synth.audio-channels", &audioChannels);

    /* Clear format structure */
    ZeroMemory(&wfx, sizeof(WAVEFORMATEXTENSIBLE));

    /* check the format */
    if(settingsStrEqual(settings, "audio.sample-format", "float") || func)
    {
        LOG(DBG, "Selected 32 bit sample format");

        sampleSize = sizeof(float);
        writePtr = func ? waveoutWriteProcessedChannels : synthWriteFloatChannels;
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
        wfx.Format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    }
    else if(settingsStrEqual(settings, "audio.sample-format", "16bits"))
    {
        LOG(DBG, "Selected 16 bit sample format");

        sampleSize = sizeof(short);
        writePtr = synthWriteS16_channels;
        wfx.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
        wfx.Format.wFormatTag = WAVE_FORMAT_PCM;
    }
    else
    {
        LOG(ERR, "Unhandled sample format");
        return NULL;
    }

    /* Set frequency to integer */
    frequency = (int)sampleRate;

    /* Initialize the format structure */
    wfx.Format.nChannels  = audioChannels * 2;

    if(audioChannels > WAVEOUT_MAX_STEREO_CHANNELS)
    {
        LOG(ERR, "Channels number %d exceed internal limit %d",
                  wfx.Format.nChannels, WAVEOUT_MAX_STEREO_CHANNELS * 2);
        return NULL;
    }

    wfx.Format.wBitsPerSample  = sampleSize * 8;
    wfx.Format.nBlockAlign     = sampleSize * wfx.Format.nChannels;
    wfx.Format.nSamplesPerSec  = frequency;
    wfx.Format.nAvgBytesPerSec = frequency * wfx.Format.nBlockAlign;
    /* WAVEFORMATEXTENSIBLE extension is used only when channels number
       is above 2.
       When channels number is below 2, only WAVEFORMATEX structure
       will be used by the Windows driver. This ensures compatibility with
       Windows 9X/NT in the case these versions does not accept the
       WAVEFORMATEXTENSIBLE structure.
    */
    if(wfx.Format.nChannels > 2)
    {
        wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        wfx.Format.cbSize = 22;
        wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;
        wfx.dwChannelMask = channelMaskSpeakers[audioChannels - 1];
    }

    /* allocate the internal waveout buffers:
      The length of a single buffer in bytes is dependent of periodSize.
    */
    lenBuffer = wfx.Format.nBlockAlign * periodSize;
    /* create and clear the driver data */
    dev = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                    sizeof(waveoutAudioDriverT) +
                    lenBuffer * periods +
                    sizeof(WAVEHDR) * periods);
    if(dev == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    /* Assign extra memory to WAVEHDR */
    dev->waveHeader = (WAVEHDR *)((uintptrT)(dev + 1) + lenBuffer * periods);

    /* Save copy of synth */
    dev->synth = data;
    dev->func = func;

    /* Save copy of other variables */
    dev->writePtr = writePtr;
    /* number of frames in a buffer */
    dev->numFrames = periodSize;
    /* number of buffers */
    dev->periods = periods;
    dev->channelsCount = wfx.Format.nChannels;

    /* Set default device to use */
    device = WAVE_MAPPER;

    if(func)
    {
        /* allocate extra buffer used by waveoutWriteProcessedChannels().
           These buffers are buffer adaptation between the rendering
           API synthProcess() and the waveout internal buffers
           Note: the size (in bytes) of these extra buffer (drybuf[]) must be the
           same that the size of internal waveout buffers.
        */
        dev->drybuf = ARRAY(float*, audioChannels * 2);
        if(dev->drybuf == NULL)
        {
            LOG(ERR, "Out of memory");
            deleteWaveoutAudioDriver(&dev->driver);
            return NULL;
        }
        MEMSET(dev->drybuf, 0, sizeof(float*) * audioChannels * 2);
        for(i = 0; i < audioChannels * 2; ++i)
        {
            /* The length of a single buffer drybuf[i] is dependent of periodSize */
            dev->drybuf[i] = ARRAY(float, periodSize);
            if(dev->drybuf[i] == NULL)
            {
                LOG(ERR, "Out of memory");
                deleteWaveoutAudioDriver(&dev->driver);
                return NULL;
            }
        }
    }

    /* get the selected device name. if none is specified, use default device. */
    if(settingsCopystr(settings, "audio.waveout.device", devName, MAXPNAMELEN) == OK
            && devName[0] != '\0')
    {
        UINT nDevs = waveOutGetNumDevs();
        UINT n;
#ifdef _UNICODE
        WCHAR lpwDevName[MAXPNAMELEN];

        MultiByteToWideChar(CP_UTF8, 0, devName, -1, lpwDevName, MAXPNAMELEN);
#endif

        for(n = 0; n < nDevs; n++)
        {
            WAVEOUTCAPS caps;
            MMRESULT    res;

            res = waveOutGetDevCaps(n, &caps, sizeof(caps));

            if(res == MMSYSERR_NOERROR)
            {
#ifdef _UNICODE

                if(wcsicmp(lpwDevName, caps.szPname) == 0)
#else
                if(STRCASECMP(devName, caps.szPname) == 0)
#endif
                {
                    LOG(DBG, "Selected audio device GUID: %s", devName);
                    device = n;
                    break;
                }
            }
        }
    }

    do
    {

        dev->hQuit = CreateEvent(NULL, FALSE, FALSE, NULL);

        if(dev->hQuit == NULL)
        {
            LOG(ERR, "Failed to create quit event: '%s'", getWindowsError());
            break;
        }

        /* Create thread which processes re-adding SYSEX buffers */
        dev->hThread = CreateThread(
                           NULL,
                           0,
                           (LPTHREAD_START_ROUTINE)
                           waveoutSynthThread,
                           dev,
                           0,
                           &dev->dwThread);

        if(dev->hThread == NULL)
        {
            LOG(ERR, "Failed to create waveOut thread: '%s'", getWindowsError());
            break;
        }

        errCode = waveOutOpen(&dev->hWaveOut,
                              device,
                              (WAVEFORMATEX *)&wfx,
                              (DWORD_PTR)dev->dwThread,
                              0,
                              CALLBACK_THREAD);

        if(errCode != MMSYSERR_NOERROR)
        {
            LOG(ERR, "Failed to open waveOut device: '%s'", waveoutError(errCode));
            break;
        }

        /* Get pointer to sound buffer memory */
        ptrBuffer = (LPSTR)(dev + 1);

        /* Setup the sample buffers */
        for(i = 0; i < periods; i++)
        {
            /* Clear the sample buffer */
            memset(ptrBuffer, 0, lenBuffer);

            /* Clear descriptor buffer */
            memset(dev->waveHeader + i, 0, sizeof(WAVEHDR));

            /* Compile descriptor buffer */
            dev->waveHeader[i].lpData         = ptrBuffer;
            dev->waveHeader[i].dwBufferLength = lenBuffer;
            dev->waveHeader[i].dwUser         = (DWORD_PTR)dev;

            waveOutPrepareHeader(dev->hWaveOut, &dev->waveHeader[i], sizeof(WAVEHDR));

            ptrBuffer += lenBuffer;
        }

        /* Play the sample buffers */
        for(i = 0; i < periods; i++)
        {
            waveOutWrite(dev->hWaveOut, &dev->waveHeader[i], sizeof(WAVEHDR));
        }

        return (audioDriverT *) dev;

    }
    while(0);

    deleteWaveoutAudioDriver(&dev->driver);
    return NULL;
}


void deleteWaveoutAudioDriver(audioDriverT *d)
{
    int i;

    waveoutAudioDriverT *dev = (waveoutAudioDriverT *) d;
    returnIfFail(dev != NULL);

    /* release all the allocated resources */
    if(dev->hWaveOut != NULL)
    {
        dev->nQuit = dev->periods;
        WaitForSingleObject(dev->hQuit, INFINITE);

        waveOutClose(dev->hWaveOut);
    }

    if(dev->hThread != NULL)
    {
        PostThreadMessage(dev->dwThread, WM_CLOSE, 0, 0);
        WaitForSingleObject(dev->hThread, INFINITE);

        CloseHandle(dev->hThread);
    }

    if(dev->hQuit != NULL)
    {
        CloseHandle(dev->hQuit);
    }

    if(dev->drybuf != NULL)
    {
        for(i = 0; i < dev->channelsCount; ++i)
        {
            FREE(dev->drybuf[i]);
        }
    }

    FREE(dev->drybuf);

    HeapFree(GetProcessHeap(), 0, dev);
}

static int waveoutWriteProcessedChannels(Synthesizer *data, int len,
                               int channelsCount,
                               void *channelsOut[], int channelsOff[],
                               int channelsIncr[])
{
    int i, ch;
    int ret;
    waveoutAudioDriverT *drv = (waveoutAudioDriverT*) data;
    float *optr[WAVEOUT_MAX_STEREO_CHANNELS * 2];
    for(ch = 0; ch < drv->channelsCount; ++ch)
    {
        MEMSET(drv->drybuf[ch], 0, len * sizeof(float));
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

static char *waveoutError(MMRESULT hr)
{
    char *s = "Don't know why";

    switch(hr)
    {
    case MMSYSERR_NOERROR:
        s = "The operation completed successfully :)";
        break;

    case MMSYSERR_ALLOCATED:
        s = "Specified resource is already allocated.";
        break;

    case MMSYSERR_BADDEVICEID:
        s = "Specified device identifier is out of range";
        break;

    case MMSYSERR_NODRIVER:
        s = "No device driver is present";
        break;

    case MMSYSERR_NOMEM:
        s = "Unable to allocate or lock memory";
        break;

    case WAVERR_BADFORMAT:
        s = "Attempted to open with an unsupported waveform-audio format";
        break;

    case WAVERR_SYNC:
        s = "The device is synchronous but waveOutOpen was called without using the WAVE_ALLOWSYNC flag";
        break;
    }

    return s;
}

#endif /* WAVEOUT_SUPPORT */
