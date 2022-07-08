struct _Chorus;

#ifndef _CHORUS_H
#define _CHORUS_H

#include "fluidbean.h"
#include <math.h>

#define MAX_CHORUS	99
#define MAX_DELAY	100
#define MAX_DEPTH	10
#define MIN_SPEED_HZ	0.29
#define MAX_SPEED_HZ    5
#define MAX_SAMPLES_LN2 12

#define MAX_SAMPLES (1 << (MAX_SAMPLES_LN2-1))
#define MAX_SAMPLES_ANDMASK (MAX_SAMPLES-1)

/* Interpolate how many steps between samples? Must be power of two
   For example: 8 => use a resolution of 256 steps between any two
   samples
*/
#define INTERPOLATION_SUBSAMPLES_LN2 8
#define INTERPOLATION_SUBSAMPLES (1 << (INTERPOLATION_SUBSAMPLES_LN2-1))
#define INTERPOLATION_SUBSAMPLES_ANDMASK (INTERPOLATION_SUBSAMPLES-1)

/* Use how many samples for interpolation? Must be odd.  '7' sounds
   relatively clean, when listening to the modulated delay signal
   alone.  For a demo on aliasing try '1' With '3', the aliasing is
   still quite pronounced for some input frequencies
*/
#define INTERPOLATION_SAMPLES 5

#define CHORUS_DEFAULT_N 3																/**< Default chorus voice count */
#define CHORUS_DEFAULT_LEVEL 2.0f													/**< Default chorus level */
#define CHORUS_DEFAULT_SPEED 0.3f													/**< Default chorus speed */
#define CHORUS_DEFAULT_DEPTH 8.0f													/**< Default chorus depth */
#define CHORUS_DEFAULT_TYPE CHORUS_MOD_SINE					/**< Default chorus waveform type */

/**
 * Chorus modulation waveform type.
 */
enum chorusMod
{
    CHORUS_MOD_SINE = 0,            /**< Sine wave chorus modulation */
    CHORUS_MOD_TRIANGLE = 1         /**< Triangle wave chorus modulation */
};

typedef struct _Chorus {
	/* Store the values between chorusSetXxx and chorusUpdate
	 * Logic behind this:
	 * - both 'parameter' and 'newParameter' hold the same value.
	 * - To change the chorus settings, 'newParameter' is modified and
	 *   chorusUpdate is called.
	 * - If the new value is valid, it is copied to 'parameter'.
	 * - If it is invalid, 'newParameter' is restored to 'parameter'.
	 */
	int type;											/* current value */
	int newType;									/* next value, if parameter check is OK */
	S16 depthMs;				/* current value */
	S16 newDepthMs;		/* next value, if parameter check is OK */
	S16 level;						/* current value */
	S16 newLevel;				/* next value, if parameter check is OK */
	S16 speed_Hz;				/* current value */
	S16 newSpeed_Hz;		/* next value, if parameter check is OK */
	int numberBlocks;						/* current value */
	int newNumberBlocks;				/* next value, if parameter check is OK */

	S16 *chorusbuf;
	int counter;
	long phase[MAX_CHORUS];
	long modulationPeriodSamples;
	int *lookupTab;
	S16 sampleRate;

	/* sinc lookup table */
	S16 sincTable[INTERPOLATION_SAMPLES][INTERPOLATION_SUBSAMPLES];
} Chorus;

/* * chorus */
Chorus *newChorus (S16 sampleRate);
void deleteChorus (Chorus * chorus);
void chorusProcessmix (Chorus * chorus, S16 * in,
															S16 * leftOut,
															S16 * rightOut);
void chorusProcessreplace (Chorus * chorus, S16 * in,
																	S16 * leftOut,
																	S16 * rightOut);

S32 chorusInit (Chorus * chorus);
void chorusReset (Chorus * chorus);

void chorusSetNr (Chorus * chorus, S32 nr);
void chorusSetLevel (Chorus * chorus, S16 level);
void chorusSetSpeed_Hz (Chorus * chorus,
																S16 speed_Hz);
void chorusSetDepthMs (Chorus * chorus,
																S16 depthMs);
void chorusSetType (Chorus * chorus, S32 type);
S32 chorusUpdate (Chorus * chorus);
S32 chorusGetNr (Chorus * chorus);
S16 chorusGetLevel (Chorus * chorus);
S16 chorusGetSpeed_Hz (Chorus * chorus);
S16 chorusGetDepthMs (Chorus * chorus);
S32 chorusGetType (Chorus * chorus);


#endif /* _CHORUS_H */
