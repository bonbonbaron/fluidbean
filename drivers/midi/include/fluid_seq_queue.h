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

#ifndef _FLUID_SEQ_QUE_H
#define _FLUID_SEQ_QUE_H

#include "fluidsynth.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "fluidEvent.h"
#include "fluidSeqbindNotes.h"

void* newFluidSeqQueue(int nbEvents);
void deleteFluidSeqQueue(void *queue);
int fluidSeqQueuePush(void *queue, const fluidEventT *evt);
void fluidSeqQueueRemove(void *queue, fluidSeqIdT src, fluidSeqIdT dest, int type);
void fluidSeqQueueProcess(void *que, fluidSequencerT *seq, unsigned int curTicks);
void fluidSeqQueueInvalidateNotePrivate(void *que, fluidSeqIdT dest, fluidNoteIdT id);

int eventCompareForTest(const fluidEventT* left, const fluidEventT* right);

#ifdef __cplusplus
}
#endif

#endif /* _FLUID_SEQ_QUE_H */
