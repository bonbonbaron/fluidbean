/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 * Copyright (C) 2021  Chris Xiong
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

#if WASAPI_SUPPORT

#define CINTERFACE
#define COBJMACROS

#include <objbase.h>
#include <windows.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <audiosessiontypes.h>
#include <functiondiscoverykeysDevpkey.h>
#include <mmreg.h>
#include <oaidl.h>
#include <ksguid.h>
#include <ksmedia.h>

// these symbols are either never found in headers, or
// only defined but there are no library containing the actual symbol...

#ifndef AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM
#define AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM 0x80000000
#endif

#ifndef AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY
#define AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY 0x08000000
#endif

static const CLSID _CLSID_MMDeviceEnumerator =
{
    0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}
};

static const IID   _IID_IMMDeviceEnumerator =
{
    0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}
};
static const IID   _IID_IAudioClient =
{
    0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}
};
static const IID   _IID_IAudioRenderClient =
{
    0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}
};
/*
 * WASAPI Driver for synth
 *
 * Current limitations:
 *  - Only one stereo audio output.
 *  - If audio.sample-format is "16bits", a conversion from float
 *    without dithering is used.
 *
 * Available settings:
 *  - audio.wasapi.exclusive-mode
 *    0 = shared mode, 1 = exclusive mode
 *  - audio.wasapi.device
 *    Used for device selection. Leave unchanged (or use the value "default")
 *    to use the default Windows audio device for media playback.
 *
 * Notes:
 *  - If exclusive mode is selected, audio.period-size is used as the periodicity
 *    of the IAudioClient stream, which is the sole factor of audio latency.
 *    audio.periods still determines the buffer size, but has no direct impact on
 *    the latency (at least according to Microsoft). The valid range for
 *    audio.period-size may vary depending on the driver and sample rate.
 *  - In shared mode, audio.period-size is completely ignored. Instead, a value
 *    provided by the audio driver is used. In theory this means the latency in
 *    shared mode is out of synth's control, but you may still increase
 *    audio.periods for a larger buffer to fix buffer underruns in case there
 *    are any.
 *  - The sample rate and sample format of synth must be supported by the
 *    audio device in exclusive mode. Otherwise driver creation will fail. Use
 *    `synth ---query-audio-devices` to find out the modes supported by
 *    the soundcards installed on the system.
 *  - In shared mode, if the sample rate of the synth doesn't match what is
 *    configured in the 'advanced' tab of the audio device properties dialog,
 *    Windows will automatically resample the output (obviously). Windows
 *    Vista doesn't seem to support the resampling method with better quality.
 *  - Under Windows 10, this driver may report a latency of 0ms in shared mode.
 *    This is nonsensical and should be disregarded.
 *
 */

#define WASAPI_MAX_OUTPUTS 1

typedef void(*wasapiDevenumCallbackT)(IMMDevice *, void *);

static DWORD WINAPI wasapiAudioRun(void *p);
static int wasapiWriteProcessedChannels(void *data, int len,
        int channelsCount,
        void *channelsOut[], int channelsOff[],
        int channelsIncr[]);
static void wasapiForeachDevice(wasapiDevenumCallbackT callback, void *data);
static void wasapiRegisterCallback(IMMDevice *dev, void *data);
static void wasapiFinddevCallback(IMMDevice *dev, void *data);
static IMMDevice *wasapiFindDevice(IMMDeviceEnumerator *denum, const char *name);
static INLINE int16_t roundClipToI16(float x);

typedef struct
{
    const char *name;
    wcharT *id;
} wasapiFinddevDataT;

typedef struct
{
    audioDriverT driver;

    void *userPointer;
    audioFuncT func;
    audioChannelsCallbackT write;
    float **drybuf;

    UINT32 nframes;
    double bufferDuration;
    int channelsCount;
    int floatSamples;

    HANDLE startEv;
    HANDLE thread;
    DWORD threadId;
    HANDLE quitEv;

    IAudioClient *aucl;
    IAudioRenderClient *arcl;

    double sampleRate;
    int periods, periodSize;
    longLongT bufferDurationReftime;
    longLongT periodsReftime;
    longLongT latencyReftime;
    int audioChannels;
    int sampleSize;
    char *dname;
    int exclusive;
    unsigned short sampleFormat;

} wasapiAudioDriverT;

audioDriverT *newWasapiAudioDriver(Settings *settings, Synthesizer *synth)
{
    return newWasapiAudioDriver2(settings, (audioFuncT)synthProcess, synth);
}

audioDriverT *newWasapiAudioDriver2(Settings *settings, audioFuncT func, void *data)
{
    DWORD ret;
    HANDLE waitHandles[2];
    wasapiAudioDriverT *dev = NULL;
    OSVERSIONINFOEXW vi = {sizeof(vi), 6, 0, 0, 0, {0}, 0, 0, 0, 0, 0};

    if(!VerifyVersionInfoW(&vi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
                           VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                   VER_MINORVERSION, VER_GREATER_EQUAL),
                                   VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL)))
    {
        LOG(ERR, "wasapi: this driver requires Windows Vista or newer.");
        return NULL;
    }

    dev = NEW(wasapiAudioDriverT);

    if(dev == NULL)
    {
        LOG(ERR, "wasapi: out of memory.");
        return NULL;
    }

    MEMSET(dev, 0, sizeof(wasapiAudioDriverT));

    /* Retrieve the settings */

    settingsGetnum(settings, "synth.sample-rate", &dev->sampleRate);
    settingsGetint(settings, "audio.periods", &dev->periods);
    settingsGetint(settings, "audio.period-size", &dev->periodSize);
    settingsGetint(settings, "synth.audio-channels", &dev->audioChannels);
    settingsGetint(settings, "audio.wasapi.exclusive-mode", &dev->exclusive);

    if(dev->audioChannels > WASAPI_MAX_OUTPUTS)
    {
        LOG(ERR, "wasapi: channel configuration with more than one stereo pair is not supported.");
        goto cleanup;
    }

    if(settingsStrEqual(settings, "audio.sample-format", "16bits"))
    {
        dev->sampleSize = sizeof(int16_t);
        dev->sampleFormat = WAVE_FORMAT_PCM;
    }
    else
    {
        dev->sampleSize = sizeof(float);
        dev->sampleFormat = WAVE_FORMAT_IEEE_FLOAT;
    }

    if(settingsDupstr(settings, "audio.wasapi.device", &dev->dname) != OK)
    {
        LOG(ERR, "wasapi: out of memory.");
        goto cleanup;
    }

    dev->func = func;
    dev->userPointer = data;
    dev->bufferDuration = dev->periods * dev->periodSize / dev->sampleRate;
    dev->channelsCount = dev->audioChannels * 2;
    dev->floatSamples = (dev->sampleFormat == WAVE_FORMAT_IEEE_FLOAT);
    dev->bufferDurationReftime = (longLongT)(dev->bufferDuration * 1e7 + .5);
    dev->periodsReftime = (longLongT)(dev->periodSize / dev->sampleRate * 1e7 + .5);

    dev->quitEv = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(dev->quitEv == NULL)
    {
        LOG(ERR, "wasapi: failed to create quit event: '%s'", getWindowsError());
        goto cleanup;
    }

    dev->startEv = CreateEvent(NULL, FALSE, FALSE, NULL);

    if(dev->startEv == NULL)
    {
        LOG(ERR, "wasapi: failed to create start event: '%s'", getWindowsError());
        goto cleanup;
    }

    dev->thread = CreateThread(NULL, 0, wasapiAudioRun, dev, 0, &dev->threadId);

    if(dev->thread == NULL)
    {
        LOG(ERR, "wasapi: failed to create audio thread: '%s'", getWindowsError());
        goto cleanup;
    }

    /* start event must be first */
    waitHandles[0] = dev->startEv;
    waitHandles[1] = dev->thread;
    ret = WaitForMultipleObjects(N_ELEMENTS(waitHandles), waitHandles, FALSE, 2000);

    switch(ret)
    {
    case WAIT_OBJECT_0:
        return &dev->driver;

    case WAIT_TIMEOUT:
        LOG(WARN, "wasapi: initialization timeout!");
        break;

    default:
        break;
    }

cleanup:

    deleteWasapiAudioDriver(&dev->driver);
    return NULL;
}

void deleteWasapiAudioDriver(audioDriverT *p)
{
    wasapiAudioDriverT *dev = (wasapiAudioDriverT *) p;
    int i;

    returnIfFail(dev != NULL);

    if(dev->thread != NULL)
    {
        SetEvent(dev->quitEv);

        if(WaitForSingleObject(dev->thread, 2000) != WAIT_OBJECT_0)
        {
            LOG(WARN, "wasapi: couldn't join the audio thread. killing it.");
            TerminateThread(dev->thread, 0);
        }

        CloseHandle(dev->thread);
    }

    if(dev->quitEv != NULL)
    {
        CloseHandle(dev->quitEv);
    }

    if(dev->startEv != NULL)
    {
        CloseHandle(dev->startEv);
    }

    if(dev->drybuf)
    {
        for(i = 0; i < dev->channelsCount; ++i)
        {
            FREE(dev->drybuf[i]);
        }
    }

    FREE(dev->dname);

    FREE(dev->drybuf);

    FREE(dev);
}

void wasapiAudioDriverSettings(Settings *settings)
{
    settingsRegisterInt(settings, "audio.wasapi.exclusive-mode", 0, 0, 1, HINT_TOGGLED);
    settingsRegisterStr(settings, "audio.wasapi.device", "default", 0);
    settingsAddOption(settings, "audio.wasapi.device", "default");
    wasapiForeachDevice(wasapiRegisterCallback, settings);
}

static DWORD WINAPI wasapiAudioRun(void *p)
{
    wasapiAudioDriverT *dev = (wasapiAudioDriverT *)p;
    DWORD timeToSleep;
    UINT32 pos;
    DWORD len;
    void *channelsOut[2];
    int channelsOff[2] = {0, 1};
    int channelsIncr[2] = {2, 2};
    BYTE *pbuf;
    HRESULT ret;
    IMMDeviceEnumerator *denum = NULL;
    IMMDevice *mmdev = NULL;
    DWORD flags = 0;
    WAVEFORMATEXTENSIBLE wfx;
    WAVEFORMATEXTENSIBLE *rwfx = NULL;
    AUDCLNT_SHAREMODE shareMode;
    OSVERSIONINFOEXW vi = {sizeof(vi), 6, 0, 0, 0, {0}, 0, 0, 0, 0, 0};
    int needsComUninit = FALSE;
    int i;

    /* Clear format structure */
    ZeroMemory(&wfx, sizeof(WAVEFORMATEXTENSIBLE));

    wfx.Format.nChannels  = 2;
    wfx.Format.wBitsPerSample  = dev->sampleSize * 8;
    wfx.Format.nBlockAlign     = dev->sampleSize * wfx.Format.nChannels;
    wfx.Format.nSamplesPerSec  = (DWORD) dev->sampleRate;
    wfx.Format.nAvgBytesPerSec = (DWORD) dev->sampleRate * wfx.Format.nBlockAlign;
    wfx.Format.wFormatTag      = dev->sampleFormat;
    //wfx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
    //wfx.Format.cbSize = 22;
    //wfx.SubFormat = guidFloat;
    //wfx.dwChannelMask = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT;
    //wfx.Samples.wValidBitsPerSample = wfx.Format.wBitsPerSample;

    /* initialize COM in a worker thread to avoid a potential double initialization in the callers thread */
    ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot initialize COM. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    needsComUninit = TRUE;

    ret = CoCreateInstance(
              &_CLSID_MMDeviceEnumerator, NULL,
              CLSCTX_ALL, &_IID_IMMDeviceEnumerator,
              (void **)&denum);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot create device enumerator. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    mmdev = wasapiFindDevice(denum, dev->dname);

    if(mmdev == NULL)
    {
        goto cleanup;
    }

    ret = IMMDevice_Activate(mmdev,
                             &_IID_IAudioClient,
                             CLSCTX_ALL, NULL,
                             (void **)&dev->aucl);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot activate audio client. 0x%x", (unsigned)ret);
        goto cleanup;
    }


    if(dev->exclusive)
    {
        shareMode = AUDCLNT_SHAREMODE_EXCLUSIVE;
        LOG(DBG, "wasapi: using exclusive mode.");
    }
    else
    {
        longLongT defp;
        shareMode = AUDCLNT_SHAREMODE_SHARED;
        LOG(DBG, "wasapi: using shared mode.");
        dev->periodsReftime = 0;

        //use default period size of the device
        if(SUCCEEDED(IAudioClient_GetDevicePeriod(dev->aucl, &defp, NULL)))
        {
            dev->periodSize = (int)(defp / 1e7 * dev->sampleRate);
            dev->bufferDuration = dev->periods * dev->periodSize / dev->sampleRate;
            dev->bufferDurationReftime = (longLongT)(dev->bufferDuration * 1e7 + .5);
            LOG(DBG, "wasapi: using device period size: %d", dev->periodSize);
        }
    }

    ret = IAudioClient_IsFormatSupported(dev->aucl, shareMode, (const WAVEFORMATEX *)&wfx, (WAVEFORMATEX **)&rwfx);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: device doesn't support the mode we want. 0x%x", (unsigned)ret);
        goto cleanup;
    }
    else if(ret == S_FALSE)
    {
        //rwfx is non-null only in this case
        LOG(INFO, "wasapi: requested mode cannot be fully satisfied.");

        if(rwfx->Format.nSamplesPerSec != wfx.Format.nSamplesPerSec) // needs resampling
        {
            flags = AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM;
            vi.dwMinorVersion = 1;

            if(VerifyVersionInfoW(&vi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR,
                                  VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                          VER_MAJORVERSION, VER_GREATER_EQUAL),
                                          VER_MINORVERSION, VER_GREATER_EQUAL),
                                          VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL)))
                //IAudioClient::Initialize in Vista fails with E_INVALIDARG if this flag is set
            {
                flags |= AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
            }
        }

        CoTaskMemFree(rwfx);
    }

    ret = IAudioClient_Initialize(dev->aucl, shareMode, flags,
                                  dev->bufferDurationReftime, dev->periodsReftime, (WAVEFORMATEX *)&wfx, &GUID_NULL);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: failed to initialize audio client. 0x%x", (unsigned)ret);

        if(ret == AUDCLNT_E_INVALID_DEVICE_PERIOD)
        {
            longLongT defp, minp;
            LOG(ERR, "wasapi: the device period size is invalid.");

            if(SUCCEEDED(IAudioClient_GetDevicePeriod(dev->aucl, &defp, &minp)))
            {
                int defpf = (int)(defp / 1e7 * dev->sampleRate);
                int minpf = (int)(minp / 1e7 * dev->sampleRate);
                LOG(ERR, "wasapi: minimum period is %d, default period is %d. selected %d.", minpf, defpf, dev->periodSize);
            }
        }

        goto cleanup;
    }

    ret = IAudioClient_GetBufferSize(dev->aucl, &dev->nframes);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get audio buffer size. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    LOG(DBG, "wasapi: requested %d frames of buffers, got %u.", dev->periods * dev->periodSize, dev->nframes);
    dev->bufferDuration = dev->nframes / dev->sampleRate;
    timeToSleep = dev->bufferDuration * 1000 / 2;
    if(timeToSleep < 1)
    {
        timeToSleep = 1;
    }

    dev->drybuf = ARRAY(float *, dev->audioChannels * 2);

    if(dev->drybuf == NULL)
    {
        LOG(ERR, "wasapi: out of memory");
        goto cleanup;
    }

    MEMSET(dev->drybuf, 0, sizeof(float *) * dev->audioChannels * 2);

    for(i = 0; i < dev->audioChannels * 2; ++i)
    {
        dev->drybuf[i] = ARRAY(float, dev->nframes);

        if(dev->drybuf[i] == NULL)
        {
            LOG(ERR, "wasapi: out of memory");
            goto cleanup;
        }
    }

    ret = IAudioClient_GetService(dev->aucl, &_IID_IAudioRenderClient, (void **)&dev->arcl);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get audio render device. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    if(SUCCEEDED(IAudioClient_GetStreamLatency(dev->aucl, &dev->latencyReftime)))
    {
        LOG(DBG, "wasapi: latency: %fms.", dev->latencyReftime / 1e4);
    }

    ret = IAudioClient_Start(dev->aucl);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: failed to start audio client. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    /* Signal the success of the driver initialization */
    SetEvent(dev->startEv);

    for(;;)
    {
        ret = IAudioClient_GetCurrentPadding(dev->aucl, &pos);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: cannot get buffer padding. 0x%x", (unsigned)ret);
            goto cleanup;
        }

        len = dev->nframes - pos;

        if(len == 0)
        {
            Sleep(0);
            continue;
        }

        ret = IAudioRenderClient_GetBuffer(dev->arcl, len, &pbuf);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: cannot get buffer. 0x%x", (unsigned)ret);
            goto cleanup;
        }

        channelsOut[0] = channelsOut[1] = (void *)pbuf;

        wasapiWriteProcessedChannels(dev, len, 2,
                                              channelsOut, channelsOff, channelsIncr);

        ret = IAudioRenderClient_ReleaseBuffer(dev->arcl, len, 0);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: failed to release buffer. 0x%x", (unsigned)ret);
            goto cleanup;
        }

        if(WaitForSingleObject(dev->quitEv, timeToSleep) == WAIT_OBJECT_0)
        {
            break;
        }
    }

cleanup:

    if(dev->aucl != NULL)
    {
        IAudioClient_Stop(dev->aucl);
        IAudioClient_Release(dev->aucl);
    }

    if(dev->arcl != NULL)
    {
        IAudioRenderClient_Release(dev->arcl);
    }

    if(mmdev != NULL)
    {
        IMMDevice_Release(mmdev);
    }

    if(denum != NULL)
    {
        IMMDeviceEnumerator_Release(denum);
    }

    if(needsComUninit)
    {
        CoUninitialize();
    }

    return 0;
}

static int wasapiWriteProcessedChannels(void *data, int len,
        int channelsCount,
        void *channelsOut[], int channelsOff[],
        int channelsIncr[])
{
    int i, ch;
    int ret;
    wasapiAudioDriverT *drv = (wasapiAudioDriverT *) data;
    float *optr[WASAPI_MAX_OUTPUTS * 2];
    int16_t *ioptr[WASAPI_MAX_OUTPUTS * 2];
    int efxNch = 0;
    float **efxBuf = NULL;

    for(ch = 0; ch < drv->channelsCount; ++ch)
    {
        MEMSET(drv->drybuf[ch], 0, len * sizeof(float));
        optr[ch] = (float *)channelsOut[ch] + channelsOff[ch];
        ioptr[ch] = (int16_t *)channelsOut[ch] + channelsOff[ch];
    }

    if(drv->func == (audioFuncT)synthProcess)
    {
        efxNch = drv->channelsCount;
        efxBuf = drv->drybuf;
    }
    ret = drv->func(drv->userPointer, len, efxNch, efxBuf, drv->channelsCount, drv->drybuf);

    for(ch = 0; ch < drv->channelsCount; ++ch)
    {
        for(i = 0; i < len; ++i)
        {
            if(drv->floatSamples)
            {
                *optr[ch] = drv->drybuf[ch][i];
                optr[ch] += channelsIncr[ch];
            }
            else //This code is taken from synth.c. No dithering yet.
            {
                *ioptr[ch] = roundClipToI16(drv->drybuf[ch][i] * 32766.0f);
                ioptr[ch] += channelsIncr[ch];
            }
        }
    }

    return ret;
}

static void wasapiForeachDevice(wasapiDevenumCallbackT callback, void *data)
{
    IMMDeviceEnumerator *denum = NULL;
    IMMDeviceCollection *dcoll = NULL;
    UINT cnt, i;
    HRESULT ret;
    int comWasInitialized = FALSE;

    ret = CoInitializeEx(NULL, COINIT_MULTITHREADED);

    if(FAILED(ret))
    {
        if(ret == RPC_E_CHANGED_MODE)
        {
            comWasInitialized = TRUE;
            LOG(DBG, "wasapi: COM was already initialized");
        }
        else
        {
            LOG(ERR, "wasapi: cannot initialize COM. 0x%x", (unsigned)ret);
            return;
        }
    }

    ret = CoCreateInstance(
              &_CLSID_MMDeviceEnumerator, NULL,
              CLSCTX_ALL, &_IID_IMMDeviceEnumerator,
              (void **)&denum);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot create device enumerator. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    ret = IMMDeviceEnumerator_EnumAudioEndpoints(
              denum, eRender,
              DEVICE_STATE_ACTIVE, &dcoll);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot enumerate audio devices. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    ret = IMMDeviceCollection_GetCount(dcoll, &cnt);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get device count. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    for(i = 0; i < cnt; ++i)
    {
        IMMDevice *dev = NULL;

        ret = IMMDeviceCollection_Item(dcoll, i, &dev);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: cannot get device #%u. 0x%x", i, (unsigned)ret);
            continue;
        }

        callback(dev, data);

        IMMDevice_Release(dev);
    }

cleanup:

    if(dcoll != NULL)
    {
        IMMDeviceCollection_Release(dcoll);
    }

    if(denum != NULL)
    {
        IMMDeviceEnumerator_Release(denum);
    }

    if(!comWasInitialized)
    {
        CoUninitialize();
    }
}

static void wasapiRegisterCallback(IMMDevice *dev, void *data)
{
    Settings *settings = (Settings *)data;
    IPropertyStore *prop = NULL;
    PROPVARIANT var;
    int ret;

    ret = IMMDevice_OpenPropertyStore(dev, STGM_READ, &prop);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get properties of device. 0x%x", (unsigned)ret);
        return;
    }

    PropVariantInit(&var);

    ret = IPropertyStore_GetValue(prop, &PKEY_Device_FriendlyName, &var);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get friendly name of device. 0x%x", (unsigned)ret);
    }
    else
    {
        int nsz;
        char *name;

        nsz = WideCharToMultiByte(CP_ACP, 0, var.pwszVal, -1, 0, 0, 0, 0);
        name = ARRAY(char, nsz + 1);
        WideCharToMultiByte(CP_ACP, 0, var.pwszVal, -1, name, nsz, 0, 0);
        settingsAddOption(settings, "audio.wasapi.device", name);
        FREE(name);
    }

    IPropertyStore_Release(prop);
    PropVariantClear(&var);
}

static void wasapiFinddevCallback(IMMDevice *dev, void *data)
{
    wasapiFinddevDataT *d = (wasapiFinddevDataT *)data;
    int nsz;
    char *name = NULL;
    wcharT *id = NULL;
    IPropertyStore *prop = NULL;
    PROPVARIANT var;
    HRESULT ret;

    ret = IMMDevice_OpenPropertyStore(dev, STGM_READ, &prop);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get properties of device. 0x%x", (unsigned)ret);
        return;
    }

    PropVariantInit(&var);

    ret = IPropertyStore_GetValue(prop, &PKEY_Device_FriendlyName, &var);

    if(FAILED(ret))
    {
        LOG(ERR, "wasapi: cannot get friendly name of device. 0x%x", (unsigned)ret);
        goto cleanup;
    }

    nsz = WideCharToMultiByte(CP_ACP, 0, var.pwszVal, -1, 0, 0, 0, 0);
    name = ARRAY(char, nsz + 1);
    WideCharToMultiByte(CP_ACP, 0, var.pwszVal, -1, name, nsz, 0, 0);

    if(!STRCASECMP(name, d->name))
    {
        ret = IMMDevice_GetId(dev, &id);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: cannot get id of device. 0x%x", (unsigned)ret);
            goto cleanup;
        }

        nsz = wcslen(id);
        d->id = ARRAY(wcharT, nsz + 1);
        MEMCPY(d->id, id, sizeof(wcharT) * (nsz + 1));
    }

cleanup:
    PropVariantClear(&var);
    IPropertyStore_Release(prop);
    CoTaskMemFree(id);
    FREE(name);
}

static IMMDevice *wasapiFindDevice(IMMDeviceEnumerator *denum, const char *name)
{
    wasapiFinddevDataT d;
    IMMDevice *dev;
    HRESULT ret;
    d.name = name;
    d.id = NULL;

    if(!STRCASECMP(name, "default"))
    {
        ret = IMMDeviceEnumerator_GetDefaultAudioEndpoint(
                  denum,
                  eRender,
                  eMultimedia,
                  &dev);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: cannot get default audio device. 0x%x", (unsigned)ret);
            return NULL;
        }
        else
        {
            return dev;
        }
    }

    wasapiForeachDevice(wasapiFinddevCallback, &d);

    if(d.id != NULL)
    {
        ret = IMMDeviceEnumerator_GetDevice(denum, d.id, &dev);
        FREE(d.id);

        if(FAILED(ret))
        {
            LOG(ERR, "wasapi: cannot find device with id. 0x%x", (unsigned)ret);
            return NULL;
        }

        return dev;
    }
    else
    {
        LOG(ERR, "wasapi: cannot find device \"%s\".", name);
        return NULL;
    }
}

static INLINE int16_t
roundClipToI16(float x)
{
    long i;

    if(x >= 0.0f)
    {
        i = (long)(x + 0.5f);

        if(UNLIKELY(i > 32767))
        {
            i = 32767;
        }
    }
    else
    {
        i = (long)(x - 0.5f);

        if(UNLIKELY(i < -32768))
        {
            i = -32768;
        }
    }

    return (int16_t)i;
}
#endif /* WASAPI_SUPPORT */

