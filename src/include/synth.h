struct _Synthesizer;

#ifndef _SYNTH_H
#define _SYNTH_H
#include "fluidbean.h"
#include "chan.h"
#include "voice.h"
#include "rev.h"
#include "soundfont.h"
/**
 * Synthesis interpolation method.
 */
enum interp {
    INTERP_NONE = 0,        /**< No interpolation: Fastest, but questionable audio quality */
    INTERP_LINEAR = 1,      /**< Straight-line interpolation: A bit slower, reasonable audio quality */
    INTERP_4THORDER = 4,    /**< Fourth-order interpolation, good quality, the default */
    INTERP_7THORDER = 7,    /**< Seventh-order interpolation */

    INTERP_DEFAULT = INTERP_4THORDER, /**< Default interpolation method */
    INTERP_HIGHEST = INTERP_7THORDER, /**< Highest interpolation method */
};


/**
 * Enum used with synthAddDefaultMod() to specify how to handle duplicate modulators.
 */
enum synthAddMod
{
    SYNTH_OVERWRITE,        /**< Overwrite any existing matching modulator */
    SYNTH_ADD,              /**< Sum up modulator amounts */
};

/***************************************************************
 *
 *                         INCLUDES
 */


/***************************************************************
 *
 *                         DEFINES
 */
#define NUM_PROGRAMS      128
#define DRUM_INST_BANK		128

#if defined(WITH_FLOAT)
#define SAMPLE_FORMAT     SAMPLE_FLOAT
#else
#define SAMPLE_FORMAT     SAMPLE_DOUBLE
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
enum loop {
	UNLOOPED = 0,
	LOOP_DURING_RELEASE = 1,
	NOTUSED = 2,
	LOOP_UNTIL_RELEASE = 3
};

enum synthStatus {
	SYNTH_CLEAN,
	SYNTH_PLAYING,
	SYNTH_QUIET,
	SYNTH_STOPPED
};


typedef struct _fluidBankOffsetT bankOffsetT;

struct _fluidBankOffsetT {
	S32 sfontId;
	S32 offset;
};


/* * Synthesizer */
typedef struct _Synthesizer {
	/* settingsOldT settingsOld;  the old synthesizer settings */
  struct _SynthSettings *settingsP;
	S32 polyphony;										 /** maximum polyphony */
	S8 withReverb;									 /** Should the synth use the built-in reverb unit? */
	S8 withChorus;									 /** Should the synth use the built-in chorus unit? */
	double sampleRate;								 /** The sample rate */
	U8 midiChannels;								 /** the number of MIDI channels (>= 16) */
	U8 audioChannels;								 /** the number of audio channels (1 channel=left+right) */
	U8 audioGroups;									 /** the number of (stereo) 'sub'groups from the synth.  Typically equal to audioChannels. */
	S32 effectsChannels;							 /** the number of effects channels (= 2) */
	U32 state;								 /** the synthesizer state */
	U32 ticks;								 /** the number of audio samples since the start */
	double gain;												/** master gain */
	struct _Channel **channel;					/** the channels */
	U8 numChannels;										/** the number of channels */
	U8 nvoice;													/** the length of the synthesis process array */
	struct _Voice **voice;							/** the synthesis processes */
	U8 noteid;								/** the id is incremented for every new note. it's used for noteoff's  */
	U32 storeid;
	S32 nbuf;														/** How many audio buffers are used? (depends on nr of audio channels / groups)*/
	S16 **leftBuf;
	S16 **rightBuf;
	S16 **fxLeftBuf;
	S16 **fxRightBuf;
	revmodelT *reverb;
	struct _Chorus *chorus;
	S32 cur;													 /** the current sample in the audio buffers to be output */
	S32 ditherIndex;							/* current index in random dither value buffer: synth_(writeS16|ditherS16) */
	S8 outbuf[256];									 /** buffer for message output */
	tuningT ***tuning;						/** 128 banks of 128 programs for the tunings */
	tuningT *curTuning;					/** current tuning in the iteration */
	U32 minNoteLengthTicks;	/**< If note-offs are triggered just after a note-on, they will be delayed */
  Soundfont *soundfontP;  // I assume we're only ever going to use one soundfont at a time.
} Synthesizer;


Synthesizer *newSynth ();

S32 synthOneBlock (Synthesizer * synth, S32 doNotMixFxToOut);

S32 synthAllNotesOff (Synthesizer * synth, S32 chan);
S32 synthAllSoundsOff (Synthesizer * synth, S32 chan);
S32 synthModulateVoices (Synthesizer * synth, S32 chan, S32 isCc,
																 S32 ctrl);
S32 synthModulateVoicesAll (Synthesizer * synth, S32 chan);
S32 synthDampVoices (Synthesizer * synth, S32 chan);
S32 synthKillVoice (Synthesizer * synth, struct _Voice * voice);
void synthKillByExclusiveClass (Synthesizer * synth,
																					struct _Voice * voice);
void synthReleaseVoiceOnSameNote (Synthesizer * synth, S32 chan,
																						 S32 key);
void synthSfunloadMacos9 (Synthesizer * synth);

/** This function assures that every MIDI channels has a valid preset
 *  (NULL is okay). This function is called after a SoundFont is
 *  unloaded or reloaded. */
void synthUpdatePresets (Synthesizer * synth);


S32 synthUpdateGain (Synthesizer * synth, S8 *name, double value);
S32 synthUpdatePolyphony (Synthesizer * synth, S8 *name,
																	S32 value);

void synthDitherS16 (S32 *ditherIndex, S32 len, float *lin,
														 float *rin, void *lout, S32 loff, S32 lincr,
														 void *rout, S32 roff, S32 rincr);

#endif /* _SYNTH_H */
