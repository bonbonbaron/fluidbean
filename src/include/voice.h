struct _Voice;

#ifndef _FLUID_VOICE_H
#define _FLUID_VOICE_H

#include "chan.h"
#include "gen.h"
#include "mod.h"
#include "phase.h"

#define NO_CHANNEL             0xff

enum fluid_voice_status {
	FLUID_VOICE_CLEAN,
	FLUID_VOICE_ON,
	FLUID_VOICE_SUSTAINED,
	FLUID_VOICE_OFF
};

/* envelope data */
typedef struct _fluid_env_data_t {
	U32 count;
	fluid_real_t coeff;
	fluid_real_t incr;
	fluid_real_t min;
	fluid_real_t max;
} fluid_env_data_t;

/* Indices for envelope tables */
enum fluid_voice_envelope_index_t {
	FLUID_VOICE_ENVDELAY,
	FLUID_VOICE_ENVATTACK,
	FLUID_VOICE_ENVHOLD,
	FLUID_VOICE_ENVDECAY,
	FLUID_VOICE_ENVSUSTAIN,
	FLUID_VOICE_ENVRELEASE,
	FLUID_VOICE_ENVFINISHED,
	FLUID_VOICE_ENVLAST
};

/*
 * Voice
 */
typedef struct _Voice {
	U32 id;							/* the id is incremented for every new noteon.
																   it's used for noteoff's  */
	U8 status;
	U8 chan;						/* the channel number, quick access for channel messages */
	U8 key;						/* the key, quick acces for noteoff */
	U8 vel;						/* the velocity */
	struct _Channel *channel;
	Generator gen[GEN_LAST];
	S32 has_looped;								/* Flag that is set as soon as the first loop is completed. */
	Sample *sampleP;
	S32 check_sample_sanity_flag;	/* Flag that initiates, that sample-related parameters
																   have to be checked. */

	/* basic parameters */
	fluid_real_t output_rate;			/* the sample rate of the synthesizer */

	U32 start_time;
	U32 ticks;
	U32 noteoff_ticks;		/* Delay note-off until this tick */

	fluid_real_t amp;							/* current linear amplitude */
	fluid_phase_t phase;					/* the phase of the sample wave */

	/* Temporary variables used in fluid_voice_write() */

	fluid_real_t phase_incr;			/* the phase increment for the next 64 samples */
	fluid_real_t amp_incr;				/* amplitude increment value */
	fluid_real_t *dsp_buf;				/* buffer to store interpolated sample data to */

	/* End temporary variables */

	/* basic parameters */
	fluid_real_t pitch;						/* the pitch in midicents */
	fluid_real_t attenuation;			/* the attenuation in centibels */
	fluid_real_t min_attenuation_cB;	/* Estimate on the smallest possible attenuation
																		 * during the lifetime of the voice */
	fluid_real_t root_pitch;

	/* sample and loop start and end points (offset in sample memory).  */
	S32 start;
	S32 end;
	S32 loopstart;
	S32 loopend;									/* Note: first point following the loop (superimposed on loopstart) */

	/* master gain */
	fluid_real_t synth_gain;

	/* vol env */
	fluid_env_data_t volenv_data[FLUID_VOICE_ENVLAST];
	U32 volenv_count;
	S32 volenv_section;
	fluid_real_t volenv_val;
	fluid_real_t amplitude_that_reaches_noise_floor_nonloop;
	fluid_real_t amplitude_that_reaches_noise_floor_loop;

	/* mod env */
	fluid_env_data_t modenv_data[FLUID_VOICE_ENVLAST];
	U32 modenv_count;
	S32 modenv_section;
	fluid_real_t modenv_val;			/* the value of the modulation envelope */
	fluid_real_t modenv_to_fc;
	fluid_real_t modenv_to_pitch;

	/* mod lfo */
	fluid_real_t modlfo_val;			/* the value of the modulation LFO */
	U32 modlfo_delay;		/* the delay of the lfo in samples */
	fluid_real_t modlfo_incr;			/* the lfo frequency is converted to a per-buffer increment */
	fluid_real_t modlfo_to_fc;
	fluid_real_t modlfo_to_pitch;
	fluid_real_t modlfo_to_vol;

	/* vib lfo */
	fluid_real_t viblfo_val;			/* the value of the vibrato LFO */
	U32 viblfo_delay;		/* the delay of the lfo in samples */
	fluid_real_t viblfo_incr;			/* the lfo frequency is converted to a per-buffer increment */
	fluid_real_t viblfo_to_pitch;

	/* resonant filter */
	fluid_real_t fres;						/* the resonance frequency, in cents (not absolute cents) */
	fluid_real_t last_fres;				/* Current resonance frequency of the IIR filter */
	/* Serves as a flag: A deviation between fres and last_fres */
	/* indicates, that the filter has to be recalculated. */
	fluid_real_t q_lin;						/* the q-factor on a linear scale */
	fluid_real_t filter_gain;			/* Gain correction factor, depends on q */
	fluid_real_t hist1, hist2;		/* Sample history for the IIR filter */
	S32 filter_startup;						/* Flag: If set, the filter will be set directly.
																   Else it changes smoothly. */

	/* filter coefficients */
	/* The coefficients are normalized to a0. */
	/* b0 and b2 are identical => b02 */
	fluid_real_t b02;							/* b0 / a0 */
	fluid_real_t b1;							/* b1 / a0 */
	fluid_real_t a1;							/* a0 / a0 */
	fluid_real_t a2;							/* a1 / a0 */

	fluid_real_t b02_incr;
	fluid_real_t b1_incr;
	fluid_real_t a1_incr;
	fluid_real_t a2_incr;
	S32 filter_coeff_incr_count;

	/* pan */
	fluid_real_t pan;
	fluid_real_t amp_left;
	fluid_real_t amp_right;

	/* reverb */
	fluid_real_t reverb_send;
	fluid_real_t amp_reverb;

	/* chorus */
	fluid_real_t chorus_send;
	fluid_real_t amp_chorus;

	S32 interp_method;

	/* for debugging */
	S32 debug;
} Voice;


Voice *new_fluid_voice (fluid_real_t output_rate);
S32 delete_fluid_voice (Voice * voice);

void fluid_voice_start (Voice * voice);

S32 fluid_voice_write (Voice * voice,
											 fluid_real_t * left, fluid_real_t * right,
											 fluid_real_t * reverb_buf, fluid_real_t * chorus_buf);

S32 fluid_voice_init (Voice * voice, Sample * sample,
											struct _Channel * channel, S32 key, S32 vel,
											U32 id, U32 time, fluid_real_t gain);

S32 fluid_voice_modulate (Voice * voice, S32 cc, S32 ctrl);
S32 fluid_voice_modulate_all (Voice * voice);

/** Set the NRPN value of a generator. */
S32 fluid_voice_set_param (Voice * voice, S32 gen, fluid_real_t value, S32 abs);


/** Set the gain. */
S32 fluid_voice_set_gain (Voice * voice, fluid_real_t gain);


/** Update all the synthesis parameters, which depend on generator
    'gen'. This is only necessary after changing a generator of an
    already operating voice.  Most applications will not need this
    function.*/
void fluid_voice_update_param (Voice * voice, S32 gen);

S32 fluid_voice_noteoff (Voice * voice);
S32 fluid_voice_off (Voice * voice);
S32 fluid_voice_calculate_runtime_synthesis_parameters (Voice *
																												voice);
struct _Channel *fluid_voice_get_channel (Voice * voice);
S32 calculate_hold_decay_buffers (Voice * voice, S32 gen_base,
																	S32 gen_key2base, S32 is_decay);
S32 fluid_voice_kill_excl (Voice * voice);
fluid_real_t fluid_voice_get_lower_boundary_for_attenuation (Voice *
																														 voice);
fluid_real_t
	fluid_voice_determine_amplitude_that_reaches_noise_floor_for_sample
	(Voice * voice);
void fluid_voice_check_sample_sanity (Voice * voice);

#define fluid_voice_set_id(_voice, _id)  { (_voice)->id = (_id); }
#define fluid_voice_get_chan(_voice)     (_voice)->chan


#define _PLAYING(voice)  (((voice)->status == FLUID_VOICE_ON) || ((voice)->status == FLUID_VOICE_SUSTAINED))

/* A voice is 'ON', if it has not yet received a noteoff
 * event. Sending a noteoff event will advance the envelopes to
 * section 5 (release). */
#define _ON(voice)  ((voice)->status == FLUID_VOICE_ON && (voice)->volenv_section < FLUID_VOICE_ENVRELEASE)
#define _SUSTAINED(voice)  ((voice)->status == FLUID_VOICE_SUSTAINED)
#define _AVAILABLE(voice)  (((voice)->status == FLUID_VOICE_CLEAN) || ((voice)->status == FLUID_VOICE_OFF))
#define _RELEASED(voice)  ((voice)->chan == NO_CHANNEL)
#define _SAMPLEMODE(voice) ((S32)(voice)->gen[GEN_SAMPLEMODE].amount.s16)

fluid_real_t fluid_voice_gen_value (Voice * voice, S32 num);

#define _GEN(_voice, _n) \
  ((fluid_real_t)(_voice)->gen[_n].val \
   + (fluid_real_t)(_voice)->gen[_n].mod \
   + (fluid_real_t)(_voice)->gen[_n].nrpn)

#define FLUID_SAMPLESANITY_CHECK (1 << 0)
#define FLUID_SAMPLESANITY_STARTUP (1 << 1)


/* defined in fluid_dsp_float.c */

void fluid_dsp_float_config (void);
S32 fluid_dsp_float_interpolate_none (Voice * voice);
S32 fluid_dsp_float_interpolate_linear (Voice * voice);
S32 fluid_dsp_float_interpolate_4th_order (Voice * voice);
S32 fluid_dsp_float_interpolate_7th_order (Voice * voice);

#endif /* _FLUID_VOICE_H */
