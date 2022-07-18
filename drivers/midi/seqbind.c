#include "fluidbean.h"
#include "synth.h"
#include "midi.h"
#include "event.h"
#include "seqbindNotes.h"

/* SEQUENCER BINDING */
struct _SeqbindT {
    Synthesizer *synth;
    sequencerT *seq;
    SampleimerT *sampleTimer;
    seqIdT clientId;
    void* noteContainer;
};
typedef struct _SeqbindT seqbindT;

extern void sequencerInvalidateNote(sequencerT *seq, seqIdT dest, noteIdT id);

int seqbindTimerCallback(void *data, unsigned int msec);
void seqsynthCallback(unsigned int time, eventT *event, sequencerT *seq, void *data);

/* Proper cleanup of the seqbind struct. */
void deleteSeqbind(seqbindT *seqbind) {
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

    deleteNoteContainer(seqbind->noteContainer);
    FREE(seqbind);
}

/**
 * Registers a synthesizer as a destination client of the given sequencer.
 *
 * @param seq Sequencer instance
 * @param synth Synthesizer instance
 * @returns Sequencer client ID, or #FAILED on error.
 *
 * A convenience wrapper function around sequencerRegisterClient(), that allows you to
 * easily process and render enqueued sequencer events with synth's synthesizer.
 * The client being registered will be named @c synth.
 *
 * @note Implementations are encouraged to explicitly unregister this client either by calling
 * sequencerUnregisterClient() or by sending an unregistering event to the sequencer. Before
 * synth 2.1.1 this was mandatory to avoid memory leaks.
 *
 * @code{.cpp}
 * seqIdT seqid = sequencerRegistersynth(seq, synth);
 *
 * // ... do work
 *
 * eventT* evt = newEvent();
 * eventSetSource(evt, -1);
 * eventSetDest(evt, seqid);
 * eventUnregistering(evt);
 *
 * // unregister the "synth" client immediately
 * sequencerSendNow(seq, evt);
 * deleteEvent(evt);
 * deleteSynth(synth);
 * deleteSequencer(seq);
 * @endcode
 */
seqIdT sequencerRegistersynth(sequencerT *seq, Synthesizer *synth) {
    seqbindT *seqbind;

    returnValIfFail(seq != NULL, FAILED);
    returnValIfFail(synth != NULL, FAILED);

    seqbind = NEW(seqbindT);

    if(seqbind == NULL)
      return FAILED;

    MEMSET(seqbind, 0, sizeof(*seqbind));

    seqbind->clientId = -1;
    seqbind->synth = synth;
    seqbind->seq = seq;

    /* set up the sample timer */
    if(!sequencerGetUseSystemTimer(seq)) {
        seqbind->sampleTimer =
            new_Sampleimer(synth, seqbindTimerCallback, (void *) seqbind);

        if(seqbind->sampleTimer == NULL) {
            LOG(PANIC, "sequencer: Out of memory\n");
            FREE(seqbind);
            return FAILED;
        }
    }

    seqbind->noteContainer = newNoteContainer();
    if(seqbind->noteContainer == NULL) {
        delete_Sampleimer(seqbind->synth, seqbind->sampleTimer);
        FREE(seqbind);
        return FAILED;
    }

    /* register synth itself */
    seqbind->clientId = sequencerRegisterClient(seq, "synth", seqsynthCallback, (void *)seqbind);

    if(seqbind->clientId == FAILED) {
        deleteNoteContainer(seqbind->noteContainer);
        delete_Sampleimer(seqbind->synth, seqbind->sampleTimer);
        FREE(seqbind);
        return FAILED;
    }

    return seqbind->clientId;
}

/* Callback for sample timer */
int seqbindTimerCallback(void *data, unsigned int msec) {
    seqbindT *seqbind = (seqbindT *) data;
    sequencerProcess(seqbind->seq, msec);
    return 1;
}

/* Callback for midi events */
void seqsynthCallback(unsigned int time, eventT *evt, sequencerT *seq, void *data) {
    Synthesizer synth = seqbind->synth;
    seqbindT *seqbind = (seqbindT *) data;

    switch(eventGetType(evt))
    {
    case SEQ_NOTEON:
        synthNoteon(synth, eventGetChannel(evt), eventGetKey(evt), eventGetVelocity(evt));
        break;

    case SEQ_NOTEOFF:
    {
        noteIdT id = eventGetId(evt);
        if(id != -1)
        {
            noteContainerRemove(seqbind->noteContainer, id);
        }
        synthNoteoff(synth, eventGetChannel(evt), eventGetKey(evt));
    }
    break;

    case SEQ_NOTE:
    {
        unsigned int dur = eventGetDuration(evt);
        short vel = eventGetVelocity(evt);
        short key = eventGetKey(evt);
        int chan = eventGetChannel(evt);

        noteIdT id = noteComputeId(chan, key);

        int res = noteContainerInsert(seqbind->noteContainer, id);
        if(res == FAILED)
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
        if(res == FAILED)
        {
            err:
            LOG(ERR, "seqbind: Unable to process SEQ_NOTE event, something went horribly wrong");
            return;
        }

        synthNoteon(synth, chan, key, vel);
    }
    break;

    case SEQ_ALLSOUNDSOFF:
        noteContainerClear(seqbind->noteContainer);
        synthAllSoundsOff(synth, eventGetChannel(evt));
        break;

    case SEQ_ALLNOTESOFF:
        noteContainerClear(seqbind->noteContainer);
        synthAllNotesOff(synth, eventGetChannel(evt));
        break;

    case SEQ_BANKSELECT:
        synthBankSelect(synth, eventGetChannel(evt), eventGetBank(evt));
        break;

    case SEQ_PROGRAMCHANGE:
        synthProgramChange(synth, eventGetChannel(evt), eventGetProgram(evt));
        break;

    case SEQ_PROGRAMSELECT:
        synthProgramSelect(synth,
                                   eventGetChannel(evt),
                                   eventGetSfontId(evt),
                                   eventGetBank(evt),
                                   eventGetProgram(evt));
        break;

    case SEQ_PITCHBEND:
        synthPitchBend(synth, eventGetChannel(evt), eventGetPitch(evt));
        break;

    case SEQ_PITCHWHEELSENS:
        synthPitchWheelSens(synth, eventGetChannel(evt), eventGetValue(evt));
        break;

    case SEQ_CONTROLCHANGE:
        synthCc(synth, eventGetChannel(evt), eventGetControl(evt), eventGetValue(evt));
        break;

    case SEQ_MODULATION:
        synthCc(synth, eventGetChannel(evt), MODULATION_MSB, eventGetValue(evt));
        break;

    case SEQ_SUSTAIN:
        synthCc(synth, eventGetChannel(evt), SUSTAIN_SWITCH, eventGetValue(evt));
        break;

    case SEQ_PAN:
        synthCc(synth, eventGetChannel(evt), PAN_MSB, eventGetValue(evt));
        break;

    case SEQ_VOLUME:
        synthCc(synth, eventGetChannel(evt), VOLUME_MSB, eventGetValue(evt));
        break;

    case SEQ_REVERBSEND:
        synthCc(synth, eventGetChannel(evt), EFFECTS_DEPTH1, eventGetValue(evt));
        break;

    case SEQ_CHORUSSEND:
        synthCc(synth, eventGetChannel(evt), EFFECTS_DEPTH3, eventGetValue(evt));
        break;

    case SEQ_CHANNELPRESSURE:
        synthChannelPressure(synth, eventGetChannel(evt), eventGetValue(evt));
        break;

    case SEQ_KEYPRESSURE:
        synthKeyPressure(synth,
                                 eventGetChannel(evt),
                                 eventGetKey(evt),
                                 eventGetValue(evt));
        break;

    case SEQ_SYSTEMRESET:
        synthSystemReset(synth);
        break;

    case SEQ_UNREGISTERING: /* free ourselves */
        deleteSeqbind(seqbind);
        break;

    case SEQ_TIMER:
        /* nothing in synth */
        break;

    case SEQ_SCALE:
        sequencerSetTimeScale(seq, eventGetScale(evt));
        break;

    default:
        break;
    }
}

static seqIdT getsynthDest(sequencerT *seq) {
    int i;
    seqIdT id;
    char *name;
    int j = sequencerCountClients(seq);

    for(i = 0; i < j; i++) {
        id = sequencerGetClientId(seq, i);
        name = sequencerGetClientName(seq, id);

        if(name && (STRCMP(name, "synth") == 0))
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
 * @return #OK or #FAILED
 *
 * The signature of this function is of type #handleMidiEventFuncT.
 *
 * @since 1.1.0
 */
int sequencerAddMidiEventToBuffer(void *data, midiEventT *event) {
    eventT evt;
    sequencerT *seq;

    returnValIfFail(data != NULL, FAILED);
    returnValIfFail(event != NULL, FAILED);

    seq = (sequencerT *)data;

    eventClear(&evt);
    eventFromMidiEvent(&evt, event);
    eventSetDest(&evt, getsynthDest(seq));

    /* Schedule for sending at next call to sequencerProcess */
    return sequencerSendAt(seq, &evt, 0, 0);
}
