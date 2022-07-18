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



/*
  2002 : API design by Peter Hanappe and Antoine Schmitt
  August 2002 : Implementation by Antoine Schmitt as@gratin.org
  as part of the infiniteCD author project
  http://www.infiniteCD.org/
*/

#include "event.h"
#include "sys.h"	// timer, threads, etc...
#include "list.h"
#include "seqQueue.h"

/***************************************************************
 *
 *                           SEQUENCER
 */

#define SEQUENCER_EVENTS_MAX	1000

/* Private data for SEQUENCER */
struct _SequencerT
{
    // A backup of currentMs when we have received the last scale change
    unsigned int startMs;

    // The number of milliseconds passed since we have started the sequencer,
    // as indicated by the synth's sample timer
    atomicIntT currentMs;

    // A backup of curTicks when we have received the last scale change
    unsigned int startTicks;

    // The tick count which we've used for the most recent event dispatching
    unsigned int curTicks;

    int useSystemTimer;

    // The current time scale in ticks per second.
    // If you think of MIDI, this is equivalent to: (PPQN / 1000) / (USPQN / 1000)
    double scale;

    listT *clients;
    seqIdT clientsID;

    // Pointer to the C++ event queue
    void *queue;
    recMutexT mutex;
};

/* Private data for clients */
typedef struct _SequencerClientT
{
    seqIdT id;
    char *name;
    eventCallbackT callback;
    void *data;
} sequencerClientT;


/* API implementation */

/**
 * Create a new sequencer object which uses the system timer.
 *
 * @return New sequencer instance
 *
 * Use newSequencer2() to specify whether the system timer or
 * sequencerProcess() is used to advance the sequencer.
 *
 * @deprecated As of synth 2.1.1 the use of the system timer has been deprecated.
 */
sequencerT *
newSequencer(void)
{
    return newSequencer2(TRUE);
}

/**
 * Create a new sequencer object.
 *
 * @param useSystemTimer If TRUE, sequencer will advance at the rate of the
 *   system clock. If FALSE, call sequencerProcess() to advance
 *   the sequencer.
 * @return New sequencer instance
 *
 * @note As of synth 2.1.1 the use of the system timer has been deprecated.
 *
 * @since 1.1.0
 */
sequencerT *
newSequencer2(int useSystemTimer)
{
    sequencerT *seq;

    if(useSystemTimer)
    {
        LOG(WARN, "sequencer: Usage of the system timer has been deprecated!");
    }

    seq = NEW(sequencerT);

    if(seq == NULL)
    {
        LOG(PANIC, "sequencer: Out of memory\n");
        return NULL;
    }

    MEMSET(seq, 0, sizeof(sequencerT));

    seq->scale = 1000;	// default value
    seq->useSystemTimer = useSystemTimer ? 1 : 0;
    seq->startMs = seq->useSystemTimer ? curtime() : 0;

    recMutexInit(seq->mutex);

    seq->queue = newSeqQueue(SEQUENCER_EVENTS_MAX);
    if(seq->queue == NULL)
    {
        LOG(PANIC, "sequencer: Out of memory\n");
        deleteSequencer(seq);
        return NULL;
    }

    return(seq);
}

/**
 * Free a sequencer object.
 *
 * @param seq Sequencer to delete
 *
 * @note Before synth 2.1.1 registered sequencer clients may not be fully freed by this function.
 */
void
deleteSequencer(sequencerT *seq)
{
    returnIfFail(seq != NULL);

    /* cleanup clients */
    while(seq->clients)
    {
        sequencerClientT *client = (sequencerClientT *)seq->clients->data;
        sequencerUnregisterClient(seq, client->id);
    }

    recMutexDestroy(seq->mutex);
    deleteSeqQueue(seq->queue);

    FREE(seq);
}

/**
 * Check if a sequencer is using the system timer or not.
 *
 * @param seq Sequencer object
 * @return TRUE if system timer is being used, FALSE otherwise.
 *
 * @deprecated As of synth 2.1.1 the usage of the system timer has been deprecated.
 *
 * @since 1.1.0
 */
int
sequencerGetUseSystemTimer(sequencerT *seq)
{
    returnValIfFail(seq != NULL, FALSE);

    return seq->useSystemTimer;
}


/* clients */

/**
 * Register a sequencer client.
 *
 * @param seq Sequencer object
 * @param name Name of sequencer client
 * @param callback Sequencer client callback or NULL for a source client.
 * @param data User data to pass to the \a callback
 * @return Unique sequencer ID or #FAILED on error
 *
 * Clients can be sources or destinations of events. Sources don't need to
 * register a callback.
 *
 * @note Implementations are encouraged to explicitly unregister any registered client with sequencerUnregisterClient() before deleting the sequencer.
 */
seqIdT
sequencerRegisterClient(sequencerT *seq, const char *name,
                                eventCallbackT callback, void *data)
{
    sequencerClientT *client;
    char *nameCopy;

    returnValIfFail(seq != NULL, FAILED);

    client = NEW(sequencerClientT);

    if(client == NULL)
    {
        LOG(PANIC, "sequencer: Out of memory\n");
        return FAILED;
    }

    nameCopy = STRDUP(name);

    if(nameCopy == NULL)
    {
        LOG(PANIC, "sequencer: Out of memory\n");
        FREE(client);
        return FAILED;
    }

    seq->clientsID++;

    client->name = nameCopy;
    client->id = seq->clientsID;
    client->callback = callback;
    client->data = data;

    seq->clients = listAppend(seq->clients, (void *)client);

    return (client->id);
}

/**
 * Unregister a previously registered client.
 *
 * @param seq Sequencer object
 * @param id Client ID as returned by sequencerRegisterClient().
 *
 * The client's callback function will receive a SEQ_UNREGISTERING event right before it is being unregistered.
 */
void
sequencerUnregisterClient(sequencerT *seq, seqIdT id)
{
    listT *tmp;
    eventT evt;
    unsigned int now = sequencerGetTick(seq);

    returnIfFail(seq != NULL);

    eventClear(&evt);
    eventUnregistering(&evt);
    eventSetDest(&evt, id);
    eventSetTime(&evt, now);

    tmp = seq->clients;

    while(tmp)
    {
        sequencerClientT *client = (sequencerClientT *)tmp->data;

        if(client->id == id)
        {
            // client found, remove it from the list to avoid recursive call when calling callback
            seq->clients = listRemoveLink(seq->clients, tmp);

            // call the callback (if any), to free underlying memory (e.g. seqbind structure)
            if (client->callback != NULL)
            {
                (client->callback)(now, &evt, seq, client->data);
            }

            if(client->name)
            {
                FREE(client->name);
            }
            delete1_List(tmp);
            FREE(client);
            return;
        }

        tmp = tmp->next;
    }

    return;
}

/**
 * Count a sequencers registered clients.
 *
 * @param seq Sequencer object
 * @return Count of sequencer clients.
 */
int
sequencerCountClients(sequencerT *seq)
{
    if(seq == NULL || seq->clients == NULL)
    {
        return 0;
    }

    return listSize(seq->clients);
}

/**
 * Get a client ID from its index (order in which it was registered).
 *
 * @param seq Sequencer object
 * @param index Index of register client
 * @return Client ID or #FAILED if not found
 */
seqIdT sequencerGetClientId(sequencerT *seq, int index)
{
    listT *tmp;

    returnValIfFail(seq != NULL, FAILED);
    returnValIfFail(index >= 0, FAILED);

    tmp = listNth(seq->clients, index);

    if(tmp == NULL)
    {
        return FAILED;
    }
    else
    {
        sequencerClientT *client = (sequencerClientT *)tmp->data;
        return client->id;
    }
}

/**
 * Get the name of a registered client.
 *
 * @param seq Sequencer object
 * @param id Client ID
 * @return Client name or NULL if not found.  String is internal and should not
 *   be modified or freed.
 */
char *
sequencerGetClientName(sequencerT *seq, seqIdT id)
{
    listT *tmp;

    returnValIfFail(seq != NULL, NULL);

    tmp = seq->clients;

    while(tmp)
    {
        sequencerClientT *client = (sequencerClientT *)tmp->data;

        if(client->id == id)
        {
            return client->name;
        }

        tmp = tmp->next;
    }

    return NULL;
}

/**
 * Check if a client is a destination client.
 *
 * @param seq Sequencer object
 * @param id Client ID
 * @return TRUE if client is a destination client, FALSE otherwise or if not found
 */
int
sequencerClientIsDest(sequencerT *seq, seqIdT id)
{
    listT *tmp;

    returnValIfFail(seq != NULL, FALSE);

    tmp = seq->clients;

    while(tmp)
    {
        sequencerClientT *client = (sequencerClientT *)tmp->data;

        if(client->id == id)
        {
            return (client->callback != NULL);
        }

        tmp = tmp->next;
    }

    return FALSE;
}

/**
 * Send an event immediately.
 *
 * @param seq Sequencer object
 * @param evt Event to send (not copied, used directly)
 */
void sequencerSendNow(sequencerT *seq, eventT *evt) {
    seqIdT destID;
    listT *tmp;

    returnIfFail(seq != NULL);
    returnIfFail(evt != NULL);

    destID = eventGetDest(evt);

    /* find callback */
    tmp = seq->clients;

    while(tmp)
    {
        sequencerClientT *dest = (sequencerClientT *)tmp->data;

        if(dest->id == destID)
        {
            if(eventGetType(evt) == SEQ_UNREGISTERING)
            {
                sequencerUnregisterClient(seq, destID);
            }
            else
            {
                if(dest->callback)
                {
                    (dest->callback)(sequencerGetTick(seq), evt, seq, dest->data);
                }
            }
            return;
        }

        tmp = tmp->next;
    }
}


/**
 * Schedule an event for sending at a later time.
 *
 * @param seq Sequencer object
 * @param evt Event to send (will be copied into internal queue)
 * @param time Time value in ticks (in milliseconds with the default time scale of 1000).
 * @param absolute TRUE if \a time is absolute sequencer time (time since sequencer
 *   creation), FALSE if relative to current time.
 * @return #OK on success, #FAILED otherwise
 *
 * @note The sequencer sorts events according to their timestamp \c time. For events that have
 * the same timestamp, synth (as of version 2.2.0) uses the following order, according to
 * which events will be dispatched to the client's callback function.
 *  - #SEQ_SYSTEMRESET events precede any other event type.
 *  - #SEQ_UNREGISTERING events succeed #SEQ_SYSTEMRESET and precede other event type.
 *  - #SEQ_NOTEON and #SEQ_NOTE events succeed any other event type.
 *  - Otherwise the order is undefined.
 * \n
 * Or mathematically: #SEQ_SYSTEMRESET < #SEQ_UNREGISTERING < ... < (#SEQ_NOTEON && #SEQ_NOTE)
 *
 * @warning Be careful with relative ticks when sending many events! See #eventCallbackT for details.
 */
int
sequencerSendAt(sequencerT *seq, eventT *evt,
                        unsigned int time, int absolute)
{
    int res;
    unsigned int now = sequencerGetTick(seq);

    returnValIfFail(seq != NULL, FAILED);
    returnValIfFail(evt != NULL, FAILED);

    /* set absolute */
    if(!absolute)
    {
        time = now + time;
    }

    /* time stamp event */
    eventSetTime(evt, time);

    recMutexLock(seq->mutex);
    res = seqQueuePush(seq->queue, evt);
    recMutexUnlock(seq->mutex);

    return res;
}

/**
 * Remove events from the event queue.
 *
 * @param seq Sequencer object
 * @param source Source client ID to match or -1 for wildcard
 * @param dest Destination client ID to match or -1 for wildcard
 * @param type Event type to match or -1 for wildcard (#seqEventType)
 */
void
sequencerRemoveEvents(sequencerT *seq, seqIdT source,
                              seqIdT dest, int type)
{
    returnIfFail(seq != NULL);

    recMutexLock(seq->mutex);
    seqQueueRemove(seq->queue, source, dest, type);
    recMutexUnlock(seq->mutex);
}


/*************************************
	time
**************************************/

static unsigned int
sequencerGetTick_LOCAL(sequencerT *seq, unsigned int curMsec)
{
    unsigned int absMs;
    double nowFloat;
    unsigned int now;

    returnValIfFail(seq != NULL, 0u);

    absMs = seq->useSystemTimer ? (unsigned int) curtime() : curMsec;
    nowFloat = ((double)(absMs - seq->startMs)) * seq->scale / 1000.0f;
    now = nowFloat;
    return seq->startTicks + now;
}

/**
 * Get the current tick of the sequencer scaled by the time scale currently set.
 *
 * @param seq Sequencer object
 * @return Current tick value
 */
unsigned int
sequencerGetTick(sequencerT *seq)
{
    return sequencerGetTick_LOCAL(seq, atomicIntGet(&seq->currentMs));
}

/**
 * Set the time scale of a sequencer.
 *
 * @param seq Sequencer object
 * @param scale Sequencer scale value in ticks per second
 *   (default is 1000 for 1 tick per millisecond)
 *
 * If there are already scheduled events in the sequencer and the scale is changed
 * the events are adjusted accordingly.
 *
 * @note May only be called from a sequencer callback or initially when no event dispatching happens.
 * Otherwise it will mess up your event timing, because you have zero control over which events are
 * affected by the scale change.
 */
void
sequencerSetTimeScale(sequencerT *seq, double scale)
{
    returnIfFail(seq != NULL);

    if(scale != scale)
    {
        LOG(WARN, "sequencer: scale NaN\n");
        return;
    }

    if(scale <= 0)
    {
        LOG(WARN, "sequencer: scale <= 0 : %f\n", scale);
        return;
    }

    seq->scale = scale;
    seq->startMs = atomicIntGet(&seq->currentMs);
    seq->startTicks = seq->curTicks;
}

/**
 * Get a sequencer's time scale.
 *
 * @param seq Sequencer object.
 * @return Time scale value in ticks per second.
 */
double
sequencerGetTimeScale(sequencerT *seq)
{
    returnValIfFail(seq != NULL, 0);
    return seq->scale;
}

/**
 * Advance a sequencer.
 *
 * @param seq Sequencer object
 * @param msec Time to advance sequencer to (absolute time since sequencer start).
 *
 * If you have registered the synthesizer as client (sequencerRegistersynth()), the synth
 * will take care of calling sequencerProcess(). Otherwise it is up to the user to
 * advance the sequencer manually.
 *
 * @since 1.1.0
 */
void sequencerProcess(sequencerT *seq, unsigned int msec) {
    atomicIntSet(&seq->currentMs, msec);
    seq->curTicks = sequencerGetTick_LOCAL(seq, msec);

    recMutexLock(seq->mutex);
    seqQueueProcess(seq->queue, seq, seq->curTicks);
    recMutexUnlock(seq->mutex);
}


/**
 * @internal
 * only used privately by seqbind and only from sequencer callback, thus lock acquire is not needed.
 */
void sequencerInvalidateNote(sequencerT *seq, seqIdT dest, noteIdT id)
{
    seqQueueInvalidateNotePrivate(seq->queue, dest, id);
}
