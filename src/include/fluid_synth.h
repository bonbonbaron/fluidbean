/* FluidSynth - A Software Synthesizer
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


#ifndef _FLUID_SYNTH_H
#define _FLUID_SYNTH_H


/***************************************************************
 *
 *                         INCLUDES
 */

//#include "fluid_sfont.h"
#include "fluid_chorus.h"
#include "fluid_mod.h"
#include "fluid_rev.h"
#include "fluid_list.h"

/***************************************************************
 *
 *                         DEFINES
 */
#define FLUID_NUM_PROGRAMS      128
#define DRUM_INST_BANK		128

#if defined(WITH_FLOAT)
#define FLUID_SAMPLE_FORMAT     FLUID_SAMPLE_FLOAT
#else
#define FLUID_SAMPLE_FORMAT     FLUID_SAMPLE_DOUBLE
#endif


typedef enum {
  VERBOSE = 0x01,
  DUMP    = 0x02,
  REVERB_IS_ACTIVE = 0x04,
  CHORUS_IS_ACTIVE = 0x08,
  LADSPA_IS_ACTIVE = 0x10,
  DRUM_CHANNEL_IS_ACTIVE = 0x11
} SettingsFlag;

typedef struct {
  S32 val, min, max;
} Setting;

struct _FluidSettings {
  U8   flags;    //  Refer to SettingsFlag definition above.
  S8   midiPortName[100];  // TODO: There must be a better way to do this.
  //  synth settings
  Setting synthNAudioChannels;
  Setting synthNAudioGroups;
  Setting synthNEffectsChannels;
  Setting synthSampleRate;
  Setting synthMinNoteLen;
  Setting synthPolyphony;
  Setting synthNMidiChannels;
  Setting synthGain;
};

#define settingSet_(setting_, val_, min_, max_) \
  setting_.val = val_; \
  setting_.min = min_; \
  setting_.max = max_; 

/***************************************************************
 *
 *                         ENUM
 */
enum fluid_loop {
	FLUID_UNLOOPED = 0,
	FLUID_LOOP_DURING_RELEASE = 1,
	FLUID_NOTUSED = 2,
	FLUID_LOOP_UNTIL_RELEASE = 3
};

enum fluid_synth_status {
	FLUID_SYNTH_CLEAN,
	FLUID_SYNTH_PLAYING,
	FLUID_SYNTH_QUIET,
	FLUID_SYNTH_STOPPED
};


typedef struct _fluid_bank_offset_t fluid_bank_offset_t;

struct _fluid_bank_offset_t {
	S32 sfont_id;
	S32 offset;
};


/*
 * fluid_synth_t
 */

struct _fluid_synth_t {
	/* fluid_settings_old_t settings_old;  the old synthesizer settings */
  struct _FluidSettings *settingsP;
	S32 polyphony;										 /** maximum polyphony */
	S8 with_reverb;									 /** Should the synth use the built-in reverb unit? */
	S8 with_chorus;									 /** Should the synth use the built-in chorus unit? */
	S8 verbose;											 /** Turn verbose mode on? */
	S8 dump;												 /** Dump events to stdout to hook up a user interface? */
	double sample_rate;								 /** The sample rate */
	S32 midi_channels;								 /** the number of MIDI channels (>= 16) */
	S32 audio_channels;								 /** the number of audio channels (1 channel=left+right) */
	S32 audio_groups;									 /** the number of (stereo) 'sub'groups from the synth.
					 Typically equal to audio_channels. */
	S32 effects_channels;							 /** the number of effects channels (= 2) */
	U32 state;								 /** the synthesizer state */
	U32 ticks;								 /** the number of audio samples since the start */

	fluid_list_t *loaders;							/** the soundfont loaders */
	fluid_list_t *sfont;								/** the loaded soundfont */
	U32 sfont_id;
	fluid_list_t *bank_offsets;				/** the offsets of the soundfont banks */

#if defined(MACOS9)
	fluid_list_t *unloading;						/** the soundfonts that need to be unloaded */
#endif

	double gain;												/** master gain */
	fluid_channel_t **channel;					/** the channels */
	S32 num_channels;										/** the number of channels */
	S32 nvoice;													/** the length of the synthesis process array */
	fluid_voice_t **voice;							/** the synthesis processes */
	U32 noteid;								/** the id is incremented for every new note. it's used for noteoff's  */
	U32 storeid;
	S32 nbuf;														/** How many audio buffers are used? (depends on nr of audio channels / groups)*/

	fluid_real_t **left_buf;
	fluid_real_t **right_buf;
	fluid_real_t **fx_left_buf;
	fluid_real_t **fx_right_buf;

	fluid_revmodel_t *reverb;
	fluid_chorus_t *chorus;
	S32 cur;													 /** the current sample in the audio buffers to be output */
	S32 dither_index;							/* current index in random dither value buffer: fluid_synth_(write_s16|dither_s16) */

	S8 outbuf[256];									 /** buffer for message output */

	fluid_tuning_t ***tuning;						/** 128 banks of 128 programs for the tunings */
	fluid_tuning_t *cur_tuning;					/** current tuning in the iteration */

	U32 min_note_length_ticks;	/**< If note-offs are triggered just after a note-on, they will be delayed */
};


fluid_synth_t *new_fluid_synth (FluidSettings *settingsP);
/** returns 1 if the value has been set, 0 otherwise */
S32 fluid_synth_setstr (fluid_synth_t * synth, S8 *name, S8 *str);

/** returns 1 if the value exists, 0 otherwise */
S32 fluid_synth_getstr (fluid_synth_t * synth, S8 *name, S8 **str);

/** returns 1 if the value has been set, 0 otherwise */
S32 fluid_synth_setnum (fluid_synth_t * synth, S8 *name, double val);

/** returns 1 if the value exists, 0 otherwise */
S32 fluid_synth_getnum (fluid_synth_t * synth, S8 *name, double *val);

/** returns 1 if the value has been set, 0 otherwise */
S32 fluid_synth_setint (fluid_synth_t * synth, S8 *name, S32 val);

/** returns 1 if the value exists, 0 otherwise */
S32 fluid_synth_getint (fluid_synth_t * synth, S8 *name, S32 *val);


S32 fluid_synth_set_reverb_preset (fluid_synth_t * synth, S32 num);

S32 fluid_synth_one_block (fluid_synth_t * synth, S32 do_not_mix_fx_to_out);

fluid_preset_t *fluid_synth_get_preset (fluid_synth_t * synth,
																				U32 sfontnum,
																				U32 banknum,
																				U32 prognum);

fluid_preset_t *fluid_synth_find_preset (fluid_synth_t * synth,
																				 U32 banknum,
																				 U32 prognum);

S32 fluid_synth_all_notes_off (fluid_synth_t * synth, S32 chan);
S32 fluid_synth_all_sounds_off (fluid_synth_t * synth, S32 chan);
S32 fluid_synth_modulate_voices (fluid_synth_t * synth, S32 chan, S32 is_cc,
																 S32 ctrl);
S32 fluid_synth_modulate_voices_all (fluid_synth_t * synth, S32 chan);
S32 fluid_synth_damp_voices (fluid_synth_t * synth, S32 chan);
S32 fluid_synth_kill_voice (fluid_synth_t * synth, fluid_voice_t * voice);
void fluid_synth_kill_by_exclusive_class (fluid_synth_t * synth,
																					fluid_voice_t * voice);
void fluid_synth_release_voice_on_same_note (fluid_synth_t * synth, S32 chan,
																						 S32 key);
void fluid_synth_sfunload_macos9 (fluid_synth_t * synth);

void fluid_synth_print_voice (fluid_synth_t * synth);

/** This function assures that every MIDI channels has a valid preset
 *  (NULL is okay). This function is called after a SoundFont is
 *  unloaded or reloaded. */
void fluid_synth_update_presets (fluid_synth_t * synth);


S32 fluid_synth_update_gain (fluid_synth_t * synth, S8 *name, double value);
S32 fluid_synth_update_polyphony (fluid_synth_t * synth, S8 *name,
																	S32 value);

fluid_bank_offset_t *fluid_synth_get_bank_offset0 (fluid_synth_t * synth,
																									 S32 sfont_id);
void fluid_synth_remove_bank_offset (fluid_synth_t * synth, S32 sfont_id);

void fluid_synth_dither_s16 (S32 *dither_index, S32 len, float *lin,
														 float *rin, void *lout, S32 loff, S32 lincr,
														 void *rout, S32 roff, S32 rincr);
/*
 * misc
 */

void fluid_synth_settings (FluidSettings *settingsP);

S32 fluid_synth_sfload (fluid_synth_t * synth, void *sfDataP, U32 sfDataLen, S32 reset_presets);
#endif /* _FLUID_SYNTH_H */
