#include <math.h>
#include "fluidbean.h"
#include "stbVorbis.c"
#include "synth.h"
#include "soundfont.h"
#include "conv.h"
#include "chorus.h"
#include "enums.h"
#include "voice.h"
#include "midi.h"
#include "mod.h"
#include "gen.h"


int synthProgramSelect2 (Synthesizer * synth, int chan, char *sfontName, U32 bankNum, U32 presetNum);
int synthSetGen2 (Synthesizer * synth, int chan, int genId, float value, int absolute, int normalized);
void synthSetReverb (Synthesizer * synth, double roomsize, double damping, double width, double level);
int synthBankSelect (Synthesizer * synth, int chan, U32 bank);
int deleteSynth (Synthesizer * synth);
int synthNoteon (Synthesizer * synth, U8 chan, U8 key, U8 vel);
int synthNoteoff (Synthesizer * synth, int chan, int key);
int synthStart (Synthesizer * synth, U32 id, Preset * preset, int audioChan, int midiChan, int key, int vel);
int synthStop (Synthesizer * synth, U32 id);
int synthTuneNotes (Synthesizer * synth, int bank, int prog, int len, int *key, double *pitch, int apply);
int synthActivateOctaveTuning (Synthesizer * synth, int bank, int prog, const char *name, const double *pitch, int apply);
int synthActivateTuning (Synthesizer * synth, int chan, int bank, int prog, int apply);
int synthWriteS16 (Synthesizer * synth, int len, void *lout, int loff, int lincr, void *rout, int roff, int rincr);
int presetNoteon (Preset *presetP, Synthesizer * synth, int chan, int key, int vel);
static tuningT *synthCreateTuning (Synthesizer * synth, int bank, int prog, const char *name);

/* GLOBAL */
/* has the synth module been initialized? */
static int synthInitialized = 0;
static void synthInit (void);
static void initDither (void);

static int synthSysexMidiTuning (Synthesizer * synth,
																					const char *data, int len,
																					char *response, int *responseLen,
																					int availResponse, int *handled,
																					int dryrun);

/* default modulators
 * SF2.01 page 52 ff:
 *
 * There is a set of predefined default modulators. They have to be
 * explicitly overridden by the soundfont in order to turn them off.
 */

/* SF2.01 page 53 section 8.4.1: MIDI Note-On Velocity to Initial Attenuation */
Modulator defaultVel2attMod = {	/* The modulator we are programming here */
  .src1 = MOD_VELOCITY,	/* Source. VELOCITY corresponds to 'index=2'. */
  .xformType1 =  MOD_GC	/* Not a MIDI continuous controller */
               | MOD_CONCAVE	/* Curve shape. Corresponds to 'type=1' */
               | MOD_UNIPOLAR	/* Polarity. Corresponds to 'P=0' */
               | MOD_NEGATIVE,	/* Direction. Corresponds to 'D=1' */
  .src2 = 0,
  .xformType2 = 0,	/* No 2nd source */
  .dest = GEN_ATTENUATION,	/* Target: Initial attenuation */
  .amount = 960	/* Modulation amount: 960 */
};

	/* SF2.01 page 53 section 8.4.2: MIDI Note-On Velocity to Filter Cutoff
	 * Have to make a design decision here. The specs don't make any sense this way or another.
	 * One sound font, 'Kingston Piano', which has been praised for its quality, tries to
	 * override this modulator with an amount of 0 and positive polarity (instead of what
	 * the specs say, D=1) for the secondary source.
	 * So if we change the polarity to 'positive', one of the best free sound fonts works...
	 */
Modulator defaultVel2filterMod = {	/* The modulator we are programming here */
  .src1 = MOD_VELOCITY,	/* Source. VELOCITY corresponds to 'index=2'. */
  .xformType1 =  MOD_GC	/* Not a MIDI continuous controller */
               | MOD_LINEAR	/* Curve shape. Corresponds to 'type=1' */
               | MOD_UNIPOLAR	/* Polarity. Corresponds to 'P=0' */
               | MOD_NEGATIVE,	/* Direction. Corresponds to 'D=1' */
  .src2 = MOD_VELOCITY,
  .xformType2 = MOD_GC
              | MOD_SWITCH
              | MOD_UNIPOLAR
              | MOD_POSITIVE,	/* No 2nd source */
  .dest = GEN_FILTERFC,	/* Target: Initial attenuation */
  .amount = -2400
};

/* SF2.01 page 53 section 8.4.3: MIDI Channel pressure to Vibrato LFO pitch depth */
Modulator defaultAt2viblfoMod = {
	.src1 = MOD_CHANNELPRESSURE,	/* Index=13 */
  .xformType1 =  MOD_GC	/* CC=0 */
					  	 | MOD_LINEAR	/* type=0 */
					  	 | MOD_UNIPOLAR	/* P=0 */
					  	 | MOD_POSITIVE,	/* D=0 */
	.src2 = 0, 
  .xformType2 = 0,
	.dest = GEN_VIBLFOTOPITCH,	/* Target: Vib. LFO => pitch */
	.amount = 50
};

/* SF2.01 page 53 section 8.4.4: Mod wheel (Controller 1) to Vibrato LFO pitch depth */
Modulator defaultMod2viblfoMod = {
	.src1 = 1,	/* Index=1 */
  .xformType1 =						 MOD_CC	/* CC=1 */
												 | MOD_LINEAR	/* type=0 */
												 | MOD_UNIPOLAR	/* P=0 */
												 | MOD_POSITIVE,	/* D=0 */
	.src2 = 0, 
  .xformType2 = 0,
	.dest = GEN_VIBLFOTOPITCH,	/* Target: Vib. LFO => pitch */
	.amount = 50
};

/* SF2.01 page 55 section 8.4.5: MIDI continuous controller 7 to initial attenuation */
Modulator defaultAttMod = {		/* SF2.01 section 8.4.5  */
	.src1 = 7,	/* index=7 */
	.src2 =								   MOD_CC	/* CC=1 */
												 | MOD_CONCAVE	/* type=1 */
												 | MOD_UNIPOLAR	/* P=0 */
												 | MOD_NEGATIVE,	/* D=1 */
	.src2 = 0, 
  .xformType2 =0,	/* No second source */
	.dest = GEN_ATTENUATION,	/* Target: Initial attenuation */
	.amount = 960
};

/* SF2.01 page 55 section 8.4.6 MIDI continuous controller 10 to Pan Position */
Modulator defaultPanMod = {
	.src1 = 10,	/* index=10 */
	.xformType1 =						 MOD_CC	/* CC=1 */
												 | MOD_LINEAR	/* type=0 */
												 | MOD_BIPOLAR	/* P=1 */
												 | MOD_POSITIVE,	/* D=0 */
	.src2 = 0, 
  .xformType2 = 0,	/* No second source */
	.dest = GEN_PAN,	/* Target: pan */
	/* Amount: 500. The SF specs $8.4.6, p. 55 syas: "Amount = 1000
	   tenths of a percent". The center value (64) corresponds to 50%,
	   so it follows that amount = 50% x 1000/% = 500. */
	.amount = 500
};

/* SF2.01 page 55 section 8.4.7: MIDI continuous controller 11 to initial attenuation */
Modulator defaultExprMod = {
	.src1 = 11,	/* index=11 */
	.xformType1 =											 MOD_CC	/* CC=1 */
												 | MOD_CONCAVE	/* type=1 */
												 | MOD_UNIPOLAR	/* P=0 */
												 | MOD_NEGATIVE,	/* D=1 */
	.src2 = 0, 
  .xformType2 =0,	/* No second source */
	.dest = GEN_ATTENUATION,	/* Target: Initial attenuation */
	.amount = 960
};

/* SF2.01 page 55 section 8.4.8: MIDI continuous controller 91 to Reverb send */
Modulator defaultReverbMod = {
	.src1 =  91,	/* index=91 */
	.xformType1 =											 MOD_CC	/* CC=1 */
												 | MOD_LINEAR	/* type=0 */
												 | MOD_UNIPOLAR	/* P=0 */
												 | MOD_POSITIVE,	/* D=0 */
	.src2 = 0, 
  .xformType2 = 0,	/* No second source */
	.dest = GEN_REVERBSEND,	/* Target: Reverb send */
	.amount = 200
};	

/* SF2.01 page 55 section 8.4.9: MIDI continuous controller 93 to Reverb send */
Modulator defaultChorusMod = {
	.src1 = 93,	/* index=93 */
	.xformType1 =						 MOD_CC	/* CC=1 */
												 | MOD_LINEAR	/* type=0 */
												 | MOD_UNIPOLAR	/* P=0 */
												 | MOD_POSITIVE,	/* D=0 */
	.src2 = 0, 
  .xformType2 = 0,
	.dest = GEN_CHORUSSEND,	/* Target: Chorus */
	.amount =  200
};	/* Amount: 200 ('tenths of a percent') */

/* SF2.01 page 57 section 8.4.10 MIDI Pitch Wheel to Initial Pitch ... */
Modulator defaultPitchBendMod = {
	.src1 = MOD_PITCHWHEEL,	/* Index=14 */
	.xformType1 =						 MOD_GC	/* CC =0 */
												 | MOD_LINEAR	/* type=0 */
												 | MOD_BIPOLAR	/* P=1 */
												 | MOD_POSITIVE,	/* D=0 */
	.src2 = MOD_PITCHWHEELSENS,	/* Index = 16 */
	.xformType2 =						 MOD_GC	/* CC=0 */
												 | MOD_LINEAR	/* type=0 */
												 | MOD_UNIPOLAR	/* P=0 */
												 | MOD_POSITIVE,	/* D=0 */
	.dest = GEN_PITCH,	/* Destination: Initial pitch */
	.amount = 12700
};

/* reverb presets */
static revmodelPresetsT revmodelPreset[] = {
	/* name *//* roomsize *//* damp *//* width *//* level */
	{"Test 1", 0.2f, 0.0f, 0.5f, 0.9f},
	{"Test 2", 0.4f, 0.2f, 0.5f, 0.8f},
	{"Test 3", 0.6f, 0.4f, 0.5f, 0.7f},
	{"Test 4", 0.8f, 0.7f, 0.5f, 0.6f},
	{"Test 5", 0.8f, 1.0f, 0.5f, 0.5f},
	{NULL, 0.0f, 0.0f, 0.0f, 0.0f}
};


/*************** INITIALIZATION & UTILITIES */
struct _SynthSettings synthSettings = {
  0,  // flags
  "", // midi port name (limited to 100 characters)
  {256, 16, 4096},        // polyphony
  {16, 16, 256},          // # midi channels
  {1, 1, 1},              // gain
  {1, 1, 256},            // # audio channels
  {1, 1, 256},            // # audio groups
  {2, 2, 2},              // # effects channels
  {44100, 22050, 96000},  // sample rate
  {10, 0, 65535}          // min note length
};

#define DITHER_SIZE 48000
#define DITHER_CHANNELS 2

static S16 randTable[DITHER_CHANNELS][DITHER_SIZE];

void initDither (void) {
	S16 d, dp;
	int c, i;

	for (c = 0; c < DITHER_CHANNELS; c++) {
		dp = 0;
		for (i = 0; i < DITHER_SIZE - 1; i++) {
			d = rand() / 32767;
			randTable[c][i] = d - dp;
			dp = d;
		}
		randTable[c][DITHER_SIZE - 1] = 0 - dp;
	}
}


/*
 * void synthInit
 *
 * Does all the initialization for this module.
 */
static void synthInit () {
	synthInitialized++;
	conversionConfig();
	//dspFloatConfig();
	//sysConfig();
	initDither();
}

/* SYNTH */
/* newSynth */
Synthesizer *newSynth () {
	int i;
	Synthesizer *synth;

	/* initialize all the conversion tables and other stuff */
	if (synthInitialized == 0) {
		synthInit ();
	}

	/* allocate a new synthesizer object */
	synth = NEW (Synthesizer);
	if (synth == NULL) 
		return NULL;
	MEMSET (synth, 0, sizeof (Synthesizer));

	synth->settingsP            = &synthSettings;
	synth->withReverb           = synthSettings.flags & REVERB_IS_ACTIVE;
	synth->withChorus           = synthSettings.flags & CHORUS_IS_ACTIVE;
  synth->polyphony            = synthSettings.synthPolyphony.val;
  synth->sampleRate           = synthSettings.synthSampleRate.val;
  synth->midiChannels         = synthSettings.synthNMidiChannels.val;
  synth->audioChannels        = synthSettings.synthNAudioChannels.val;
  synth->audioGroups          = synthSettings.synthNAudioGroups.val;
  synth->effectsChannels      = synthSettings.synthNEffectsChannels.val;
  synth->gain                 = (double) synthSettings.synthGain.val;
  synth->minNoteLengthTicks = (U32) (synthSettings.synthMinNoteLen.val*synth->sampleRate/1000.0f);

	/* do some basic sanity checking on the settings */
#define clipSetting_(synthSetting_, setting_) \
  if (synthSetting_ < setting_.min) synthSetting_ = setting_.min; \
  else if (synthSetting_ > setting_.max) synthSetting_ = setting_.max;
	if (synth->midiChannels % 16 != 0) 
		synthSettings.synthNMidiChannels.val = synth->midiChannels = ((synth->midiChannels/16)+1)*16;

  clipSetting_(synth->audioChannels,   synthSettings.synthNAudioChannels);
  clipSetting_(synth->audioGroups,     synthSettings.synthNAudioGroups);
  clipSetting_(synth->effectsChannels, synthSettings.synthNEffectsChannels);

	/* The number of buffers is determined by the higher number of nr
	 * groups / nr audio channels.  If LADSPA is unused, they should be
	 * the same. */
	synth->nbuf = synth->audioChannels;
	if (synth->audioGroups > synth->nbuf) 
		synth->nbuf = synth->audioGroups;
#ifdef LADSPA
	/* Create and initialize the Fx unit. */
	synth->LADSPA_FxUnit = new_LADSPA_FxUnit (synth);
#endif

	/* as soon as the synth is created it starts playing. */
	synth->state = SYNTH_PLAYING;
	synth->soundfontP = NULL;
	synth->noteid = 0;
	synth->ticks = 0;
	synth->tuning = NULL;


	/* allocate all channel objects */
	synth->channel = ARRAY (Channel *, synth->midiChannels);
	if (synth->channel == NULL) 
		goto errorRecovery;
	for (i = 0; i < synth->midiChannels; i++) {
		synth->channel[i] = newChannel (synth, i);
		if (synth->channel[i] == NULL) {
			goto errorRecovery;
		}
	}

	/* allocate all synthesis processes */
	synth->nvoice = synth->polyphony;
	synth->voice = ARRAY (Voice *, synth->nvoice);
	if (synth->voice == NULL) {
		goto errorRecovery;
	}
	for (i = 0; i < synth->nvoice; i++) {
		synth->voice[i] = newVoice (synth->sampleRate);
		if (synth->voice[i] == NULL) {
			goto errorRecovery;
		}
	}

	/* Allocate the sample buffers */
	synth->leftBuf = NULL;
	synth->rightBuf = NULL;
	synth->fxLeftBuf = NULL;
	synth->fxRightBuf = NULL;

	/* Left and right audio buffers */

	synth->leftBuf = ARRAY (S16*, synth->nbuf);
	synth->rightBuf = ARRAY (S16*, synth->nbuf);

	if ((synth->leftBuf == NULL) || (synth->rightBuf == NULL)) 
		goto errorRecovery;

	MEMSET (synth->leftBuf, 0, synth->nbuf * sizeof (S16*));
	MEMSET (synth->rightBuf, 0, synth->nbuf * sizeof (S16*));

	for (i = 0; i < synth->nbuf; i++) {

		synth->leftBuf[i] = ARRAY (S16, BUFSIZE);
		synth->rightBuf[i] = ARRAY (S16, BUFSIZE);

		if ((synth->leftBuf[i] == NULL) || (synth->rightBuf[i] == NULL)) 
			goto errorRecovery;
	}

	/* Effects audio buffers */

	synth->fxLeftBuf = ARRAY (S16 *, synth->effectsChannels);
	synth->fxRightBuf = ARRAY (S16 *, synth->effectsChannels);

	if ((synth->fxLeftBuf == NULL) || (synth->fxRightBuf == NULL)) 
		goto errorRecovery;

	MEMSET (synth->fxLeftBuf, 0, 2 * sizeof (S16 *));
	MEMSET (synth->fxRightBuf, 0, 2 * sizeof (S16 *));

	for (i = 0; i < synth->effectsChannels; i++) {
		synth->fxLeftBuf[i] = ARRAY (S16, BUFSIZE);
		synth->fxRightBuf[i] = ARRAY (S16, BUFSIZE);

		if ((synth->fxLeftBuf[i] == NULL) || (synth->fxRightBuf[i] == NULL)) 
			goto errorRecovery;
	}


	synth->cur = BUFSIZE;
	synth->ditherIndex = 0;

	/* allocate the reverb module */
	synth->reverb = newRevmodel ();
	if (synth->reverb == NULL) 
		goto errorRecovery;

	synthSetReverb (synth,
													REVERB_DEFAULT_ROOMSIZE,
													REVERB_DEFAULT_DAMP,
													REVERB_DEFAULT_WIDTH,
													REVERB_DEFAULT_LEVEL);

	/* allocate the chorus module */
	synth->chorus = newChorus (synth->sampleRate);
	if (synth->chorus == NULL) 
		goto errorRecovery;

	if (synthSettings.flags & DRUM_CHANNEL_IS_ACTIVE)
		synthBankSelect (synth, 9, DRUM_INST_BANK);

	return synth;

errorRecovery:
	deleteSynth (synth);
	return NULL;
}

/**
 * Set sample rate of the synth.
 * NOTE: This function is currently experimental and should only be
 * used when no voices or notes are active, and before any rendering calls.
 * @param synth Synth instance
 * @param sampleRate New sample rate (Hz)
 * @since 1.1.2
 */
void synthSetSampleRate (Synthesizer * synth, float sampleRate) {
	int i;
	for (i = 0; i < synth->nvoice; i++) {
		deleteVoice (synth->voice[i]);
		synth->voice[i] = newVoice (synth->sampleRate);
	}

	deleteChorus (synth->chorus);
	synth->chorus = newChorus (synth->sampleRate);
}

/*
 * deleteSynth
 */
int deleteSynth (Synthesizer * synth) {
	int i, k;
	bankOffsetT *bankOffset;

	if (synth == NULL) {
		return OK;
	}

	synth->state = SYNTH_STOPPED;

	/* turn off all voices, needed to unload SoundFont data */
	if (synth->voice != NULL) {
		for (i = 0; i < synth->nvoice; i++) {
			if (synth->voice[i] && _PLAYING(synth->voice[i]))
				voiceOff (synth->voice[i]);
		}
	}

	if (synth->channel != NULL) {
		for (i = 0; i < synth->midiChannels; i++) {
			if (synth->channel[i] != NULL) {
				deleteChannel (synth->channel[i]);
			}
		}
		FREE (synth->channel);
	}

	if (synth->voice != NULL) {
		for (i = 0; i < synth->nvoice; i++) {
			if (synth->voice[i] != NULL) {
				deleteVoice (synth->voice[i]);
			}
		}
		FREE (synth->voice);
	}

	/* free all the sample buffers */
	if (synth->leftBuf != NULL) {
		for (i = 0; i < synth->nbuf; i++) {
			if (synth->leftBuf[i] != NULL) {
				FREE (synth->leftBuf[i]);
			}
		}
		FREE (synth->leftBuf);
	}

	if (synth->rightBuf != NULL) {
		for (i = 0; i < synth->nbuf; i++) {
			if (synth->rightBuf[i] != NULL) {
				FREE (synth->rightBuf[i]);
			}
		}
		FREE (synth->rightBuf);
	}

	if (synth->fxLeftBuf != NULL) {
		for (i = 0; i < 2; i++) {
			if (synth->fxLeftBuf[i] != NULL) {
				FREE (synth->fxLeftBuf[i]);
			}
		}
		FREE (synth->fxLeftBuf);
	}

	if (synth->fxRightBuf != NULL) {
		for (i = 0; i < 2; i++) {
			if (synth->fxRightBuf[i] != NULL) {
				FREE (synth->fxRightBuf[i]);
			}
		}
		FREE (synth->fxRightBuf);
	}

	/* release the reverb module */
	if (synth->reverb != NULL) {
		deleteRevmodel (synth->reverb);
	}

	/* release the chorus module */
	if (synth->chorus != NULL) {
		deleteChorus (synth->chorus);
	}

	/* free the tunings, if any */
	if (synth->tuning != NULL) {
		for (i = 0; i < 128; i++) {
			if (synth->tuning[i] != NULL) {
				for (k = 0; k < 128; k++) {
					if (synth->tuning[i][k] != NULL) {
						FREE (synth->tuning[i][k]);
					}
				}
				FREE (synth->tuning[i]);
			}
		}
		FREE (synth->tuning);
	}
#ifdef LADSPA
	/* Release the LADSPA Fx unit */
	LADSPA_shutdown (synth->LADSPA_FxUnit);
	FREE (synth->LADSPA_FxUnit);
#endif

	//mutexDestroy(synth->busy);

	FREE (synth);

	return OK;
}

/*
 * synthError
 *
 * The error messages are not thread-save, yet. They are still stored
 * in a global message buffer (see sys.c).
 * */
char *synthError (Synthesizer * synth) {
	return error ();
}

/*
 * synthNoteon
 */
int synthNoteon (Synthesizer * synth, U8 chan, U8 key, U8 vel) {
	Channel *channel;

	/* check the ranges of the arguments */
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}

	/* notes with velocity zero go to noteoff  */
	if (vel == 0) {
		return synthNoteoff (synth, chan, key);
	}

	channel = synth->channel[chan];

	/* make sure this channel has a preset */
	if (channel->presetP == NULL) {
		return FAILED;
	}

	/* If there is another voice process on the same channel and key,
	   advance it to the release phase. */
	synthReleaseVoiceOnSameNote (synth, chan, key);

	return presetNoteon(channel->presetP, 0, chan, key, vel);
}

/*
 * synthNoteoff
 */
int synthNoteoff (Synthesizer * synth, int chan, int key) {
	int i;
	Voice *voice;
	int status = FAILED;

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (_ON (voice) && (voice->chan == chan) && (voice->key == key)) {
			voiceNoteoff (voice);
			status = OK;
		}														/* if voice on */
	}															/* for all voices */
	return status;
}

/*
 * synthDampVoices
 */
int synthDampVoices (Synthesizer * synth, int chan) {
	int i;
	Voice *voice;

/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if ((voice->chan == chan) && _SUSTAINED (voice)) {
/*        printf("turned off sustained note: chan=%d, key=%d, vel=%d\n", voice->chan, voice->key, voice->vel); */
			voiceNoteoff (voice);
		}
	}

	return OK;
}

/*
 * synthCc
 */
int synthCc (Synthesizer * synth, int chan, int num, int val) {
/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	/* check the ranges of the arguments */
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}
	if ((num < 0) || (num >= 128)) {
		return FAILED;
	}
	if ((val < 0) || (val >= 128)) {
		return FAILED;
	}
}

/*
 * synthCc
 */
int synthGetCc (Synthesizer * synth, int chan, int num, int *pval) {
	/* check the ranges of the arguments */
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}
	if ((num < 0) || (num >= 128)) {
		return FAILED;
	}

	*pval = synth->channel[chan]->cc[num];
	return OK;
}

/**
 * Process a MIDI SYSEX (system exclusive) message.
 * @param synth Synth instance
 * @param data Buffer containing SYSEX data (not including 0xF0 and 0xF7)
 * @param len Length of data in buffer
 * @param response Buffer to store response to or NULL to ignore
 * @param responseLen IN/OUT parameter, in: size of response buffer, out:
 *   amount of data written to response buffer (if FAILED is returned and
 *   this value is non-zero, it indicates the response buffer is too small)
 * @param handled Optional location to store boolean value if message was
 *   recognized and handled or not (set to TRUE if it was handled)
 * @param dryrun TRUE to just do a dry run but not actually execute the SYSEX
 *   command (useful for checking if a SYSEX message would be handled)
 * @return OK on success, FAILED otherwise
 * @since 1.1.0
 */
/* SYSEX format (0xF0 and 0xF7 not passed to this function):
 * Non-realtime:    0xF0 0x7E <DeviceId> [BODY] 0xF7
 * Realtime:        0xF0 0x7F <DeviceId> [BODY] 0xF7
 * Tuning messages: 0xF0 0x7E/0x7F <DeviceId> 0x08 <sub ID2> [BODY] <ChkSum> 0xF7
 */
int
synthSysex (Synthesizer * synth, const char *data, int len,
									 char *response, int *responseLen, int *handled,
									 int dryrun) {
	int availResponse = 0;

	if (handled)
		*handled = 0;								//FALSE

	if (responseLen) {
		availResponse = *responseLen;
		*responseLen = 0;
	}

	if (!(synth != NULL))
		return FAILED;
	if (!(data != NULL))
		return FAILED;
	if (!(len > 0))
		return FAILED;
	if (!(!response || responseLen))
		return FAILED;

	if (len < 4)
		return OK;

	/* MIDI tuning SYSEX message? */
	if ((data[0] == MIDI_SYSEX_UNIV_NON_REALTIME
			 || data[0] == MIDI_SYSEX_UNIV_REALTIME)
			&& data[2] == MIDI_SYSEX_MIDI_TUNING_ID)
		//&& (data[1] == synth->deviceId || data[1] == MIDI_SYSEX_DEVICE_ID_ALL) -> we don't handle device id
	{
		int result;
		//synthApiEnter(synth); -> we don't handle mutex
		result = synthSysexMidiTuning (synth, data, len, response,
																						responseLen, availResponse,
																						handled, dryrun);

		return result;
	}
	return OK;
}

/* Handler for MIDI tuning SYSEX messages */
static int
synthSysexMidiTuning (Synthesizer * synth, const char *data,
															 int len, char *response, int *responseLen,
															 int availResponse, int *handled, int dryrun) {
	int realtime, msgid;
	int bank = 0, prog, channels;
	double tunedata[128];
	int keys[128];
	char name[17];
	int note, frac, frac2;
	uint8 chksum;
	int i, count, index;
	const char *dataptr;
	char *resptr;

	realtime = data[0] == MIDI_SYSEX_UNIV_REALTIME;
	msgid = data[3];

	switch (msgid) {
	case MIDI_SYSEX_TUNING_BULK_DUMP_REQ:
	case MIDI_SYSEX_TUNING_BULK_DUMP_REQ_BANK:
		if (data[3] == MIDI_SYSEX_TUNING_BULK_DUMP_REQ) {
			if (len != 5 || data[4] & 0x80 || !response)
				return OK;

			*responseLen = 406;
			prog = data[4];
		} else {
			if (len != 6 || data[4] & 0x80 || data[5] & 0x80 || !response)
				return OK;

			*responseLen = 407;
			bank = data[4];
			prog = data[5];
		}

		if (dryrun) {
			if (handled)
				*handled = 1;						//TRUE
			return OK;
		}

		if (availResponse < *responseLen)
			return FAILED;

		/* Get tuning data, return if tuning not found */
		resptr = response;

		*resptr++ = MIDI_SYSEX_UNIV_NON_REALTIME;
		*resptr++ = 0;							//no synth->deviceId
		*resptr++ = MIDI_SYSEX_MIDI_TUNING_ID;
		*resptr++ = MIDI_SYSEX_TUNING_BULK_DUMP;

		if (msgid == MIDI_SYSEX_TUNING_BULK_DUMP_REQ_BANK)
			*resptr++ = bank;

		*resptr++ = prog;
		strncpy (resptr, name, 16);	//STRNCPY
		resptr += 16;

		for (i = 0; i < 128; i++) {
			note = tunedata[i] / 100.0;
			clip (note, 0, 127);

			frac = ((tunedata[i] - note * 100.0) * 16384.0 + 50.0) / 100.0;
			clip (frac, 0, 16383);

			*resptr++ = note;
			*resptr++ = frac >> 7;
			*resptr++ = frac & 0x7F;
		}

		if (msgid == MIDI_SYSEX_TUNING_BULK_DUMP_REQ) {	/* NOTE: Checksum is not as straight forward as the bank based messages */
			chksum = MIDI_SYSEX_UNIV_NON_REALTIME ^ MIDI_SYSEX_MIDI_TUNING_ID
				^ MIDI_SYSEX_TUNING_BULK_DUMP ^ prog;

			for (i = 21; i < 128 * 3 + 21; i++)
				chksum ^= response[i];
		} else {
			for (i = 1, chksum = 0; i < 406; i++)
				chksum ^= response[i];
		}

		*resptr++ = chksum & 0x7F;

		if (handled)
			*handled = 1;							//TRUE
		break;
	case MIDI_SYSEX_TUNING_NOTE_TUNE:
	case MIDI_SYSEX_TUNING_NOTE_TUNE_BANK:
		dataptr = data + 4;

		if (msgid == MIDI_SYSEX_TUNING_NOTE_TUNE) {
			if (len < 10 || data[4] & 0x80 || data[5] & 0x80
					|| len != data[5] * 4 + 6)
				return OK;
		} else {
			if (len < 11 || data[4] & 0x80 || data[5] & 0x80 || data[6] & 0x80
					|| len != data[5] * 4 + 7)
				return OK;

			bank = *dataptr++;
		}

		if (dryrun) {
			if (handled)
				*handled = 1;						//TRUE
			return OK;
		}

		prog = *dataptr++;
		count = *dataptr++;

		for (i = 0, index = 0; i < count; i++) {
			note = *dataptr++;
			if (note & 0x80)
				return OK;
			keys[index] = note;

			note = *dataptr++;
			frac = *dataptr++;
			frac2 = *dataptr++;

			if (note & 0x80 || frac & 0x80 || frac2 & 0x80)
				return OK;

			frac = frac << 7 | frac2;

			/* No change pitch value?  Doesn't really make sense to send that, but.. */
			if (note == 0x7F && frac == 16383)
				continue;

			tunedata[index] = note * 100.0 + (frac * 100.0 / 16384.0);
			index++;
		}

		if (index > 0) {
			if (synthTuneNotes (synth, bank, prog, index, keys, tunedata,
																	realtime) == FAILED)
				return FAILED;
		}

		if (handled)
			*handled = 1;							//TRUE
		break;
	case MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE:
	case MIDI_SYSEX_TUNING_OCTAVE_TUNE_2BYTE:
		if ((msgid == MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE && len != 19)
				|| (msgid == MIDI_SYSEX_TUNING_OCTAVE_TUNE_2BYTE && len != 31))
			return OK;

		if (data[4] & 0x80 || data[5] & 0x80 || data[6] & 0x80)
			return OK;

		if (dryrun) {
			if (handled)
				*handled = 1;						//TRUE
			return OK;
		}

		channels = (data[4] & 0x03) << 14 | data[5] << 7 | data[6];

		if (msgid == MIDI_SYSEX_TUNING_OCTAVE_TUNE_1BYTE) {
			for (i = 0; i < 12; i++) {
				frac = data[i + 7];
				if (frac & 0x80)
					return OK;
				tunedata[i] = (int) frac - 64;
			}
		} else {
			for (i = 0; i < 12; i++) {
				frac = data[i * 2 + 7];
				frac2 = data[i * 2 + 8];
				if (frac & 0x80 || frac2 & 0x80)
					return OK;
				tunedata[i] =
					(((int) frac << 7 | (int) frac2) - 8192) * (200.0 / 16384.0);
			}
		}

		if (synthActivateOctaveTuning (synth, 0, 0, "SYSEX",
																						tunedata,
																						realtime) == FAILED)
			return FAILED;

		if (channels) {
			for (i = 0; i < 16; i++) {
				if (channels & (1 << i))
					synthActivateTuning (synth, i, 0, 0, realtime);
			}
		}

		if (handled)
			*handled = 1;							//TRUE
		break;
	}

	return OK;
}

/*
 * synthAllNotesOff
 *
 * put all notes on this channel into released state.
 */
int synthAllNotesOff (Synthesizer * synth, int chan) {
	int i;
	Voice *voice;

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (_PLAYING (voice) && (voice->chan == chan)) {
			voiceNoteoff (voice);
		}
	}
	return OK;
}

/*
 * synthAllSoundsOff
 *
 * immediately stop all notes on this channel.
 */
int synthAllSoundsOff (Synthesizer * synth, int chan) {
	int i;
	Voice *voice;

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (_PLAYING (voice) && (voice->chan == chan)) {
			voiceOff (voice);
		}
	}
	return OK;
}

/*
 * synthSystemReset
 *
 * Purpose:
 * Respond to the MIDI command 'system reset' (0xFF, big red 'panic' button)
 */
int synthSystemReset (Synthesizer * synth) {
	int i;
	Voice *voice;

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (_PLAYING (voice)) 
			voiceOff (voice);
	}

	for (i = 0; i < synth->midiChannels; i++) 
		channelReset (synth->channel[i]);

	chorusReset (synth->chorus);
	revmodelReset (synth->reverb);

	return OK;
}

/*
 * synthModulateVoices
 *
 * tell all synthesis processes on this channel to update their
 * synthesis parameters after a control change.
 */
int synthModulateVoices (Synthesizer * synth, int chan, int isCc, int ctrl) {
	int i;
	Voice *voice;

/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (voice->chan == chan) 
			voiceModulate (voice, isCc, ctrl);
	}
	return OK;
}

/*
 * synthModulateVoicesAll
 *
 * Tell all synthesis processes on this channel to update their
 * synthesis parameters after an all control off message (i.e. all
 * controller have been reset to their default value).
 */
int synthModulateVoicesAll (Synthesizer * synth, int chan) {
	int i;
	Voice *voice;

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (voice->chan == chan) 
			voiceModulateAll (voice);
	}
	return OK;
}

/**
 * Set the MIDI channel pressure controller value.
 * @param synth Synth instance
 * @param chan MIDI channel number
 * @param val MIDI channel pressure value (7 bit, 0-127)
 * @return OK on success
 *
 * Assign to the MIDI channel pressure controller value on a specific MIDI channel
 * in real time.
 */
int synthChannelPressure (Synthesizer * synth, int chan, int val) {

/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	/* check the ranges of the arguments */
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}

	/* set the channel pressure value in the channel */
	channelPressure (synth->channel[chan], val);

	return OK;
}

/**
 * Set the MIDI polyphonic key pressure controller value.
 * @param synth Synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param key MIDI key number (0-127)
 * @param val MIDI key pressure value (0-127)
 * @return OK on success, FAILED otherwise
 */
int
synthKeyPressure (Synthesizer * synth, int chan, int key, int val) {
	int result = OK;
	if (key < 0 || key > 127) {
		return FAILED;
	}
	if (val < 0 || val > 127) {
		return FAILED;
	}

	channelSetKeyPressure (synth->channel[chan], key, val);

	// synthUpdateKeyPressure_LOCAL
	{
		Voice *voice;
		int i;

		for (i = 0; i < synth->polyphony; i++) {
			voice = synth->voice[i];

			if (voice->chan == chan && voice->key == key) {
				result = voiceModulate (voice, 0, MOD_KEYPRESSURE);
				if (result != OK)
					break;
			}
		}
	}

	return result;
}

int synthPitchBend (Synthesizer * synth, int chan, int val) {
/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	channelPitchBend (synth->channel[chan], val);
	return OK;
}

int synthPitchWheelSens (Synthesizer * synth, int chan, int val) {
	if ((chan < 0) || (chan >= synth->midiChannels)) 
		return FAILED;
	channelPitchWheelSens (synth->channel[chan], val);
	return OK;
}

int synthGetPitchWheelSens (Synthesizer * synth, int chan, int *pval) {
	if ((chan < 0) || (chan >= synth->midiChannels)) 
		return FAILED;
	*pval = synth->channel[chan]->pitchWheelSensitivity;
	return OK;
}

int synthProgramChange (Synthesizer * synth, int chan, int prognum) {
	Preset *preset = NULL;
	Channel *channel;
	U32 banknum;
	U32 sfontId;
	int substBank, substProg;

	if ((prognum < 0) || (prognum >= NUM_PROGRAMS) ||
			(chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}

	channel = synth->channel[chan];
	banknum = channel->banknum;

	/* inform the channel of the new program number */
	channel->prognum = prognum;


	if (channel->channum == 9 && synth->settingsP->flags & DRUM_CHANNEL_IS_ACTIVE)
		preset = &synth->soundfontP->bankA[DRUM_INST_BANK].presetA[prognum];
	else 
		preset = &synth->soundfontP->bankA[banknum].presetA[prognum];

	/* Fallback to another preset if not found */
	if (!preset) {
		substBank = banknum;
		substProg = prognum;

		/* Melodic instrument? */
		if (banknum != DRUM_INST_BANK) {
			substBank = 0;

			/* Fallback first to bank 0:prognum */
      preset = &synth->soundfontP->bankA[0].presetA[prognum];

			/* Fallback to first preset in bank 0 */
			if (!preset && prognum != 0) {
        preset = &synth->soundfontP->bankA[0].presetA[0];
				substProg = 0;
			}
		} else {										/* Percussion: Fallback to preset 0 in percussion bank */
      preset = &synth->soundfontP->bankA[DRUM_INST_BANK].presetA[0];
			substProg = 0;
		}
	}

  channel->presetP = preset;

	return OK;
}

/*
 * synthBankSelect
 */
int synthBankSelect (Synthesizer * synth, int chan, U32 bank) {
	if ((chan >= 0) && (chan < synth->midiChannels)) {
		synth->channel[chan]->banknum = bank;
		return OK;
	}
	return FAILED;
}


/*
 * synthSfontSelect
 */
int synthSfontSelect (Synthesizer * synth, int chan, U32 sfontId) {
	if ((chan >= 0) && (chan < synth->midiChannels)) {
    synth->channel[chan]->sfontnum = sfontId;
		return OK;
	}
	return FAILED;
}

/* * synthGetProgram */
int synthGetProgram (Synthesizer * synth, int chan, U32 *sfontId, U32 *bankNum, U32 *presetNum) {
	Channel *channel;
	if ((chan >= 0) && (chan < synth->midiChannels)) {
		channel = synth->channel[chan];
		*sfontId = channel->sfontnum;
		*bankNum = channel->banknum;
		*presetNum = channel->prognum;
		return OK;
	}
	return FAILED;
}

/*
 * synthProgramSelect
 */
int
synthProgramSelect (Synthesizer * synth,
														int chan,
														U32 sfontId,
														U32 bankNum, U32 presetNum) {
	Preset *preset = NULL;
	Channel *channel;

	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}
	channel = synth->channel[chan];

  preset = &synth->soundfontP->bankA[bankNum].presetA[presetNum];
	if (preset == NULL) 
		return FAILED;

	/* inform the channel of the new bank and program number */
  channel->sfontnum = sfontId;
  channel->banknum = bankNum;
  channel->prognum = presetNum;

	channel->presetP = preset;

	return OK;
}

/*
 * synthUpdatePresets
 */
void synthUpdatePresets (Synthesizer * synth) {
	int chan;
	Channel *channel;

	for (chan = 0; chan < synth->midiChannels; chan++) {
		channel = synth->channel[chan];
    channel->presetP = &synth->soundfontP->bankA[channel->banknum].presetA[channel->prognum];
	}
}


/*
 * synthSetGain
 */
void synthSetGain (Synthesizer * synth, float gain) {
	int i;

	clip (gain, 0.0f, 10.0f);
	synth->gain = gain;

	for (i = 0; i < synth->polyphony; i++) {
		Voice *voice = synth->voice[i];
		if (_PLAYING (voice)) {
			voiceSetGain (voice, gain);
		}
	}
}

/*
 * synthUpdateGain
 */
int synthUpdateGain (Synthesizer * synth, char *name, double value) {
	synthSetGain (synth, (float) value);
	return 0;
}

/*
 * synthSetPolyphony
 */
int synthSetPolyphony (Synthesizer * synth, int polyphony) {
	int i;

	if (polyphony < 1 || polyphony > synth->nvoice) {
		return FAILED;
	}

	/* turn off any voices above the new limit */
	for (i = polyphony; i < synth->nvoice; i++) {
		Voice *voice = synth->voice[i];
		if (_PLAYING (voice)) {
			voiceOff (voice);
		}
	}

	synth->polyphony = polyphony;

	return OK;
}

/*
 * synthUpdatePolyphony
 */
int
synthUpdatePolyphony (Synthesizer * synth, char *name, int value) {
	synthSetPolyphony (synth, value);
	return 0;
}

/*
 * synthGetPolyphony
 */
int synthGetPolyphony (Synthesizer * synth) {
	return synth->polyphony;
}

/*
 * synthGetInternalBufferSize
 */
int synthGetInternalBufsize (Synthesizer * synth) {
	return BUFSIZE;
}

/*
 * synthProgramReset
 *
 * Resend a bank select and a program change for every channel. This
 * function is called mainly after a SoundFont has been loaded,
 * unloaded or reloaded.  */
int synthProgramReset (Synthesizer * synth) {
	int i;
	/* try to set the correct presets */
	for (i = 0; i < synth->midiChannels; i++) 
		synthProgramChange (synth, i, synth->channel[i]->prognum);
	return OK;
}

/*
 * synthSetReverbPreset
 */
int synthSetReverbPreset (Synthesizer * synth, int num) {
	int i = 0;
	while (revmodelPreset[i].name != NULL) {
		if (i == num) {
			revmodelSetroomsize (synth->reverb, revmodelPreset[i].roomsize);
			revmodelSetdamp (synth->reverb, revmodelPreset[i].damp);
			revmodelSetwidth (synth->reverb, revmodelPreset[i].width);
			revmodelSetlevel (synth->reverb, revmodelPreset[i].level);
			return OK;
		}
		i++;
	}
	return FAILED;
}

/*
 * synthSetReverb
 */
void synthSetReverb (Synthesizer * synth, double roomsize, double damping, double width, double level) {
/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	revmodelSetroomsize (synth->reverb, roomsize);
	revmodelSetdamp (synth->reverb, damping);
	revmodelSetwidth (synth->reverb, width);
	revmodelSetlevel (synth->reverb, level);
}

/*
 * synthSetChorus
 */
void
synthSetChorus (Synthesizer * synth, int nr, double level,
												double speed, double depthMs, int type) {
/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	chorusSetNr (synth->chorus, nr);
	chorusSetLevel (synth->chorus, (realT) level);
	chorusSetSpeed_Hz (synth->chorus, (realT) speed);
	chorusSetDepthMs (synth->chorus, (realT) depthMs);
	chorusSetType (synth->chorus, type);
	chorusUpdate (synth->chorus);
}

/******************************************************

#define COMPRESS      1
#define COMPRESS_X1   4.0
#define COMPRESS_Y1   0.6
#define COMPRESS_X2   10.0
#define COMPRESS_Y2   1.0

  len2 = 2 * len;
  alpha1 = COMPRESS_Y1 / COMPRESS_X1;
  alpha2 = (COMPRESS_Y2 - COMPRESS_Y1) / (COMPRESS_X2 - COMPRESS_X1);
  if (COMPRESS_X1 == COMPRESS_Y1) {
    for (j = 0; j < len2; j++) {
      if (buf[j] > COMPRESS_X1) {
	if (buf[j] > COMPRESS_X2) {
	  buf[j] = COMPRESS_Y2;
	} else {
	  buf[j] = COMPRESS_Y1 + alpha2 * (buf[j] - COMPRESS_X1);
	}
      } else if (buf[j] < -COMPRESS_X1) {
	if (buf[j] < -COMPRESS_X2) {
	  buf[j] = -COMPRESS_Y2;
	} else {
	  buf[j] = -COMPRESS_Y1 + alpha2 * (buf[j] + COMPRESS_X1);
	}
      }
    }
  } else {
    for (j = 0; j < len2; j++) {
      if ((buf[j] >= -COMPRESS_X1) && (buf[j] <= COMPRESS_X1)) {
	buf[j] *= alpha1;
      } else if (buf[j] > COMPRESS_X1) {
	if (buf[j] > COMPRESS_X2) {
	  buf[j] = COMPRESS_Y2;
	} else {
	  buf[j] = COMPRESS_Y1 + alpha2 * (buf[j] - COMPRESS_X1);
	}
      } else {
	if (buf[j] < -COMPRESS_X2) {
	  buf[j] = -COMPRESS_Y2;
	} else {
	  buf[j] = -COMPRESS_Y1 + alpha2 * (buf[j] + COMPRESS_X1);
	}
      }
    }
  }

***************************************************/

/*
 *  synthNwriteS16
 */
int
synthNwriteS16 (Synthesizer * synth, int len,
									S16 **left, S16 **right,
									S16 **fxLeft, S16 **fxRight) {
	S16 **leftIn = synth->leftBuf;
	S16 **rightIn = synth->rightBuf;
	int i, num, available, count, bytes;

	/* make sure we're playing */
	if (synth->state != SYNTH_PLAYING) {
		return 0;
	}

	/* First, take what's still available in the buffer */
	count = 0;
	num = synth->cur;
	if (synth->cur < BUFSIZE) {
		available = BUFSIZE - synth->cur;

		num = (available > len) ? len : available;
		bytes = num * sizeof (S16);

		for (i = 0; i < synth->audioChannels; i++) {
			MEMCPY (left[i], leftIn[i] + synth->cur, bytes);
			MEMCPY (right[i], rightIn[i] + synth->cur, bytes);
		}
		count += num;
		num += synth->cur;					/* if we're now done, num becomes the new synth->cur below */
	}

	/* Then, run oneBlock() and copy till we have 'len' samples  */
	while (count < len) {
		synthOneBlock (synth, 1);

		num = (BUFSIZE > len - count) ? len - count : BUFSIZE;
		bytes = num * sizeof (S16);

		for (i = 0; i < synth->audioChannels; i++) {
			MEMCPY (left[i] + count, leftIn[i], bytes);
			MEMCPY (right[i] + count, rightIn[i], bytes);
		}

		count += num;
	}

	synth->cur = num;

/*   printf("CPU: %.2f\n", synth->cpuLoad); */

	return 0;
}


int
synthProcess (Synthesizer * synth, int len,
							int nin, S16 **in, int nout, S16 **out) {
	if (nout == 2) {
		return synthWriteS16 (synth, len, out[0], 0, 1, out[1], 0, 1);
	} else {
		S16 **left, **right;
		int i;
		left = ARRAY (S16*, nout / 2);
		right = ARRAY (S16*, nout / 2);
		for (i = 0; i < nout / 2; i++) {
			left[i] = out[2 * i];
			right[i] = out[2 * i + 1];
		}
		synthNwriteS16 (synth, len, left, right, NULL, NULL);
		FREE (left);
		FREE (right);
		return 0;
	}
}

/*
 *  synthWriteS16
 */
int synthWriteS16 (Synthesizer * synth, int len, void *lout, int loff, int lincr, void *rout, int roff, int rincr) {
	int i, j, k, cur;
	S16 *leftOut = (S16*) lout;
	S16 *rightOut = (S16*) rout;
	S16 *leftIn = synth->leftBuf[0];
	S16 *rightIn = synth->rightBuf[0];
	S16 leftSample;
	S16 rightSample;
	int di = synth->ditherIndex;

	/* make sure we're playing */
	if (synth->state != SYNTH_PLAYING) {
		return 0;
	}

	cur = synth->cur;

	for (i = 0, j = loff, k = roff; i < len; i++, cur++, j += lincr, k += rincr) {

		/* fill up the buffers as needed */
		if (cur == BUFSIZE) {
			synthOneBlock (synth, 0);
			cur = 0;
		}

		leftSample = leftIn[cur] * 32766.0f + randTable[0][di];
		rightSample = rightIn[cur] * 32766.0f + randTable[1][di];

		di++;
		if (di >= DITHER_SIZE)
			di = 0;

		/* digital clipping */
		if (leftSample > 32767.0f)
			leftSample = 32767.0f;
		if (leftSample < -32768.0f)
			leftSample = -32768.0f;
		if (rightSample > 32767.0f)
			rightSample = 32767.0f;
		if (rightSample < -32768.0f)
			rightSample = -32768.0f;

		leftOut[j] = (S16) leftSample;
		rightOut[k] = (S16) rightSample;
	}

	synth->cur = cur;
	synth->ditherIndex = di;			/* keep dither buffer continous */

/*   printf("CPU: %.2f\n", synth->cpuLoad); */

	return 0;
}

/*
 * synthDitherS16
 * Converts stereo floating point sample data to signed 16 bit data with
 * dithering.  'ditherIndex' parameter is a caller supplied pointer to an
 * integer which should be initialized to 0 before the first call and passed
 * unmodified to additional calls which are part of the same synthesis output.
 * Only used internally currently.
 */
void synthDitherS16 (int *ditherIndex, int len, float *lin, float *rin,
                             void *lout, int loff, int lincr,
                             void *rout, int roff, int rincr) {
	int i, j, k;
	S16 *leftOut = (S16 *) lout;
	S16 *rightOut = (S16 *) rout;
	S16 leftSample;
	S16 rightSample;
	int di = *ditherIndex;

	for (i = 0, j = loff, k = roff; i < len; i++, j += lincr, k += rincr) {

		leftSample = lin[i] * 32766.0f + randTable[0][di];
		rightSample = rin[i] * 32766.0f + randTable[1][di++];

		if (di >= DITHER_SIZE)
			di = 0;

		/* digital clipping */
		if (leftSample > 32767.0f)
			leftSample = 32767.0f;
		if (leftSample < -32768.0f)
			leftSample = -32768.0f;
		if (rightSample > 32767.0f)
			rightSample = 32767.0f;
		if (rightSample < -32768.0f)
			rightSample = -32768.0f;

		leftOut[j] = (S16) leftSample;
		rightOut[k] = (S16) rightSample;
	}

	*ditherIndex = di;						/* keep dither buffer continous */
}

/*
 *  synthOneBlock
 */
int synthOneBlock (Synthesizer * synth, int doNotMixFxToOut) {
	int i, auchan;
	Voice *voice;
	S16 *leftBuf;
	S16 *rightBuf;
	S16 *reverbBuf;
	S16 *chorusBuf;
	int byteSize = BUFSIZE * sizeof (S16);  // 64 * 4?

/*   mutexLock(synth->busy); /\* Here comes the audio thread. Lock the synth. *\/ */

	/* clean the audio buffers */
  // MB TODO: look at how this is defined so you can memset it in one fell swoop. This is lame.
	for (i = 0; i < synth->nbuf; i++) {
		MEMSET (synth->leftBuf[i], 0, byteSize);
		MEMSET (synth->rightBuf[i], 0, byteSize);
	}

	for (i = 0; i < synth->effectsChannels; i++) {
		MEMSET (synth->fxLeftBuf[i], 0, byteSize);
		MEMSET (synth->fxRightBuf[i], 0, byteSize);
	}

	/* Set up the reverb / chorus buffers only, when the effect is
	 * enabled on synth level.  Nonexisting buffers are detected in the
	 * DSP loop. Not sending the reverb / chorus signal saves some time
	 * in that case. */
	reverbBuf = synth->withReverb ? synth->fxLeftBuf[0] : NULL;
	chorusBuf = synth->withChorus ? synth->fxLeftBuf[1] : NULL;

	/* call all playing synthesis processes */
	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];

		if (_PLAYING (voice)) {
			/* The output associated with a MIDI channel is wrapped around
			 * using the number of audio groups as modulo divider.  This is
			 * typically the number of output channels on the 'sound card',
			 * as long as the LADSPA Fx unit is not used. In case of LADSPA
			 * unit, think of it as subgroups on a mixer.
			 *
			 * For example: Assume that the number of groups is set to 2.
			 * Then MIDI channel 1, 3, 5, 7 etc. go to output 1, channels 2,
			 * 4, 6, 8 etc to output 2.  Or assume 3 groups: Then MIDI
			 * channels 1, 4, 7, 10 etc go to output 1; 2, 5, 8, 11 etc to
			 * output 2, 3, 6, 9, 12 etc to output 3.
			 */
			auchan = voice->chan;
			auchan %= synth->audioGroups;
			leftBuf = synth->leftBuf[auchan];
			rightBuf = synth->rightBuf[auchan];

			voiceWrite (voice, leftBuf, rightBuf, reverbBuf, chorusBuf);
		}
	}

	/* if multi channel output, don't mix the output of the chorus and
	   reverb in the final output. The effects outputs are send
	   separately. */

	if (doNotMixFxToOut) {

		/* send to reverb */
		if (reverbBuf) {
			revmodelProcessreplace (synth->reverb, reverbBuf,
																		 synth->fxLeftBuf[0],
																		 synth->fxRightBuf[0]);
		}

		/* send to chorus */
		if (chorusBuf) {
			chorusProcessreplace (synth->chorus, chorusBuf,
																	 synth->fxLeftBuf[1],
																	 synth->fxRightBuf[1]);
		}

	} else {

		/* send to reverb */
		if (reverbBuf) {
			revmodelProcessmix (synth->reverb, reverbBuf,
																 synth->leftBuf[0], synth->rightBuf[0]);
		}

		/* send to chorus */
		if (chorusBuf) {
			chorusProcessmix (synth->chorus, chorusBuf,
															 synth->leftBuf[0], synth->rightBuf[0]);
		}
	}


#ifdef LADSPA
	/* Run the signal through the LADSPA Fx unit */
	LADSPA_run (synth->LADSPA_FxUnit, synth->leftBuf, synth->rightBuf,
										synth->fxLeftBuf, synth->fxRightBuf);
	checkFpe ("LADSPA");
#endif

	synth->ticks += BUFSIZE;

	return 0;
}


/*
 * synthFreeVoiceByKill
 *
 * selects a voice for killing. the selection algorithm is a refinement
 * of the algorithm previously in synthAllocVoice.
 */
Voice *synthFreeVoiceByKill (Synthesizer * synth) {
	int i;
	U32 bestPrio = 999999.;
	U32 thisVoicePrio;
	Voice *voice;
	int bestVoiceIndex = -1;

/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	for (i = 0; i < synth->polyphony; i++) {

		voice = synth->voice[i];

		/* safeguard against an available voice. */
		if (_AVAILABLE (voice)) {
			return voice;
		}

		/* Determine, how 'important' a voice is.
		 * Start with an arbitrary number */
		thisVoicePrio = 10000.;

		/* Is this voice on the drum channel?
		 * Then it is very important.
		 * Also, forget about the released-note condition:
		 * Typically, drum notes are triggered only very briefly, they run most
		 * of the time in release phase.
		 */
		if (_RELEASED (voice)) {
			/* The key for this voice has been released. Consider it much less important
			 * than a voice, which is still held.
			 */
			thisVoicePrio -= 2000.;
		}

		if (_SUSTAINED (voice)) {
			/* The sustain pedal is held down on this channel.
			 * Consider it less important than non-sustained channels.
			 * This decision is somehow subjective. But usually the sustain pedal
			 * is used to play 'more-voices-than-fingers', so it shouldn't hurt
			 * if we kill one voice.
			 */
			thisVoicePrio -= 1000;
		}

		/* We are not enthusiastic about releasing voices, which have just been started.
		 * Otherwise hitting a chord may result in killing notes belonging to that very same
		 * chord.
		 * So subtract the age of the voice from the priority - an older voice is just a little
		 * bit less important than a younger voice.
		 * This is a number between roughly 0 and 100.*/
		thisVoicePrio -= (synth->noteid - voice->id);

		/* take a rough estimate of loudness into account. Louder voices are more important. */
		if (voice->volenvSection != VOICE_ENVATTACK) {
			thisVoicePrio += voice->volenvVal * 1000.;
		}

		/* check if this voice has less priority than the previous candidate. */
		if (thisVoicePrio < bestPrio)
			bestVoiceIndex = i, bestPrio = thisVoicePrio;
	}

	if (bestVoiceIndex < 0) {
		return NULL;
	}

	voice = synth->voice[bestVoiceIndex];
	voiceOff (voice);

	return voice;
}

/*
 * synthAllocVoice
 */
Voice *synthAllocVoice (Synthesizer * synth,
																				Sample * sample, int chan,
																				int key, int vel) {
	int i, k;
	Voice *voice = NULL;
	Channel *channel = NULL;

/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	/* check if there's an available synthesis process */
	for (i = 0; i < synth->polyphony; i++) {
		if (_AVAILABLE (synth->voice[i])) {
			voice = synth->voice[i];
			break;
		}
	}

	/* No success yet? Then stop a running voice. */
	if (voice == NULL) {
		voice = synthFreeVoiceByKill (synth);
	}

	if (voice == NULL) {
		return NULL;
	}

	if (chan >= 0) {
		channel = synth->channel[chan];
	} else {
		return NULL;
	}

	if (voiceInit (voice, sample, channel, key, vel,
												synth->storeid, synth->ticks,
												synth->gain) != OK) {
		return NULL;
	}

	/* add the default modulators to the synthesis process. */
	voiceAddMod (voice, &defaultVel2attMod, VOICE_DEFAULT);	/* SF2.01 $8.4.1  */
	voiceAddMod (voice, &defaultVel2filterMod, VOICE_DEFAULT);	/* SF2.01 $8.4.2  */
	voiceAddMod (voice, &defaultAt2viblfoMod, VOICE_DEFAULT);	/* SF2.01 $8.4.3  */
	voiceAddMod (voice, &defaultMod2viblfoMod, VOICE_DEFAULT);	/* SF2.01 $8.4.4  */
	voiceAddMod (voice, &defaultAttMod, VOICE_DEFAULT);	/* SF2.01 $8.4.5  */
	voiceAddMod (voice, &defaultPanMod, VOICE_DEFAULT);	/* SF2.01 $8.4.6  */
	voiceAddMod (voice, &defaultExprMod, VOICE_DEFAULT);	/* SF2.01 $8.4.7  */
	voiceAddMod (voice, &defaultReverbMod, VOICE_DEFAULT);	/* SF2.01 $8.4.8  */
	voiceAddMod (voice, &defaultChorusMod, VOICE_DEFAULT);	/* SF2.01 $8.4.9  */
	voiceAddMod (voice, &defaultPitchBendMod, VOICE_DEFAULT);	/* SF2.01 $8.4.10 */

	return voice;
}

/*
 * synthKillByExclusiveClass
 */
void synthKillByExclusiveClass (Synthesizer * synth, Voice * newVoice) {
	/** Kill all voices on a given channel, which belong into
      exclClass.  This function is called by a SoundFont's preset in
      response to a noteon event.  If one noteon event results in
      several voice processes (stereo samples), ignore_ID must name
      the voice ID of the first generated voice (so that it is not
      stopped). The first voice uses ignore_ID=-1, which will
      terminate all voices on a channel belonging into the exclusive
      class exclClass.
  */

	int i;
	int exclClass = _GEN (newVoice, GEN_EXCLUSIVECLASS);

	/* Check if the voice belongs to an exclusive class. In that case,
	   previous notes from the same class are released. */

	/* Excl. class 0: No exclusive class */
	if (exclClass == 0) {
		return;
	}

	/* Kill all notes on the same channel with the same exclusive class */

	for (i = 0; i < synth->polyphony; i++) {
		Voice *existingVoice = synth->voice[i];

		/* Existing voice does not play? Leave it alone. */
		if (!_PLAYING (existingVoice)) {
			continue;
		}

		/* An exclusive class is valid for a whole channel (or preset).
		 * Is the voice on a different channel? Leave it alone. */
		if (existingVoice->chan != newVoice->chan) {
			continue;
		}

		/* Existing voice has a different (or no) exclusive class? Leave it alone. */
		if ((int) _GEN (existingVoice, GEN_EXCLUSIVECLASS) != exclClass) {
			continue;
		}

		/* Existing voice is a voice process belonging to this noteon
		 * event (for example: stereo sample)?  Leave it alone. */
		if (existingVoice->id == newVoice->id) 
			continue;

		voiceKillExcl (existingVoice);
	};
};

/*
 * synthStartVoice
 */
void synthStartVoice (Synthesizer * synth, Voice * voice) {
/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	/* Find the exclusive class of this voice. If set, kill all voices
	 * that match the exclusive class and are younger than the first
	 * voice process created by this noteon event. */
	synthKillByExclusiveClass (synth, voice);

	/* Start the new voice */

	voiceStart (voice);
}

/* * synthGetVoicelist */
void synthGetVoicelist (Synthesizer * synth, Voice * buf[], int bufsize, int ID) {
	int i;
	int count = 0;
	for (i = 0; i < synth->polyphony; i++) {
		Voice *voice = synth->voice[i];
		if (count >= bufsize) 
			return;

		if (_PLAYING (voice) && ((int) voice->id == ID || ID < 0)) 
			buf[count++] = voice;
	}
	if (count >= bufsize) 
		return;
	buf[count++] = NULL;
}


/* Purpose:
 *
 * If the same note is hit twice on the same channel, then the older
 * voice process is advanced to the release stage.  Using a mechanical
 * MIDI controller, the only way this can happen is when the sustain
 * pedal is held.  In this case the behaviour implemented here is
 * natural for many instruments.  Note: One noteon event can trigger
 * several voice processes, for example a stereo sample.  Don't
 * release those...
 */
void
synthReleaseVoiceOnSameNote (Synthesizer * synth, int chan,
																				int key) {
	int i;
	Voice *voice;

/*   mutexLock(synth->busy); /\* Don't interfere with the audio thread *\/ */
/*   mutexUnlock(synth->busy); */

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (_PLAYING (voice)
				&& (voice->chan == chan)
				&& (voice->key == key)
				&& (voice->id != synth->noteid)) {
			voiceNoteoff (voice);
		}
	}
}

/* Purpose:
 * Sets the interpolation method to use on channel chan.
 * If chan is < 0, then set the interpolation method on all channels.
 */
int
synthSetInterpMethod (Synthesizer * synth, int chan,
															 int interpMethod) {
	int i;
	for (i = 0; i < synth->midiChannels; i++) {
		if (synth->channel[i] == NULL) {
			return FAILED;
		};
		if (chan < 0 || synth->channel[i]->channum == chan) {
			synth->channel[i]->interpMethod = interpMethod;
		};
	};
	return OK;
};

tuningT *synthCreateTuning (Synthesizer * synth, int bank, int prog, const char *name) {
	if ((bank < 0) || (bank >= 128)) {
		return NULL;
	}
	if ((prog < 0) || (prog >= 128)) {
		return NULL;
	}
	if (synth->tuning == NULL) {
		synth->tuning = ARRAY (tuningT **, 128);
		if (synth->tuning == NULL) {
			return NULL;
		}
		MEMSET (synth->tuning, 0, 128 * sizeof (tuningT **));
	}

	if (synth->tuning[bank] == NULL) {
		synth->tuning[bank] = ARRAY (tuningT *, 128);
		if (synth->tuning[bank] == NULL) {
			return NULL;
		}
		MEMSET (synth->tuning[bank], 0, 128 * sizeof (tuningT *));
	}

	if (synth->tuning[bank][prog] == NULL) {
		synth->tuning[bank][prog] = newTuning (name, bank, prog);
		if (synth->tuning[bank][prog] == NULL) {
			return NULL;
		}
	}

	if ((tuningGetName (synth->tuning[bank][prog]) == NULL)
			||
			(STRCMP (tuningGetName (synth->tuning[bank][prog]), name)
			 != 0)) {
		tuningSetName (synth->tuning[bank][prog], name);
	}

	return synth->tuning[bank][prog];
}

int
synthCreateKeyTuning (Synthesizer * synth,
															 int bank, int prog,
															 const char *name, double *pitch) {
	tuningT *tuning =
		synthCreateTuning (synth, bank, prog, name);
	if (tuning == NULL) {
		return FAILED;
	}
	if (pitch) {
		tuningSetAll (tuning, pitch);
	}
	return OK;
}


int
synthCreateOctaveTuning (Synthesizer * synth,
																	int bank, int prog,
																	const char *name, const double *pitch) {
	tuningT *tuning;

	if (!(synth != NULL))
		return FAILED;				//returnValIfFail
	if (!(bank >= 0 && bank < 128))
		return FAILED;				//returnValIfFail
	if (!(prog >= 0 && prog < 128))
		return FAILED;				//returnValIfFail
	if (!(name != NULL))
		return FAILED;				//returnValIfFail
	if (!(pitch != NULL))
		return FAILED;				//returnValIfFail

	tuning = synthCreateTuning (synth, bank, prog, name);
	if (tuning == NULL) {
		return FAILED;
	}
	tuningSetOctave (tuning, pitch);
	return OK;
}

/**
 * Activate an octave tuning on every octave in the MIDI note scale.
 * @param synth Synth instance
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param name Label name for this tuning
 * @param pitch Array of pitch values (length of 12 for each note of an octave
 *   starting at note C, values are number of offset cents to add to the normal
 *   tuning amount)
 * @param apply TRUE to apply new tuning in realtime to existing notes which
 *   are using the replaced tuning (if any), FALSE otherwise
 * @return OK on success, FAILED otherwise
 * @since 1.1.0
 */
int synthActivateOctaveTuning (Synthesizer * synth, int bank, int prog, const char *name, const double *pitch, int apply) {
	return synthCreateOctaveTuning (synth, bank, prog, name, pitch);
}


int synthTuneNotes (Synthesizer * synth, int bank, int prog, int len, int *key, double *pitch, int apply) {
	tuningT *tuning;
	int i;

	if (!(synth != NULL))
		return FAILED;				//returnValIfFail
	if (!(bank >= 0 && bank < 128))
		return FAILED;				//returnValIfFail
	if (!(prog >= 0 && prog < 128))
		return FAILED;				//returnValIfFail
	if (!(len > 0))
		return FAILED;				//returnValIfFail
	if (!(key != NULL))
		return FAILED;				//returnValIfFail
	if (!(pitch != NULL))
		return FAILED;				//returnValIfFail

	tuning = synth->tuning[bank][prog];

	if (!tuning)
		tuning = newTuning ("Unnamed", bank, prog);

	if (tuning == NULL) {
		return FAILED;
	}

	for (i = 0; i < len; i++) {
		tuningSetPitch (tuning, key[i], pitch[i]);
	}

	return OK;
}

int synthSelectTuning (Synthesizer * synth, int chan, int bank, int prog) {
	tuningT *tuning;

	if (!(synth != NULL))
		return FAILED;				//returnValIfFail
	if (!(bank >= 0 && bank < 128))
		return FAILED;				//returnValIfFail
	if (!(prog >= 0 && prog < 128))
		return FAILED;				//returnValIfFail

	tuning = synth->tuning[bank][prog];

	if (tuning == NULL) {
		return FAILED;
	}
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return FAILED;
	}

	synth->channel[chan]->tuning = tuning;

	return OK;
}

/**
 * Activate a tuning scale on a MIDI channel.
 * @param synth Synth instance
 * @param chan MIDI channel number (0 to MIDI channel count - 1)
 * @param bank Tuning bank number (0-127), not related to MIDI instrument bank
 * @param prog Tuning preset number (0-127), not related to MIDI instrument program
 * @param apply TRUE to apply tuning change to active notes, FALSE otherwise
 * @return OK on success, FAILED otherwise
 * @since 1.1.0
 *
 * NOTE: A default equal tempered scale will be created, if no tuning exists
 * on the given bank and prog.
 */
int synthActivateTuning (Synthesizer * synth, int chan, int bank, int prog, int apply) {
	return synthSelectTuning (synth, chan, bank, prog);
}

int synthResetTuning (Synthesizer * synth, int chan) {
	if ((chan < 0) || (chan >= synth->midiChannels)) 
		return FAILED;

  synth->channel[chan]->tuning = NULL;
	//channelSetTuning (synth->channel[chan], NULL);

	return OK;
}

void synthTuningIterationStart (Synthesizer * synth) {
	synth->curTuning = NULL;
}

int synthTuningIterationNext (Synthesizer * synth, int *bank, int *prog) {
	int b = 0, p = 0;

	if (synth->tuning == NULL) 
		return 0;

	if (synth->curTuning != NULL) {
		/* get the next program number */
		b = tuningGetBank (synth->curTuning);
		p = 1 + tuningGetProg (synth->curTuning);
		if (p >= 128) {
			p = 0;
			b++;
		}
	}

	while (b < 128) {
		if (synth->tuning[b] != NULL) {
			while (p < 128) {
				if (synth->tuning[b][p] != NULL) {
					synth->curTuning = synth->tuning[b][p];
					*bank = b;
					*prog = p;
					return 1;
				}
				p++;
			}
		}
		p = 0;
		b++;
	}

	return 0;
}

int synthSetGen (Synthesizer * synth, int chan, int genId, S16 value) {
	if ((chan < 0) || (chan >= synth->midiChannels)) 
		return FAILED;
	if ((genId < 0) || (genId >= GEN_LAST)) 
		return FAILED;

  synth->channel[chan]->genAbs[genId] = value;

	Voice *voice;
	for (int i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (voice->chan == chan) 
			voiceSetParam(voice, genId, value, 0);
	}

	return OK;
}

/** Change the value of a generator. This function allows to control
    all synthesis parameters in real-time. The changes are additive,
    i.e. they add up to the existing parameter value. This function is
    similar to sending an NRPN message to the synthesizer. The
    function accepts a U32 as the value of the parameter. The
    parameter numbers and ranges are described in the SoundFont 2.01
    specification, paragraph 8.1.3, page 48. See also
    'genType'.

    Using the synthSetGen2() function, it is possible to set
    the absolute value of a generator. This is an extension to the
    SoundFont standard. If 'absolute' is non-zero, the value of the
    generator specified in the SoundFont is completely ignored and the
    generator is fixed to the value passed as argument. To undo this
    behavior, you must call synthSetGen2 again, with
    'absolute' set to 0 (and possibly 'value' set to zero).

    If 'normalized' is non-zero, the value is supposed to be
    normalized between 0 and 1. Before applying the value, it will be
    scaled and shifted to the range defined in the SoundFont
    specifications.

 */
int synthSetGen2 (Synthesizer * synth, int chan, int genId, U32 value, int absolute, int normalized) {
	if ((chan < 0) || (chan >= synth->midiChannels)) 
		return FAILED;
	if ((genId < 0) || (genId >= GEN_LAST)) 
		return FAILED;

	S16 v = (normalized) ? genScale (genId, value) : value;
	channelSetGen (synth->channel[chan], genId, v, absolute);

	Voice *voice;
	for (int i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];
		if (voice->chan == chan) 
			voiceSetParam (voice, genId, v, absolute);
	}

	return OK;
}

float synthGetGen (Synthesizer * synth, int chan, int genId) {
	if ((chan < 0) || (chan >= synth->midiChannels)) {
		return 0.0;
	}

	if ((genId < 0) || (genId >= GEN_LAST)) {
		return 0.0;
	}

	return channelGetGen (synth->channel[chan], genId);
}

//midiRouter disabled
/* The synth needs to know the router for the command line handlers (they only
 * supply the synth as argument)
 */
//void synthSetMidiRouter(Synthesizer* synth, midiRouterT* router){
//  synth->midiRouter=router;
//};

/* Purpose:
 * Any MIDI event from the MIDI router arrives here and is handed
 * to the appropriate function.
 */

//synthHandleMidiEvent disabled

int synthStop (Synthesizer * synth, U32 id) {
	int i;
	Voice *voice;
	int status = FAILED;
	int count = 0;

	for (i = 0; i < synth->polyphony; i++) {
		voice = synth->voice[i];

		if (_ON (voice) && (voice->id == id)) {
			count++;
			voiceNoteoff (voice);
			status = OK;
		}
	}

	return status;
}

#define keyVelAreInRange_(zoneP, key, vel) \
  (key) >= zoneP->keylo && (key) <= zoneP->keyhi && \
  (vel) >= zoneP->vello && (vel) <= zoneP->velhi

int presetNoteon (Preset *presetP, Synthesizer * synth, int chan, int key, int vel) {
  Modulator *modP, *modEndP;
  Zone *pzoneP, *globalPresetZone;
  Instrument *instP;
  Zone *izoneP, *globalIzoneP;
  Sample *sampleP;
  Voice *voiceP;
  Modulator *modList[NUM_MOD]; /* list for 'sorting' preset modulators */
  Generator *genP, *genEndP;
  int modListCount;
  int i;

  globalPresetZone = presetP->globalZoneP;

  /* run thru all the zones of this preset */
  Zone *pzoneEndP = presetP->zoneA + presetP->nZones;
  for (Zone *pzoneP = presetP->zoneA; pzoneP < pzoneEndP; ++pzoneP) {
    /* check if the note falls into the key and velocity range of this preset */
    if (keyVelAreInRange_(pzoneP, key, vel)) {
      instP = pzoneP->u.instP;
      globalIzoneP = instP->globalZoneP;

      /* run thru all the zones of this instrument */
      Zone *izoneEndP = instP->zoneA + instP->nZones;
      for (izoneP = instP->zoneA; izoneP < izoneEndP; ++izoneP) {
        /* make sure this instrument zone has a valid sample */
        sampleP = izoneP->u.sampleP;
        /* check if the note falls into the key and velocity range of this instrument */
        if (keyVelAreInRange_(izoneP, key, vel) && sampleP != NULL) {
          /* this is a good zone. allocate a new synthesis process and initialize it */
          voiceP = synthAllocVoice (synth, sampleP, chan, key, vel);
          if (voiceP == NULL) 
            return FAILED;
          /* Instrument level, generators */
          genEndP = izoneP->genA + izoneP->nGens;
          for (genP = izoneP->genA; genP < genEndP; ++genP) {
            /* SF 2.01 section 9.4 'bullet' 4:
             *
             *A generator in a local instrument zone supersedes a
             *global instrument zone generator.  Both cases supersede
             *the default generator -> voiceGenSet */
            if (genP->flags) 
              voiceP->gen[genP->genType] = *genP;
            else if ((globalIzoneP != NULL) && (globalIzoneP->genA[i].flags)) 
              voiceP->gen[genP->genType] = globalIzoneP->genA[genP->genType];
            voiceP->gen[i].flags = (voiceP->gen[i].val != 0);  // true evalutes to GEN_SET enum (1).
          }                     /* for all generators */

          /* global instrument zone, modulators: Put them all into a list. */
          modListCount = 0;
          if (globalIzoneP) {
            Generator *genEndP = globalIzoneP->genA + 
                                 globalIzoneP->nGens;
            for (Generator *genP = globalIzoneP->genA;
                genP < genEndP;
                ++genP) {
              
              memcpy(&modList[modListCount],
                      genP->modA,
                      sizeof(Modulator) * genP->nMods);
              modListCount += genP->nMods;
            }
          }

          /* local instrument zone, modulators.
           *Replace modulators with the same definition in the list:
           *SF 2.01 page 69, 'bullet' 8
           */
          genEndP = izoneP->genA + izoneP->nGens;
          for (Generator *genP = izoneP->genA;
              genP < genEndP;
              ++genP) {
              
            Modulator *modEndP = genP->modA + genP->nMods;
            for (Modulator *modP = genP->modA;
                modP < modEndP;
                ++modP) {

              /* 'Identical' modulators will be deleted by setting their
               * list entry to NULL.  The list length is known, NULL
               * entries will be ignored later.  SF2.01 section 9.5.1
               * page 69, 'bullet' 3 defines 'identical'.  */

              // MB test for identity another, faster way
              for (i = 0; i < modListCount; i++) 
                if (modList[i] && modTestIdentity (modP, modList[i])) 
                  modList[i] = NULL;

            /* Finally add the new modulator to to the list. */
              modList[modListCount++] = modP;
            }
          }
          /* Add instrument modulators (global / local) to the voiceP. */
          for (i = 0; i < modListCount; i++) {
            modP = modList[i];
            if (modP != NULL)   /* disabled modulators CANNOT be skipped. */

              /* Instrument modulators -supersede- existing (default)
               *modulators.  SF 2.01 page 69, 'bullet' 6 */
              voiceAddMod (voiceP, modP, VOICE_OVERWRITE);
          }

          /* Preset level, generators */
          for (i = 0; i < GEN_LAST; i++) {

            /* SF 2.01 section 8.5 page 58: If some generators are
             *encountered at preset level, they should be ignored */
            if ((i != GEN_STARTADDROFS)
                && (i != GEN_ENDADDROFS)
                && (i != GEN_STARTLOOPADDROFS)
                && (i != GEN_ENDLOOPADDROFS)
                && (i != GEN_STARTADDRCOARSEOFS)
                && (i != GEN_ENDADDRCOARSEOFS)
                && (i != GEN_STARTLOOPADDRCOARSEOFS)
                && (i != GEN_KEYNUM)
                && (i != GEN_VELOCITY)
                && (i != GEN_ENDLOOPADDRCOARSEOFS)
                && (i != GEN_SAMPLEMODE)
                && (i != GEN_EXCLUSIVECLASS)
                && (i != GEN_OVERRIDEROOTKEY)) {

              /* SF 2.01 section 9.4 'bullet' 9: A generator in a
               *local preset zone supersedes a global preset zone
               *generator.  The effect is -added- to the destination
               *summing node -> voiceGenIncr */

              if (pzoneP->genA[i].flags) {
                voiceP->gen[i].val += pzoneP->genA[i].val;
              } else if ((globalPresetZone != NULL)
                         && globalPresetZone->genA[i].flags) {
                voiceP->gen[i].val += globalPresetZone->genA[i].val;
              } else {
                /* The generator has not been defined in this preset
                 *Do nothing, leave it unchanged.
                 */
              }
              voiceP->gen[i].flags = (voiceP->gen[i].val != 0);  // true evalutes to GEN_SET enum (1).
            }                   /* if available at preset level */
          }                     /* for all generators */


          /* Global preset zone, modulators: put them all into a list. */
          modListCount = 0;
          if (globalPresetZone) {
            Generator *genEndP = globalPresetZone->genA + 
                                 globalPresetZone->nGens;
            for (Generator *genP = globalPresetZone->genA;
                genP < genEndP;
                ++genP) {
              Modulator *modEndP = genP->modA + genP->nMods;
              for (Modulator *modP = genP->modA;
                   modP < modEndP;
                   ++modP)
                modList[modListCount++] = modP;
            }
          }

          /* Process the modulators of the local preset zone.  Kick
           *out all identical modulators from the global preset zone
           *(SF 2.01 page 69, second-last bullet) */

          Generator *genEndP = pzoneP->genA + 
                                 pzoneP->nGens;
          for (Generator *genP = pzoneP->genA;
                genP < genEndP;
                ++genP) {
              Modulator *modEndP = genP->modA + genP->nMods;
              for (Modulator *modP = genP->modA;
                   modP < modEndP;
                   ++modP)
                for (i = 0; i < modListCount; i++) 
                  if (modList[i] && modTestIdentity (modP, modList[i])) 
                    modList[i] = NULL;
                /* Finally add the new modulator to the list. */
                modList[modListCount++] = modP;
            }
          }

          /* Add preset modulators (global / local) to the voiceP. */
          Modulator **modEndPP = modList + modListCount;
          U32 nModsActuallyUsed = 0;
          for (Modulator **modPP = modList;
              modPP < modEndPP;
              ++modPP) {
            if ((*modPP != NULL) && ((*modPP)->amount != 0)) {  /* disabled modulators can be skipped. */

              /* Preset modulators -add- to existing instrument /
               *default modulators.  SF2.01 page 70 first bullet on page */
              voiceAddMod (voiceP, (*modPP), VOICE_ADD);
              ++nModsActuallyUsed;
            }
          }
          voiceP->nMods = nModsActuallyUsed;

          /* add the synthesis process to the synthesis loop. */
          synthStartVoice (synth, voiceP);

          /* Store the ID of the first voiceP that was created by this noteon event.
           *Exclusive class may only terminate older voices.
           *That avoids killing voices, which have just been created.
           *(a noteon event can create several voice processes with the same exclusive
           *class - for example when using stereo samples)
           */
        }
      }  // if note/vel falls in izone's range
    }  // if note/vel falls in pzone's range
  }

  return OK;
}
