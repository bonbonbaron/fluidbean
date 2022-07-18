/*

  CHANGES

  - Adapted for fluidsynth, Peter Hanappe, March 2002

  - Variable delay line implementation using bandlimited
    interpolation, code reorganization: Markus Nentwig May 2002

 */


/*
 * 	Chorus effect.
 *
 * Flow diagram scheme for n delays ( 1 <= n <= MAX_CHORUS ):
 *
 *        * gain-in                                           ___
 * ibuff -----+--------------------------------------------->|   |
 *            |      _________                               |   |
 *            |     |         |                   * level 1  |   |
 *            +---->| delay 1 |----------------------------->|   |
 *            |     |_________|                              |   |
 *            |        /|\                                   |   |
 *            :         |         (mod wave sine or triangle)|   |
 *            : +-----------------+   +--------------+       | + |
 *            : | Delay control 1 |<--| mod. speed 1 |       |   |
 *            : +-----------------+   +--------------+       |   |
 *            |      _________                               |   |
 *            |     |         |                   * level n  |   |
 *            +---->| delay n |----------------------------->|   |
 *                  |_________|                              |   |
 *                     /|\                                   |___|
 *                      |                                      |
 *              +-----------------+   +--------------+         | * gain-out
 *              | Delay control n |<--| mod. speed n |         |
 *              +-----------------+   +--------------+         +----->obuff
 *
 *
 * The delay i is controlled by a sine or triangle modulation i ( 1 <= i <= n).
 *
 * The delay of each block is modulated between 0..depth ms
 *
 */


/* Variable delay line implementation
 * ==================================
 *
 * The modulated delay needs the value of the delayed signal between
 * samples.  A lowpass filter is used to obtain intermediate values
 * between samples (bandlimited interpolation).  The sample pulse
 * train is convoluted with the impulse response of the low pass
 * filter (sinc function).  To make it work with a small number of
 * samples, the sinc function is windowed (Hamming window).
 *
 */

#include "chorus.h"
#include "soundfont.h"

/* Length of one delay line in samples:
 * Set through MAX_SAMPLES_LN2.
 * For example:
 * MAX_SAMPLES_LN2=12
 * => MAX_SAMPLES=pow(2,12)=4096
 * => MAX_SAMPLES_ANDMASK=4095
 */
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

/* Private data for SKEL file */


void chorusTriangle (int *buf, int len, int depth);
void chorusSine (int *buf, int len, int depth);

// MB: I was wrong to get worked up about this. newChorus() is only called at synth creation time.
Chorus *newChorus (S16 sampleRate) {
	Chorus *chorus = NEW (Chorus);
	if (chorus == NULL) 
		return NULL;
	MEMSET (chorus, 0, sizeof (Chorus));
	chorus->sampleRate = sampleRate;
  // Currently makes a 5 x 128 (640) array.
	/* Lookup table for the SI function (impulse response of an ideal low pass) */
	/* i: Offset in terms of whole samples */
	for (int i = 0; i < INTERPOLATION_SAMPLES; ++i) {
		/* ii: Offset in terms of fractional samples ('subsamples') */
		for (int ii = 0; ii < INTERPOLATION_SUBSAMPLES; +ii) {
			/* Move the origin into the center of the table */
                           // i - 5/2 + ii/128
			double iShifted =    ((double) i - ((double) INTERPOLATION_SAMPLES) / 2.
													+ (double) ii / (double) INTERPOLATION_SUBSAMPLES);
			if (fabs (iShifted) < 0.000001) {
				/* sinc(0) cannot be calculated straightforward (limit needed
				   for 0/0) */
				chorus->sincTable[i][ii] = (S16) 1.;
			} else {
        // sine is a 0-1 function being scaled down even further.
        // So when you multiply it by a cosine function also with 0-1 range,
        // you're going to get a very small number for lots of these.
        // That means we're going to have to figure out how to handle possible 
        // fixed point arithmetic *overflow*.
        // TODO: understand what the hell is going on here. Accomplish tonight's goal first.
				chorus->sincTable[i][ii] = (S16) sin (iShifted * M_PI) / (M_PI * iShifted);
				/* Hamming window */
				chorus->sincTable[i][ii] *=
					(S16) 0.5 *(1.0 + cos (2.0 * M_PI * iShifted / (S16) INTERPOLATION_SAMPLES));
			};
		};
	};

	/* allocate lookup tables */
	chorus->lookupTab = ARRAY (int, (int) (chorus->sampleRate / MIN_SPEED_HZ));
	if (chorus->lookupTab == NULL) 
		goto errorRecovery;

	/* allocate sample buffer */
	chorus->chorusbuf = ARRAY (S16, MAX_SAMPLES);
	if (chorus->chorusbuf == NULL) 
		goto errorRecovery;

	if (chorusInit (chorus) != OK) 
		goto errorRecovery;

	return chorus;

errorRecovery:
	deleteChorus (chorus);
	return NULL;
}


int chorusInit (Chorus * chorus) {
	int i;

  memset(chorus->chorusbuf, 0, sizeof(chorus->chorusbuf[0] * MAX_SAMPLES));

	/* initialize the chorus with the default settings */
  chorus->numberBlocks = CHORUS_DEFAULT_N;
  chorus->level = CHORUS_DEFAULT_N;
  chorus->speed_Hz = CHORUS_DEFAULT_SPEED;
  chorus->depthMs = CHORUS_DEFAULT_DEPTH;
  chorus->type = CHORUS_MOD_SINE;

	return chorusUpdate (chorus);
}

void deleteChorus (Chorus * chorus) {
	if (chorus == NULL) {
		return;
	}

	if (chorus->chorusbuf != NULL) {
		FREE (chorus->chorusbuf);
	}

	if (chorus->lookupTab != NULL) {
		FREE (chorus->lookupTab);
	}

	FREE (chorus);
}


#define clip_(dst_, min_, max_) \
  if (dst_ < min_) dst_ = min_; \
  else if (dst_ > max_) dst_ = max_;

/* chorusUpdate():
 * Calculates the internal chorus parameters using the settings from chorusSetXxx. */
int chorusUpdate (Chorus * chorus) {
  clip_(chorus->newNumberBlocks, 0, MAX_CHORUS);
  clip_(chorus->newSpeed_Hz, MIN_SPEED_HZ, MAX_SPEED_HZ);
	if (chorus->newDepthMs < 0.0) chorus->newDepthMs = 0.0;

	/* Depth: Check for too high value through modulationDepthSamples. */
  clip_(chorus->newLevel, 0, 10);

	/* The modulating LFO goes through a full period every x samples: */
	chorus->modulationPeriodSamples = chorus->sampleRate / chorus->newSpeed_Hz;

	/* The variation in delay time is x: */
	int modulationDepthSamples = (int) (chorus->newDepthMs / 1000.0 * chorus->sampleRate);

	if (modulationDepthSamples > MAX_SAMPLES) 
		modulationDepthSamples = MAX_SAMPLES;

	/* initialize LFO table */
	if (chorus->type == CHORUS_MOD_SINE) {
		chorusSine (chorus->lookupTab, chorus->modulationPeriodSamples,
											 modulationDepthSamples);
	} else if (chorus->type == CHORUS_MOD_TRIANGLE) {
		chorusTriangle (chorus->lookupTab, chorus->modulationPeriodSamples, modulationDepthSamples);
	} else {
		chorus->type = CHORUS_MOD_SINE;
		chorusSine (chorus->lookupTab, chorus->modulationPeriodSamples,
											 modulationDepthSamples);
	};

	for (int i = 0; i < chorus->numberBlocks; i++) 
		/* Set the phase of the chorus blocks equally spaced */
		chorus->phase[i] = (int) ((double) chorus->modulationPeriodSamples
															* (double) i / (double) chorus->numberBlocks);

	/* Start of the circular buffer */
	chorus->counter = 0;
	chorus->type = chorus->newType;
	chorus->depthMs = chorus->newDepthMs;
	chorus->level = chorus->newLevel;
	chorus->speed_Hz = chorus->newSpeed_Hz;
	chorus->numberBlocks = chorus->newNumberBlocks;
	return OK;
}

void chorusProcessmix (Chorus *chorus, S16 *in, S16 *leftOut, S16 *rightOut) {
	int sampleIndex;
	int i;
	S16 dIn, dOut;

	for (sampleIndex = 0; sampleIndex < BUFSIZE; sampleIndex++) {
		dIn = in[sampleIndex];
		dOut = 0.0f;

		/* Write the current sample into the circular buffer */
		chorus->chorusbuf[chorus->counter] = dIn;

		for (i = 0; i < chorus->numberBlocks; i++) {
			int ii;
			/* Calculate the delay in subsamples for the delay line of chorus block nr. */

			/* The value in the lookup table is so, that this expression
			 * will always be positive.  It will always include a number of
			 * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
			 * remain positive at all times. */
			int posSubsamples = (INTERPOLATION_SUBSAMPLES * chorus->counter
														- chorus->lookupTab[chorus->phase[i]]);

			int posSamples = posSubsamples / INTERPOLATION_SUBSAMPLES;

			/* modulo divide by INTERPOLATION_SUBSAMPLES */
			posSubsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

			for (ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
				/* Add the delayed signal to the chorus sum dOut Note: The
				 * delay in the delay line moves backwards for increasing
				 * delay!*/

				/* The & in chorusbuf[...] is equivalent to a division modulo
				   MAX_SAMPLES, only faster. */
				dOut += chorus->chorusbuf[posSamples & MAX_SAMPLES_ANDMASK]
					* chorus->sincTable[ii][posSubsamples];

				posSamples--;
			};
			/* Cycle the phase of the modulating LFO */
			chorus->phase[i]++;
			chorus->phase[i] %= (chorus->modulationPeriodSamples);
		}														/* foreach chorus block */

		dOut *= chorus->level;

		/* Add the chorus sum dOut to output */
		leftOut[sampleIndex] += dOut;
		rightOut[sampleIndex] += dOut;

		/* Move forward in circular buffer */
		chorus->counter++;
		chorus->counter %= MAX_SAMPLES;
	}															/* foreach sample */
}

/* Duplication of code ... (replaces sample data instead of mixing) */
void chorusProcessreplace (Chorus * chorus, S16 * in, S16 * leftOut, S16 * rightOut) {
	int sampleIndex;
	int i;
	S16 dIn, dOut;

	for (sampleIndex = 0; sampleIndex < BUFSIZE; sampleIndex++) {

		dIn = in[sampleIndex];
		dOut = 0.0f;

		/* Write the current sample into the circular buffer */
		chorus->chorusbuf[chorus->counter] = dIn;

		for (i = 0; i < chorus->numberBlocks; i++) {
			int ii;
			/* Calculate the delay in subsamples for the delay line of chorus block nr. */
			/* The value in the lookup table is so, that this expression
			 * will always be positive.  It will always include a number of
			 * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
			 * remain positive at all times. */
			int posSubsamples = (INTERPOLATION_SUBSAMPLES * chorus->counter
														- chorus->lookupTab[chorus->phase[i]]);

			int posSamples = posSubsamples / INTERPOLATION_SUBSAMPLES;

			/* modulo divide by INTERPOLATION_SUBSAMPLES */
			posSubsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

			for (ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
				/* Add the delayed signal to the chorus sum dOut Note: The
				 * delay in the delay line moves backwards for increasing
				 * delay!*/

				/* The & in chorusbuf[...] is equivalent to a division modulo
				   MAX_SAMPLES, only faster. */
				dOut += chorus->chorusbuf[posSamples & MAX_SAMPLES_ANDMASK]
					* chorus->sincTable[ii][posSubsamples];

				posSamples--;
			};
			/* Cycle the phase of the modulating LFO */
			chorus->phase[i]++;
			chorus->phase[i] %= (chorus->modulationPeriodSamples);
		}														/* foreach chorus block */

		dOut *= chorus->level;

		/* Store the chorus sum dOut to output */
		leftOut[sampleIndex] = dOut;
		rightOut[sampleIndex] = dOut;

		/* Move forward in circular buffer */
		chorus->counter++;
		chorus->counter %= MAX_SAMPLES;

	}															/* foreach sample */
}

// MB SLOW!! Computing sine in a loop, wow!
/* Purpose:
 *
 * Calculates a modulation waveform (sine) Its value ( modulo
 * MAXSAMPLES) varies between 0 and depth*INTERPOLATION_SUBSAMPLES.
 * The waveform data will be used modulo MAXSAMPLES only.  
 *
 * Since MAXSAMPLES is substracted from the waveform
 * a couple of times here, the resulting (current position in
 * buffer)-(waveform sample) will always be positive.
 */
void chorusSine (int *buf, int periodLen, int depth) {
	double val;
	for (int i = 0; i < periodLen; i++) {
		val = sin ((double) i / (double) periodLen * 2.0 * M_PI);
		buf[i] = (int) ((1.0 + val) 
                    * (double) depth 
                    / 2.0 * (double) INTERPOLATION_SUBSAMPLES
                   );
		buf[i] -= 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;
	}
}

/* Purpose:
 * Calculates a modulation waveform (triangle)
 * See chorusSine for comments.
 */
void chorusTriangle (int *buf, int len, int depth) {
	int i = 0;
	int ii = len - 1;
	double val;
	double val2;

	while (i <= ii) {
		val = i * 2.0 / len * (double) depth *(double) INTERPOLATION_SUBSAMPLES;
		val2 = (int) (val + 0.5) - 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;
		buf[i++] = (int) val2;
		buf[ii--] = (int) val2;   // triangle is symmetric, so populate end too.
	}
}

void chorusReset (Chorus * chorus) {
	chorusInit (chorus);
}
