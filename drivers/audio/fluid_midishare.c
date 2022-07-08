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


/* midishare.c
 *
 * Author: Stephane Letz  (letz@grame.fr)  Grame
 *
 * Interface to Grame's MidiShare drivers (www.grame.fr/MidiShare)
 * 21/12/01 : Add a compilation flag (MIDISHARE_DRIVER) for driver or application mode
 * 29/01/02 : Compilation on MacOSX, use a task for typeNote management
 * 03/06/03 : Adapdation for FluidSynth API
 * 18/03/04 : In application mode, connect MidiShare to the fluidsynth client (midishareOpenAppl)
 */

#include "config.h"

#if MIDISHARE_SUPPORT

#include "midi.h"
#include "mdriver.h"
#include <MidiShare.h>

/* constants definitions    */
#define MidiShareDrvRef		127

#if defined(MACINTOSH) && defined(MACOS9)
#define MSHSlotName		"\pfluidsynth"
#define MSHDriverName	"\pfluidsynth"
#else
#define MSHSlotName		"fluidsynth"
#define MSHDriverName	"fluidsynth"
#endif

typedef struct
{
    midiDriverT driver;
    int status;
    short refnum;
    MidiFilterPtr filter;
#if defined(MACINTOSH) && defined(MACOS9)
    UPPRcvAlarmPtr uppAlarmPtr;
    UPPDriverPtr   uppWakeupPtr;
    UPPDriverPtr   uppSleepPtr;
    UPPTaskPtr     uppTaskPtr;
#endif
    SlotRefNum	slotRef;
    unsigned char sysexbuf[FLUID_MIDI_PARSER_MAX_DATA_SIZE];
} midishareMidiDriverT;


static void midishareMidiDriverReceive(short ref);

#if defined(MIDISHARE_DRIVER)
static int midishareOpenDriver(midishareMidiDriverT *dev);
static void midishareCloseDriver(midishareMidiDriverT *dev);
#else
static int midishareOpenAppl(midishareMidiDriverT *dev);
static void midishareCloseAppl(midishareMidiDriverT *dev);
#endif

/*
 * newFluidMidishareMidiDriver
 */
midiDriverT *
newFluidMidishareMidiDriver(FluidSettings *settings,
                                handleMidiEventFuncT handler,
                                void *data)
{
    midishareMidiDriverT *dev;
    int i;

    FLUID_LOG(FLUID_WARN,
              "\n\n"
              "================ MidiShare MIDI driver has been deprecated! =================\n"
              "You're using the MidiShare driver. This driver is old, unmaintained and believed\n"
              "to be unused. If you still need it, pls. let us know by posting to our\n"
              "mailing list at fluid-dev@nongnu.org - otherwise this driver might be removed\n"
              "in a future release of FluidSynth!\n"
              "================ MidiShare MIDI driver has been deprecated! =================\n"
              "\n"
    );

    /* not much use doing anything */
    if(handler == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Invalid argument");
        return NULL;
    }

    /* allocate the device */
    dev = FLUID_NEW(midishareMidiDriverT);

    if(dev == NULL)
    {
        FLUID_LOG(FLUID_ERR, "Out of memory");
        return NULL;
    }

    FLUID_MEMSET(dev, 0, sizeof(midishareMidiDriverT));
    dev->driver.handler = handler;
    dev->driver.data = data;

    /* register to MidiShare as Application or Driver */
#if defined(MIDISHARE_DRIVER)

    if(!midishareOpenDriver(dev))
    {
        goto errorRecovery;
    }

#else

    if(!midishareOpenAppl(dev))
    {
        goto errorRecovery;
    }

#endif

    /*MidiSetInfo(dev->refnum, dev->router->synth); */
    MidiSetInfo(dev->refnum, dev);
    dev->filter = MidiNewFilter();

    if(dev->filter == 0)
    {
        FLUID_LOG(FLUID_ERR, "Can not allocate MidiShare filter");
        goto errorRecovery;
    }

    for(i = 0 ; i < 256; i++)
    {
        MidiAcceptPort(dev->filter, i, 1); /* accept all ports */
        MidiAcceptType(dev->filter, i, 0); /* reject all types */
    }

    for(i = 0 ; i < 16; i++)
    {
        MidiAcceptChan(dev->filter, i, 1); /* accept all chan */
    }

    /* accept only the following types */
    MidiAcceptType(dev->filter, typeNote, 1);
    MidiAcceptType(dev->filter, typeKeyOn, 1);
    MidiAcceptType(dev->filter, typeKeyOff, 1);
    MidiAcceptType(dev->filter, typeCtrlChange, 1);
    MidiAcceptType(dev->filter, typeProgChange, 1);
    MidiAcceptType(dev->filter, typePitchWheel, 1);
    MidiAcceptType(dev->filter, typeSysEx, 1);

    /* set the filter */
    MidiSetFilter(dev->refnum, dev->filter);

    dev->status = FLUID_MIDI_READY;
    return (midiDriverT *) dev;

errorRecovery:
    deleteFluidMidishareMidiDriver((midiDriverT *) dev);
    return NULL;
}

/*
 * deleteFluidMidishareMidiDriver
 */
void deleteFluidMidishareMidiDriver(midiDriverT *p)
{
    midishareMidiDriverT *dev = (midishareMidiDriverT *) p;
    returnIfFail(dev != NULL);

    if(dev->filter)
    {
        MidiFreeFilter(dev->filter);
    }

#if defined(MIDISHARE_DRIVER)
    midishareCloseDriver(dev);
#else
    midishareCloseAppl(dev);
#endif

#if defined(MACINTOSH) && defined(MACOS9)
    DisposeRoutineDescriptor(dev->uppAlarmPtr);
    DisposeRoutineDescriptor(dev->uppWakeupPtr);
    DisposeRoutineDescriptor(dev->uppSleepPtr);
    DisposeRoutineDescriptor(dev->uppTaskPtr);
#endif

    dev->status = FLUID_MIDI_DONE;

    FLUID_FREE(dev);
}


/*
 * midishareKeyoffTask
 */
static void midishareKeyoffTask(long date, short ref, long a1, long a2, long a3)
{
    midishareMidiDriverT *dev = (midishareMidiDriverT *)MidiGetInfo(ref);
    midiEventT newEvent;
    MidiEvPtr e = (MidiEvPtr)a1;

    midiEventSetType(&newEvent, NOTE_OFF);
    midiEventSetChannel(&newEvent, Chan(e));
    midiEventSetPitch(&newEvent, Pitch(e));
    midiEventSetVelocity(&newEvent, Vel(e)); /* release vel */

    /* and send it on its way to the router */
    (*dev->driver.handler)(dev->driver.data, &newEvent);

    MidiFreeEv(e);
}


/*
 * midishareMidiDriverReceive
 */
static void midishareMidiDriverReceive(short ref)
{
    midishareMidiDriverT *dev = (midishareMidiDriverT *)MidiGetInfo(ref);
    midiEventT newEvent;
    MidiEvPtr e;
    int count, i;

    while((e = MidiGetEv(ref)))
    {
        switch(EvType(e))
        {
        case typeNote:
            /* Copy the data to midiEventT */
            midiEventSetType(&newEvent, NOTE_ON);
            midiEventSetChannel(&newEvent, Chan(e));
            midiEventSetPitch(&newEvent, Pitch(e));
            midiEventSetVelocity(&newEvent, Vel(e));

            /* and send it on its way to the router */
            (*dev->driver.handler)(dev->driver.data, &newEvent);

#if defined(MACINTOSH) && defined(MACOS9)
            MidiTask(dev->uppTaskPtr, MidiGetTime() + Dur(e), ref, (long)e, 0, 0);
#else
            MidiTask(midishareKeyoffTask, MidiGetTime() + Dur(e), ref, (long)e, 0, 0);
#endif

            /* e gets freed in midishareKeyoffTask */
            continue;

        case typeKeyOn:
            /* Copy the data to midiEventT */
            midiEventSetType(&newEvent, NOTE_ON);
            midiEventSetChannel(&newEvent, Chan(e));
            midiEventSetPitch(&newEvent, Pitch(e));
            midiEventSetVelocity(&newEvent, Vel(e));
            break;

        case typeKeyOff:
            /* Copy the data to midiEventT */
            midiEventSetType(&newEvent, NOTE_OFF);
            midiEventSetChannel(&newEvent, Chan(e));
            midiEventSetPitch(&newEvent, Pitch(e));
            midiEventSetVelocity(&newEvent, Vel(e)); /* release vel */
            break;

        case typeCtrlChange:
            /* Copy the data to midiEventT */
            midiEventSetType(&newEvent, CONTROL_CHANGE);
            midiEventSetChannel(&newEvent, Chan(e));
            midiEventSetControl(&newEvent, MidiGetField(e, 0));
            midiEventSetValue(&newEvent, MidiGetField(e, 1));
            break;

        case typeProgChange:
            /* Copy the data to midiEventT */
            midiEventSetType(&newEvent, PROGRAM_CHANGE);
            midiEventSetChannel(&newEvent, Chan(e));
            midiEventSetProgram(&newEvent, MidiGetField(e, 0));
            break;

        case typePitchWheel:
            /* Copy the data to midiEventT */
            midiEventSetType(&newEvent, PITCH_BEND);
            midiEventSetChannel(&newEvent, Chan(e));
            midiEventSetValue(&newEvent, ((MidiGetField(e, 0)
                                                    + (MidiGetField(e, 1) << 7))
                                                    - 8192));
            break;

        case typeSysEx:
            count = MidiCountFields(e);

            /* Discard empty or too large SYSEX messages */
            if(count == 0 || count > FLUID_MIDI_PARSER_MAX_DATA_SIZE)
            {
                MidiFreeEv(e);
                continue;
            }

            /* Copy SYSEX data, one byte at a time */
            for(i = 0; i < count; i++)
            {
                dev->sysexbuf[i] = MidiGetField(e, i);
            }

            midiEventSetSysex(&newEvent, dev->sysexbuf, count, FALSE);
            break;

        default:
            MidiFreeEv(e);
            continue;
        }

        MidiFreeEv(e);

        /* Send the MIDI event */
        (*dev->driver.handler)(dev->driver.data, &newEvent);
    }
}


#if defined(MIDISHARE_DRIVER)

/*
 * midishareWakeup
 */
static void midishareWakeup(short r)
{
    MidiConnect(MidiShareDrvRef, r, true);
    MidiConnect(r, MidiShareDrvRef, true);
}

/*
 * midishareSleep
 */
static void midishareSleep(short r) {}

/*
 * midishareOpenDriver
 */
static int midishareOpenDriver(midishareMidiDriverT *dev)
{
    /* gcc wanted me to use {0,0} to initialize the reserved[2] fields */
    TDriverInfos infos = { MSHDriverName, 100, 0, { 0, 0 } };
    TDriverOperation op = { midishareWakeup, midishareSleep, { 0, 0, 0 } };

    /* register to MidiShare */
#if defined(MACINTOSH) && defined(MACOS9)
    dev->uppWakeupPtr = NewDriverPtr(midishareWakeup);
    dev->uppSleepPtr = NewDriverPtr(midishareSleep);

    op.wakeup = (WakeupPtr)dev->uppWakeupPtr;
    op.sleep = (SleepPtr)dev->uppSleepPtr;

    dev->refnum = MidiRegisterDriver(&infos, &op);

    if(dev->refnum < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can not open MidiShare Application client");
        return 0;
    }

    dev->slotRef = MidiAddSlot(dev->refnum, MSHSlotName, MidiOutputSlot);
    dev->uppAlarmPtr = NewRcvAlarmPtr(midishareMidiDriverReceive);
    dev->uppTaskPtr = NewTaskPtr(midishareKeyoffTask);
    MidiSetRcvAlarm(dev->refnum, dev->uppAlarmPtr);
#else
    dev->refnum = MidiRegisterDriver(&infos, &op);

    if(dev->refnum < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can not open MidiShare Application client");
        return 0;
    }

    dev->slotRef = MidiAddSlot(dev->refnum, MSHSlotName, MidiOutputSlot);
    MidiSetRcvAlarm(dev->refnum, midishareMidiDriverReceive);
#endif
    return 1;
}

/*
 * midishareCloseDriver
 */
static void midishareCloseDriver(midishareMidiDriverT *dev)
{
    if(dev->refnum > 0)
    {
        MidiUnregisterDriver(dev->refnum);
    }
}

#else   /* #if defined(MIDISHARE_DRIVER) */

/*
 * midishareOpenAppl
 */
static int midishareOpenAppl(midishareMidiDriverT *dev)
{
    /* register to MidiShare */
#if defined(MACINTOSH) && defined(MACOS9)
    dev->refnum = MidiOpen(MSHDriverName);

    if(dev->refnum < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can not open MidiShare Driver client");
        return 0;
    }

    dev->uppAlarmPtr = NewRcvAlarmPtr(midishareMidiDriverReceive);
    dev->uppTaskPtr = NewTaskPtr(midishareKeyoffTask);
    MidiSetRcvAlarm(dev->refnum, dev->uppAlarmPtr);
#else
    dev->refnum = MidiOpen(MSHDriverName);

    if(dev->refnum < 0)
    {
        FLUID_LOG(FLUID_ERR, "Can not open MidiShare Driver client");
        return 0;
    }

    MidiSetRcvAlarm(dev->refnum, midishareMidiDriverReceive);
    MidiConnect(0, dev->refnum, true);
#endif
    return 1;
}

/*
 * midishareCloseAppl
 */
static void midishareCloseAppl(midishareMidiDriverT *dev)
{
    if(dev->refnum > 0)
    {
        MidiClose(dev->refnum);
    }
}

#endif  /* #if defined(MIDISHARE_DRIVER) */

#endif /* MIDISHARE_SUPPORT */
