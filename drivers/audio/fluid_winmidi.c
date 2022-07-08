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


/* winmidi.c
 *
 * Driver for Windows MIDI
 *
 * NOTE: Unfortunately midiInAddBuffer(), for SYSEX data, should not be called
 * from within the MIDI input callback, despite many examples contrary to that
 * on the Internet.  Some MIDI devices will deadlock.  Therefore we add MIDIHDR
 * pointers to a queue and re-add them in a separate thread.  Lame-o API! :(
 *
 * Multiple/single devices handling capabilities:
 * This driver is able to handle multiple devices chosen by the user through
 * the settings midi.winmidi.device.
 * For example, let the following device names:
 * 0:Port MIDI SB Live! [CE00], 1:SB PCI External MIDI, default, x[;y;z;..]
 * Then the driver is able receive MIDI messages coming from distinct devices
 * and forward these messages on distinct MIDI channels set.
 * 1.1)For example, if the user chooses 2 devices at index 0 and 1, the user
 * must specify this by putting the name "0;1" in midi.winmidi.device setting.
 * We get a fictif device composed of real devices (0,1). This fictif device
 * behaves like a device with 32 MIDI channels whose messages are forwarded to
 * driver output as this:
 * - MIDI messages from real device 0 are output to MIDI channels set 0 to 15.
 * - MIDI messages from real device 1 are output to MIDI channels set 15 to 31.
 *
 * 1.2)Now another example with the name "1;0". The driver will forward
 * MIDI messages as this:
 * - MIDI messages from real device 1 are output to MIDI channels set 0 to 15.
 * - MIDI messages from real device 0 are output to MIDI channels set 15 to 31.
 * So, the device order specified in the setting allows the user to choose the
 * MIDI channel set associated with this real device at the driver output
 * according this formula: outputChannel = inputChannel + deviceOrder * 16.
 *
 * 2)Note also that the driver handles single device by putting the device name
 * in midi.winmidi.device setting.
 * The user can set the device name "0:Port MIDI SB Live! [CE00]" in the setting.
 * or use the multi device naming "0" (specifying only device index 0).
 * Both naming choice allows the driver to handle the same single device.
 *
 */

#include "fluidsynthPriv.h"

#if WINMIDI_SUPPORT

#include "midi.h"
#include "mdriver.h"
#include "settings.h"

#define MIDI_SYSEX_MAX_SIZE     512
#define MIDI_SYSEX_BUF_COUNT    16

typedef struct winmidiDriver winmidiDriverT;

/* device infos structure for only one midi device */
typedef struct deviceInfos
{
    winmidiDriverT *dev; /* driver structure*/
    unsigned char midiNum;      /* device order number */
    unsigned char channelMap;   /* MIDI channel mapping from input to output */
    UINT devIdx;                /* device index */
    HMIDIIN hmidiin;             /* device handle */
    /* MIDI HDR for SYSEX buffer */
    MIDIHDR sysExHdrs[MIDI_SYSEX_BUF_COUNT];
    /* Sysex data buffer */
    unsigned char sysExBuf[MIDI_SYSEX_BUF_COUNT * MIDI_SYSEX_MAX_SIZE];
} deviceInfosT;

/* driver structure */
struct winmidiDriver
{
    midiDriverT driver;

    /* Thread for SYSEX re-add thread */
    HANDLE hThread;
    DWORD  dwThread;

    /* devices information table */
    int devCount;   /* device information count in devInfos[] table */
    deviceInfosT devInfos[1];
};

#define msgType(_m)  ((unsigned char)(_m & 0xf0))
#define msgChan(_m)  ((unsigned char)(_m & 0x0f))
#define msgP1(_m)    ((_m >> 8) & 0x7f)
#define msgP2(_m)    ((_m >> 16) & 0x7f)

static char *
winmidiInputError(char *strError, MMRESULT no)
{
#ifdef _UNICODE
    WCHAR wStr[MAXERRORLENGTH];

    midiInGetErrorText(no, wStr, MAXERRORLENGTH);
    WideCharToMultiByte(CP_UTF8, 0, wStr, -1, strError, MAXERRORLENGTH, 0, 0);
#else
    midiInGetErrorText(no, strError, MAXERRORLENGTH);
#endif

    return strError;
}

/*
  callback function called by any MIDI device sending a MIDI message.
  @param dwInstance, pointer on deviceInfos structure of this
  device.
*/
static void CALLBACK
winmidiCallback(HMIDIIN hmi, UINT wMsg, DWORD_PTR dwInstance,
                       DWORD_PTR dwParam1, DWORD_PTR dwParam2)
{
    deviceInfosT *devInfos = (deviceInfosT *) dwInstance;
    winmidiDriverT *dev = devInfos->dev;
    midiEventT event;
    LPMIDIHDR pMidiHdr;
    unsigned char *data;
    unsigned int msgParam = (unsigned int) dwParam1;

    switch(wMsg)
    {
    case MIM_OPEN:
        break;

    case MIM_CLOSE:
        break;

    case MIM_DATA:
        event.type = msgType(msgParam);
        event.channel = msgChan(msgParam) + devInfos->channelMap;

        FLUID_LOG(FLUID_DBG, "\ndevice at index %d sending MIDI message on channel %d, forwarded on channel: %d",
                  devInfos->devIdx, msgChan(msgParam), event.channel);

        if(event.type != PITCH_BEND)
        {
            event.param1 = msgP1(msgParam);
            event.param2 = msgP2(msgParam);
        }
        else      /* Pitch bend is a 14 bit value */
        {
            event.param1 = (msgP2(msgParam) << 7) | msgP1(msgParam);
            event.param2 = 0;
        }

        (*dev->driver.handler)(dev->driver.data, &event);
        break;

    case MIM_LONGDATA:    /* SYSEX data */
        FLUID_LOG(FLUID_DBG, "\ndevice at index %d sending MIDI sysex message",
                  devInfos->devIdx);

        if(dev->hThread == NULL)
        {
            break;
        }

        pMidiHdr = (LPMIDIHDR)dwParam1;
        data = (unsigned char *)(pMidiHdr->lpData);

        /* We only process complete SYSEX messages (discard those that are too small or too large) */
        if(pMidiHdr->dwBytesRecorded > 2 && data[0] == 0xF0
                && data[pMidiHdr->dwBytesRecorded - 1] == 0xF7)
        {
            midiEventSetSysex(&event, pMidiHdr->lpData + 1,
                                       pMidiHdr->dwBytesRecorded - 2, FALSE);
            (*dev->driver.handler)(dev->driver.data, &event);
        }

        /* request the sysex thread to re-add this buffer into the device devInfos->midiNum */
        PostThreadMessage(dev->dwThread, MM_MIM_LONGDATA, devInfos->midiNum, dwParam1);
        break;

    case MIM_ERROR:
        break;

    case MIM_LONGERROR:
        break;

    case MIM_MOREDATA:
        break;
    }
}

/**
 * build a device name prefixed by its index. The format of the returned
 * name is: devIdx:devName
 * The name returned is convenient for midi.winmidi.device setting.
 * It allows the user to identify a device index through its name or vice
 * versa. This allows the user to specify a multi device name using a list of
 * devices index (see winmidiMidiDriverSettings()).
 *
 * @param devIdx, device index.
 * @param devName, name of the device.
 * @return the new device name (that must be freed when finish with it) or
 *  NULL if memory allocation error.
 */
static char *winmidiGetDeviceName(int devIdx, char *devName)
{
    char *newDevName;

    int i =  devIdx;
    size_t size = 0; /* index size */

    do
    {
        size++;
        i = i / 10 ;
    }
    while(i);

    /* index size + separator + name length + zero termination */
    newDevName = FLUID_MALLOC(size + 2 + FLUID_STRLEN(devName));

    if(newDevName)
    {
        /* the name is filled if allocation is successful */
        FLUID_SPRINTF(newDevName, "%d:%s", devIdx, devName);
    }
    else
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
    }

    return newDevName;
}

/*
 Add setting midi.winmidi.device in the settings.

 MIDI devices names are enumerated and added to midi.winmidi.device setting
 options. Example:
 0:Port MIDI SB Live! [CE00], 1:SB PCI External MIDI, default, x[;y;z;..]

 Devices name prefixed by index (i.e 1:SB PCI External MIDI) are real devices.
 "default" name is the default device.
 "x[;y;z;..]" is the multi device naming. Its purpose is to indicate to
 the user how he must specify a multi device name in the setting.
 A multi devices name must be a list of real devices index separated by semicolon:
 Example: "5;3;0"
*/
void winmidiMidiDriverSettings(FluidSettings *settings)
{
    MMRESULT res;
    MIDIINCAPS inCaps;
    UINT i, num;

    /* register midi.winmidi.device */
    settingsRegisterStr(settings, "midi.winmidi.device", "default", 0);
    num = midiInGetNumDevs();

    if(num > 0)
    {
        settingsAddOption(settings, "midi.winmidi.device", "default");

        /* add real devices names in options list */
        for(i = 0; i < num; i++)
        {
            res = midiInGetDevCaps(i, &inCaps, sizeof(MIDIINCAPS));

            if(res == MMSYSERR_NOERROR)
            {
                /* add new device name (prefixed by its index) */
                char *newDevName = winmidiGetDeviceName(i, inCaps.szPname);

                if(!newDevName)
                {
                    break;
                }

                settingsAddOption(settings, "midi.winmidi.device",
                                          newDevName);
                FLUID_FREE(newDevName);
            }
        }
    }
}

/* Thread for re-adding SYSEX buffers */
static DWORD WINAPI winmidiAddSysexThread(void *data)
{
    winmidiDriverT *dev = (winmidiDriverT *)data;
    MSG msg;
    int code;

    for(;;)
    {
        code = GetMessage(&msg, NULL, 0, 0);

        if(code < 0)
        {
            FLUID_LOG(FLUID_ERR, "winmidiAddSysexThread: GetMessage() failed.");
            break;
        }

        if(msg.message == WM_CLOSE)
        {
            break;
        }

        switch(msg.message)
        {
        case MM_MIM_LONGDATA:
            /* re-add the buffer into the device designed by msg.wParam parameter */
            midiInAddBuffer(dev->devInfos[msg.wParam].hmidiin,
                            (LPMIDIHDR)msg.lParam, sizeof(MIDIHDR));
            break;
        }
    }

    return 0;
}

/**
 * Parse device name
 * @param dev if not NULL pointer on driver structure in which device index
 *  are returned.
 * @param devName device name which is expected to be:
 *  - a multi devices naming (i.e "1;0;2") or
 *  - a single device name (i.e "0:Port MIDI SB Live! [CE00]"
 * @return count of devices parsed or 0 if device name doesn't exist.
 */
static int
winmidiParseDeviceName(winmidiDriverT *dev, char *devName)
{
    int devCount = 0; /* device count */
    int devIdx;       /* device index */
    char *curIdx, *nextIdx;      /* current and next ascii index pointer */
    char cpyDevName[MAXPNAMELEN];
    int num = midiInGetNumDevs(); /* get number of real devices installed */

    /* look for a multi device naming */
    /* multi devices name "x;[y;..]". parse devices index: x;y;..
       Each ascii index are separated by a semicolon character.
    */
    FLUID_STRCPY(cpyDevName, devName); /* strtok() will overwrite */
    nextIdx = cpyDevName;

    while(NULL != (curIdx = strtok(&nextIdx, " ;")))
    {
        /* try to convert current ascii index */
        char *endIdx = curIdx;
        devIdx = FLUID_STRTOL(curIdx, &endIdx, 10);

        if(curIdx == endIdx      /* not an integer number */
           || devIdx < 0          /* invalid device index */
           || devIdx >= num       /* invalid device index */
          )
        {
            if(dev)
            {
                dev->devCount = 0;
            }

            devCount = 0; /* error, end of parsing */
            break;
        }

        /* memorize device index in devInfos table */
        if(dev)
        {
            dev->devInfos[dev->devCount++].devIdx = devIdx;
        }

        devCount++;
    }

    /* look for single device if multi devices not found */
    if(!devCount)
    {
        /* default device index: devIdx = 0, devCount = 1 */
        devCount = 1;
        devIdx = 0;

        if(FLUID_STRCASECMP("default", devName) != 0)
        {
            int i;
            devCount = 0; /* reset count of devices found */

            for(i = 0; i < num; i++)
            {
                char strError[MAXERRORLENGTH];
                MIDIINCAPS inCaps;
                MMRESULT res;
                res = midiInGetDevCaps(i, &inCaps, sizeof(MIDIINCAPS));

                if(res == MMSYSERR_NOERROR)
                {
                    int strCmpRes;
                    char *newDevName = winmidiGetDeviceName(i, inCaps.szPname);

                    if(!newDevName)
                    {
                        break;
                    }

#ifdef _UNICODE
                    WCHAR wDevName[MAXPNAMELEN];
                    MultiByteToWideChar(CP_UTF8, 0, devName, -1, wDevName, MAXPNAMELEN);

                    strCmpRes = wcsicmp(wDevName, newDevName);
#else
                    strCmpRes = FLUID_STRCASECMP(devName, newDevName);
#endif

                    FLUID_LOG(FLUID_DBG, "Testing midi device \"%s\"", newDevName);
                    FLUID_FREE(newDevName);

                    if(strCmpRes == 0)
                    {
                        FLUID_LOG(FLUID_DBG, "Selected midi device number: %u", i);
                        devIdx = i;
                        devCount = 1;
                        break;
                    }
                }
                else
                {
                    FLUID_LOG(FLUID_DBG, "Error testing midi device %u of %u: %s (error %d)",
                              i, num, winmidiInputError(strError, res), res);
                }
            }
        }

        if(dev && devCount)
        {
            dev->devInfos[0].devIdx = devIdx;
            dev->devCount = 1;
        }
    }

    if(num < devCount)
    {
        FLUID_LOG(FLUID_ERR, "not enough MIDI in devices found. Expected:%d found:%d",
                  devCount, num);
        devCount = 0;
    }

    return devCount;
}

/*
 * newFluidWinmidiDriver
 */
midiDriverT *
newFluidWinmidiDriver(FluidSettings *settings,
                         handleMidiEventFuncT handler, void *data)
{
    winmidiDriverT *dev;
    MMRESULT res;
    int i, j;
    int maxDevices;  /* maximum number of devices to handle */
    char strError[MAXERRORLENGTH];
    char devName[MAXPNAMELEN];

    /* not much use doing anything */
    if(handler == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Invalid argument");
        return NULL;
    }

    /* get the device name. if none is specified, use the default device. */
    if(settingsCopystr(settings, "midi.winmidi.device", devName, MAXPNAMELEN) != FLUID_OK)
    {
        FLUID_LOG(FLUID_DBG, "No MIDI in device selected, using \"default\"");
        FLUID_STRCPY(devName, "default");
    }

    /* parse device name, get the maximum number of devices to handle */
    maxDevices = winmidiParseDeviceName(NULL, devName);

    /* check if any device has be found	*/
    if(!maxDevices)
    {
        FLUID_LOG(FLUID_ERR, "Device \"%s\" does not exists", devName);
        return NULL;
    }

    /* allocation of driver structure size dependent of maxDevices */
    i = sizeof(winmidiDriverT) + (maxDevices - 1) * sizeof(deviceInfosT);
    dev = FLUID_MALLOC(i);

    if(dev == NULL)
    {
        return NULL;
    }

    FLUID_MEMSET(dev, 0, i); /* reset structure members */

    /* parse device name, get devices index  */
    winmidiParseDeviceName(dev, devName);

    dev->driver.handler = handler;
    dev->driver.data = data;

    /* try opening the devices */
    for(i = 0; i < dev->devCount; i++)
    {
        deviceInfosT *devInfos = &dev->devInfos[i];
        devInfos->dev = dev;            /* driver structure */
        devInfos->midiNum = i;         /* device order number */
        devInfos->channelMap = i * 16; /* map from input to output */
        FLUID_LOG(FLUID_DBG, "opening device at index %d", devInfos->devIdx);
        res = midiInOpen(&devInfos->hmidiin, devInfos->devIdx,
                         (DWORD_PTR) winmidiCallback,
                         (DWORD_PTR) devInfos, CALLBACK_FUNCTION);

        if(res != MMSYSERR_NOERROR)
        {
            FLUID_LOG(FLUID_ERR, "Couldn't open MIDI input: %s (error %d)",
                      winmidiInputError(strError, res), res);
            goto errorRecovery;
        }

        /* Prepare and add SYSEX buffers */
        for(j = 0; j < MIDI_SYSEX_BUF_COUNT; j++)
        {
            MIDIHDR *hdr = &devInfos->sysExHdrs[j];

            hdr->lpData = (LPSTR)&devInfos->sysExBuf[j * MIDI_SYSEX_MAX_SIZE];
            hdr->dwBufferLength = MIDI_SYSEX_MAX_SIZE;

            /* Prepare a buffer for SYSEX data and add it */
            res = midiInPrepareHeader(devInfos->hmidiin, hdr, sizeof(MIDIHDR));

            if(res == MMSYSERR_NOERROR)
            {
                res = midiInAddBuffer(devInfos->hmidiin, hdr, sizeof(MIDIHDR));

                if(res != MMSYSERR_NOERROR)
                {
                    FLUID_LOG(FLUID_WARN, "Failed to prepare MIDI SYSEX buffer: %s (error %d)",
                              winmidiInputError(strError, res), res);
                    midiInUnprepareHeader(devInfos->hmidiin, hdr, sizeof(MIDIHDR));
                }
            }
            else
                FLUID_LOG(FLUID_WARN, "Failed to prepare MIDI SYSEX buffer: %s (error %d)",
                          winmidiInputError(strError, res), res);
        }
    }


    /* Create thread which processes re-adding SYSEX buffers */
    dev->hThread = CreateThread(
                       NULL,
                       0,
                       (LPTHREAD_START_ROUTINE)
                       winmidiAddSysexThread,
                       dev,
                       0,
                       &dev->dwThread);

    if(dev->hThread == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create SYSEX buffer processing thread");
        goto errorRecovery;
    }

    /* Start the MIDI input interface */
    for(i = 0; i < dev->devCount; i++)
    {
        if(midiInStart(dev->devInfos[i].hmidiin) != MMSYSERR_NOERROR)
        {
            FLUID_LOG(FLUID_ERR, "Failed to start the MIDI input. MIDI input not available.");
            goto errorRecovery;
        }
    }

    return (midiDriverT *) dev;

errorRecovery:

    deleteFluidWinmidiDriver((midiDriverT *) dev);
    return NULL;
}

/*
 * deleteFluidWinmidiDriver
 */
void
deleteFluidWinmidiDriver(midiDriverT *p)
{
    int i, j;

    winmidiDriverT *dev = (winmidiDriverT *) p;
    returnIfFail(dev != NULL);

    /* request the sysex thread to terminate */
    if(dev->hThread != NULL)
    {
        PostThreadMessage(dev->dwThread, WM_CLOSE, 0, 0);
        WaitForSingleObject(dev->hThread, INFINITE);

        CloseHandle(dev->hThread);
        dev->hThread = NULL;
    }

    /* stop MIDI in devices and free allocated buffers */
    for(i = 0; i < dev->devCount; i++)
    {
        deviceInfosT *devInfos = &dev->devInfos[i];

        if(devInfos->hmidiin != NULL)
        {
            /* stop the device and mark any pending data blocks as being done */
            midiInReset(devInfos->hmidiin);

            /* free allocated buffers associated to this device */
            for(j = 0; j < MIDI_SYSEX_BUF_COUNT; j++)
            {
                MIDIHDR *hdr = &devInfos->sysExHdrs[j];

                if((hdr->dwFlags & MHDR_PREPARED))
                {
                    midiInUnprepareHeader(devInfos->hmidiin, hdr, sizeof(MIDIHDR));
                }
            }

            /* close the device */
            midiInClose(devInfos->hmidiin);
        }
    }

    FLUID_FREE(dev);
}

#endif /* WINMIDI_SUPPORT */
