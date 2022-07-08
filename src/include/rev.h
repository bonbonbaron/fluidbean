/* Synth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */


#ifndef _REV_H
#define _REV_H

#include "fluidbean.h"

#define REVERB_DEFAULT_ROOMSIZE 0.2f			/**< Default reverb room size */
#define REVERB_DEFAULT_DAMP 0.0f					/**< Default reverb damping */
#define REVERB_DEFAULT_WIDTH 0.5f					/**< Default reverb width */
#define REVERB_DEFAULT_LEVEL 0.9f					/**< Default reverb level */

typedef struct _fluidRevmodelT revmodelT;


/*
 * reverb
 */
revmodelT *newRevmodel (void);
void deleteRevmodel (revmodelT * rev);

void revmodelProcessmix (revmodelT * rev, S16 * in,
																S16 * leftOut,
																S16 * rightOut);

void revmodelProcessreplace (revmodelT * rev, S16 * in,
																		S16 * leftOut,
																		S16 * rightOut);

void revmodelReset (revmodelT * rev);

void revmodelSetroomsize (revmodelT * rev, S16 value);
void revmodelSetdamp (revmodelT * rev, S16 value);
void revmodelSetlevel (revmodelT * rev, S16 value);
void revmodelSetwidth (revmodelT * rev, S16 value);
void revmodelSetmode (revmodelT * rev, S16 value);

S16 revmodelGetroomsize (revmodelT * rev);
S16 revmodelGetdamp (revmodelT * rev);
S16 revmodelGetlevel (revmodelT * rev);
S16 revmodelGetwidth (revmodelT * rev);

/*
 * reverb preset
 */
typedef struct _fluidRevmodelPresetsT {
	S8 *name;
	S16 roomsize;
	S16 damp;
	S16 width;
	S16 level;
} revmodelPresetsT;


#endif /* _REV_H */
