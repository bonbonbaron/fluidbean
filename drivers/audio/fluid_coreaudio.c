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

/* coreaudio.c
 *
 * Driver for the Apple's CoreAudio on MacOS X
 *
 */

#include "adriver.h"
#include "settings.h"

/*
 * !!! Make sure that no include above includes <netinet/tcp.h> !!!
 * It #defines some macros that collide with enum definitions of OpenTransportProviders.h, which is included from OSServices.h, included from CoreServices.h
 *
 * https://trac.macports.org/ticket/36962
 */

#if COREAUDIO_SUPPORT
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudioTypes.h>
#include <CoreAudio/AudioHardware.h>
#include <AudioUnit/AudioUnit.h>

/*
 * coreAudioDriverT
 *
 */
typedef struct
{
    audioDriverT driver;
    AudioUnit outputUnit;
    AudioStreamBasicDescription format;
    audioFuncT callback;
    void *data;
    unsigned int bufferSize;
    unsigned int bufferCount;
    float **buffers;
    double phase;
} coreAudioDriverT;


OSStatus coreAudioCallback(void *data,
                                   AudioUnitRenderActionFlags *ioActionFlags,
                                   const AudioTimeStamp *inTimeStamp,
                                   UInt32 inBusNumber,
                                   UInt32 inNumberFrames,
                                   AudioBufferList *ioData);


/**************************************************************
 *
 *        CoreAudio audio driver
 *
 */

#define OK(x) (x == noErr)

#if __MAC_OS_X_VERSION_MAX_ALLOWED < 120000
#define kAudioObjectPropertyElementMain (kAudioObjectPropertyElementMaster)
#endif

int
getNumOutputs(AudioDeviceID deviceID)
{
    int i, total = 0;
    UInt32 size;
    AudioObjectPropertyAddress pa;
    pa.mSelector = kAudioDevicePropertyStreamConfiguration;
    pa.mScope = kAudioDevicePropertyScopeOutput;
    pa.mElement = kAudioObjectPropertyElementMain;

    if(OK(AudioObjectGetPropertyDataSize(deviceID, &pa, 0, 0, &size)) && size > 0)
    {
        AudioBufferList *bufList = FLUID_MALLOC(size);

        if(bufList == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Out of memory");
            return 0;
        }

        if(OK(AudioObjectGetPropertyData(deviceID, &pa, 0, 0, &size, bufList)))
        {
            int numStreams = bufList->mNumberBuffers;

            for(i = 0; i < numStreams; ++i)
            {
                AudioBuffer b = bufList->mBuffers[i];
                total += b.mNumberChannels;
            }
        }

        FLUID_FREE(bufList);
    }

    return total;
}

void
setChannelMap(AudioUnit outputUnit, int audioChannels, const char *mapString)
{
    OSStatus status;
    long int numberOfChannels;
    int i, *channelMap;
    UInt32 propertySize;
    Boolean writable = false;

    status = AudioUnitGetPropertyInfo(outputUnit,
                                      kAudioOutputUnitProperty_ChannelMap,
                                      kAudioUnitScope_Output,
                                      0,
                                      &propertySize, &writable);
    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to get the channel map size. Status=%ld\n", (long int) status);
        return;
    }

    numberOfChannels = propertySize / sizeof(int);
    if(!numberOfChannels)
    {
        return;
    }

    channelMap = FLUID_ARRAY(int, numberOfChannels);
    if(channelMap == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory.\n");
        return;
    }

    FLUID_MEMSET(channelMap, 0xff, propertySize);

    status = AudioUnitGetProperty(outputUnit,
                                  kAudioOutputUnitProperty_ChannelMap,
                                  kAudioUnitScope_Output,
                                  0,
                                  channelMap, &propertySize);
    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to get the existing channel map. Status=%ld\n", (long int) status);
        FLUID_FREE(channelMap);
        return;
    }

    settingsSplitCsv(mapString, channelMap, (int) numberOfChannels);
    for(i = 0; i < numberOfChannels; i++)
    {
        if(channelMap[i] < -1 || channelMap[i] >= audioChannels)
        {
            FLUID_LOG(FLUID_DBG, "Channel map of output channel %d is out-of-range. Silencing.", i);
            channelMap[i] = -1;
        }
    }

    status = AudioUnitSetProperty(outputUnit,
                                  kAudioOutputUnitProperty_ChannelMap,
                                  kAudioUnitScope_Output,
                                  0,
                                  channelMap, propertySize);
    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to set the channel map. Status=%ld\n", (long int) status);
    }

    FLUID_FREE(channelMap);
}

void
coreAudioDriverSettings(FluidSettings *settings)
{
    int i;
    UInt32 size;
    AudioObjectPropertyAddress pa;
    pa.mSelector = kAudioHardwarePropertyDevices;
    pa.mScope = kAudioObjectPropertyScopeWildcard;
    pa.mElement = kAudioObjectPropertyElementMain;

    settingsRegisterStr(settings, "audio.coreaudio.device", "default", 0);
    settingsRegisterStr(settings, "audio.coreaudio.channel-map", "", 0);
    settingsAddOption(settings, "audio.coreaudio.device", "default");

    if(OK(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &pa, 0, 0, &size)))
    {
        int num = size / (int) sizeof(AudioDeviceID);
        AudioDeviceID devs [num];

        if(OK(AudioObjectGetPropertyData(kAudioObjectSystemObject, &pa, 0, 0, &size, devs)))
        {
            for(i = 0; i < num; ++i)
            {
                char name [1024];
                size = sizeof(name);
                pa.mSelector = kAudioDevicePropertyDeviceName;

                if(OK(AudioObjectGetPropertyData(devs[i], &pa, 0, 0, &size, name)))
                {
                    if(getNumOutputs(devs[i]) > 0)
                    {
                        settingsAddOption(settings, "audio.coreaudio.device", name);
                    }
                }
            }
        }
    }
}

/*
 * newFluidCoreAudioDriver
 */
audioDriverT *
newFluidCoreAudioDriver(FluidSettings *settings, synthT *synth)
{
    return newFluidCoreAudioDriver2(settings,
                                        NULL,
                                        synth);
}

/*
 * newFluidCoreAudioDriver2
 */
audioDriverT *
newFluidCoreAudioDriver2(FluidSettings *settings, audioFuncT func, void *data)
{
    char *devname = NULL, *channelMap = NULL;
    coreAudioDriverT *dev = NULL;
    int periodSize, periods, audioChannels = 1;
    double sampleRate;
    OSStatus status;
    UInt32 size;
    int i;
#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    ComponentDescription desc;
    Component comp;
#else
    AudioComponentDescription desc;
    AudioComponent comp;
#endif
    AURenderCallbackStruct render;

    dev = FLUID_NEW(coreAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(coreAudioDriverT));

    dev->callback = func;
    dev->data = data;

    // Open the default output unit
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput; //kAudioUnitSubType_DefaultOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;
    desc.componentFlags = 0;
    desc.componentFlagsMask = 0;

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    comp = FindNextComponent(NULL, &desc);
#else
    comp = AudioComponentFindNext(NULL, &desc);
#endif

    if(comp == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Failed to get the default audio device");
        goto errorRecovery;
    }

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    status = OpenAComponent(comp, &dev->outputUnit);
#else
    status = AudioComponentInstanceNew(comp, &dev->outputUnit);
#endif

    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to open the default audio device. Status=%ld\n", (long int)status);
        goto errorRecovery;
    }

    // Set up a callback function to generate output
    render.inputProc = coreAudioCallback;
    render.inputProcRefCon = (void *) dev;
    status = AudioUnitSetProperty(dev->outputUnit,
                                  kAudioUnitProperty_SetRenderCallback,
                                  kAudioUnitScope_Input,
                                  0,
                                  &render,
                                  sizeof(render));

    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Error setting the audio callback. Status=%ld\n", (long int)status);
        goto errorRecovery;
    }

    settingsGetint(settings, "synth.audio-channels", &audioChannels);
    settingsGetnum(settings, "synth.sample-rate", &sampleRate);
    settingsGetint(settings, "audio.periods", &periods);
    settingsGetint(settings, "audio.period-size", &periodSize);

    /* audio channels are in stereo, with a minimum of one pair */
    audioChannels = (audioChannels > 0) ? (2 * audioChannels) : 2;

    /* get the selected device name. if none is specified, use NULL for the default device. */
    if(settingsDupstr(settings, "audio.coreaudio.device", &devname) == FLUID_OK   /* alloc device name */
            && devname && strlen(devname) > 0)
    {
        AudioObjectPropertyAddress pa;
        pa.mSelector = kAudioHardwarePropertyDevices;
        pa.mScope = kAudioObjectPropertyScopeWildcard;
        pa.mElement = kAudioObjectPropertyElementMain;

        if(OK(AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &pa, 0, 0, &size)))
        {
            int num = size / (int) sizeof(AudioDeviceID);
            AudioDeviceID devs [num];

            if(OK(AudioObjectGetPropertyData(kAudioObjectSystemObject, &pa, 0, 0, &size, devs)))
            {
                for(i = 0; i < num; ++i)
                {
                    char name [1024];
                    size = sizeof(name);
                    pa.mSelector = kAudioDevicePropertyDeviceName;

                    if(OK(AudioObjectGetPropertyData(devs[i], &pa, 0, 0, &size, name)))
                    {
                        if(getNumOutputs(devs[i]) > 0 && FLUID_STRCASECMP(devname, name) == 0)
                        {
                            AudioDeviceID selectedID = devs[i];
                            status = AudioUnitSetProperty(dev->outputUnit,
                                                          kAudioOutputUnitProperty_CurrentDevice,
                                                          kAudioUnitScope_Global,
                                                          0,
                                                          &selectedID,
                                                          sizeof(AudioDeviceID));

                            if(status != noErr)
                            {
                                FLUID_LOG(FLUID_ERR, "Error setting the selected output device. Status=%ld\n", (long int)status);
                                goto errorRecovery;
                            }
                        }
                    }
                }
            }
        }
    }

    FLUID_FREE(devname);  /* free device name */

    dev->bufferSize = periodSize * periods;

    // The DefaultOutputUnit should do any format conversions
    // necessary from our format to the device's format.
    dev->format.mSampleRate = sampleRate; // sample rate of the audio stream
    dev->format.mFormatID = kAudioFormatLinearPCM; // encoding type of the audio stream
    dev->format.mFormatFlags = kLinearPCMFormatFlagIsFloat | kAudioFormatFlagIsNonInterleaved;
    dev->format.mBytesPerPacket = sizeof(float);
    dev->format.mFramesPerPacket = 1;
    dev->format.mBytesPerFrame = sizeof(float);
    dev->format.mChannelsPerFrame = audioChannels;
    dev->format.mBitsPerChannel = 8 * sizeof(float);

    FLUID_LOG(FLUID_DBG, "mSampleRate %g", dev->format.mSampleRate);
    FLUID_LOG(FLUID_DBG, "mFormatFlags %08X", dev->format.mFormatFlags);
    FLUID_LOG(FLUID_DBG, "mBytesPerPacket %d", dev->format.mBytesPerPacket);
    FLUID_LOG(FLUID_DBG, "mFramesPerPacket %d", dev->format.mFramesPerPacket);
    FLUID_LOG(FLUID_DBG, "mChannelsPerFrame %d", dev->format.mChannelsPerFrame);
    FLUID_LOG(FLUID_DBG, "mBytesPerFrame %d", dev->format.mBytesPerFrame);
    FLUID_LOG(FLUID_DBG, "mBitsPerChannel %d", dev->format.mBitsPerChannel);

    status = AudioUnitSetProperty(dev->outputUnit,
                                  kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Input,
                                  0,
                                  &dev->format,
                                  sizeof(AudioStreamBasicDescription));

    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Error setting the audio format. Status=%ld\n", (long int)status);
        goto errorRecovery;
    }

    if(settingsDupstr(settings, "audio.coreaudio.channel-map", &channelMap) == FLUID_OK   /* alloc channel map */
            && channelMap && strlen(channelMap) > 0)
    {
        setChannelMap(dev->outputUnit, audioChannels, channelMap);
    }
    FLUID_FREE(channelMap);  /* free channel map */

    status = AudioUnitSetProperty(dev->outputUnit,
                                  kAudioUnitProperty_MaximumFramesPerSlice,
                                  kAudioUnitScope_Input,
                                  0,
                                  &dev->bufferSize,
                                  sizeof(unsigned int));

    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to set the MaximumFramesPerSlice. Status=%ld\n", (long int)status);
        goto errorRecovery;
    }

    FLUID_LOG(FLUID_DBG, "MaximumFramesPerSlice = %d", dev->bufferSize);

    dev->buffers = FLUID_ARRAY(float *, audioChannels);

    if(dev->buffers == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory.");
        goto errorRecovery;
    }

    dev->bufferCount = (unsigned int) audioChannels;

    // Initialize the audio unit
    status = AudioUnitInitialize(dev->outputUnit);

    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Error calling AudioUnitInitialize(). Status=%ld\n", (long int)status);
        goto errorRecovery;
    }

    // Start the rendering
    status = AudioOutputUnitStart(dev->outputUnit);

    if(status != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Error calling AudioOutputUnitStart(). Status=%ld\n", (long int)status);
        goto errorRecovery;
    }

    return (audioDriverT *) dev;

errorRecovery:

    deleteFluidCoreAudioDriver((audioDriverT *) dev);
    return NULL;
}

/*
 * deleteFluidCoreAudioDriver
 */
void
deleteFluidCoreAudioDriver(audioDriverT *p)
{
    coreAudioDriverT *dev = (coreAudioDriverT *) p;
    returnIfFail(dev != NULL);

#if MAC_OS_X_VERSION_MIN_REQUIRED < 1060
    CloseComponent(dev->outputUnit);
#else
    AudioComponentInstanceDispose(dev->outputUnit);
#endif

    if(dev->buffers != NULL)
    {
        FLUID_FREE(dev->buffers);
    }

    FLUID_FREE(dev);
}

OSStatus
coreAudioCallback(void *data,
                          AudioUnitRenderActionFlags *ioActionFlags,
                          const AudioTimeStamp *inTimeStamp,
                          UInt32 inBusNumber,
                          UInt32 inNumberFrames,
                          AudioBufferList *ioData)
{
    coreAudioDriverT *dev = (coreAudioDriverT *) data;
    int len = inNumberFrames;
    UInt32 i, nBuffers = ioData->mNumberBuffers;
    audioFuncT callback = (dev->callback != NULL) ? dev->callback : (audioFuncT) synthProcess;

    for(i = 0; i < ioData->mNumberBuffers && i < dev->bufferCount; i++)
    {
        dev->buffers[i] = ioData->mBuffers[i].mData;
        FLUID_MEMSET(dev->buffers[i], 0, len * sizeof(float));
    }

    callback(dev->data, len, nBuffers, dev->buffers, nBuffers, dev->buffers);

    return noErr;
}


#endif /* #if COREAUDIO_SUPPORT */
