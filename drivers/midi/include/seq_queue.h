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

#ifndef _SEQ_QUE_H
#define _SEQ_QUE_H

#include "synth.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "Event.h"
#include "SeqbindNotes.h"

void* newSeqQueue(int nbEvents);
void deleteSeqQueue(void *queue);
int SeqQueuePush(void *queue, const EventT *evt);
void SeqQueueRemove(void *queue, SeqIdT src, SeqIdT dest, int type);
void SeqQueueProcess(void *que, SequencerT *seq, unsigned int curTicks);
void SeqQueueInvalidateNotePrivate(void *que, SeqIdT dest, NoteIdT id);

int eventCompareForTest(const EventT* left, const EventT* right);

#ifdef __cplusplus
}
#endif

#endif /* _SEQ_QUE_H */
