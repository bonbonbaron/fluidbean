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

#include "mdriver.h"
#include "settings.h"


/*
 * mdriverDefinition
 */
struct _MdriverDefinitionT
{
    const char *name;
    midiDriverT *(*new)(Settings *settings,
                                handleMidiEventFuncT eventHandler,
                                void *eventHandlerData);
    void (*free)(midiDriverT *p);
    void (*settings)(Settings *settings);
};


static const mdriverDefinitionT midiDrivers[] =
{
#if ALSA_SUPPORT
    {
        "alsaSeq",
        newAlsaSeqDriver,
        deleteAlsaSeqDriver,
        alsaSeqDriverSettings
    },
    {
        "alsaRaw",
        newAlsaRawmidiDriver,
        deleteAlsaRawmidiDriver,
        alsaRawmidiDriverSettings
    },
#endif
#if JACK_SUPPORT
    {
        "jack",
        newJackMidiDriver,
        deleteJackMidiDriver,
        jackMidiDriverSettings
    },
#endif
#if OSS_SUPPORT
    {
        "oss",
        newOssMidiDriver,
        deleteOssMidiDriver,
        ossMidiDriverSettings
    },
#endif
#if WINMIDI_SUPPORT
    {
        "winmidi",
        newWinmidiDriver,
        deleteWinmidiDriver,
        winmidiMidiDriverSettings
    },
#endif
#if MIDISHARE_SUPPORT
    {
        "midishare",
        newMidishareMidiDriver,
        deleteMidishareMidiDriver,
        NULL
    },
#endif
#if COREMIDI_SUPPORT
    {
        "coremidi",
        newCoremidiDriver,
        deleteCoremidiDriver,
        coremidiDriverSettings
    },
#endif
    /* NULL terminator to avoid zero size array if no driver available */
    { NULL, NULL, NULL, NULL }
};


void midiDriverSettings(Settings *settings)
{
    unsigned int i;
    const char *defName = NULL;

    settingsRegisterInt(settings, "midi.autoconnect", 0, 0, 1, HINT_TOGGLED);

    settingsRegisterInt(settings, "midi.realtime-prio",
                                DEFAULT_MIDI_RT_PRIO, 0, 99, 0);
    
    settingsRegisterStr(settings, "midi.driver", "", 0);

    for(i = 0; i < N_ELEMENTS(midiDrivers) - 1; i++)
    {
        /* Select the default driver */
        if (defName == NULL)
        {
            defName = midiDrivers[i].name;
        }
    
        /* Add the driver to the list of options */
        settingsAddOption(settings, "midi.driver", midiDrivers[i].name);

        if(midiDrivers[i].settings != NULL)
        {
            midiDrivers[i].settings(settings);
        }
    }

    /* Set the default driver, if any */
    if(defName != NULL)
    {
        settingsSetstr(settings, "midi.driver", defName);
    }
}

/**
 * Create a new MIDI driver instance.
 *
 * @param settings Settings used to configure new MIDI driver. See \ref settingsMidi for available options.
 * @param handler MIDI handler callback (for example: midiRouterHandleMidiEvent()
 *   for MIDI router)
 * @param eventHandlerData Caller defined data to pass to 'handler'
 * @return New MIDI driver instance or NULL on error
 *
 * Which MIDI driver is actually created depends on the \ref settingsMidiDriver option.
 */
midiDriverT *newMidiDriver(Settings *settings, handleMidiEventFuncT handler, void *eventHandlerData)
{
    midiDriverT *driver = NULL;
    char *allnames;
    const mdriverDefinitionT *def;

    for(def = midiDrivers; def->name != NULL; def++)
    {
        if(settingsStrEqual(settings, "midi.driver", def->name))
        {
            LOG(DBG, "Using '%s' midi driver", def->name);
            driver = def->new(settings, handler, eventHandlerData);

            if(driver)
            {
                driver->define = def;
            }

            return driver;
        }
    }

    LOG(ERR, "Couldn't find the requested midi driver.");
    allnames = settingsOptionConcat(settings, "midi.driver", NULL);
    if(allnames != NULL)
    {
        if(allnames[0] != '\0')
        {
            LOG(INFO, "This build of synth supports the following MIDI drivers: %s", allnames);
        }
        else
        {
            LOG(INFO, "This build of synth doesn't support any MIDI drivers.");
        }

        FREE(allnames);
    }

    return NULL;
}

/**
 * Delete a MIDI driver instance.
 * @param driver MIDI driver to delete
 */
void deleteMidiDriver(midiDriverT *driver)
{
    returnIfFail(driver != NULL);
    driver->define->free(driver);
}
