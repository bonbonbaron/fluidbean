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

/* coremidi.c
 *
 * Driver for Mac OSX native MIDI
 * Pedro Lopez-Cabanillas, Jan 2009
 */

#include "fluidsynthPriv.h"

#if COREMIDI_SUPPORT

#include "midi.h"
#include "mdriver.h"
#include "settings.h"

/* Work around for OSX 10.4 */

/* enum definition in OpenTransportProviders.h defines these tokens
   which are #defined from <netinet/tcp.h> */
#ifdef TCP_NODELAY
#undef TCP_NODELAY
#endif
#ifdef TCP_MAXSEG
#undef TCP_MAXSEG
#endif
#ifdef TCP_KEEPALIVE
#undef TCP_KEEPALIVE
#endif

/* End work around */

#include <unistd.h>
#include <os/log.h>
#include <CoreServices/CoreServices.h>
#include <CoreMIDI/MIDIServices.h>

typedef struct
{
    midiDriverT driver;
    MIDIClientRef client;
    MIDIEndpointRef endpoint;
    MIDIPortRef inputPort;
    midiParserT *parser;
    int autoconnInputs;
} coremidiDriverT;

static const MIDIClientRef invalidClient = (MIDIClientRef)-1;
static const MIDIEndpointRef invalidEndpoint = (MIDIEndpointRef)-1;
static const MIDIPortRef invalidPort = (MIDIPortRef)-1;

void coremidiCallback(const MIDIPacketList *list, void *p, void *src);

void coremidiDriverSettings(FluidSettings *settings)
{
    settingsRegisterStr(settings, "midi.coremidi.id", "pid", 0);
}

static void coremidiAutoconnect(coremidiDriverT *dev, MIDIPortRef inputPort)
{
    int i;
    int sourceCount = MIDIGetNumberOfSources();
    for(i = 0; i < sourceCount; ++i)
    {
        MIDIEndpointRef source = MIDIGetSource(i);

        CFStringRef externalName;
        OSStatus result = MIDIObjectGetStringProperty(source, kMIDIPropertyName, &externalName);
        const char *sourceName = CFStringGetCStringPtr(externalName, kCFStringEncodingASCII);
        CFRelease(externalName);

        result = MIDIPortConnectSource(inputPort, source, NULL);
        if(result != noErr)
        {
            FLUID_LOG(FLUID_ERR, "Failed to connect \"%s\" device to input port.", sourceName);
        }
        else
        {
            FLUID_LOG(FLUID_DBG, "Connected input port to \"%s\".", sourceName);
        }
    }
}

/*
 * newFluidCoremidiDriver
 */
midiDriverT *
newFluidCoremidiDriver(FluidSettings *settings, handleMidiEventFuncT handler, void *data)
{
    coremidiDriverT *dev;
    MIDIClientRef client;
    MIDIEndpointRef endpoint;
    char clientid[32];
    char *portname;
    char *id;
    CFStringRef strPortname;
    CFStringRef strClientname;
    OSStatus result;
    CFStringRef strInputPortname;

    /* not much use doing anything */
    if(handler == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Invalid argument");
        return NULL;
    }

    dev = FLUID_MALLOC(sizeof(coremidiDriverT));

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    dev->client = invalidClient;
    dev->endpoint = invalidEndpoint;
    dev->inputPort = invalidPort;
    dev->parser = NULL;
    dev->driver.handler = handler;
    dev->driver.data = data;

    dev->parser = newFluidMidiParser();

    if(dev->parser == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        goto errorRecovery;
    }

    settingsDupstr(settings, "midi.coremidi.id", &id);     /* ++ alloc id string */
    memset(clientid, 0, sizeof(clientid));

    if(id != NULL)
    {
        if(FLUID_STRCMP(id, "pid") == 0)
        {
            FLUID_SNPRINTF(clientid, sizeof(clientid), " (%d)", getpid());
        }
        else
        {
            FLUID_SNPRINTF(clientid, sizeof(clientid), " (%s)", id);
        }

        FLUID_FREE(id);   /* -- free id string */
    }

    strClientname = CFStringCreateWithFormat(NULL, NULL,
                     CFSTR("FluidSynth%s"), clientid);

    settingsDupstr(settings, "midi.portname", &portname);  /* ++ alloc port name */

    if(!portname || strlen(portname) == 0)
        strPortname = CFStringCreateWithFormat(NULL, NULL,
                                                CFSTR("FluidSynth virtual port%s"),
                                                clientid);
    else
        strPortname = CFStringCreateWithCString(NULL, portname,
                       kCFStringEncodingASCII);

    if(portname)
    {
        FLUID_FREE(portname);    /* -- free port name */
    }

    result = MIDIClientCreate(strClientname, NULL, NULL, &client);
    CFRelease(strClientname);

    if(result != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create the MIDI input client");
        goto errorRecovery;
    }

    dev->client = client;

    result = MIDIDestinationCreate(client, strPortname,
                                   coremidiCallback,
                                   (void *)dev, &endpoint);

    if(result != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create the MIDI input port. MIDI input not available.");
        goto errorRecovery;
    }

    strInputPortname = CFSTR("input");
    result = MIDIInputPortCreate(client, strInputPortname,
                                 coremidiCallback,
                                 (void *)dev, &dev->inputPort);
    CFRelease(strInputPortname);

    if(result != noErr)
    {
        FLUID_LOG(FLUID_ERR, "Failed to create input port.");
        goto errorRecovery;
    }

    settingsGetint(settings, "midi.autoconnect", &dev->autoconnInputs);

    if(dev->autoconnInputs)
    {
        coremidiAutoconnect(dev, dev->inputPort);
    }

    dev->endpoint = endpoint;

    return (midiDriverT *) dev;

errorRecovery:
    deleteFluidCoremidiDriver((midiDriverT *) dev);
    return NULL;
}

/*
 * deleteFluidCoremidiDriver
 */
void
deleteFluidCoremidiDriver(midiDriverT *p)
{
    coremidiDriverT *dev = (coremidiDriverT *) p;
    returnIfFail(dev != NULL);

    if(dev->inputPort != invalidPort)
    {
        MIDIPortDispose(dev->inputPort);
    }

    if(dev->client != invalidClient)
    {
        MIDIClientDispose(dev->client);
    }

    if(dev->endpoint != invalidEndpoint)
    {
        MIDIEndpointDispose(dev->endpoint);
    }

    if(dev->parser != NULL)
    {
        deleteFluidMidiParser(dev->parser);
    }

    FLUID_FREE(dev);
}

void
coremidiCallback(const MIDIPacketList *list, void *p, void *src)
{
    unsigned int i, j;
    midiEventT *event;
    coremidiDriverT *dev = (coremidiDriverT *)p;
    const MIDIPacket *packet = &list->packet[0];

    for(i = 0; i < list->numPackets; ++i)
    {
        for(j = 0; j < packet->length; ++j)
        {
            event = midiParserParse(dev->parser, packet->data[j]);

            if(event != NULL)
            {
                (*dev->driver.handler)(dev->driver.data, event);
            }
        }

        packet = MIDIPacketNext(packet);
    }
}

#endif /* COREMIDI_SUPPORT */
