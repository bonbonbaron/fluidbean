/* Synth - A Software Synthesizer
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

#ifndef _MDRIVER_H
#define _MDRIVER_H

#include "sys.h"
#include "midi.h"

/* * MidiDriverT */

typedef struct _MdriverDefinitionT MdriverDefinitionT;

typedef struct _MidiDriverT {
    const MdriverDefinitionT *define;
    handleMidiEventFuncT handler;
    void *data;
} MidiDriverT;

/* ALSA */
#if ALSA_SUPPORT
MidiDriverT *newAlsaRawmidiDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteAlsaRawmidiDriver(MidiDriverT *p);
void AlsaRawmidiDriverSettings(Settings *settings);

MidiDriverT *newAlsaSeqDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteAlsaSeqDriver(MidiDriverT *p);
void AlsaSeqDriverSettings(Settings *settings);
#endif

/* JACK */
#if JACK_SUPPORT
void JackMidiDriverSettings(Settings *settings);
MidiDriverT *newJackMidiDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *data);
void deleteJackMidiDriver(MidiDriverT *p);
#endif

/* OSS */
#if OSS_SUPPORT
MidiDriverT *newOssMidiDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteOssMidiDriver(MidiDriverT *p);
void OssMidiDriverSettings(Settings *settings);
#endif

/* Windows MIDI service */
#if WINMIDI_SUPPORT
MidiDriverT *newWinmidiDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteWinmidiDriver(MidiDriverT *p);
void WinmidiMidiDriverSettings(Settings *settings);
#endif

/* definitions for the MidiShare driver */
#if MIDISHARE_SUPPORT
MidiDriverT *newMidishareMidiDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteMidishareMidiDriver(MidiDriverT *p);
#endif

/* definitions for the CoreMidi driver */
#if COREMIDI_SUPPORT
MidiDriverT *newCoremidiDriver(Settings *settings,
        handleMidiEventFuncT handler,
        void *eventHandlerData);
void deleteCoremidiDriver(MidiDriverT *p);
void CoremidiDriverSettings(Settings *settings);
#endif

#endif  /* _AUDRIVER_H */
