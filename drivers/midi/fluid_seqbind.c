#include "fluidsynthPriv.h"
#include "synth.h"
#include "midi.h"
#include "event.h"
#include "seqbindNotes.h"

/* SEQUENCER BINDING */
struct _fluidSeqbindT {
    synthT *synth;
    sequencerT *seq;
    SampleimerT *sampleTimer;
    seqIdT clientId;
    void* noteContainer;
};
typedef struct _fluidSeqbindT seqbindT;

extern void sequencerInvalidateNote(sequencerT *seq, seqIdT dest, noteIdT id);

int seqbindTimerCallback(void *data, unsigned int msec);
void seqFluidsynthCallback(unsigned int time, eventT *event, sequencerT *seq, void *data);

/* Proper cleanup of the seqbind struct. */
void deleteFluidSeqbind(seqbindT *seqbind) {
    returnIfFail(seqbind != NULL);

    if((seqbind->clientId != -1) && (seqbind->seq != NULL))
    {
        sequencerUnregisterClient(seqbind->seq, seqbind->clientId);
        seqbind->clientId = -1;
    }

    if((seqbind->sampleTimer != NULL) && (seqbind->synth != NULL))
    {
        delete_Sampleimer(seqbind->synth, seqbind->sampleTimer);
        seqbind->sampleTimer = NULL;
    }

    deleteFluidNoteContainer(seqbind->noteContainer);
    FLUID_FREE(seqbind);
}

/**
 * Registers a synthesizer as a destination client of the given sequencer.
 *
 * @param seq Sequencer instance
 * @param synth Synthesizer instance
 * @returns Sequencer client ID, or #FLUID_FAILED on error.
 *
 * A convenience wrapper function around sequencerRegisterClient(), that allows you to
 * easily process and render enqueued sequencer events with fluidsynth's synthesizer.
 * The client being registered will be named @c fluidsynth.
 *
 * @note Implementations are encouraged to explicitly unregister this client either by calling
 * sequencerUnregisterClient() or by sending an unregistering event to the sequencer. Before
 * fluidsynth 2.1.1 this was mandatory to avoid memory leaks.
 *
 * @code{.cpp}
 * seqIdT seqid = sequencerRegisterFluidsynth(seq, synth);
 *
 * // ... do work
 *
 * eventT* evt = newFluidEvent();
 * eventSetSource(evt, -1);
 * eventSetDest(evt, seqid);
 * eventUnregistering(evt);
 *
 * // unregister the "fluidsynth" client immediately
 * sequencerSendNow(seq, evt);
 * deleteFluidEvent(evt);
 * deleteFluidSynth(synth);
 * deleteFluidSequencer(seq);
 * @endcode
 */
seqIdT sequencerRegisterFluidsynth(sequencerT *seq, synthT *synth) {
    seqbindT *seqbind;

    returnValIfFail(seq != NULL, FLUID_FAILED);
    returnValIfFail(synth != NULL, FLUID_FAILED);

    seqbind = FLUID_NEW(seqbindT);

    if(seqbind == NULL)
      return FLUID_FAILED;

    FLUID_MEMSET(seqbind, 0, sizeof(*seqbind));

    seqbind->clientId = -1;
    seqbind->synth = synth;
    seqbind->seq = seq;

    /* set up the sample timer */
    if(!sequencerGetUseSystemTimer(seq)) {
        seqbind->sampleTimer =
            new_Sampleimer(synth, seqbindTimerCallback, (void *) seqbind);

        if(seqbind->sampleTimer == NULL) {
            FLUID_LOG(FLUID_PANIC, "sequencer: Out of memory\n");
            FLUID_FREE(seqbind);
            return FLUID_FAILED;
        }
    }

    seqbind->noteContainer = newFluidNoteContainer();
    if(seqbind->noteContainer == NULL) {
        delete_Sampleimer(seqbind->synth, seqbind->sampleTimer);
        FLUID_FREE(seqbind);
        return FLUID_FAILED;
    }

    /* register fluidsynth itself */
    seqbind->clientId = sequencerRegisterClient(seq, "fluidsynth", seqFluidsynthCallback, (void *)seqbind);

    if(seqbind->clientId == FLUID_FAILED) {
        deleteFluidNoteContainer(seqbind->noteContainer);
        delete_Sampleimer(seqbind->synth, seqbind->sampleTimer);
        FLUID_FREE(seqbind);
        return FLUID_FAILED;
    }

    return seqbind->clientId;
}

/* Callback for sample timer */
int seqbindTimerCallback(void *data, unsigned int msec)
{
    seqbindT *seqbind = (seqbindT *) data;
    sequencerProcess(seqbind->seq, msec);
    return 1;
}

/* Callback for midi events */
void seqFluidsynthCallback(unsigned int time, eventT *evt, sequencerT *seq, void *data) {
    synthT *synth;
    seqbindT *seqbind = (seqbindT *) data;
    synth = seqbind->synth;

    switch(eventGetType(evt))
    {
    case FLUID_SEQ_NOTEON:
        synthNoteon(synth, eventGetChannel(evt), eventGetKey(evt), eventGetVelocity(evt));
        break;

    case FLUID_SEQ_NOTEOFF:
    {
        noteIdT id = eventGetId(evt);
        if(id != -1)
        {
            noteContainerRemove(seqbind->noteContainer, id);
        }
        synthNoteoff(synth, eventGetChannel(evt), eventGetKey(evt));
    }
    break;

    case FLUID_SEQ_NOTE:
    {
        unsigned int dur = eventGetDuration(evt);
        short vel = eventGetVelocity(evt);
        short key = eventGetKey(evt);
        int chan = eventGetChannel(evt);

        noteIdT id = noteComputeId(chan, key);

        int res = noteContainerInsert(seqbind->noteContainer, id);
        if(res == FLUID_FAILED)
        {
            goto err;
        }
        else if(res)
        {
            // Note is already playing ATM, the following call to synthNoteon() will kill that note.
            // Thus, we need to remove its noteoff from the queue
            sequencerInvalidateNote(seqbind->seq, seqbind->clientId, id);
        }
        else
        {
            // Note not playing, all good.
        }

        eventNoteoff(evt, chan, key);
        eventSetId(evt, id);

        res = sequencerSendAt(seq, evt, dur, 0);
        if(res == FLUID_FAILED)
        {
            err:
            FLUID_LOG(FLUID_ERR, "seqbind: Unable to process FLUID_SEQ_NOTE event, something went horribly wrong");
            return;
        }

        synthNoteon(synth, chan, key, vel);
    }
    break;

    case FLUID_SEQ_ALLSOUNDSOFF:
        noteContainerClear(seqbind->noteContainer);
        synthAllSoundsOff(synth, eventGetChannel(evt));
        break;

    case FLUID_SEQ_ALLNOTESOFF:
        noteContainerClear(seqbind->noteContainer);
        synthAllNotesOff(synth, eventGetChannel(evt));
        break;

    case FLUID_SEQ_BANKSELECT:
        synthBankSelect(synth, eventGetChannel(evt), eventGetBank(evt));
        break;

    case FLUID_SEQ_PROGRAMCHANGE:
        synthProgramChange(synth, eventGetChannel(evt), eventGetProgram(evt));
        break;

    case FLUID_SEQ_PROGRAMSELECT:
        synthProgramSelect(synth,
                                   eventGetChannel(evt),
                                   eventGetSfontId(evt),
                                   eventGetBank(evt),
                                   eventGetProgram(evt));
        break;

    case FLUID_SEQ_PITCHBEND:
        synthPitchBend(synth, eventGetChannel(evt), eventGetPitch(evt));
        break;

    case FLUID_SEQ_PITCHWHEELSENS:
        synthPitchWheelSens(synth, eventGetChannel(evt), eventGetValue(evt));
        break;

    case FLUID_SEQ_CONTROLCHANGE:
        synthCc(synth, eventGetChannel(evt), eventGetControl(evt), eventGetValue(evt));
        break;

    case FLUID_SEQ_MODULATION:
        synthCc(synth, eventGetChannel(evt), MODULATION_MSB, eventGetValue(evt));
        break;

    case FLUID_SEQ_SUSTAIN:
        synthCc(synth, eventGetChannel(evt), SUSTAIN_SWITCH, eventGetValue(evt));
        break;

    case FLUID_SEQ_PAN:
        synthCc(synth, eventGetChannel(evt), PAN_MSB, eventGetValue(evt));
        break;

    case FLUID_SEQ_VOLUME:
        synthCc(synth, eventGetChannel(evt), VOLUME_MSB, eventGetValue(evt));
        break;

    case FLUID_SEQ_REVERBSEND:
        synthCc(synth, eventGetChannel(evt), EFFECTS_DEPTH1, eventGetValue(evt));
        break;

    case FLUID_SEQ_CHORUSSEND:
        synthCc(synth, eventGetChannel(evt), EFFECTS_DEPTH3, eventGetValue(evt));
        break;

    case FLUID_SEQ_CHANNELPRESSURE:
        synthChannelPressure(synth, eventGetChannel(evt), eventGetValue(evt));
        break;

    case FLUID_SEQ_KEYPRESSURE:
        synthKeyPressure(synth,
                                 eventGetChannel(evt),
                                 eventGetKey(evt),
                                 eventGetValue(evt));
        break;

    case FLUID_SEQ_SYSTEMRESET:
        synthSystemReset(synth);
        break;

    case FLUID_SEQ_UNREGISTERING: /* free ourselves */
        deleteFluidSeqbind(seqbind);
        break;

    case FLUID_SEQ_TIMER:
        /* nothing in fluidsynth */
        break;

    case FLUID_SEQ_SCALE:
        sequencerSetTimeScale(seq, eventGetScale(evt));
        break;

    default:
        break;
    }
}

static seqIdT getFluidsynthDest(sequencerT *seq) {
    int i;
    seqIdT id;
    char *name;
    int j = sequencerCountClients(seq);

    for(i = 0; i < j; i++) {
        id = sequencerGetClientId(seq, i);
        name = sequencerGetClientName(seq, id);

        if(name && (FLUID_STRCMP(name, "fluidsynth") == 0))
            return id;
    }

    return -1;
}

/**
 * Transforms an incoming MIDI event (from a MIDI driver or MIDI router) to a
 * sequencer event and adds it to the sequencer queue for sending as soon as possible.
 *
 * @param data The sequencer, must be a valid #sequencerT
 * @param event MIDI event
 * @return #FLUID_OK or #FLUID_FAILED
 *
 * The signature of this function is of type #handleMidiEventFuncT.
 *
 * @since 1.1.0
 */
int sequencerAddMidiEventToBuffer(void *data, midiEventT *event) {
    eventT evt;
    sequencerT *seq;

    returnValIfFail(data != NULL, FLUID_FAILED);
    returnValIfFail(event != NULL, FLUID_FAILED);

    seq = (sequencerT *)data;

    eventClear(&evt);
    eventFromMidiEvent(&evt, event);
    eventSetDest(&evt, getFluidsynthDest(seq));

    /* Schedule for sending at next call to sequencerProcess */
    return sequencerSendAt(seq, &evt, 0, 0);
}
