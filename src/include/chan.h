struct _Channel;
#ifndef _FLUID_CHAN_H
#define _FLUID_CHAN_H

#include "fluidbean.h"
#include "tuning.h"
#include "midi.h"
#include "gen.h"

/* Channel */
typedef struct _Channel {
	S32 channum;
	U32 sfontnum;
	U32 banknum;
	U32 prognum;
	Preset *presetP;
	struct _Synthesizer *synth;
	S8 key_pressure[128];
	S16 channel_pressure;
	S16 pitch_bend;
	S16 pitch_wheel_sensitivity;
	/* controller values */
	S16 cc[128];
	/* cached values of last MSB values of MSB/LSB controllers */
	U8 bank_msb;
	S32 interp_method;
	fluid_tuning_t *tuning;  /* the micro-tuning */
	/* NRPN system */
	S16 nrpn_select;
	S16 nrpn_active;						/* 1 if data entry CCs are for NRPN, 0 if RPN */
	/* The values of the generators, set by NRPN messages, or by * fluid_synth_set_gen(), are cached in the channel so they can be * applied to future notes. They are copied to a voice's generators * in fluid_voice_init(), wihich calls fluid_gen_init().  */
	fluid_real_t gen[GEN_LAST];
	/* By default, the NRPN values are relative to the values of the * generators set in the SoundFont. For example, if the NRPN * specifies an attack of 100 msec then 100 msec will be added to the * combined attack time of the sound font and the modulators.  * * However, it is useful to be able to specify the generator value * absolutely, completely ignoring the generators of the sound font * and the values of modulators. The gen_abs field, is a boolean * flag indicating whether the NRPN value is absolute or not.
	 */
	S8 gen_abs[GEN_LAST];
} Channel;

Channel *new_fluid_channel (struct _Synthesizer * synth, S32 num);
S32 delete_fluid_channel (Channel * chan);
void fluid_channel_init (Channel * chan);
void fluid_channel_init_ctrl (Channel * chan, S32 is_all_ctrl_off);
void fluid_channel_reset (Channel * chan);
S32 fluid_channel_cc (Channel * chan, S32 ctrl, S32 val);
S32 fluid_channel_pressure (Channel * chan, S32 val);
S32 fluid_channel_pitch_bend (Channel * chan, S32 val);
S32 fluid_channel_pitch_wheel_sens (Channel * chan, S32 val);

#define fluid_channel_get_key_pressure(chan, key) \
  ((chan)->key_pressure[key])
#define fluid_channel_set_key_pressure(chan, key, val) \
  ((chan)->key_pressure[key] = (val))
#define fluid_channel_set_tuning(_c, _t)        { (_c)->tuning = _t; }
#define fluid_channel_has_tuning(_c)            ((_c)->tuning != NULL)
#define fluid_channel_get_tuning(_c)            ((_c)->tuning)
#define fluid_channel_sustained(_c)             ((_c)->cc[SUSTAIN_SWITCH] >= 64)
#define fluid_channel_set_gen(_c, _n, _v, _a)   { (_c)->gen[_n] = _v; (_c)->gen_abs[_n] = _a; }
#define fluid_channel_get_gen(_c, _n)           ((_c)->gen[_n])
#define fluid_channel_get_gen_abs(_c, _n)       ((_c)->gen_abs[_n])

#define fluid_channel_get_min_note_length_ticks(chan) \
  ((chan)->synth->min_note_length_ticks)

#endif /* _FLUID_CHAN_H */
