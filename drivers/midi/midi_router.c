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

/* Original author: Markus Nentwig, nentwig@users.sourceforge.net
 *
 * Josh Green made it general purpose with a complete usable public API and
 * cleaned it up a bit.
 */

#include "midiRouter.h"
#include "midi.h"
#include "synth.h"

/*
 * midiRouter
 */
struct _MidiRouterT
{
    mutexT rulesMutex;
    midiRouterRuleT *rules[MIDI_ROUTER_RULE_COUNT];        /* List of rules for each rule type */
    midiRouterRuleT *freeRules;      /* List of rules to free (was waiting for final events which were received) */

    handleMidiEventFuncT eventHandler;    /* Callback function for generated events */
    void *eventHandlerData;                  /* One arg for the callback */

    int nrMidiChannels;                      /* For clipping the midi channel */
};

struct _MidiRouterRuleT
{
    int chanMin;                            /* Channel window, for which this rule is valid */
    int chanMax;
    realT chanMul;                   /* Channel multiplier (usually 0 or 1) */
    int chanAdd;                            /* Channel offset */

    int par1_min;                            /* Parameter 1 window, for which this rule is valid */
    int par1_max;
    realT par1_mul;
    int par1_add;

    int par2_min;                            /* Parameter 2 window, for which this rule is valid */
    int par2_max;
    realT par2_mul;
    int par2_add;

    int pendingEvents;                      /* In case of noteon: How many keys are still down? */
    char keysCc[128];                       /* Flags, whether a key is down / controller is set (sustain) */
    midiRouterRuleT *next;          /* next entry */
    int waiting;                             /* Set to TRUE when rule has been deactivated but there are still pendingEvents */
};


/**
 * Create a new midi router.
 *
 * @param settings Settings used to configure MIDI router
 * @param handler MIDI event callback.
 * @param eventHandlerData Caller defined data pointer which gets passed to 'handler'
 * @return New MIDI router instance or NULL on error
 *
 * The new router will start with default rules and therefore pass all events unmodified.
 *
 * The MIDI handler callback should process the possibly filtered/modified MIDI
 * events from the MIDI router and forward them on to a synthesizer for example.
 * The function synthHandleMidiEvent() can be used for \a handle and
 * a #Synthesizer passed as the \a eventHandlerData parameter for this purpose.
 */
midiRouterT *
newMidiRouter(Settings *settings, handleMidiEventFuncT handler,
                      void *eventHandlerData)
{
    midiRouterT *router = NULL;
    int i;

    router = NEW(midiRouterT);

    if(router == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(router, 0, sizeof(midiRouterT));

    /* Retrieve the number of MIDI channels for range limiting */
    settingsGetint(settings, "synth.midi-channels", &router->nrMidiChannels);

    mutexInit(router->rulesMutex);

    router->eventHandler = handler;
    router->eventHandlerData = eventHandlerData;

    /* Create default routing rules which pass all events unmodified */
    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        router->rules[i] = newMidiRouterRule();

        if(!router->rules[i])
        {
            goto errorRecovery;
        }
    }

    return router;

errorRecovery:
    deleteMidiRouter(router);
    return NULL;
}

/**
 * Delete a MIDI router instance.
 * @param router MIDI router to delete
 * @return Returns #OK on success, #FAILED otherwise (only if NULL
 *   \a router passed really)
 */
void
deleteMidiRouter(midiRouterT *router)
{
    midiRouterRuleT *rule;
    midiRouterRuleT *nextRule;
    int i;

    returnIfFail(router != NULL);

    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        for(rule = router->rules[i]; rule; rule = nextRule)
        {
            nextRule = rule->next;
            FREE(rule);
        }
    }

    mutexDestroy(router->rulesMutex);
    FREE(router);
}

/**
 * Set a MIDI router to use default "unity" rules.
 *
 * @param router Router to set to default rules.
 * @return #OK on success, #FAILED otherwise
 *
 * Such a router will pass all events unmodified.
 *
 * @since 1.1.0
 */
int
midiRouterSetDefaultRules(midiRouterT *router)
{
    midiRouterRuleT *newRules[MIDI_ROUTER_RULE_COUNT];
    midiRouterRuleT *delRules[MIDI_ROUTER_RULE_COUNT];
    midiRouterRuleT *rule, *nextRule, *prevRule;
    int i, i2;

    returnValIfFail(router != NULL, FAILED);

    /* Allocate new default rules outside of lock */

    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        newRules[i] = newMidiRouterRule();

        if(!newRules[i])
        {
            /* Free already allocated rules */
            for(i2 = 0; i2 < i; i2++)
            {
                deleteMidiRouterRule(newRules[i2]);
            }

            return FAILED;
        }
    }


    mutexLock(router->rulesMutex);        /* ++ lock */

    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        delRules[i] = NULL;
        prevRule = NULL;

        /* Process existing rules */
        for(rule = router->rules[i]; rule; rule = nextRule)
        {
            nextRule = rule->next;

            if(rule->pendingEvents == 0)     /* Rule has no pending events? */
            {
                /* Remove rule from rule list */
                if(prevRule)
                {
                    prevRule->next = nextRule;
                }
                else if(rule == router->rules[i])
                {
                    router->rules[i] = nextRule;
                }

                /* Prepend to delete list */
                rule->next = delRules[i];
                delRules[i] = rule;
            }
            else
            {
                rule->waiting = TRUE;          /* Pending events, mark as waiting */
                prevRule = rule;
            }
        }

        /* Prepend new default rule */
        newRules[i]->next = router->rules[i];
        router->rules[i] = newRules[i];
    }

    mutexUnlock(router->rulesMutex);      /* -- unlock */


    /* Free old rules outside of lock */

    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        for(rule = delRules[i]; rule; rule = nextRule)
        {
            nextRule = rule->next;
            FREE(rule);
        }
    }

    return OK;
}

/**
 * Clear all rules in a MIDI router.
 *
 * @param router Router to clear all rules from
 * @return #OK on success, #FAILED otherwise
 *
 * An empty router will drop all events until rules are added.
 *
 * @since 1.1.0
 */
int
midiRouterClearRules(midiRouterT *router)
{
    midiRouterRuleT *delRules[MIDI_ROUTER_RULE_COUNT];
    midiRouterRuleT *rule, *nextRule, *prevRule;
    int i;

    returnValIfFail(router != NULL, FAILED);

    mutexLock(router->rulesMutex);        /* ++ lock */

    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        delRules[i] = NULL;
        prevRule = NULL;

        /* Process existing rules */
        for(rule = router->rules[i]; rule; rule = nextRule)
        {
            nextRule = rule->next;

            if(rule->pendingEvents == 0)     /* Rule has no pending events? */
            {
                /* Remove rule from rule list */
                if(prevRule)
                {
                    prevRule->next = nextRule;
                }
                else if(rule == router->rules[i])
                {
                    router->rules[i] = nextRule;
                }

                /* Prepend to delete list */
                rule->next = delRules[i];
                delRules[i] = rule;
            }
            else
            {
                rule->waiting = TRUE;           /* Pending events, mark as waiting */
                prevRule = rule;
            }
        }
    }

    mutexUnlock(router->rulesMutex);      /* -- unlock */


    /* Free old rules outside of lock */

    for(i = 0; i < MIDI_ROUTER_RULE_COUNT; i++)
    {
        for(rule = delRules[i]; rule; rule = nextRule)
        {
            nextRule = rule->next;
            FREE(rule);
        }
    }

    return OK;
}

/**
 * Add a rule to a MIDI router.
 * @param router MIDI router
 * @param rule Rule to add (used directly and should not be accessed again following a
 *   successful call to this function).
 * @param type The type of rule to add (#midiRouterRuleType)
 * @return #OK on success, #FAILED otherwise (invalid rule for example)
 * @since 1.1.0
 */
int
midiRouterAddRule(midiRouterT *router, midiRouterRuleT *rule,
                           int type)
{
    midiRouterRuleT *freeRules, *nextRule;

    returnValIfFail(router != NULL, FAILED);
    returnValIfFail(rule != NULL, FAILED);
    returnValIfFail(type >= 0 && type < MIDI_ROUTER_RULE_COUNT, FAILED);


    mutexLock(router->rulesMutex);        /* ++ lock */

    /* Take over free rules list, if any (to free outside of lock) */
    freeRules = router->freeRules;
    router->freeRules = NULL;

    rule->next = router->rules[type];
    router->rules[type] = rule;

    mutexUnlock(router->rulesMutex);      /* -- unlock */


    /* Free any deactivated rules which were waiting for events and are now done */

    for(; freeRules; freeRules = nextRule)
    {
        nextRule = freeRules->next;
        FREE(freeRules);
    }

    return OK;
}

/**
 * Create a new MIDI router rule.
 *
 * @return Newly allocated router rule or NULL if out of memory.
 *
 * The new rule is a "unity" rule which will accept any values and wont modify
 * them.
 *
 * @since 1.1.0
 */
midiRouterRuleT *
newMidiRouterRule(void)
{
    midiRouterRuleT *rule;

    rule = NEW(midiRouterRuleT);

    if(rule == NULL)
    {
        LOG(ERR, "Out of memory");
        return NULL;
    }

    MEMSET(rule, 0, sizeof(midiRouterRuleT));

    rule->chanMin = 0;
    rule->chanMax = 999999;
    rule->chanMul = 1.0;
    rule->chanAdd = 0;
    rule->par1_min = 0;
    rule->par1_max = 999999;
    rule->par1_mul = 1.0;
    rule->par1_add = 0;
    rule->par2_min = 0;
    rule->par2_max = 999999;
    rule->par2_mul = 1.0;
    rule->par2_add = 0;

    return rule;
};

/**
 * Free a MIDI router rule.
 *
 * @param rule Router rule to free
 *
 * Note that rules which have been added to a router are managed by the router,
 * so this function should seldom be needed.
 *
 * @since 1.1.0
 */
void
deleteMidiRouterRule(midiRouterRuleT *rule)
{
    returnIfFail(rule != NULL);
    FREE(rule);
}

/**
 * Set the channel portion of a rule.
 *
 * @param rule MIDI router rule
 * @param min Minimum value for rule match
 * @param max Maximum value for rule match
 * @param mul Value which is multiplied by matching event's channel value (1.0 to not modify)
 * @param add Value which is added to matching event's channel value (0 to not modify)
 *
 * The \a min and \a max parameters define a channel range window to match
 * incoming events to.  If \a min is less than or equal to \a max then an event
 * is matched if its channel is within the defined range (including \a min
 * and \a max). If \a min is greater than \a max then rule is inverted and matches
 * everything except in *between* the defined range (so \a min and \a max would match).
 *
 * The \a mul and \a add values are used to modify event channel values prior to
 * sending the event, if the rule matches.
 *
 * @since 1.1.0
 */
void
midiRouterRuleSetChan(midiRouterRuleT *rule, int min, int max,
                                float mul, int add)
{
    returnIfFail(rule != NULL);
    rule->chanMin = min;
    rule->chanMax = max;
    rule->chanMul = mul;
    rule->chanAdd = add;
}

/**
 * Set the first parameter portion of a rule.
 *
 * @param rule MIDI router rule
 * @param min Minimum value for rule match
 * @param max Maximum value for rule match
 * @param mul Value which is multiplied by matching event's 1st parameter value (1.0 to not modify)
 * @param add Value which is added to matching event's 1st parameter value (0 to not modify)
 *
 * The 1st parameter of an event depends on the type of event.  For note events
 * its the MIDI note #, for CC events its the MIDI control number, for program
 * change events its the MIDI program #, for pitch bend events its the bend value,
 * for channel pressure its the channel pressure value and for key pressure
 * its the MIDI note number.
 *
 * Pitch bend values have a maximum value of 16383 (8192 is pitch bend center) and all
 * other events have a max of 127.  All events have a minimum value of 0.
 *
 * The \a min and \a max parameters define a parameter range window to match
 * incoming events to.  If \a min is less than or equal to \a max then an event
 * is matched if its 1st parameter is within the defined range (including \a min
 * and \a max). If \a min is greater than \a max then rule is inverted and matches
 * everything except in *between* the defined range (so \a min and \a max would match).
 *
 * The \a mul and \a add values are used to modify event 1st parameter values prior to
 * sending the event, if the rule matches.
 *
 * @since 1.1.0
 */
void
midiRouterRuleSetParam1(midiRouterRuleT *rule, int min, int max,
                                  float mul, int add)
{
    returnIfFail(rule != NULL);
    rule->par1_min = min;
    rule->par1_max = max;
    rule->par1_mul = mul;
    rule->par1_add = add;
}

/**
 * Set the second parameter portion of a rule.
 *
 * @param rule MIDI router rule
 * @param min Minimum value for rule match
 * @param max Maximum value for rule match
 * @param mul Value which is multiplied by matching event's 2nd parameter value (1.0 to not modify)
 * @param add Value which is added to matching event's 2nd parameter value (0 to not modify)
 *
 * The 2nd parameter of an event depends on the type of event.  For note events
 * its the MIDI velocity, for CC events its the control value and for key pressure
 * events its the key pressure value.  All other types lack a 2nd parameter.
 *
 * All applicable 2nd parameters have the range 0-127.
 *
 * The \a min and \a max parameters define a parameter range window to match
 * incoming events to.  If \a min is less than or equal to \a max then an event
 * is matched if its 2nd parameter is within the defined range (including \a min
 * and \a max). If \a min is greater than \a max then rule is inverted and matches
 * everything except in *between* the defined range (so \a min and \a max would match).
 *
 * The \a mul and \a add values are used to modify event 2nd parameter values prior to
 * sending the event, if the rule matches.
 *
 * @since 1.1.0
 */
void
midiRouterRuleSetParam2(midiRouterRuleT *rule, int min, int max,
                                  float mul, int add)
{
    returnIfFail(rule != NULL);
    rule->par2_min = min;
    rule->par2_max = max;
    rule->par2_mul = mul;
    rule->par2_add = add;
}

/**
 * Handle a MIDI event through a MIDI router instance.
 * @param data MIDI router instance #midiRouterT, its a void * so that
 *   this function can be used as a callback for other subsystems
 *   (newMidiDriver() for example).
 * @param event MIDI event to handle
 * @return #OK if all rules were applied successfully, #FAILED if
 *  an error occurred while applying a rule or (since 2.2.2) the event was
 *  ignored because a parameter was out-of-range after the rule had been applied.
 *  See the note below.
 *
 * Purpose: The midi router is called for each event, that is received
 * via the 'physical' midi input. Each event can trigger an arbitrary number
 * of generated events (one for each rule that matches).
 *
 * In default mode, a noteon event is just forwarded to the synth's 'noteon' function,
 * a 'CC' event to the synth's 'CC' function and so on.
 *
 * The router can be used to:
 * - filter messages (for example: Pass sustain pedal CCs only to selected channels)
 * - split the keyboard (noteon with notenr < x: to ch 1, >x to ch 2)
 * - layer sounds (for each noteon received on ch 1, create a noteon on ch1, ch2, ch3,...)
 * - velocity scaling (for each noteon event, scale the velocity by 1.27 to give DX7 users
 *   a chance)
 * - velocity switching ("v <=100: Angel Choir; V > 100: Hell's Bells")
 * - get rid of aftertouch
 * - ...
 *
 * @note Each input event has values (ch, par1, par2) that could be changed by a rule.
 * After a rule has been applied on any value and the value is out of range, the event
 * can be either ignored or the value can be clamped depending on the type of the event:
 * - To get full benefice of the rule the value is clamped and the event passed to the output.
 * - To avoid MIDI messages conflicts at the output, the event is ignored
 *   (i.e not passed to the output).
 *   - ch out of range: event is ignored regardless of the event type.
 *   - par1 out of range: event is ignored for PROG_CHANGE or CONTROL_CHANGE type,
 *     par1 is clamped otherwise.
 *   - par2 out of range: par2 is clamped regardless of the event type.
 */
int
midiRouterHandleMidiEvent(void *data, midiEventT *event)
{
    midiRouterT *router = (midiRouterT *)data;
    midiRouterRuleT **rulep, *rule, *nextRule, *prevRule = NULL;
    int eventHasPar2 = 0; /* Flag, indicates that current event needs two parameters */
    int isPar1_ignored = 0; /* Flag, indicates that current event should be
                                ignored/clamped when par1 is getting out of range
                                value after the rule had been applied:1:ignored, 0:clamped */
    int par1_max = 127;     /* Range limit for par1 */
    int par2_max = 127;     /* Range limit for par2 */
    int retVal = OK;

    int chan; /* Channel of the generated event */
    int par1; /* par1 of the generated event */
    int par2;
    int eventPar1;
    int eventPar2;
    midiEventT newEvent;

    /* Some keyboards report noteoff through a noteon event with vel=0.
     * Convert those to noteoff to ease processing. */
    if(event->type == NOTE_ON && event->param2 == 0)
    {
        event->type = NOTE_OFF;
        event->param2 = 127;        /* Release velocity */
    }

    mutexLock(router->rulesMutex);    /* ++ lock rules */

    /* Depending on the event type, choose the correct list of rules. */
    switch(event->type)
    {
    /* For NOTE_ON event, par1(pitch) and par2(velocity) will be clamped if
       they are out of range after the rule had been applied */
    case NOTE_ON:
        rulep = &router->rules[MIDI_ROUTER_RULE_NOTE];
        eventHasPar2 = 1;
        break;

    /* For NOTE_OFF event, par1(pitch) and par2(velocity) will be clamped if
       they are out of range after the rule had been applied */
    case NOTE_OFF:
        rulep = &router->rules[MIDI_ROUTER_RULE_NOTE];
        eventHasPar2 = 1;
        break;

    /* CONTROL_CHANGE event will be ignored if par1 (ctrl num) is out
       of range after the rule had been applied */
    case CONTROL_CHANGE:
        rulep = &router->rules[MIDI_ROUTER_RULE_CC];
        eventHasPar2 = 1;
        isPar1_ignored = 1;
        break;

    /* PROGRAM_CHANGE event will be ignored if par1 (program num) is out
       of range after the rule had been applied */
    case PROGRAM_CHANGE:
        rulep = &router->rules[MIDI_ROUTER_RULE_PROG_CHANGE];
        isPar1_ignored = 1;
        break;

    /* For PITCH_BEND event, par1(bend value) will be clamped if
       it is out of range after the rule had been applied */
    case PITCH_BEND:
        rulep = &router->rules[MIDI_ROUTER_RULE_PITCH_BEND];
        par1_max = 16383;
        break;

    /* For CHANNEL_PRESSURE event, par1(pressure value) will be clamped if
       it is out of range after the rule had been applied */
    case CHANNEL_PRESSURE:
        rulep = &router->rules[MIDI_ROUTER_RULE_CHANNEL_PRESSURE];
        break;

    /* For KEY_PRESSURE event, par1(pitch) and par2(pressure value) will be
       clamped if they are out of range after the rule had been applied */
    case KEY_PRESSURE:
        rulep = &router->rules[MIDI_ROUTER_RULE_KEY_PRESSURE];
        eventHasPar2 = 1;
        break;

    case MIDI_SYSTEM_RESET:
    case MIDI_SYSEX:
        retVal = router->eventHandler(router->eventHandlerData, event);
        mutexUnlock(router->rulesMutex);   /* -- unlock rules */
        return retVal;

    default:
        rulep = NULL;    /* Event will not be passed on */
        break;
    }

    /* Loop over rules in the list, looking for matches for this event. */
    for(rule = rulep ? *rulep : NULL; rule; prevRule = rule, rule = nextRule)
    {
        eventPar1 = (int)event->param1;
        eventPar2 = (int)event->param2;
        nextRule = rule->next;     /* Rule may get removed from list, so get next here */

        /* Channel window */
        if(rule->chanMin > rule->chanMax)
        {
            /* Inverted rule: Exclude everything between max and min (but not min/max) */
            if(event->channel > rule->chanMax && event->channel < rule->chanMin)
            {
                continue;
            }
        }
        else        /* Normal rule: Exclude everything < max or > min (but not min/max) */
        {
            if(event->channel > rule->chanMax || event->channel < rule->chanMin)
            {
                continue;
            }
        }

        /* Par 1 window */
        if(rule->par1_min > rule->par1_max)
        {
            /* Inverted rule: Exclude everything between max and min (but not min/max) */
            if(eventPar1 > rule->par1_max && eventPar1 < rule->par1_min)
            {
                continue;
            }
        }
        else        /* Normal rule: Exclude everything < max or > min (but not min/max)*/
        {
            if(eventPar1 > rule->par1_max || eventPar1 < rule->par1_min)
            {
                continue;
            }
        }

        /* Par 2 window (only applies to event types, which have 2 pars)
         * For noteoff events, velocity switching doesn't make any sense.
         * Velocity scaling might be useful, though.
         */
        if(eventHasPar2 && event->type != NOTE_OFF)
        {
            if(rule->par2_min > rule->par2_max)
            {
                /* Inverted rule: Exclude everything between max and min (but not min/max) */
                if(eventPar2 > rule->par2_max && eventPar2 < rule->par2_min)
                {
                    continue;
                }
            }
            else      /* Normal rule: Exclude everything < max or > min (but not min/max)*/
            {
                if(eventPar2 > rule->par2_max || eventPar2 < rule->par2_min)
                {
                    continue;
                }
            }
        }

        /* Channel scaling / offset
         * Note: rule->chanMul will probably be 0 or 1. If it's 0, input from all
         * input channels is mapped to the same synth channel.
         */
        chan = rule->chanAdd + (int)((realT)event->channel * rule->chanMul
                     + (realT)0.5);

        /* We ignore the event if chan is out of range */
        if((chan < 0) || (chan >= router->nrMidiChannels))
        {
            retVal = FAILED;
            continue; /* go to next rule */
        }

        /* par 1 scaling / offset */
        par1 = rule->par1_add + (int)((realT)eventPar1 * rule->par1_mul
                     + (realT)0.5);

        if(isPar1_ignored)
        {
            /* We ignore the event if par1 is out of range */
            if((par1 < 0) || (par1 > par1_max))
            {
                retVal = FAILED;
                continue; /* go to next rule */
            }
        }
        else
        {
            /* par1 range clamping */
            if(par1 < 0)
            {
                par1 = 0;
            }
            else if(par1 > par1_max)
            {
                par1 = par1_max;
            }
        }

        /* par 2 scaling / offset, if applicable */
        if(eventHasPar2)
        {
            par2 = rule->par2_add + (int)((realT)eventPar2 * rule->par2_mul
                         + (realT)0.5);

            /* par2 range clamping */
            if(par2 < 0)
            {
                par2 = 0;
            }
            else if(par2 > par2_max)
            {
                par2 = par2_max;
            }
        }
        else
        {
            par2 = 0;
        }

        /* At this point we have to create an event of event->type on 'chan' with par1 (maybe par2).
         * We keep track on the state of noteon and sustain pedal events. If the application tries
         * to delete a rule, it will only be fully removed, if pending noteoff / pedal off events have
         * arrived. In the meantime while waiting, it will only let through 'negative' events
         * (noteoff or pedal up).
         */
        if(event->type == NOTE_ON || (event->type == CONTROL_CHANGE
                                      && par1 == SUSTAIN_SWITCH && par2 >= 64))
        {
            /* Noteon or sustain pedal down event generated */
            if(rule->keysCc[par1] == 0)
            {
                rule->keysCc[par1] = 1;
                rule->pendingEvents++;
            }
        }
        else if(event->type == NOTE_OFF || (event->type == CONTROL_CHANGE
                                            && par1 == SUSTAIN_SWITCH && par2 < 64))
        {
            /* Noteoff or sustain pedal up event generated */
            if(rule->keysCc[par1] > 0)
            {
                rule->keysCc[par1] = 0;
                rule->pendingEvents--;

                /* Rule is waiting for negative event to be destroyed? */
                if(rule->waiting)
                {
                    if(rule->pendingEvents == 0)
                    {
                        /* Remove rule from rule list */
                        if(prevRule)
                        {
                            prevRule->next = nextRule;
                        }
                        else
                        {
                            *rulep = nextRule;
                        }

                        /* Add to free list */
                        rule->next = router->freeRules;
                        router->freeRules = rule;

                        rule = prevRule;   /* Set rule to previous rule, which gets assigned to the next prevRule value (in for() statement) */
                    }

                    goto sendEvent;      /* Pass the event to complete the cycle */
                }
            }
        }

        /* Rule is still waiting for negative event? (note off or pedal up) */
        if(rule->waiting)
        {
            continue;    /* Skip (rule is inactive except for matching negative event) */
        }

sendEvent:

        /* At this point it is decided, what is sent to the synth.
         * Create a new event and make the appropriate call */

        midiEventSetType(&newEvent, event->type);
        midiEventSetChannel(&newEvent, chan);
        newEvent.param1 = par1;
        newEvent.param2 = par2;

        /* On failure, continue to process events, but return failure to caller. */
        if(router->eventHandler(router->eventHandlerData, &newEvent) != OK)
        {
            retVal = FAILED;
        }
    }

    mutexUnlock(router->rulesMutex);          /* -- unlock rules */

    return retVal;
}

/**
 * MIDI event callback function to display event information to stdout
 * @param data MIDI router instance
 * @param event MIDI event data
 * @return #OK on success, #FAILED otherwise
 *
 * An implementation of the #handleMidiEventFuncT function type, used for
 * displaying MIDI event information between the MIDI driver and router to
 * stdout.  Useful for adding into a MIDI router chain for debugging MIDI events.
 */
int midiDumpPrerouter(void *data, midiEventT *event)
{
    switch(event->type)
    {
    case NOTE_ON:
        fprintf(stdout, "eventPreNoteon %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    case NOTE_OFF:
        fprintf(stdout, "eventPreNoteoff %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    case CONTROL_CHANGE:
        fprintf(stdout, "eventPreCc %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    case PROGRAM_CHANGE:
        fprintf(stdout, "eventPreProg %i %i\n", event->channel, event->param1);
        break;

    case PITCH_BEND:
        fprintf(stdout, "eventPrePitch %i %i\n", event->channel, event->param1);
        break;

    case CHANNEL_PRESSURE:
        fprintf(stdout, "eventPreCpress %i %i\n", event->channel, event->param1);
        break;

    case KEY_PRESSURE:
        fprintf(stdout, "eventPreKpress %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    default:
        break;
    }

    return midiRouterHandleMidiEvent((midiRouterT *) data, event);
}

/**
 * MIDI event callback function to display event information to stdout
 * @param data MIDI router instance
 * @param event MIDI event data
 * @return #OK on success, #FAILED otherwise
 *
 * An implementation of the #handleMidiEventFuncT function type, used for
 * displaying MIDI event information between the MIDI driver and router to
 * stdout.  Useful for adding into a MIDI router chain for debugging MIDI events.
 */
int midiDumpPostrouter(void *data, midiEventT *event)
{
    switch(event->type)
    {
    case NOTE_ON:
        fprintf(stdout, "eventPostNoteon %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    case NOTE_OFF:
        fprintf(stdout, "eventPostNoteoff %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    case CONTROL_CHANGE:
        fprintf(stdout, "eventPostCc %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    case PROGRAM_CHANGE:
        fprintf(stdout, "eventPostProg %i %i\n", event->channel, event->param1);
        break;

    case PITCH_BEND:
        fprintf(stdout, "eventPostPitch %i %i\n", event->channel, event->param1);
        break;

    case CHANNEL_PRESSURE:
        fprintf(stdout, "eventPostCpress %i %i\n", event->channel, event->param1);
        break;

    case KEY_PRESSURE:
        fprintf(stdout, "eventPostKpress %i %i %i\n",
                event->channel, event->param1, event->param2);
        break;

    default:
        break;
    }

    return synthHandleMidiEvent((Synthesizer *) data, event);
}
