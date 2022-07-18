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

#ifndef _SEQBIND_NOTE_H
#define _SEQBIND_NOTE_H

#include "synth.h"
#include "Event.h"

#ifdef __cplusplus
extern "C" {
#endif

NoteIdT NoteComputeId(int chan, short key);
void* newNoteContainer(void);
void deleteNoteContainer(void *cont);
int NoteContainerInsert(void* cont, NoteIdT id);
void NoteContainerRemove(void* cont, NoteIdT id);
void NoteContainerClear(void* cont);

#ifdef __cplusplus
}
#endif

#endif /* _SEQBIND_NOTE_H */
