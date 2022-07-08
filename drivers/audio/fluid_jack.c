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


/* jack.c
 *
 * Driver for the JACK
 *
 * This code is derived from the simpleClient example in the JACK
 * source distribution. Many thanks to Paul Davis.
 *
 */

#include "synth.h"
#include "adriver.h"
#include "mdriver.h"
#include "settings.h"

#if JACK_SUPPORT

#include <jack/jack.h>
#include <jack/midiport.h>

#include "lash.h"


typedef struct _fluidJackAudioDriverT jackAudioDriverT;
typedef struct _fluidJackMidiDriverT jackMidiDriverT;


/* Clients are shared for drivers using the same server. */
typedef struct
{
    jackClientT *client;
    char *server;                 /* Jack server name used */
    jackAudioDriverT *audioDriver;
    jackMidiDriverT *midiDriver;
} jackClientT;

/* Jack audio driver instance */
struct _fluidJackAudioDriverT
{
    audioDriverT driver;
    jackClientT *clientRef;

    jackPortT **outputPorts;
    int numOutputPorts;
    float **outputBufs;

    jackPortT **fxPorts;
    int numFxPorts;
    float **fxBufs;

    audioFuncT callback;
    void *data;
};

/* Jack MIDI driver instance */
struct _fluidJackMidiDriverT
{
    midiDriverT driver;
    jackClientT *clientRef;
    int midiPortCount;
    jackPortT **midiPort; // array of midi port handles
    midiParserT *parser;
    int autoconnectInputs;
    atomicIntT autoconnectIsOutdated;
};

static jackClientT *newFluidJackClient(FluidSettings *settings,
        int isaudio, void *driver);
static int jackClientRegisterPorts(void *driver, int isaudio,
        jackClientT *client,
        FluidSettings *settings);

void jackDriverShutdown(void *arg);
int jackDriverSrate(jackNframesT nframes, void *arg);
int jackDriverBufsize(jackNframesT nframes, void *arg);
int jackDriverProcess(jackNframesT nframes, void *arg);
void jackPortRegistration(jackPortIdT port, int isRegistering, void *arg);

static mutexT lastClientMutex = FLUID_MUTEX_INIT;     /* Probably not necessary, but just in case drivers are created by multiple threads */
static jackClientT *lastClient = NULL;       /* Last unpaired client. For audio/MIDI driver pairing. */


void
jackAudioDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "audio.jack.id", "fluidsynth", 0);
    settingsRegisterInt(settings, "audio.jack.multi", 0, 0, 1, FLUID_HINT_TOGGLED);
    settingsRegisterInt(settings, "audio.jack.autoconnect", 0, 0, 1, FLUID_HINT_TOGGLED);
    settingsRegisterStr(settings, "audio.jack.server", "", 0);
}

/*
 * Connect all midi input ports to all terminal midi output ports
 */
void
jackMidiAutoconnect(jackClientT *client, jackMidiDriverT *midiDriver)
{
    int i, j;
    const char **midiSourcePorts;

    midiSourcePorts = jackGetPorts(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput | JackPortIsTerminal);

    if(midiSourcePorts != NULL)
    {
        for(j = 0; midiSourcePorts[j] != NULL; j++)
        {
            for(i = 0; i < midiDriver->midiPortCount; i++)
            {
                FLUID_LOG(FLUID_INFO, "jack midi autoconnect \"%s\" to \"%s\"", midiSourcePorts[j], jackPortName(midiDriver->midiPort[i]));
                jackConnect(client, midiSourcePorts[j], jackPortName(midiDriver->midiPort[i]));
            }
        }

        jackFree(midiSourcePorts);
    }

    atomicIntSet(&midiDriver->autoconnectIsOutdated, FALSE);
}

/*
 * Create Jack client as necessary, share clients of the same server.
 * @param settings Settings object
 * @param isaudio TRUE if audio driver, FALSE if MIDI
 * @param driver jackAudioDriverT or jackMidiDriverT
 * @param data The user data instance associated with the driver (synthT for example)
 * @return New or paired Audio/MIDI Jack client
 */
static jackClientT *
newFluidJackClient(FluidSettings *settings, int isaudio, void *driver)
{
    jackClientT *clientRef = NULL;
    char *server = NULL;
    char *clientName;
    char name[64];

    if(settingsDupstr(settings, isaudio ? "audio.jack.server"          /* ++ alloc server name */
                             : "midi.jack.server", &server) != FLUID_OK)
    {
        return NULL;
    }

    mutexLock(lastClientMutex);      /* ++ lock lastClient */

    /* If the last client uses the same server and is not the same type (audio or MIDI),
     * then re-use the client. */
    if(lastClient &&
            (lastClient->server != NULL && server != NULL && FLUID_STRCMP(lastClient->server, server) == 0) &&
            ((!isaudio && lastClient->midiDriver == NULL) || (isaudio && lastClient->audioDriver == NULL)))
    {
        clientRef = lastClient;

        /* Register ports */
        if(jackClientRegisterPorts(driver, isaudio, clientRef->client, settings) == FLUID_OK)
        {
            lastClient = NULL; /* No more pairing for this client */

            if(isaudio)
            {
                atomicPointerSet(&clientRef->audioDriver, driver);
            }
            else
            {
                atomicPointerSet(&clientRef->midiDriver, driver);
            }
        }
        else
        {
            // do not free clientRef and do not goto errorRecovery
            // clientRef is being used by another audio or midi driver. Freeing it here will lead to a double free.
            clientRef = NULL;
        }

        mutexUnlock(lastClientMutex);        /* -- unlock lastClient */

        if(server)
        {
            FLUID_FREE(server);
        }

        return clientRef;
    }

    /* No existing client for this Jack server */
    clientRef = FLUID_NEW(jackClientT);

    if(!clientRef)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        goto errorRecovery;
    }

    FLUID_MEMSET(clientRef, 0, sizeof(jackClientT));

    settingsDupstr(settings, isaudio ? "audio.jack.id"    /* ++ alloc client name */
                          : "midi.jack.id", &clientName);

    if(clientName != NULL && clientName[0] != 0)
    {
        FLUID_SNPRINTF(name, sizeof(name), "%s", clientName);
    }
    else
    {
        FLUID_STRNCPY(name, "fluidsynth", sizeof(name));
    }

    name[63] = '\0';

    if(clientName)
    {
        FLUID_FREE(clientName);    /* -- free client name */
    }

    /* Open a connection to the Jack server and use the server name if specified */
    if(server && server[0] != '\0')
    {
        clientRef->client = jackClientOpen(name, JackServerName, NULL, server);
    }
    else
    {
        clientRef->client = jackClientOpen(name, JackNullOption, NULL);
    }

    if(!clientRef->client)
    {
        FLUID_LOG(FLUID_ERR, "Failed to connect to Jack server.");
        goto errorRecovery;
    }

    jackSetPortRegistrationCallback(clientRef->client, jackPortRegistration, clientRef);
    jackSetProcessCallback(clientRef->client, jackDriverProcess, clientRef);
    jackSetBufferSizeCallback(clientRef->client, jackDriverBufsize, clientRef);
    jackSetSampleRateCallback(clientRef->client, jackDriverSrate, clientRef);
    jackOnShutdown(clientRef->client, jackDriverShutdown, clientRef);

    /* Register ports */
    if(jackClientRegisterPorts(driver, isaudio, clientRef->client, settings) != FLUID_OK)
    {
        goto errorRecovery;
    }

    /* tell the JACK server that we are ready to roll */
    if(jackActivate(clientRef->client))
    {
        FLUID_LOG(FLUID_ERR, "Failed to activate Jack client");
        goto errorRecovery;
    }

    /* tell the lash server our client name */
#ifdef HAVE_LASH
    {
        int enableLash = 0;
        settingsGetint(settings, "lash.enable", &enableLash);

        if(enableLash)
        {
            lashJackClientName(lashClient, name);
        }
    }
#endif /* HAVE_LASH */

    clientRef->server = server;        /* !! takes over allocation */
    server = NULL;      /* Set to NULL so it doesn't get freed below */

    lastClient = clientRef;

    if(isaudio)
    {
        atomicPointerSet(&clientRef->audioDriver, driver);
    }
    else
    {
        atomicPointerSet(&clientRef->midiDriver, driver);
    }

    mutexUnlock(lastClientMutex);        /* -- unlock lastClient */

    if(server)
    {
        FLUID_FREE(server);
    }

    return clientRef;

errorRecovery:

    mutexUnlock(lastClientMutex);        /* -- unlock clients list */

    if(server)
    {
        FLUID_FREE(server);    /* -- free server name */
    }

    if(clientRef)
    {
        if(clientRef->client)
        {
            jackClientClose(clientRef->client);
        }

        FLUID_FREE(clientRef);
    }

    return NULL;
}

static int
jackClientRegisterPorts(void *driver, int isaudio, jackClientT *client,
                                 FluidSettings *settings)
{
    jackAudioDriverT *dev;
    char name[64];
    int multi;
    int i;
    unsigned long jackSrate;
    double sampleRate;

    if(!isaudio)
    {
        jackMidiDriverT *dev = driver;
        int midiChannels, ports;

        settingsGetint(settings, "synth.midi-channels", &midiChannels);
        ports = midiChannels / 16;

        if((dev->midiPort = FLUID_ARRAY(jackPortT *, ports)) == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            return FLUID_FAILED;
        }

        for(i = 0; i < ports; i++)
        {
            FLUID_SNPRINTF(name, sizeof(name), "midi_%02d", i);
            dev->midiPort[i] = jackPortRegister(client, name, JACK_DEFAULT_MIDI_TYPE,
                                                   JackPortIsInput | JackPortIsTerminal, 0);

            if(dev->midiPort[i] == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Failed to create Jack MIDI port '%s'", name);
                FLUID_FREE(dev->midiPort);
                dev->midiPort = NULL;
                return FLUID_FAILED;
            }
        }

        dev->midiPortCount = ports;
        return FLUID_OK;
    }

    dev = driver;

    settingsGetint(settings, "audio.jack.multi", &multi);

    if(!multi)
    {
        /* create the two audio output ports */
        dev->numOutputPorts = 1;
        dev->numFxPorts = 0;

        dev->outputPorts = FLUID_ARRAY(jackPortT *, 2 * dev->numOutputPorts);

        if(dev->outputPorts == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            return FLUID_FAILED;
        }

        dev->outputBufs = FLUID_ARRAY(float *, 2 * dev->numOutputPorts);
        FLUID_MEMSET(dev->outputPorts, 0, 2 * dev->numOutputPorts * sizeof(jackPortT *));

        dev->outputPorts[0]
            = jackPortRegister(client, "left", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        dev->outputPorts[1]
            = jackPortRegister(client, "right", JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

        if(dev->outputPorts[0] == NULL || dev->outputPorts[1] == NULL)
        {
            FLUID_LOG(FLUID_ERR, "Failed to create Jack audio port '%s'",
                      (dev->outputPorts[0] == NULL ? (dev->outputPorts[1] == NULL ? "left & right" : "left") : "right"));
            goto errorRecovery;
        }
    }
    else
    {
        settingsGetint(settings, "synth.audio-channels", &dev->numOutputPorts);

        dev->outputPorts = FLUID_ARRAY(jackPortT *, 2 * dev->numOutputPorts);

        if(dev->outputPorts == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            return FLUID_FAILED;
        }

        dev->outputBufs = FLUID_ARRAY(float *, 2 * dev->numOutputPorts);

        if(dev->outputBufs == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            goto errorRecovery;
        }

        FLUID_MEMSET(dev->outputPorts, 0, 2 * dev->numOutputPorts * sizeof(jackPortT *));

        for(i = 0; i < dev->numOutputPorts; i++)
        {
            sprintf(name, "l_%02d", i);

            if((dev->outputPorts[2 * i]
                    = jackPortRegister(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)) == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Failed to create Jack audio port '%s'", name);
                goto errorRecovery;
            }

            sprintf(name, "r_%02d", i);

            if((dev->outputPorts[2 * i + 1]
                    = jackPortRegister(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)) == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Failed to create Jack audio port '%s'", name);
                goto errorRecovery;
            }
        }

        settingsGetint(settings, "synth.effects-channels", &dev->numFxPorts);
        settingsGetint(settings, "synth.effects-groups", &i);

        dev->numFxPorts *= i;
        dev->fxPorts = FLUID_ARRAY(jackPortT *, 2 * dev->numFxPorts);

        if(dev->fxPorts == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            goto errorRecovery;
        }

        dev->fxBufs = FLUID_ARRAY(float *, 2 * dev->numFxPorts);

        if(dev->fxBufs == NULL)
        {
            FLUID_LOG(FLUID_PANIC, "Out of memory");
            goto errorRecovery;
        }

        FLUID_MEMSET(dev->fxPorts, 0, 2 * dev->numFxPorts * sizeof(jackPortT *));

        for(i = 0; i < dev->numFxPorts; i++)
        {
            sprintf(name, "fxL_%02d", i);

            if((dev->fxPorts[2 * i]
                    = jackPortRegister(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)) == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Failed to create Jack fx audio port '%s'", name);
                goto errorRecovery;
            }

            sprintf(name, "fxR_%02d", i);

            if((dev->fxPorts[2 * i + 1]
                    = jackPortRegister(client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)) == NULL)
            {
                FLUID_LOG(FLUID_ERR, "Failed to create Jack fx audio port '%s'", name);
                goto errorRecovery;
            }
        }
    }

    /* Adjust sample rate to match JACK's */
    jackSrate = jackGetSampleRate(client);
    FLUID_LOG(FLUID_DBG, "Jack engine sample rate: %lu", jackSrate);

    settingsGetnum(settings, "synth.sample-rate", &sampleRate);

    if((unsigned long)sampleRate != jackSrate)
    {
        synthT* synth;
        if(jackObtainSynth(settings, &synth) == FLUID_OK)
        {
            FLUID_LOG(FLUID_INFO, "Jack sample rate mismatch, adjusting."
                  " (synth.sample-rate=%lu, jackd=%lu)", (unsigned long)sampleRate, jackSrate);
            synthSetSampleRateImmediately(synth, jackSrate);
        }
        else
        {
            FLUID_LOG(FLUID_WARN, "Jack sample rate mismatch (synth.sample-rate=%lu, jackd=%lu)"
            " impossible to adjust, because the settings object provided to newFluidAudioDriver2() was not used to create a synth."
            , (unsigned long)sampleRate, jackSrate);
        }
    }

    return FLUID_OK;

errorRecovery:

    FLUID_FREE(dev->outputPorts);
    dev->outputPorts = NULL;
    FLUID_FREE(dev->fxPorts);
    dev->fxPorts = NULL;
    FLUID_FREE(dev->outputBufs);
    dev->outputBufs = NULL;
    FLUID_FREE(dev->fxBufs);
    dev->fxBufs = NULL;
    return FLUID_FAILED;
}

static void
jackClientClose(jackClientT *clientRef, void *driver)
{
    if(clientRef->audioDriver == driver)
    {
        atomicPointerSet(&clientRef->audioDriver, NULL);
    }
    else if(clientRef->midiDriver == driver)
    {
        atomicPointerSet(&clientRef->midiDriver, NULL);
    }

    if(clientRef->audioDriver || clientRef->midiDriver)
    {
        msleep(100);  /* FIXME - Hack to make sure that resources don't get freed while Jack callback is active */
        return;
    }

    mutexLock(lastClientMutex);

    if(clientRef == lastClient)
    {
        lastClient = NULL;
    }

    mutexUnlock(lastClientMutex);

    if(clientRef->client)
    {
        jackClientClose(clientRef->client);
    }

    if(clientRef->server)
    {
        FLUID_FREE(clientRef->server);
    }

    FLUID_FREE(clientRef);
}


audioDriverT *
newFluidJackAudioDriver(FluidSettings *settings, synthT *synth)
{
    return newFluidJackAudioDriver2(settings, NULL, synth);
}

audioDriverT *
newFluidJackAudioDriver2(FluidSettings *settings, audioFuncT func, void *data)
{
    jackAudioDriverT *dev = NULL;
    jackClientT *client;
    const char **jackPorts;      /* for looking up ports */
    int autoconnect = 0;
    int i;

    dev = FLUID_NEW(jackAudioDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(jackAudioDriverT));

    dev->callback = func;
    dev->data = data;

    dev->clientRef = newFluidJackClient(settings, TRUE, dev);

    if(!dev->clientRef)
    {
        FLUID_FREE(dev);
        return NULL;
    }

    client = dev->clientRef->client;

    /* connect the ports. */

    /* find some physical ports and connect to them */
    settingsGetint(settings, "audio.jack.autoconnect", &autoconnect);

    if(autoconnect)
    {
        jackPorts = jackGetPorts(client, NULL, NULL, JackPortIsInput | JackPortIsPhysical);

        if(jackPorts && jackPorts[0])
        {
            int err, o = 0;
            int connected = 0;

            for(i = 0; i < 2 * dev->numOutputPorts; ++i)
            {
                err = jackConnect(client, jackPortName(dev->outputPorts[i]), jackPorts[o++]);

                if(err)
                {
                    FLUID_LOG(FLUID_ERR, "Error connecting jack port");
                }
                else
                {
                    connected++;
                }

                if(!jackPorts[o])
                {
                    o = 0;
                }
            }

            o = 0;
            for(i = 0; i < 2 * dev->numFxPorts; ++i)
            {
                err = jackConnect(client, jackPortName(dev->fxPorts[i]), jackPorts[o++]);

                if(err)
                {
                    FLUID_LOG(FLUID_ERR, "Error connecting jack port");
                }
                else
                {
                    connected++;
                }

                if(!jackPorts[o])
                {
                    o = 0;
                }
            }

            jackFree(jackPorts);   /* free jack ports array (not the port values!) */
        }
        else
        {
            FLUID_LOG(FLUID_WARN, "Could not connect to any physical jack ports; fluidsynth is unconnected");
        }
    }

    return (audioDriverT *) dev;
}

/*
 * deleteFluidJackAudioDriver
 */
void
deleteFluidJackAudioDriver(audioDriverT *p)
{
    jackAudioDriverT *dev = (jackAudioDriverT *) p;
    returnIfFail(dev != NULL);

    if(dev->clientRef != NULL)
    {
        jackClientClose(dev->clientRef, dev);
    }

    FLUID_FREE(dev->outputBufs);
    FLUID_FREE(dev->outputPorts);
    FLUID_FREE(dev->fxBufs);
    FLUID_FREE(dev->fxPorts);
    FLUID_FREE(dev);
}

/* Process function for audio and MIDI Jack drivers */
int
jackDriverProcess(jackNframesT nframes, void *arg)
{
    jackClientT *client = (jackClientT *)arg;
    jackAudioDriverT *audioDriver;
    jackMidiDriverT *midiDriver;
    float *left, *right;
    int i;

    jackMidiEventT midiEvent;
    midiEventT *evt;
    void *midiBuffer;
    jackNframesT eventCount;
    jackNframesT eventIndex;
    unsigned int u;

    /* Process MIDI events first, so that they take effect before audio synthesis */
    midiDriver = atomicPointerGet(&client->midiDriver);

    if(midiDriver)
    {
        if(atomicIntGet(&midiDriver->autoconnectIsOutdated))
        {
            jackMidiAutoconnect(client->client, midiDriver);
        }

        for(i = 0; i < midiDriver->midiPortCount; i++)
        {
            midiBuffer = jackPortGetBuffer(midiDriver->midiPort[i], 0);
            eventCount = jackMidiGetEventCount(midiBuffer);

            for(eventIndex = 0; eventIndex < eventCount; eventIndex++)
            {
                jackMidiEventGet(&midiEvent, midiBuffer, eventIndex);

                /* let the parser convert the data into events */
                for(u = 0; u < midiEvent.size; u++)
                {
                    evt = midiParserParse(midiDriver->parser, midiEvent.buffer[u]);

                    /* send the event to the next link in the chain */
                    if(evt != NULL)
                    {
                        midiEventSetChannel(evt, midiEventGetChannel(evt) + i * 16);
                        midiDriver->driver.handler(midiDriver->driver.data, evt);
                    }
                }
            }
        }
    }

    audioDriver = atomicPointerGet(&client->audioDriver);

    if(audioDriver == NULL)
    {
        // shutting down
        return FLUID_OK;
    }

    if(audioDriver->callback == NULL && audioDriver->numOutputPorts == 1 && audioDriver->numFxPorts == 0)  /* i.e. audio.jack.multi=no */
    {
        left = (float *) jackPortGetBuffer(audioDriver->outputPorts[0], nframes);
        right = (float *) jackPortGetBuffer(audioDriver->outputPorts[1], nframes);

        return synthWriteFloat(audioDriver->data, nframes, left, 0, 1, right, 0, 1);
    }
    else
    {
        int res;
        audioFuncT callback = (audioDriver->callback != NULL) ? audioDriver->callback : (audioFuncT) synthProcess;

        for(i = 0; i < audioDriver->numOutputPorts; i++)
        {
            int k = i * 2;

            audioDriver->outputBufs[k] = (float *)jackPortGetBuffer(audioDriver->outputPorts[k], nframes);
            FLUID_MEMSET(audioDriver->outputBufs[k], 0, nframes * sizeof(float));

            k = 2 * i + 1;
            audioDriver->outputBufs[k] = (float *)jackPortGetBuffer(audioDriver->outputPorts[k], nframes);
            FLUID_MEMSET(audioDriver->outputBufs[k], 0, nframes * sizeof(float));
        }

        for(i = 0; i < audioDriver->numFxPorts; i++)
        {
            int k = i * 2;

            audioDriver->fxBufs[k] = (float *) jackPortGetBuffer(audioDriver->fxPorts[k], nframes);
            FLUID_MEMSET(audioDriver->fxBufs[k], 0, nframes * sizeof(float));

            k = 2 * i + 1;
            audioDriver->fxBufs[k] = (float *) jackPortGetBuffer(audioDriver->fxPorts[k], nframes);
            FLUID_MEMSET(audioDriver->fxBufs[k], 0, nframes * sizeof(float));
        }

        res = callback(audioDriver->data,
                        nframes,
                        audioDriver->numFxPorts * 2,
                        audioDriver->fxBufs,
                        audioDriver->numOutputPorts * 2,
                        audioDriver->outputBufs);
        if(res != FLUID_OK)
        {
            const char *cbFuncName = (audioDriver->callback != NULL) ? "Custom audio callback function" : "synthProcess()";
            FLUID_LOG(FLUID_PANIC, "%s returned an error. As a consequence, fluidsynth will now be removed from Jack's processing loop.", cbFuncName);
        }
        return res;
    }
}

int
jackDriverBufsize(jackNframesT nframes, void *arg)
{
    /*   printf("the maximum buffer size is now %lu\n", nframes); */
    return 0;
}

int
jackDriverSrate(jackNframesT nframes, void *arg)
{
    /*   printf("the sample rate is now %lu/sec\n", nframes); */
    /* FIXME: change the sample rate of the synthesizer! */
    return 0;
}

void
jackDriverShutdown(void *arg)
{
//  jackAudioDriverT* dev = (jackAudioDriverT*) arg;
    FLUID_LOG(FLUID_ERR, "Help! Lost the connection to the JACK server");
    /*   exit (1); */
}

void
jackPortRegistration(jackPortIdT port, int isRegistering, void *arg)
{
    jackClientT *clientRef = (jackClientT *)arg;

    if(clientRef->midiDriver != NULL)
    {
        atomicIntSet(&clientRef->midiDriver->autoconnectIsOutdated, clientRef->midiDriver->autoconnectInputs && isRegistering != 0);
    }
}

void jackMidiDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "midi.jack.id", "fluidsynth-midi", 0);
    settingsRegisterStr(settings, "midi.jack.server", "", 0);
}

/*
 * newFluidJackMidiDriver
 */
midiDriverT *
newFluidJackMidiDriver(FluidSettings *settings,
                           handleMidiEventFuncT handler, void *data)
{
    jackMidiDriverT *dev;

    returnValIfFail(handler != NULL, NULL);

    /* allocate the device */
    dev = FLUID_NEW(jackMidiDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(jackMidiDriverT));

    dev->driver.handler = handler;
    dev->driver.data = data;

    /* allocate one event to store the input data */
    dev->parser = newFluidMidiParser();

    if(dev->parser == NULL)
    {
        FLUID_LOG(FLUID_PANIC, "Out of memory");
        goto errorRecovery;
    }

    settingsGetint(settings, "midi.autoconnect", &dev->autoconnectInputs);
    atomicIntSet(&dev->autoconnectIsOutdated, dev->autoconnectInputs);

    dev->clientRef = newFluidJackClient(settings, FALSE, dev);

    if(!dev->clientRef)
    {
        goto errorRecovery;
    }

    return (midiDriverT *)dev;

errorRecovery:
    deleteFluidJackMidiDriver((midiDriverT *)dev);
    return NULL;
}

void
deleteFluidJackMidiDriver(midiDriverT *p)
{
    jackMidiDriverT *dev = (jackMidiDriverT *)p;
    returnIfFail(dev != NULL);

    if(dev->clientRef != NULL)
    {
        jackClientClose(dev->clientRef, dev);
    }

    deleteFluidMidiParser(dev->parser);
    FLUID_FREE(dev->midiPort);
    FLUID_FREE(dev);
}

int jackObtainSynth(FluidSettings *settings, synthT **synth)
{
    void *data;

    if(!settingsIsRealtime(settings, "synth.gain") ||
       (data = settingsGetUserData(settings, "synth.gain")) == NULL)
    {
        return FLUID_FAILED;
    }

    *synth = data;
    return FLUID_OK;
}

#endif /* JACK_SUPPORT */
