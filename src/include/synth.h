struct _Synthesizer;

#ifndef _FLUID_SYNTH_H
#define _FLUID_SYNTH_H
#include "fluidbean.h"
#include "chan.h"
#include "voice.h"
#include "rev.h"
#include "soundfont.h"
/**
 * Synthesis interpolation method.
 */
enum fluid_interp {
    FLUID_INTERP_NONE = 0,        /**< No interpolation: Fastest, but questionable audio quality */
    FLUID_INTERP_LINEAR = 1,      /**< Straight-line interpolation: A bit slower, reasonable audio quality */
    FLUID_INTERP_4THORDER = 4,    /**< Fourth-order interpolation, good quality, the default */
    FLUID_INTERP_7THORDER = 7,    /**< Seventh-order interpolation */

    FLUID_INTERP_DEFAULT = FLUID_INTERP_4THORDER, /**< Default interpolation method */
    FLUID_INTERP_HIGHEST = FLUID_INTERP_7THORDER, /**< Highest interpolation method */
};


/**
 * Enum used with fluid_synth_add_default_mod() to specify how to handle duplicate modulators.
 */
enum fluid_synth_add_mod
{
    FLUID_SYNTH_OVERWRITE,        /**< Overwrite any existing matching modulator */
    FLUID_SYNTH_ADD,              /**< Sum up modulator amounts */
};

/***************************************************************
 *
 *                         INCLUDES
 */


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

typedef struct _SynthSettings {
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
} SynthSettings;

extern struct _SynthSettings synthSettings;

#define settingSet_(setting_, val_, min_, max_) \
  setting_.val = val_; \
  setting_.min = min_; \
  setting_.max = max_; 

/* ENUM */
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


/* * Synthesizer */
typedef struct _Synthesizer {
	/* fluid_settings_old_t settings_old;  the old synthesizer settings */
  struct _SynthSettings *settingsP;
	S32 polyphony;										 /** maximum polyphony */
	S8 with_reverb;									 /** Should the synth use the built-in reverb unit? */
	S8 with_chorus;									 /** Should the synth use the built-in chorus unit? */
	S8 verbose;											 /** Turn verbose mode on? */
	S8 dump;												 /** Dump events to stdout to hook up a user interface? */
	double sample_rate;								 /** The sample rate */
	S32 midi_channels;								 /** the number of MIDI channels (>= 16) */
	S32 audio_channels;								 /** the number of audio channels (1 channel=left+right) */
	S32 audio_groups;									 /** the number of (stereo) 'sub'groups from the synth.  Typically equal to audio_channels. */
	S32 effects_channels;							 /** the number of effects channels (= 2) */
	U32 state;								 /** the synthesizer state */
	U32 ticks;								 /** the number of audio samples since the start */
	double gain;												/** master gain */
	struct _Channel **channel;					/** the channels */
	S32 num_channels;										/** the number of channels */
	S32 nvoice;													/** the length of the synthesis process array */
	Voice **voice;							/** the synthesis processes */
	U32 noteid;								/** the id is incremented for every new note. it's used for noteoff's  */
	U32 storeid;
	S32 nbuf;														/** How many audio buffers are used? (depends on nr of audio channels / groups)*/
	fluid_real_t **left_buf;
	fluid_real_t **right_buf;
	fluid_real_t **fx_left_buf;
	fluid_real_t **fx_right_buf;
	fluid_revmodel_t *reverb;
	struct _Chorus *chorus;
	S32 cur;													 /** the current sample in the audio buffers to be output */
	S32 dither_index;							/* current index in random dither value buffer: fluid_synth_(write_s16|dither_s16) */
	S8 outbuf[256];									 /** buffer for message output */
	fluid_tuning_t ***tuning;						/** 128 banks of 128 programs for the tunings */
	fluid_tuning_t *cur_tuning;					/** current tuning in the iteration */
	U32 min_note_length_ticks;	/**< If note-offs are triggered just after a note-on, they will be delayed */
  Soundfont *soundfontP;
} Synthesizer;


Synthesizer *new_fluid_synth ();
/** returns 1 if the value has been set, 0 otherwise */
S32 fluid_synth_setstr (Synthesizer * synth, S8 *name, S8 *str);

/** returns 1 if the value exists, 0 otherwise */
S32 fluid_synth_getstr (Synthesizer * synth, S8 *name, S8 **str);

/** returns 1 if the value has been set, 0 otherwise */
S32 fluid_synth_setnum (Synthesizer * synth, S8 *name, double val);

/** returns 1 if the value exists, 0 otherwise */
S32 fluid_synth_getnum (Synthesizer * synth, S8 *name, double *val);

/** returns 1 if the value has been set, 0 otherwise */
S32 fluid_synth_setint (Synthesizer * synth, S8 *name, S32 val);

/** returns 1 if the value exists, 0 otherwise */
S32 fluid_synth_getint (Synthesizer * synth, S8 *name, S32 *val);


S32 fluid_synth_set_reverb_preset (Synthesizer * synth, S32 num);

S32 fluid_synth_one_block (Synthesizer * synth, S32 do_not_mix_fx_to_out);

Preset *fluid_synth_get_preset (Synthesizer * synth,
																				U32 sfontnum,
																				U32 banknum,
																				U32 prognum);

Preset *fluid_synth_find_preset (Synthesizer * synth,
																				 U32 banknum,
																				 U32 prognum);

S32 fluid_synth_all_notes_off (Synthesizer * synth, S32 chan);
S32 fluid_synth_all_sounds_off (Synthesizer * synth, S32 chan);
S32 fluid_synth_modulate_voices (Synthesizer * synth, S32 chan, S32 is_cc,
																 S32 ctrl);
S32 fluid_synth_modulate_voices_all (Synthesizer * synth, S32 chan);
S32 fluid_synth_damp_voices (Synthesizer * synth, S32 chan);
S32 fluid_synth_kill_voice (Synthesizer * synth, Voice * voice);
void fluid_synth_kill_by_exclusive_class (Synthesizer * synth,
																					Voice * voice);
void fluid_synth_release_voice_on_same_note (Synthesizer * synth, S32 chan,
																						 S32 key);
void fluid_synth_sfunload_macos9 (Synthesizer * synth);

void fluid_synth_print_voice (Synthesizer * synth);

/** This function assures that every MIDI channels has a valid preset
 *  (NULL is okay). This function is called after a SoundFont is
 *  unloaded or reloaded. */
void fluid_synth_update_presets (Synthesizer * synth);


S32 fluid_synth_update_gain (Synthesizer * synth, S8 *name, double value);
S32 fluid_synth_update_polyphony (Synthesizer * synth, S8 *name,
																	S32 value);

fluid_bank_offset_t *fluid_synth_get_bank_offset0 (Synthesizer * synth,
																									 S32 sfont_id);
void fluid_synth_remove_bank_offset (Synthesizer * synth, S32 sfont_id);

void fluid_synth_dither_s16 (S32 *dither_index, S32 len, float *lin,
														 float *rin, void *lout, S32 loff, S32 lincr,
														 void *rout, S32 roff, S32 rincr);
/*
 * misc
 */

void fluid_synth_settings (SynthSettings *settingsP);

S32 fluid_synth_sfload (Synthesizer * synth, void *sfDataP, U32 sfDataLen, S32 reset_presets);
#endif /* _FLUID_SYNTH_H */
