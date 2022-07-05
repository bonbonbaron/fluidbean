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
 *            :         |                                    |   |
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


void fluid_chorus_triangle (int *buf, int len, int depth);
void fluid_chorus_sine (int *buf, int len, int depth);

Chorus *new_fluid_chorus (fluid_real_t sample_rate) {
	int i;
	int ii;
	Chorus *chorus;

	chorus = FLUID_NEW (Chorus);
	if (chorus == NULL) 
		return NULL;

	FLUID_MEMSET (chorus, 0, sizeof (Chorus));
	chorus->sample_rate = sample_rate;

	/* Lookup table for the SI function (impulse response of an ideal low pass) */
	/* i: Offset in terms of whole samples */
	for (i = 0; i < INTERPOLATION_SAMPLES; i++) {
		/* ii: Offset in terms of fractional samples ('subsamples') */
		for (ii = 0; ii < INTERPOLATION_SUBSAMPLES; ii++) {
			/* Move the origin into the center of the table */
			double i_shifted = ((double) i - ((double) INTERPOLATION_SAMPLES) / 2.
													+ (double) ii / (double) INTERPOLATION_SUBSAMPLES);
			if (fabs (i_shifted) < 0.000001) {
				/* sinc(0) cannot be calculated straightforward (limit needed
				   for 0/0) */
				chorus->sinc_table[i][ii] = (fluid_real_t) 1.;
			} else {
				chorus->sinc_table[i][ii] =
					(fluid_real_t) sin (i_shifted * M_PI) / (M_PI * i_shifted);
				/* Hamming window */
				chorus->sinc_table[i][ii] *=
					(fluid_real_t) 0.5 *(1.0 +
															 cos (2.0 * M_PI * i_shifted /
																		(fluid_real_t) INTERPOLATION_SAMPLES));
			};
		};
	};

	/* allocate lookup tables */
	chorus->lookup_tab =
		FLUID_ARRAY (int, (int) (chorus->sample_rate / MIN_SPEED_HZ));
	if (chorus->lookup_tab == NULL) 
		goto error_recovery;

	/* allocate sample buffer */

	chorus->chorusbuf = FLUID_ARRAY (fluid_real_t, MAX_SAMPLES);
	if (chorus->chorusbuf == NULL) 
		goto error_recovery;

	if (fluid_chorus_init (chorus) != FLUID_OK) 
		goto error_recovery;

	return chorus;

error_recovery:
	delete_fluid_chorus (chorus);
	return NULL;
}


int fluid_chorus_init (Chorus * chorus) {
	int i;

	for (i = 0; i < MAX_SAMPLES; i++) 
		chorus->chorusbuf[i] = 0.0;

	/* initialize the chorus with the default settings */
  chorus->number_blocks = FLUID_CHORUS_DEFAULT_N;
  chorus->level = FLUID_CHORUS_DEFAULT_N;
  chorus->speed_Hz = FLUID_CHORUS_DEFAULT_SPEED;
  chorus->depth_ms = FLUID_CHORUS_DEFAULT_DEPTH;
  chorus->type = FLUID_CHORUS_MOD_SINE;

	return fluid_chorus_update (chorus);
}

void delete_fluid_chorus (Chorus * chorus) {
	if (chorus == NULL) {
		return;
	}

	if (chorus->chorusbuf != NULL) {
		FLUID_FREE (chorus->chorusbuf);
	}

	if (chorus->lookup_tab != NULL) {
		FLUID_FREE (chorus->lookup_tab);
	}

	FLUID_FREE (chorus);
}


/* Purpose:
 * Calculates the internal chorus parameters using the settings from
 * fluid_chorus_set_xxx. */
int fluid_chorus_update (Chorus * chorus) {
	int i;
	int modulation_depth_samples;

	if (chorus->new_number_blocks < 0) {
		chorus->new_number_blocks = 0;
	} else if (chorus->new_number_blocks > MAX_CHORUS) {
		chorus->new_number_blocks = MAX_CHORUS;
	};

	if (chorus->new_speed_Hz < MIN_SPEED_HZ) {
		chorus->new_speed_Hz = MIN_SPEED_HZ;
	} else if (chorus->new_speed_Hz > MAX_SPEED_HZ) {
		chorus->new_speed_Hz = MAX_SPEED_HZ;
	}
	if (chorus->new_depth_ms < 0.0) {
		chorus->new_depth_ms = 0.0;
	}
	/* Depth: Check for too high value through modulation_depth_samples. */

	if (chorus->new_level < 0.0) {
		chorus->new_level = 0.0;
	} else if (chorus->new_level > 10) {
		chorus->new_level = 0.1;
	}

	/* The modulating LFO goes through a full period every x samples: */
	chorus->modulation_period_samples =
		chorus->sample_rate / chorus->new_speed_Hz;

	/* The variation in delay time is x: */
	modulation_depth_samples = (int)
		(chorus->new_depth_ms / 1000.0	/* convert modulation depth in ms to s */
		 * chorus->sample_rate);

	if (modulation_depth_samples > MAX_SAMPLES) {
		modulation_depth_samples = MAX_SAMPLES;
	}

	/* initialize LFO table */
	if (chorus->type == FLUID_CHORUS_MOD_SINE) {
		fluid_chorus_sine (chorus->lookup_tab, chorus->modulation_period_samples,
											 modulation_depth_samples);
	} else if (chorus->type == FLUID_CHORUS_MOD_TRIANGLE) {
		fluid_chorus_triangle (chorus->lookup_tab,
													 chorus->modulation_period_samples,
													 modulation_depth_samples);
	} else {
		chorus->type = FLUID_CHORUS_MOD_SINE;
		fluid_chorus_sine (chorus->lookup_tab, chorus->modulation_period_samples,
											 modulation_depth_samples);
	};

	for (i = 0; i < chorus->number_blocks; i++) {
		/* Set the phase of the chorus blocks equally spaced */
		chorus->phase[i] = (int) ((double) chorus->modulation_period_samples
															* (double) i / (double) chorus->number_blocks);
	}

	/* Start of the circular buffer */
	chorus->counter = 0;

	chorus->type = chorus->new_type;
	chorus->depth_ms = chorus->new_depth_ms;
	chorus->level = chorus->new_level;
	chorus->speed_Hz = chorus->new_speed_Hz;
	chorus->number_blocks = chorus->new_number_blocks;
	return FLUID_OK;

}


void
fluid_chorus_processmix (Chorus * chorus, fluid_real_t * in,
												 fluid_real_t * left_out, fluid_real_t * right_out) {
	int sample_index;
	int i;
	fluid_real_t d_in, d_out;

	for (sample_index = 0; sample_index < FLUID_BUFSIZE; sample_index++) {

		d_in = in[sample_index];
		d_out = 0.0f;

#if 0
		/* Debug: Listen to the chorus signal only */
		left_out[sample_index] = 0;
		right_out[sample_index] = 0;
#endif

		/* Write the current sample into the circular buffer */
		chorus->chorusbuf[chorus->counter] = d_in;

		for (i = 0; i < chorus->number_blocks; i++) {
			int ii;
			/* Calculate the delay in subsamples for the delay line of chorus block nr. */

			/* The value in the lookup table is so, that this expression
			 * will always be positive.  It will always include a number of
			 * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
			 * remain positive at all times. */
			int pos_subsamples = (INTERPOLATION_SUBSAMPLES * chorus->counter
														- chorus->lookup_tab[chorus->phase[i]]);

			int pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

			/* modulo divide by INTERPOLATION_SUBSAMPLES */
			pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

			for (ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
				/* Add the delayed signal to the chorus sum d_out Note: The
				 * delay in the delay line moves backwards for increasing
				 * delay!*/

				/* The & in chorusbuf[...] is equivalent to a division modulo
				   MAX_SAMPLES, only faster. */
				d_out += chorus->chorusbuf[pos_samples & MAX_SAMPLES_ANDMASK]
					* chorus->sinc_table[ii][pos_subsamples];

				pos_samples--;
			};
			/* Cycle the phase of the modulating LFO */
			chorus->phase[i]++;
			chorus->phase[i] %= (chorus->modulation_period_samples);
		}														/* foreach chorus block */

		d_out *= chorus->level;

		/* Add the chorus sum d_out to output */
		left_out[sample_index] += d_out;
		right_out[sample_index] += d_out;

		/* Move forward in circular buffer */
		chorus->counter++;
		chorus->counter %= MAX_SAMPLES;

	}															/* foreach sample */
}

/* Duplication of code ... (replaces sample data instead of mixing) */
void
fluid_chorus_processreplace (Chorus * chorus, fluid_real_t * in,
														 fluid_real_t * left_out,
														 fluid_real_t * right_out) {
	int sample_index;
	int i;
	fluid_real_t d_in, d_out;

	for (sample_index = 0; sample_index < FLUID_BUFSIZE; sample_index++) {

		d_in = in[sample_index];
		d_out = 0.0f;

#if 0
		/* Debug: Listen to the chorus signal only */
		left_out[sample_index] = 0;
		right_out[sample_index] = 0;
#endif

		/* Write the current sample into the circular buffer */
		chorus->chorusbuf[chorus->counter] = d_in;

		for (i = 0; i < chorus->number_blocks; i++) {
			int ii;
			/* Calculate the delay in subsamples for the delay line of chorus block nr. */

			/* The value in the lookup table is so, that this expression
			 * will always be positive.  It will always include a number of
			 * full periods of MAX_SAMPLES*INTERPOLATION_SUBSAMPLES to
			 * remain positive at all times. */
			int pos_subsamples = (INTERPOLATION_SUBSAMPLES * chorus->counter
														- chorus->lookup_tab[chorus->phase[i]]);

			int pos_samples = pos_subsamples / INTERPOLATION_SUBSAMPLES;

			/* modulo divide by INTERPOLATION_SUBSAMPLES */
			pos_subsamples &= INTERPOLATION_SUBSAMPLES_ANDMASK;

			for (ii = 0; ii < INTERPOLATION_SAMPLES; ii++) {
				/* Add the delayed signal to the chorus sum d_out Note: The
				 * delay in the delay line moves backwards for increasing
				 * delay!*/

				/* The & in chorusbuf[...] is equivalent to a division modulo
				   MAX_SAMPLES, only faster. */
				d_out += chorus->chorusbuf[pos_samples & MAX_SAMPLES_ANDMASK]
					* chorus->sinc_table[ii][pos_subsamples];

				pos_samples--;
			};
			/* Cycle the phase of the modulating LFO */
			chorus->phase[i]++;
			chorus->phase[i] %= (chorus->modulation_period_samples);
		}														/* foreach chorus block */

		d_out *= chorus->level;

		/* Store the chorus sum d_out to output */
		left_out[sample_index] = d_out;
		right_out[sample_index] = d_out;

		/* Move forward in circular buffer */
		chorus->counter++;
		chorus->counter %= MAX_SAMPLES;

	}															/* foreach sample */
}

/* Purpose:
 *
 * Calculates a modulation waveform (sine) Its value ( modulo
 * MAXSAMPLES) varies between 0 and depth*INTERPOLATION_SUBSAMPLES.
 * Its period length is len.  The waveform data will be used modulo
 * MAXSAMPLES only.  Since MAXSAMPLES is substracted from the waveform
 * a couple of times here, the resulting (current position in
 * buffer)-(waveform sample) will always be positive.
 */
void fluid_chorus_sine (int *buf, int len, int depth) {
	int i;
	double val;

	for (i = 0; i < len; i++) {
		val = sin ((double) i / (double) len * 2.0 * M_PI);
		buf[i] =
			(int) ((1.0 +
							val) * (double) depth / 2.0 *
						 (double) INTERPOLATION_SUBSAMPLES);
		buf[i] -= 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;
		//    printf("%i %i\n",i,buf[i]);
	}
}

/* Purpose:
 * Calculates a modulation waveform (triangle)
 * See fluid_chorus_sine for comments.
 */
void fluid_chorus_triangle (int *buf, int len, int depth) {
	int i = 0;
	int ii = len - 1;
	double val;
	double val2;

	while (i <= ii) {
		val = i * 2.0 / len * (double) depth *(double) INTERPOLATION_SUBSAMPLES;
		val2 = (int) (val + 0.5) - 3 * MAX_SAMPLES * INTERPOLATION_SUBSAMPLES;
		buf[i++] = (int) val2;
		buf[ii--] = (int) val2;
	}
}

void fluid_chorus_reset (Chorus * chorus) {
	fluid_chorus_init (chorus);
}
