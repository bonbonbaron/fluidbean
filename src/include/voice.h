struct _Voice;

#ifndef _VOICE_H
#define _VOICE_H
#include "chan.h"
#include "phase.h"
#include "enums.h"
#include "soundfont.h"

enum voiceStatus {
	VOICE_CLEAN,
	VOICE_ON,
	VOICE_SUSTAINED,
	VOICE_OFF
};

/* envelope data */
typedef struct _fluidEnvDataT {
	U32 count;
	realT coeff;
	realT incr;
	realT min;
	realT max;
} envDataT;

/* Indices for envelope tables */
enum voiceEnvelopeIndexT {
	VOICE_ENVDELAY,
	VOICE_ENVATTACK,
	VOICE_ENVHOLD,
	VOICE_ENVDECAY,
	VOICE_ENVSUSTAIN,
	VOICE_ENVRELEASE,
	VOICE_ENVFINISHED,
	VOICE_ENVLAST
};

/* Voice */
typedef struct _Voice {
	U32 id;							/* the id is incremented for every new noteon.
																   it's used for noteoff's  */
	U8 status;
	U8 chan;						/* the channel number, quick access for channel messages */
	U8 key;						/* the key, quick acces for noteoff */
	U8 vel;						/* the velocity */
  U8 nGens;
  U8 nMods;
	Channel *channel;
  Modulator *mod;
	Generator gen[GEN_LAST];
	S32 hasLooped;								/* Flag that is set as soon as the first loop is completed. */
	Sample *sampleP;
	S32 checkSampleSanityFlag;	/* Flag that initiates, that sample-related parameters
																   have to be checked. */

	/* basic parameters */
	realT outputRate;			/* the sample rate of the synthesizer */

	U32 startTime;
	U32 ticks;
	U32 noteoffTicks;		/* Delay note-off until this tick */

	realT amp;							/* current linear amplitude */
	Phase phase;					/* the phase of the sample wave */

	/* Temporary variables used in voiceWrite() */

	realT phaseIncr;			/* the phase increment for the next 64 samples */
	realT ampIncr;				/* amplitude increment value */
	S16 *dspBuf;				/* buffer to store interpolated sample data to */

	/* End temporary variables */

	/* basic parameters */
	realT pitch;						/* the pitch in midicents */
	realT attenuation;			/* the attenuation in centibels */
	realT minAttenuationCB;	/* Estimate on the smallest possible attenuation
																		 * during the lifetime of the voice */
	realT rootPitch;

	/* sample and loop start and end points (offset in sample memory).  */
	S32 start;
	S32 end;
	S32 loopstart;
	S32 loopend;									/* Note: first point following the loop (superimposed on loopstart) */

	/* master gain */
	realT synthGain;

	/* vol env */
	envDataT volenvData[VOICE_ENVLAST];
	U32 volenvCount;
	S32 volenvSection;
	realT volenvVal;
	realT amplitudeThatReachesNoiseFloorNonloop;
	realT amplitudeThatReachesNoiseFloorLoop;

	/* mod env */
	envDataT modenvData[VOICE_ENVLAST];
	U32 modenvCount;
	S32 modenvSection;
	realT modenvVal;			/* the value of the modulation envelope */
	realT modenvToFc;
	realT modenvToPitch;

	/* mod lfo */
	realT modlfoVal;			/* the value of the modulation LFO */
	U32 modlfoDelay;		/* the delay of the lfo in samples */
	realT modlfoIncr;			/* the lfo frequency is converted to a per-buffer increment */
	realT modlfoToFc;
	realT modlfoToPitch;
	realT modlfoToVol;

	/* vib lfo */
	realT viblfoVal;			/* the value of the vibrato LFO */
	U32 viblfoDelay;		/* the delay of the lfo in samples */
	realT viblfoIncr;			/* the lfo frequency is converted to a per-buffer increment */
	realT viblfoToPitch;

	/* resonant filter */
	realT fres;						/* the resonance frequency, in cents (not absolute cents) */
	realT lastFres;				/* Current resonance frequency of the IIR filter */
	/* Serves as a flag: A deviation between fres and lastFres */
	/* indicates, that the filter has to be recalculated. */
	realT qLin;						/* the q-factor on a linear scale */
	realT filterGain;			/* Gain correction factor, depends on q */
	realT hist1, hist2;		/* Sample history for the IIR filter */
	S32 filterStartup;						/* Flag: If set, the filter will be set directly.
																   Else it changes smoothly. */

	/* filter coefficients */
	/* The coefficients are normalized to a0. */
	/* b0 and b2 are identical => b02 */
	realT b02;							/* b0 / a0 */
	realT b1;							/* b1 / a0 */
	realT a1;							/* a0 / a0 */
	realT a2;							/* a1 / a0 */

	realT b02_incr;
	realT b1_incr;
	realT a1_incr;
	realT a2_incr;
	S32 filterCoeffIncrCount;

	/* pan */
	realT pan;
	realT ampLeft;
	realT ampRight;

	/* reverb */
	realT reverbSend;
	realT ampReverb;

	/* chorus */
	realT chorusSend;
	realT ampChorus;

	S32 interpMethod;

} Voice;


Voice *newVoice (realT outputRate);
S32 deleteVoice (Voice * voice);

void voiceStart (Voice * voice);

S32 voiceWrite (Voice * voice,
											 S16 * left, S16 * right,
											 S16 * reverbBuf, S16 * chorusBuf);

S32 voiceInit (Voice * voice, Sample * sample,
											Channel * channel, S32 key, S32 vel,
											U32 id, U32 time, realT gain);

S32 voiceModulate (Voice * voice, S32 cc, S32 ctrl);
S32 voiceModulateAll (Voice * voice);

/** Set the NRPN value of a generator. */
S32 voiceSetParam (Voice * voice, S32 gen, realT value, S32 abs);


/** Set the gain. */
S32 voiceSetGain (Voice * voice, realT gain);


/** Update all the synthesis parameters, which depend on generator
    'gen'. This is only necessary after changing a generator of an
    already operating voice.  Most applications will not need this
    function.*/
void voiceUpdateParam (Voice * voice, S32 gen);

S32 voiceNoteoff (Voice * voice);
S32 voiceOff (Voice * voice);
S32 voiceCalculateRuntimeSynthesisParameters (Voice *
																												voice);
Channel *voiceGetChannel (Voice * voice);
S32 calculateHoldDecayBuffers (Voice * voice, S32 genBase,
																	S32 genKey2base, S32 isDecay);
S32 voiceKillExcl (Voice * voice);
realT voiceGetLowerBoundaryForAttenuation (Voice *
																														 voice);
realT
	voiceDetermineAmplitudeThatReachesNoiseFloorForSample
	(Voice * voice);
void voiceCheckSampleSanity (Voice * voice);

#define voiceSetId(_voice, _id)  { (_voice)->id = (_id); }
#define voiceGetChan(_voice)     (_voice)->chan


#define _PLAYING(voice)  (((voice)->status == VOICE_ON) || ((voice)->status == VOICE_SUSTAINED))

/* A voice is 'ON', if it has not yet received a noteoff
 * event. Sending a noteoff event will advance the envelopes to
 * section 5 (release). */
#define _ON(voice)  ((voice)->status == VOICE_ON && (voice)->volenvSection < VOICE_ENVRELEASE)
#define _SUSTAINED(voice)  ((voice)->status == VOICE_SUSTAINED)
#define _AVAILABLE(voice)  (((voice)->status == VOICE_CLEAN) || ((voice)->status == VOICE_OFF))
#define _RELEASED(voice)  ((voice)->chan == NO_CHANNEL)
#define _SAMPLEMODE(voice) ((S32)(voice)->gen[GEN_SAMPLEMODE].val)

realT voiceGenValue (Voice * voice, S32 num);
void voiceAddMod (Voice * voice, Modulator * mod, int mode);

#define _GEN(_voice, _n) \
  ((realT)(_voice)->gen[_n].val \
   + (realT)(_voice)->gen[_n].mod \
   + (realT)(_voice)->gen[_n].nrpn)

#define SAMPLESANITY_CHECK (1 << 0)
#define SAMPLESANITY_STARTUP (1 << 1)


/* defined in dspFloat.c */

void dspFloatConfig (void);
S32 dspFloatInterpolateNone (Voice * voice);
S32 dspFloatInterpolateLinear (Voice * voice);
S32 dspFloatInterpolate_4thOrder (Voice * voice);
S32 dspFloatInterpolate_7thOrder (Voice * voice);

#endif /* _VOICE_H */
