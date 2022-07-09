#include "tuning.h"


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
