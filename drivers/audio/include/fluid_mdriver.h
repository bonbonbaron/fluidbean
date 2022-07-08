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

#ifndef _FLUID_MDRIVER_H
#define _FLUID_MDRIVER_H

#include "fluidSys.h"
#include "midi.h"

/*
 * fluidMidiDriverT
 */

typedef struct _fluidMdriverDefinitionT fluidMdriverDefinitionT;

struct _fluidMidiDriverT
{
    const fluidMdriverDefinitionT *define;
    handleMidiEventFuncT handler;
    void *data;
};

void fluidMidiDriverSettings(FluidSettings *settings);

/* ALSA */
#if ALSA_SUPPORT
fluidMidiDriverT *newFluidAlsaRawmidiDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteFluidAlsaRawmidiDriver(fluidMidiDriverT *p);
void fluidAlsaRawmidiDriverSettings(FluidSettings *settings);

fluidMidiDriverT *newFluidAlsaSeqDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteFluidAlsaSeqDriver(fluidMidiDriverT *p);
void fluidAlsaSeqDriverSettings(FluidSettings *settings);
#endif

/* JACK */
#if JACK_SUPPORT
void fluidJackMidiDriverSettings(FluidSettings *settings);
fluidMidiDriverT *newFluidJackMidiDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *data);
void deleteFluidJackMidiDriver(fluidMidiDriverT *p);
#endif

/* OSS */
#if OSS_SUPPORT
fluidMidiDriverT *newFluidOssMidiDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteFluidOssMidiDriver(fluidMidiDriverT *p);
void fluidOssMidiDriverSettings(FluidSettings *settings);
#endif

/* Windows MIDI service */
#if WINMIDI_SUPPORT
fluidMidiDriverT *newFluidWinmidiDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteFluidWinmidiDriver(fluidMidiDriverT *p);
void fluidWinmidiMidiDriverSettings(FluidSettings *settings);
#endif

/* definitions for the MidiShare driver */
#if MIDISHARE_SUPPORT
fluidMidiDriverT *newFluidMidishareMidiDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteFluidMidishareMidiDriver(fluidMidiDriverT *p);
#endif

/* definitions for the CoreMidi driver */
#if COREMIDI_SUPPORT
fluidMidiDriverT *newFluidCoremidiDriver(FluidSettings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteFluidCoremidiDriver(fluidMidiDriverT *p);
void fluidCoremidiDriverSettings(FluidSettings *settings);
#endif

#endif  /* _FLUID_AUDRIVER_H */
