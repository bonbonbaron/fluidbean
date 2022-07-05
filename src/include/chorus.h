struct _Chorus;

#ifndef _FLUID_CHORUS_H
#define _FLUID_CHORUS_H

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

#define FLUID_CHORUS_DEFAULT_N 3																/**< Default chorus voice count */
#define FLUID_CHORUS_DEFAULT_LEVEL 2.0f													/**< Default chorus level */
#define FLUID_CHORUS_DEFAULT_SPEED 0.3f													/**< Default chorus speed */
#define FLUID_CHORUS_DEFAULT_DEPTH 8.0f													/**< Default chorus depth */
#define FLUID_CHORUS_DEFAULT_TYPE FLUID_CHORUS_MOD_SINE					/**< Default chorus waveform type */

/**
 * Chorus modulation waveform type.
 */
enum fluid_chorus_mod
{
    FLUID_CHORUS_MOD_SINE = 0,            /**< Sine wave chorus modulation */
    FLUID_CHORUS_MOD_TRIANGLE = 1         /**< Triangle wave chorus modulation */
};

typedef struct _Chorus {
	/* Store the values between fluid_chorus_set_xxx and fluid_chorus_update
	 * Logic behind this:
	 * - both 'parameter' and 'new_parameter' hold the same value.
	 * - To change the chorus settings, 'new_parameter' is modified and
	 *   fluid_chorus_update is called.
	 * - If the new value is valid, it is copied to 'parameter'.
	 * - If it is invalid, 'new_parameter' is restored to 'parameter'.
	 */
	int type;											/* current value */
	int new_type;									/* next value, if parameter check is OK */
	fluid_real_t depth_ms;				/* current value */
	fluid_real_t new_depth_ms;		/* next value, if parameter check is OK */
	fluid_real_t level;						/* current value */
	fluid_real_t new_level;				/* next value, if parameter check is OK */
	fluid_real_t speed_Hz;				/* current value */
	fluid_real_t new_speed_Hz;		/* next value, if parameter check is OK */
	int number_blocks;						/* current value */
	int new_number_blocks;				/* next value, if parameter check is OK */

	fluid_real_t *chorusbuf;
	int counter;
	long phase[MAX_CHORUS];
	long modulation_period_samples;
	int *lookup_tab;
	fluid_real_t sample_rate;

	/* sinc lookup table */
	fluid_real_t sinc_table[INTERPOLATION_SAMPLES][INTERPOLATION_SUBSAMPLES];
} Chorus;

/* * chorus */
Chorus *new_fluid_chorus (fluid_real_t sample_rate);
void delete_fluid_chorus (Chorus * chorus);
void fluid_chorus_processmix (Chorus * chorus, fluid_real_t * in,
															fluid_real_t * left_out,
															fluid_real_t * right_out);
void fluid_chorus_processreplace (Chorus * chorus, fluid_real_t * in,
																	fluid_real_t * left_out,
																	fluid_real_t * right_out);

S32 fluid_chorus_init (Chorus * chorus);
void fluid_chorus_reset (Chorus * chorus);

void fluid_chorus_set_nr (Chorus * chorus, S32 nr);
void fluid_chorus_set_level (Chorus * chorus, fluid_real_t level);
void fluid_chorus_set_speed_Hz (Chorus * chorus,
																fluid_real_t speed_Hz);
void fluid_chorus_set_depth_ms (Chorus * chorus,
																fluid_real_t depth_ms);
void fluid_chorus_set_type (Chorus * chorus, S32 type);
S32 fluid_chorus_update (Chorus * chorus);
S32 fluid_chorus_get_nr (Chorus * chorus);
fluid_real_t fluid_chorus_get_level (Chorus * chorus);
fluid_real_t fluid_chorus_get_speed_Hz (Chorus * chorus);
fluid_real_t fluid_chorus_get_depth_ms (Chorus * chorus);
S32 fluid_chorus_get_type (Chorus * chorus);


#endif /* _FLUID_CHORUS_H */
