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


#include "include/tuning.h"
#include "include/fluidbean.h"


tuningT *newTuning (const char *name, int bank, int prog) {
	tuningT *tuning;
	int i;

	tuning = NEW (tuningT);
	if (tuning == NULL) {
		return NULL;
	}

	tuning->name = NULL;

	if (name != NULL) {
		tuning->name = STRDUP (name);
	}

	tuning->bank = bank;
	tuning->prog = prog;

	for (i = 0; i < 128; i++) {
		tuning->pitch[i] = i * 100.0;
	}

	return tuning;
}

/* Duplicate a tuning */
tuningT *tuningDuplicate (tuningT * tuning) {
	tuningT *newTuning;
	int i;

	newTuning = NEW (tuningT);

	if (!newTuning) {
		return NULL;
	}

	if (tuning->name) {
		newTuning->name = STRDUP (tuning->name);

		if (!newTuning->name) {
			FREE (newTuning);
			return NULL;
		}
	} else
		newTuning->name = NULL;

	newTuning->bank = tuning->bank;
	newTuning->prog = tuning->prog;

	for (i = 0; i < 128; i++)
		newTuning->pitch[i] = tuning->pitch[i];

	return newTuning;
}

void deleteTuning (tuningT * tuning) {
	if (tuning == NULL) {
		return;
	}
	if (tuning->name != NULL) {
		FREE (tuning->name);
	}
	FREE (tuning);
}

void tuningSetName (tuningT * tuning, const char *name) {
	if (tuning->name != NULL) {
		FREE (tuning->name);
		tuning->name = NULL;
	}
	if (name != NULL) {
		tuning->name = STRDUP (name);
	}
}

char *tuningGetName (tuningT * tuning) {
	return tuning->name;
}

void tuningSetKey (tuningT * tuning, int key, double pitch) {
	tuning->pitch[key] = pitch;
}

void
tuningSetOctave (tuningT * tuning, const double *pitchDeriv) {
	int i;

	for (i = 0; i < 128; i++) {
		tuning->pitch[i] = i * 100.0 + pitchDeriv[i % 12];
	}
}

void tuningSetAll (tuningT * tuning, double *pitch) {
	int i;

	for (i = 0; i < 128; i++) {
		tuning->pitch[i] = pitch[i];
	}
}

void tuningSetPitch (tuningT * tuning, int key, double pitch) {
	if ((key >= 0) && (key < 128)) {
		tuning->pitch[key] = pitch;
	}
}
