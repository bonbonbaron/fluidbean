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


/*

  More information about micro tuning can be found at:

  http://www.midi.org/about-midi/tuning.htm
  http://www.midi.org/about-midi/tuning-scale.htm
  http://www.midi.org/about-midi/tuningExtens.htm

*/

#ifndef _TUNING_H
#define _TUNING_H

#include "fluidbean.h"

typedef struct _fluidTuningT {
	S8 *name;
	S32 bank;
	S32 prog;
	double pitch[128];						/* the pitch of every key, in cents */
} tuningT;

tuningT *newTuning (const S8 *name, S32 bank, S32 prog);
tuningT *tuningDuplicate (tuningT * tuning);
void deleteTuning (tuningT * tuning);

void tuningSetName (tuningT * tuning, const S8 *name);
S8 *tuningGetName (tuningT * tuning);

#define tuningGetBank(_t) ((_t)->bank)
#define tuningGetProg(_t) ((_t)->prog)

void tuningSetPitch (tuningT * tuning, S32 key, double pitch);
#define tuningGetPitch(_t, _key) ((_t)->pitch[_key])

void tuningSetOctave (tuningT * tuning,
															const double *pitchDeriv);

void tuningSetAll (tuningT * tuning, double *pitch);
#define tuningGetAll(_t) (&(_t)->pitch[0])




#endif /* _TUNING_H */
