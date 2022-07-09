#include "fluidbean.h"
#include "voice.h"
#include "chan.h"
#include "conv.h"
#include "synth.h"
#include "sys.h"
#include "soundfont.h"
#include "gen.h"
#include "mod.h"
#include "midi.h"

/* used for filter turn off optimization - if filter cutoff is above the
   specified value and filter q is below the other value, turn filter off */
#define MAX_AUDIBLE_FILTER_FC 19000.0f
#define MIN_AUDIBLE_FILTER_Q 1.2f

/* Smallest amplitude that can be perceived (full scale is +/- 0.5)
 * 16 bits => 96+4=100 dB dynamic range => 0.00001
 * 0.00001 * 2 is approximately 0.00003 :)
 */
#define NOISE_FLOOR 0.00003

/* these should be the absolute minimum that Synth can deal with */
#define MIN_LOOP_SIZE 2
#define MIN_LOOP_PAD 0

/* min vol envelope release (to stop clicks) in SoundFont timecents */
#define MIN_VOLENVRELEASE -7200.0f	/* ~16ms */

//removed inline
static void voiceEffects (Voice * voice, int count,
																 S16 * dspLeftBuf,
																 S16 * dspRightBuf,
																 S16 * dspReverbBuf,
																 S16 * dspChorusBuf);
/*
 * newVoice
 */
Voice *newVoice (realT outputRate) {
	Voice *voice;
	voice = NEW (Voice);
	if (voice == NULL) 
		return NULL;
	voice->status = VOICE_CLEAN;
	voice->chan = NO_CHANNEL;
	voice->key = 0;
	voice->vel = 0;
	voice->channel = NULL;
	voice->sampleP = NULL;
	voice->outputRate = outputRate;

	/* The 'sustain' and 'finished' segments of the volume / modulation
	 * envelope are constant. They are never affected by any modulator
	 * or generator. Therefore it is enough to initialize them once
	 * during the lifetime of the synth.
	 */
	voice->volenvData[VOICE_ENVSUSTAIN].count = 0xffffffff;
	voice->volenvData[VOICE_ENVSUSTAIN].coeff = 1.0f;
	voice->volenvData[VOICE_ENVSUSTAIN].incr = 0.0f;
	voice->volenvData[VOICE_ENVSUSTAIN].min = -1.0f;
	voice->volenvData[VOICE_ENVSUSTAIN].max = 2.0f;

	voice->volenvData[VOICE_ENVFINISHED].count = 0xffffffff;
	voice->volenvData[VOICE_ENVFINISHED].coeff = 0.0f;
	voice->volenvData[VOICE_ENVFINISHED].incr = 0.0f;
	voice->volenvData[VOICE_ENVFINISHED].min = -1.0f;
	voice->volenvData[VOICE_ENVFINISHED].max = 1.0f;

	voice->modenvData[VOICE_ENVSUSTAIN].count = 0xffffffff;
	voice->modenvData[VOICE_ENVSUSTAIN].coeff = 1.0f;
	voice->modenvData[VOICE_ENVSUSTAIN].incr = 0.0f;
	voice->modenvData[VOICE_ENVSUSTAIN].min = -1.0f;
	voice->modenvData[VOICE_ENVSUSTAIN].max = 2.0f;

	voice->modenvData[VOICE_ENVFINISHED].count = 0xffffffff;
	voice->modenvData[VOICE_ENVFINISHED].coeff = 0.0f;
	voice->modenvData[VOICE_ENVFINISHED].incr = 0.0f;
	voice->modenvData[VOICE_ENVFINISHED].min = -1.0f;
	voice->modenvData[VOICE_ENVFINISHED].max = 1.0f;

	return voice;
}

/*
 * deleteVoice
 */
int deleteVoice (Voice * voice) {
	if (voice == NULL) {
		return OK;
	}
	FREE (voice);
	return OK;
}

/* voiceInit
 *
 * Initialize the synthesis process
 */
int voiceInit (Voice * voice, Sample * sample, Channel * channel, int key, int vel, U32 id, U32 startTime, realT gain) {
	/* Note: The voice parameters will be initialized later, when the
	 * generators have been retrieved from the sound font. Here, only
	 * the 'working memory' of the voice (position in envelopes, history
	 * of IIR filters, position in sample etc) is initialized. */

	voice->id = id;
	voice->chan = channel->channum;
	voice->key = (unsigned char) key;
	voice->vel = (unsigned char) vel;
	voice->channel = channel;
	voice->sampleP = sample;
	voice->startTime = startTime;
	voice->ticks = 0;
	voice->noteoffTicks = 0;
	voice->debug = 0;
	voice->hasLooped = 0;				/* Will be set during voiceWrite when the 2nd loop point is reached */
	voice->lastFres = -1;				/* The filter coefficients have to be calculated later in the DSP loop. */
	voice->filterStartup = 1;		/* Set the filter immediately, don't fade between old and new settings */
	voice->interpMethod = voice->channel->interpMethod;

	/* vol env initialization */
	voice->volenvCount = 0;
	voice->volenvSection = 0;
	voice->volenvVal = 0.0f;
	voice->amp = 0.0f;						/* The last value of the volume envelope, used to
																   calculate the volume increment during
																   processing */

	/* mod env initialization */
	voice->modenvCount = 0;
	voice->modenvSection = 0;
	voice->modenvVal = 0.0f;

	/* mod lfo */
	voice->modlfoVal = 0.0;			/* Fixme: Retrieve from any other existing
																   voice on this channel to keep LFOs in
																   unison? */

	/* vib lfo */
	voice->viblfoVal = 0.0f;			/* Fixme: See mod lfo */

	/* Clear sample history in filter */
	voice->hist1 = 0;
	voice->hist2 = 0;

	/* Set all the generators to their default value, according to SF
	 * 2.01 section 8.1.3 (page 48). The value of NRPN messages are
	 * copied from the channel to the voice's generators. The sound font
	 * loader overwrites them. The generator values are later converted
	 * into voice parameters in
	 * voiceCalculateRuntimeSynthesisParameters.  */
  genInit (&voice->gen[0], voice->channel->gen, voice->channel->genAbs);
	//genInit (&voice->gen[0], channel);

	voice->synthGain = gain;
	/* avoid division by zero later */
	if (voice->synthGain < 0.0000001) {
		voice->synthGain = 0.0000001;
	}

	/* For a looped sample, this value will be overwritten as soon as the
	 * loop parameters are initialized (they may depend on modulators).
	 * This value can be kept, it is a worst-case estimate.
	 */

	voice->amplitudeThatReachesNoiseFloorNonloop = NOISE_FLOOR / voice->synthGain;
	voice->amplitudeThatReachesNoiseFloorLoop = NOISE_FLOOR / voice->synthGain;

	return OK;
}

void voiceGenSet (Voice * voice, int i, float val) {
	voice->gen[i].val = val;
	voice->gen[i].flags = GEN_SET;
}

void voiceGenIncr (Voice * voice, int i, float val) {
	voice->gen[i].val += val;
	voice->gen[i].flags = GEN_SET;
}

float voiceGenGet (Voice * voice, int gen) {
	return voice->gen[gen].val;
}

realT voiceGenValue (Voice * voice, int num) {
	/* This is an extension to the SoundFont standard. More
	 * documentation is available at the synthSetGen2()
	 * function. */
	if (voice->gen[num].flags == GEN_ABS_NRPN) {
		return (realT) voice->gen[num].nrpn;
	} else {
		return (realT) (voice->gen[num].val + voice->gen[num].mod +
													 voice->gen[num].nrpn);
	}
}


/*
 * voiceWrite
 *
 * This is where it all happens. This function is called by the
 * synthesizer to generate the sound samples. The synthesizer passes
 * four audio buffers: left, right, reverb out, and chorus out.
 *
 * The biggest part of this function sets the correct values for all
 * the dsp parameters (all the control data boil down to only a few
 * dsp parameters). The dsp routine is #included in several places (dspCore.c).
 */
// MB: Hmmm, okay... So what does dsp do then? 
int voiceWrite (Voice * voice, S16 * dspLeftBuf, S16 * dspRightBuf, S16 * dspReverbBuf, S16 * dspChorusBuf) {
	realT fres;
	realT targetAmp;			/* target amplitude */
	int count;

	S16 dspBuf[BUFSIZE];
	envDataT *envData;
	realT x;


	/* make sure we're playing and that we have sample data */
	if (!_PLAYING (voice))
		return OK;

	/******************* sample **********************/

	if (voice->sampleP == NULL) {
		voiceOff (voice);
		return OK;
	}

	if (voice->noteoffTicks != 0 && voice->ticks >= voice->noteoffTicks) {
		voiceNoteoff (voice);
	}

	/* Range checking for sample- and loop-related parameters
	 * Initial phase is calculated here*/
  // MB: This should be done offline, not at runtime.
	voiceCheckSampleSanity (voice);

	/******************* vol env **********************/

	envData = &voice->volenvData[voice->volenvSection];

	/* skip to the next section of the envelope if necessary */
	while (voice->volenvCount >= envData->count) {
		// If we're switching envelope stages from decay to sustain, force the value to be the end value of the previous stage
		if (envData && voice->volenvSection == VOICE_ENVDECAY)
			voice->volenvVal = envData->min * envData->coeff;

		envData = &voice->volenvData[++voice->volenvSection];
		voice->volenvCount = 0;
	}

	/* calculate the envelope value and check for valid range */
	x = envData->coeff * voice->volenvVal + envData->incr;
	if (x < envData->min) {
		x = envData->min;
		voice->volenvSection++;
		voice->volenvCount = 0;
	} else if (x > envData->max) {
		x = envData->max;
		voice->volenvSection++;
		voice->volenvCount = 0;
	}

	voice->volenvVal = x;
	voice->volenvCount++;

	if (voice->volenvSection == VOICE_ENVFINISHED) {
		voiceOff (voice);
		return OK;
	}

																					/******************* mod env **********************/

	envData = &voice->modenvData[voice->modenvSection];

	/* skip to the next section of the envelope if necessary */
	while (voice->modenvCount >= envData->count) {
		envData = &voice->modenvData[++voice->modenvSection];
		voice->modenvCount = 0;
	}

	/* calculate the envelope value and check for valid range */
	x = envData->coeff * voice->modenvVal + envData->incr;

	if (x < envData->min) {
		x = envData->min;
		voice->modenvSection++;
		voice->modenvCount = 0;
	} else if (x > envData->max) {
		x = envData->max;
		voice->modenvSection++;
		voice->modenvCount = 0;
	}

	voice->modenvVal = x;
	voice->modenvCount++;

	/******************* mod lfo **********************/

	if (voice->ticks >= voice->modlfoDelay) {
		voice->modlfoVal += voice->modlfoIncr;

		if (voice->modlfoVal > 1.0) {
			voice->modlfoIncr = -voice->modlfoIncr;
			voice->modlfoVal = (realT) 2.0 - voice->modlfoVal;
		} else if (voice->modlfoVal < -1.0) {
			voice->modlfoIncr = -voice->modlfoIncr;
			voice->modlfoVal = (realT) - 2.0 - voice->modlfoVal;
		}
	}

																					/******************* vib lfo **********************/

	if (voice->ticks >= voice->viblfoDelay) {
		voice->viblfoVal += voice->viblfoIncr;

		if (voice->viblfoVal > (realT) 1.0) {
			voice->viblfoIncr = -voice->viblfoIncr;
			voice->viblfoVal = (realT) 2.0 - voice->viblfoVal;
		} else if (voice->viblfoVal < -1.0) {
			voice->viblfoIncr = -voice->viblfoIncr;
			voice->viblfoVal = (realT) - 2.0 - voice->viblfoVal;
		}
	}

																					/******************* amplitude **********************/

	/* calculate final amplitude
	 * - initial gain
	 * - amplitude envelope
	 */

	if (voice->volenvSection == VOICE_ENVDELAY)
		goto postProcess;					/* The volume amplitude is in hold phase. No sound is produced. */

	if (voice->volenvSection == VOICE_ENVATTACK) {
		/* the envelope is in the attack section: ramp linearly to max value.
		 * A positive modlfoToVol should increase volume (negative attenuation).
		 */
		targetAmp = atten2amp (voice->attenuation)
			* cb2amp (voice->modlfoVal * -voice->modlfoToVol)
			* voice->volenvVal;
	} else {
		realT amplitudeThatReachesNoiseFloor;
		realT ampMax;

		targetAmp = atten2amp (voice->attenuation)
			* cb2amp (960.0f * (1.0f - voice->volenvVal)
											+ voice->modlfoVal * -voice->modlfoToVol);

		/* We turn off a voice, if the volume has dropped low enough. */

		/* A voice can be turned off, when an estimate for the volume
		 * (upper bound) falls below that volume, that will drop the
		 * sample below the noise floor.
		 */

		/* If the loop amplitude is known, we can use it if the voice loop is within
		 * the sample loop
		 */

		/* Is the playing pointer already in the loop? */
		if (voice->hasLooped)
			amplitudeThatReachesNoiseFloor = voice->amplitudeThatReachesNoiseFloorLoop;
		else
			amplitudeThatReachesNoiseFloor = voice->amplitudeThatReachesNoiseFloorNonloop;

		/* voice->attenuationMin is a lower boundary for the attenuation
		 * now and in the future (possibly 0 in the worst case).  Now the
		 * amplitude of sample and volenv cannot exceed ampMax (since
		 * volenvVal can only drop):
		 */

		ampMax = atten2amp (voice->minAttenuationCB) * voice->volenvVal;

		/* And if ampMax is already smaller than the known amplitude,
		 * which will attenuate the sample below the noise floor, then we
		 * can safely turn off the voice. Duh. */
		if (ampMax < amplitudeThatReachesNoiseFloor) {
			voiceOff (voice);
			goto postProcess;
		}
	}

	/* Volume increment to go from voice->amp to targetAmp in BUFSIZE steps */
	voice->ampIncr = (targetAmp - voice->amp) / BUFSIZE;

	/* no volume and not changing? - No need to process */
	if ((voice->amp == 0.0f) && (voice->ampIncr == 0.0f))
		goto postProcess;

	/* Calculate the number of samples, that the DSP loop advances
	 * through the original waveform with each step in the output
	 * buffer. It is the ratio between the frequencies of original
	 * waveform and output waveform.*/
	voice->phaseIncr = ct2hzReal
		(voice->pitch + voice->modlfoVal * voice->modlfoToPitch
		 + voice->viblfoVal * voice->viblfoToPitch
		 + voice->modenvVal * voice->modenvToPitch) / voice->rootPitch;

	/* if phaseIncr is not advancing, set it to the minimum fraction value (prevent stuckage) */
	if (voice->phaseIncr == 0)
		voice->phaseIncr = 1;

	/*************** resonant filter ******************/

	/* calculate the frequency of the resonant filter in Hz */
	fres = ct2hz (voice->fres
											+ voice->modlfoVal * voice->modlfoToFc
											+ voice->modenvVal * voice->modenvToFc);

	/* FIXME - Still potential for a click during turn on, can we interpolate
	   between 20khz cutoff and 0 Q? */

	/* I removed the optimization of turning the filter off when the
	 * resonance frequence is above the maximum frequency. Instead, the
	 * filter frequency is set to a maximum of 0.45 times the sampling
	 * rate. For a 44100 kHz sampling rate, this amounts to 19845
	 * Hz. The reason is that there were problems with anti-aliasing when the
	 * synthesizer was run at lower sampling rates. Thanks to Stephan
	 * Tassart for pointing me to this bug. By turning the filter on and
	 * clipping the maximum filter frequency at 0.45*srate, the filter
	 * is used as an anti-aliasing filter. */

	if (fres > 0.45f * voice->outputRate)
		fres = 0.45f * voice->outputRate;
	else if (fres < 5)
		fres = 5;

	/* if filter enabled and there is a significant frequency change.. */
	if ((fabs (fres - voice->lastFres) > 0.01)) {
		/* The filter coefficients have to be recalculated (filter
		 * parameters have changed). Recalculation for various reasons is
		 * forced by setting lastFres to -1.  The flag filterStartup
		 * indicates, that the DSP loop runs for the first time, in this
		 * case, the filter is set directly, instead of smoothly fading
		 * between old and new settings.
		 *
		 * Those equations from Robert Bristow-Johnson's `Cookbook
		 * formulae for audio EQ biquad filter coefficients', obtained
		 * from Harmony-central.com / Computer / Programming. They are
		 * the result of the bilinear transform on an analogue filter
		 * prototype. To quote, `BLT frequency warping has been taken
		 * into account for both significant frequency relocation and for
		 * bandwidth readjustment'. */

		realT omega =
			(realT) (2.0 * M_PI * (fres / ((float) voice->outputRate)));
		realT sinCoeff = (realT) sin (omega);
		realT cosCoeff = (realT) cos (omega);
		realT alphaCoeff = sinCoeff / (2.0f * voice->qLin);
		realT a0_inv = 1.0f / (1.0f + alphaCoeff);

		/* Calculate the filter coefficients. All coefficients are
		 * normalized by a0. Think of `a1' as `a1/a0'.
		 *
		 * Here a couple of multiplications are saved by reusing common expressions.
		 * The original equations should be:
		 *  voice->b0=(1.-cosCoeff)*a0_inv*0.5*voice->filterGain;
		 *  voice->b1=(1.-cosCoeff)*a0_inv*voice->filterGain;
		 *  voice->b2=(1.-cosCoeff)*a0_inv*0.5*voice->filterGain; */

		realT a1_temp = -2.0f * cosCoeff * a0_inv;
		realT a2_temp = (1.0f - alphaCoeff) * a0_inv;
		realT b1_temp = (1.0f - cosCoeff) * a0_inv * voice->filterGain;
		/* both b0 -and- b2 */
		realT b02_temp = b1_temp * 0.5f;

		if (voice->filterStartup) {
			/* The filter is calculated, because the voice was started up.
			 * In this case set the filter coefficients without delay.
			 */
			voice->a1 = a1_temp;
			voice->a2 = a2_temp;
			voice->b02 = b02_temp;
			voice->b1 = b1_temp;
			voice->filterCoeffIncrCount = 0;
			voice->filterStartup = 0;
		} else {

			/* The filter frequency is changed.  Calculate an increment
			 * factor, so that the new setting is reached after one buffer
			 * length. xIncr is added to the current value BUFSIZE
			 * times. The length is arbitrarily chosen. Longer than one
			 * buffer will sacrifice some performance, though.  Note: If
			 * the filter is still too 'grainy', then increase this number
			 * at will.
			 */

#define FILTER_TRANSITION_SAMPLES (BUFSIZE)

			voice->a1_incr = (a1_temp - voice->a1) / FILTER_TRANSITION_SAMPLES;
			voice->a2_incr = (a2_temp - voice->a2) / FILTER_TRANSITION_SAMPLES;
			voice->b02_incr = (b02_temp - voice->b02) / FILTER_TRANSITION_SAMPLES;
			voice->b1_incr = (b1_temp - voice->b1) / FILTER_TRANSITION_SAMPLES;
			/* Have to add the increments filterCoeffIncrCount times. */
			voice->filterCoeffIncrCount = FILTER_TRANSITION_SAMPLES;
		}
		voice->lastFres = fres;
	}


  /*********************** run the dsp chain ************************
   * The sample is mixed with the output buffer.
   * The buffer has to be filled from 0 to BUFSIZE-1.
   * Depending on the position in the loop and the loop size, this
   * may require several runs. */

	voice->dspBuf = dspBuf;

	switch (voice->interpMethod) {
	case INTERP_NONE:
		count = dspFloatInterpolateNone (voice);
		break;
	case INTERP_LINEAR:
		count = dspFloatInterpolateLinear (voice);
		break;
	case INTERP_4THORDER:
	default:
		count = dspFloatInterpolate_4thOrder (voice);
		break;
	case INTERP_7THORDER:
		count = dspFloatInterpolate_7thOrder (voice);
		break;
	}

	if (count > 0)
		voiceEffects (voice, count, dspLeftBuf, dspRightBuf,
												 dspReverbBuf, dspChorusBuf);

	/* turn off voice if short count (sample ended and not looping) */
	if (count < BUFSIZE) {
		voiceOff (voice);
	}

postProcess:
	voice->ticks += BUFSIZE;
	return OK;
}


/* Purpose:
 *
 * - filters (applies a lowpass filter with variable cutoff frequency and quality factor)
 * - mixes the processed sample to left and right output using the pan setting
 * - sends the processed sample to chorus and reverb
 *
 * Variable description:
 * - dspData: Pointer to the original waveform data
 * - dspLeftBuf: The generated signal goes here, left channel
 * - dspRightBuf: right channel
 * - dspReverbBuf: Send to reverb unit
 * - dspChorusBuf: Send to chorus unit
 * - dspA1: Coefficient for the filter
 * - dspA2: same
 * - dspB0: same
 * - dspB1: same
 * - dspB2: same
 * - voice holds the voice structure
 *
 * A couple of variables are used internally, their results are discarded:
 * - dspI: Index through the output buffer
 * - dspPhaseFractional: The fractional part of dspPhase
 * - dspCoeff: A table of four coefficients, depending on the fractional phase.
 *              Used to interpolate between samples.
 * - dspProcessBuffer: Holds the processed signal between stages
 * - dspCenternode: delay line for the IIR filter
 * - dspHist1: same
 * - dspHist2: same
 *
 */
static void voiceEffects (
                     Voice * voice, int count,
										 S16 *dspLeftBuf,
										 S16 *dspRightBuf,
										 S16 *dspReverbBuf,
										 S16 *dspChorusBuf) {
	/* IIR filter sample history */
	S16 dspHist1 = voice->hist1;
	S16 dspHist2 = voice->hist2;

	/* IIR filter coefficients */
	S16 dspA1 = voice->a1;
	S16 dspA2 = voice->a2;
	S16 dspB02 = voice->b02;
	S16 dspB1 = voice->b1;
	S16 dspA1_incr = voice->a1_incr;
	S16 dspA2_incr = voice->a2_incr;
	S16 dspB02_incr = voice->b02_incr;
	S16 dspB1_incr = voice->b1_incr;
	int dspFilterCoeffIncrCount = voice->filterCoeffIncrCount;

	S16 *dspBuf = voice->dspBuf;

	S16 dspCenternode;
	int dspI;
	S16 v;

	/* filter (implement the voice filter according to SoundFont standard) */
	/* Two versions of the filter loop. One, while the filter is
	 * changing towards its new setting. The other, if the filter
	 * doesn't change.
	 */

	if (dspFilterCoeffIncrCount > 0) {
		/* Increment is added to each filter coefficient filterCoeffIncrCount times. */
		for (dspI = 0; dspI < count; dspI++) {
			/* The filter is implemented in Direct-II form. */
			dspCenternode =
				dspBuf[dspI] - dspA1 * dspHist1 - dspA2 * dspHist2;
			dspBuf[dspI] =
				dspB02 * (dspCenternode + dspHist2) + dspB1 * dspHist1;
			dspHist2 = dspHist1;
			dspHist1 = dspCenternode;

			if (dspFilterCoeffIncrCount-- > 0) {
				dspA1 += dspA1_incr;
				dspA2 += dspA2_incr;
				dspB02 += dspB02_incr;
				dspB1 += dspB1_incr;
			}
		}														/* for dspI */
	} else {											/* The filter parameters are constant.  This is duplicated to save time. */
		for (dspI = 0; dspI < count; dspI++) {	/* The filter is implemented in Direct-II form. */
			dspCenternode =
				dspBuf[dspI] - dspA1 * dspHist1 - dspA2 * dspHist2;
			dspBuf[dspI] =
				dspB02 * (dspCenternode + dspHist2) + dspB1 * dspHist1;
			dspHist2 = dspHist1;
			dspHist1 = dspCenternode;
		}
	}

	/* pan (Copy the signal to the left and right output buffer) The voice
	 * panning generator has a range of -500 .. 500.  If it is centered,
	 * it's close to 0.  voice->ampLeft and voice->ampRight are then the
	 * same, and we can save one multiplication per voice and sample.
	 */
	if ((-0.5 < voice->pan) && (voice->pan < 0.5)) {
		/* The voice is centered. Use voice->ampLeft twice. */
		for (dspI = 0; dspI < count; dspI++) {
			v = voice->ampLeft * dspBuf[dspI];
			dspLeftBuf[dspI] += v;
			dspRightBuf[dspI] += v;
		}
	} else {											/* The voice is not centered. Stereo samples have one side zero. */
		if (voice->ampLeft != 0.0) {
			for (dspI = 0; dspI < count; dspI++)
				dspLeftBuf[dspI] += voice->ampLeft * dspBuf[dspI];
		}

		if (voice->ampRight != 0.0) {
			for (dspI = 0; dspI < count; dspI++)
				dspRightBuf[dspI] += voice->ampRight * dspBuf[dspI];
		}
	}

	/* reverb send. Buffer may be NULL. */
	if ((dspReverbBuf != NULL) && (voice->ampReverb != 0.0)) {
		for (dspI = 0; dspI < count; dspI++)
			dspReverbBuf[dspI] += voice->ampReverb * dspBuf[dspI];
	}

	/* chorus send. Buffer may be NULL. */
	if ((dspChorusBuf != NULL) && (voice->ampChorus != 0)) {
		for (dspI = 0; dspI < count; dspI++)
			dspChorusBuf[dspI] += voice->ampChorus * dspBuf[dspI];
	}

	voice->hist1 = dspHist1;
	voice->hist2 = dspHist2;
	voice->a1 = dspA1;
	voice->a2 = dspA2;
	voice->b02 = dspB02;
	voice->b1 = dspB1;
	voice->filterCoeffIncrCount = dspFilterCoeffIncrCount;
}

/* voiceStart */
void voiceStart (Voice * voice) {
	/* The maximum volume of the loop is calculated and cached once for each
	 * sample with its nominal loop settings. This happens, when the sample is used
	 * for the first time.*/
	voiceCalculateRuntimeSynthesisParameters (voice);
	/* Force setting of the phase at the first DSP loop run
	 * This cannot be done earlier, because it depends on modulators.*/
	voice->checkSampleSanityFlag = SAMPLESANITY_STARTUP;
	voice->status = VOICE_ON;
}

/*
 * voiceCalculateRuntimeSynthesisParameters
 *
 * in this function we calculate the values of all the parameters. the
 * parameters are converted to their most useful unit for the DSP
 * algorithm, for example, number of samples instead of
 * timecents. Some parameters keep their "perceptual" unit and
 * conversion will be done in the DSP function. This is the case, for
 * example, for the pitch since it is modulated by the controllers in
 * cents. */
int voiceCalculateRuntimeSynthesisParameters (Voice * voice) {
	int i;

	int listOfGeneratorsToInitialize[35] = {
		GEN_STARTADDROFS,						/* SF2.01 page 48 #0   */
		GEN_ENDADDROFS,							/*                #1   */
		GEN_STARTLOOPADDROFS,				/*                #2   */
		GEN_ENDLOOPADDROFS,					/*                #3   */
		/* GEN_STARTADDRCOARSEOFS see comment below [1]        #4   */
		GEN_MODLFOTOPITCH,					/*                #5   */
		GEN_VIBLFOTOPITCH,					/*                #6   */
		GEN_MODENVTOPITCH,					/*                #7   */
		GEN_FILTERFC,								/*                #8   */
		GEN_FILTERQ,								/*                #9   */
		GEN_MODLFOTOFILTERFC,				/*                #10  */
		GEN_MODENVTOFILTERFC,				/*                #11  */
		/* GEN_ENDADDRCOARSEOFS [1]                            #12  */
		GEN_MODLFOTOVOL,						/*                #13  */
		/* not defined                                         #14  */
		GEN_CHORUSSEND,							/*                #15  */
		GEN_REVERBSEND,							/*                #16  */
		GEN_PAN,										/*                #17  */
		/* not defined                                         #18  */
		/* not defined                                         #19  */
		/* not defined                                         #20  */
		GEN_MODLFODELAY,						/*                #21  */
		GEN_MODLFOFREQ,							/*                #22  */
		GEN_VIBLFODELAY,						/*                #23  */
		GEN_VIBLFOFREQ,							/*                #24  */
		GEN_MODENVDELAY,						/*                #25  */
		GEN_MODENVATTACK,						/*                #26  */
		GEN_MODENVHOLD,							/*                #27  */
		GEN_MODENVDECAY,						/*                #28  */
		/* GEN_MODENVSUSTAIN [1]                               #29  */
		GEN_MODENVRELEASE,					/*                #30  */
		/* GEN_KEYTOMODENVHOLD [1]                             #31  */
		/* GEN_KEYTOMODENVDECAY [1]                            #32  */
		GEN_VOLENVDELAY,						/*                #33  */
		GEN_VOLENVATTACK,						/*                #34  */
		GEN_VOLENVHOLD,							/*                #35  */
		GEN_VOLENVDECAY,						/*                #36  */
		/* GEN_VOLENVSUSTAIN [1]                               #37  */
		GEN_VOLENVRELEASE,					/*                #38  */
		/* GEN_KEYTOVOLENVHOLD [1]                             #39  */
		/* GEN_KEYTOVOLENVDECAY [1]                            #40  */
		/* GEN_STARTLOOPADDRCOARSEOFS [1]                      #45  */
		GEN_KEYNUM,									/*                #46  */
		GEN_VELOCITY,								/*                #47  */
		GEN_ATTENUATION,						/*                #48  */
		/* GEN_ENDLOOPADDRCOARSEOFS [1]                        #50  */
		/* GEN_COARSETUNE           [1]                        #51  */
		/* GEN_FINETUNE             [1]                        #52  */
		GEN_OVERRIDEROOTKEY,				/*                #58  */
		GEN_PITCH,									/*                ---  */
		-1
	};														/* end-of-list marker  */

	/* When the voice is made ready for the synthesis process, a lot of
	 * voice-internal parameters have to be calculated.
	 *
	 * At this point, the sound font has already set the -nominal- value
	 * for all generators (excluding GEN_PITCH). Most generators can be
	 * modulated - they include a nominal value and an offset (which
	 * changes with velocity, note number, channel parameters like
	 * aftertouch, mod wheel...) Now this offset will be calculated as
	 * follows:
	 *
	 *  - Process each modulator once.
	 *  - Calculate its output value.
	 *  - Find the target generator.
	 *  - Add the output value to the modulation value of the generator.
	 *
	 * Note: The generators have been initialized with
	 * genSetDefaultValues.
	 */

	for (i = 0; i < voice->nMods; i++) {
		Modulator *mod = &voice->mod[i];
		realT modval = modGetValue (mod, voice->channel, voice);
		int destGenIndex = mod->dest;
		Generator *destGen = &voice->gen[destGenIndex];
		destGen->mod += modval;
		/*      dumpModulator(mod); */
	}

	/* The GEN_PITCH is a hack to fit the pitch bend controller into the
	 * modulator paradigm.  Now the nominal pitch of the key is set.
	 * Note about SCALETUNE: SF2.01 8.1.3 says, that this generator is a
	 * non-realtime parameter. So we don't allow modulation (as opposed
	 * to _GEN(voice, GEN_SCALETUNE) When the scale tuning is varied,
	 * one key remains fixed. Here C3 (MIDI number 60) is used.
	 */
	if (channelHasTuning (voice->channel)) {
		/* pitch(60) + scale * (pitch(key) - pitch(60)) */
#define __pitch(_k) tuningGetPitch(tuning, _k)
		tuningT *tuning = channelGetTuning (voice->channel);
		voice->gen[GEN_PITCH].val =
			(__pitch (60) +
			 (voice->gen[GEN_SCALETUNE].val / 100.0f *
				(__pitch (voice->key) - __pitch (60))));
	} else {
		voice->gen[GEN_PITCH].val =
			(voice->gen[GEN_SCALETUNE].val * (voice->key - 60.0f)
			 + 100.0f * 60.0f);
	}

	/* Now the generators are initialized, nominal and modulation value.
	 * The voice parameters (which depend on generators) are calculated
	 * with voiceUpdateParam. Processing the list of generator
	 * changes will calculate each voice parameter once.
	 *
	 * Note [1]: Some voice parameters depend on several generators. For
	 * example, the pitch depends on GEN_COARSETUNE, GEN_FINETUNE and
	 * GEN_PITCH.  voice->pitch.  Unnecessary recalculation is avoided
	 * by removing all but one generator from the list of voice
	 * parameters.  Same with GEN_XXX and GEN_XXXCOARSE: the
	 * initialisation list contains only GEN_XXX.
	 */

	/* Calculate the voice parameter(s) dependent on each generator. */
	for (i = 0; listOfGeneratorsToInitialize[i] != -1; i++) {
		voiceUpdateParam (voice, listOfGeneratorsToInitialize[i]);
	}

	/* Make an estimate on how loud this voice can get at any time (attenuation). */
	voice->minAttenuationCB = voiceGetLowerBoundaryForAttenuation(voice);

	return OK;
}

/* calculateHoldDecayBuffers */
int
calculateHoldDecayBuffers (Voice * voice, int genBase,
															int genKey2base, int isDecay) {
	/* Purpose:
	 *
	 * Returns the number of DSP loops, that correspond to the hold
	 * (isDecay=0) or decay (isDecay=1) time.
	 * genBase=GEN_VOLENVHOLD, GEN_VOLENVDECAY, GEN_MODENVHOLD,
	 * GEN_MODENVDECAY genKey2base=GEN_KEYTOVOLENVHOLD,
	 * GEN_KEYTOVOLENVDECAY, GEN_KEYTOMODENVHOLD, GEN_KEYTOMODENVDECAY
	 */

	realT timecents;
	realT seconds;
	int buffers;

	/* SF2.01 section 8.4.3 # 31, 32, 39, 40
	 * GEN_KEYTOxxxENVxxx uses key 60 as 'origin'.
	 * The unit of the generator is timecents per key number.
	 * If KEYTOxxxENVxxx is 100, a key one octave over key 60 (72)
	 * will cause (60-72)*100=-1200 timecents of time variation.
	 * The time is cut in half.
	 */
	timecents =
		(_GEN (voice, genBase) +
		 _GEN (voice, genKey2base) * (60.0 - voice->key));

	/* Range checking */
	if (isDecay) {
		/* SF 2.01 section 8.1.3 # 28, 36 */
		if (timecents > 8000.0) {
			timecents = 8000.0;
		}
	} else {
		/* SF 2.01 section 8.1.3 # 27, 35 */
		if (timecents > 5000) {
			timecents = 5000.0;
		}
		/* SF 2.01 section 8.1.2 # 27, 35:
		 * The most negative number indicates no hold time
		 */
		if (timecents <= -32768.) {
			return 0;
		}
	}
	/* SF 2.01 section 8.1.3 # 27, 28, 35, 36 */
	if (timecents < -12000.0) {
		timecents = -12000.0;
	}

	seconds = tc2sec (timecents);
	/* Each DSP loop processes BUFSIZE samples. */

	/* round to next full number of buffers */
	buffers = (int) (((realT) voice->outputRate * seconds)
									 / (realT) BUFSIZE + 0.5);

	return buffers;
}

/*
 * voiceUpdateParam
 *
 * Purpose:
 *
 * The value of a generator (gen) has changed.  (The different
 * generators are listed in fluidlite.h, or in SF2.01 page 48-49)
 * Now the dependent 'voice' parameters are calculated.
 *
 * voiceUpdateParam can be called during the setup of the
 * voice (to calculate the initial value for a voice parameter), or
 * during its operation (a generator has been changed due to
 * real-time parameter modifications like pitch-bend).
 *
 * Note: The generator holds three values: The base value .val, an
 * offset caused by modulators .mod, and an offset caused by the
 * NRPN system. _GEN(voice, generatorEnumerator) returns the sum
 * of all three.
 */
void voiceUpdateParam (Voice * voice, int gen) {
	double qDB;
	realT x;
	realT y;
	U32 count;
	// Alternate attenuation scale used by EMU10K1 cards when setting the attenuation at the preset or instrument level within the SoundFont bank.
	static const float ALT_ATTENUATION_SCALE = 0.4;

	switch (gen) {

	case GEN_PAN:
		/* range checking is done in the pan function */
		voice->pan = _GEN (voice, GEN_PAN);
		voice->ampLeft =
			pan (voice->pan, 1) * voice->synthGain / 32768.0f;
		voice->ampRight =
			pan (voice->pan, 0) * voice->synthGain / 32768.0f;
		break;

	case GEN_ATTENUATION:
		voice->attenuation =
			((realT) (voice)->gen[GEN_ATTENUATION].val *
			 ALT_ATTENUATION_SCALE) +
			(realT) (voice)->gen[GEN_ATTENUATION].mod +
			(realT) (voice)->gen[GEN_ATTENUATION].nrpn;

		/* Range: SF2.01 section 8.1.3 # 48
		 * Motivation for range checking:
		 * OHPiano.SF2 sets initial attenuation to a whooping -96 dB */
		clip (voice->attenuation, 0.0, 1440.0);
		break;

		/* The pitch is calculated from three different generators.
		 * Read comment in fluidlite.h about GEN_PITCH.
		 */
	case GEN_PITCH:
	case GEN_COARSETUNE:
	case GEN_FINETUNE:
		/* The testing for allowed range is done in 'ct2hz' */
		voice->pitch = (_GEN (voice, GEN_PITCH)
										+ 100.0f * _GEN (voice, GEN_COARSETUNE)
										+ _GEN (voice, GEN_FINETUNE));
		break;

	case GEN_REVERBSEND:
		/* The generator unit is 'tenths of a percent'. */
		voice->reverbSend = _GEN (voice, GEN_REVERBSEND) / 1000.0f;
		clip (voice->reverbSend, 0.0, 1.0);
		voice->ampReverb = voice->reverbSend * voice->synthGain / 32768.0f;
		break;

	case GEN_CHORUSSEND:
		/* The generator unit is 'tenths of a percent'. */
		voice->chorusSend = _GEN (voice, GEN_CHORUSSEND) / 1000.0f;
		clip (voice->chorusSend, 0.0, 1.0);
		voice->ampChorus = voice->chorusSend * voice->synthGain / 32768.0f;
		break;

	case GEN_OVERRIDEROOTKEY:
		/* This is a non-realtime parameter. Therefore the .mod part of the generator
		 * can be neglected.
		 * NOTE: origpitch sets MIDI root note while pitchadj is a fine tuning amount
		 * which offsets the original rate.  This means that the fine tuning is
		 * inverted with respect to the root note (so subtract it, not add).
		 */
		if (voice->gen[GEN_OVERRIDEROOTKEY].val > -1) {	//FIXME: use flag instead of -1
			voice->rootPitch = voice->gen[GEN_OVERRIDEROOTKEY].val * 100.0f
				- voice->sampleP->origPitchAdj;
		} else {
			voice->rootPitch =
				voice->sampleP->origPitch * 100.0f - voice->sampleP->origPitchAdj;
		}
		voice->rootPitch = ct2hz (voice->rootPitch);
#define ONLY_SUPPORTED_SAMPLE_RATE (44100)
		if (voice->sampleP != NULL) {
			voice->rootPitch *=
				(realT) voice->outputRate / ONLY_SUPPORTED_SAMPLE_RATE;
		}
		break;

	case GEN_FILTERFC:
		/* The resonance frequency is converted from absolute cents to
		 * midicents .val and .mod are both used, this permits real-time
		 * modulation.  The allowed range is tested in the 'ct2hz'
		 * function [PH,20021214]
		 */
		voice->fres = _GEN (voice, GEN_FILTERFC);

		/* The synthesis loop will have to recalculate the filter
		 * coefficients. */
		voice->lastFres = -1.0f;
		break;

	case GEN_FILTERQ:
		/* The generator contains 'centibels' (1/10 dB) => divide by 10 to
		 * obtain dB */
		qDB = _GEN (voice, GEN_FILTERQ) / 10.0f;

		/* Range: SF2.01 section 8.1.3 # 8 (convert from cB to dB => /10) */
		clip (qDB, 0.0f, 96.0f);

		/* Short version: Modify the Q definition in a way, that a Q of 0
		 * dB leads to no resonance hump in the freq. response.
		 *
		 * Long version: From SF2.01, page 39, item 9 (initialFilterQ):
		 * "The gain at the cutoff frequency may be less than zero when
		 * zero is specified".  Assume qDB=0 / qLin=1: If we would leave
		 * q as it is, then this results in a 3 dB hump slightly below
		 * fc. At fc, the gain is exactly the DC gain (0 dB).  What is
		 * (probably) meant here is that the filter does not show a
		 * resonance hump for qDB=0. In this case, the corresponding
		 * qLin is 1/sqrt(2)=0.707.  The filter should have 3 dB of
		 * attenuation at fc now.  In this case Q_dB is the height of the
		 * resonance peak not over the DC gain, but over the frequency
		 * response of a non-resonant filter.  This idea is implemented as
		 * follows: */
		qDB -= 3.01f;

		/* The 'sound font' Q is defined in dB. The filter needs a linear
		   q. Convert. */
		voice->qLin = (realT) (pow (10.0f, qDB / 20.0f));

		/* SF 2.01 page 59:
		 *
		 *  The SoundFont specs ask for a gain reduction equal to half the
		 *  height of the resonance peak (Q).  For example, for a 10 dB
		 *  resonance peak, the gain is reduced by 5 dB.  This is done by
		 *  multiplying the total gain with sqrt(1/Q).  `Sqrt' divides dB
		 *  by 2 (100 lin = 40 dB, 10 lin = 20 dB, 3.16 lin = 10 dB etc)
		 *  The gain is later factored into the 'b' coefficients
		 *  (numerator of the filter equation).  This gain factor depends
		 *  only on Q, so this is the right place to calculate it.
		 */
		voice->filterGain = (realT) (1.0 / sqrt (voice->qLin));

		/* The synthesis loop will have to recalculate the filter coefficients. */
		voice->lastFres = -1.;
		break;

	case GEN_MODLFOTOPITCH:
		voice->modlfoToPitch = _GEN (voice, GEN_MODLFOTOPITCH);
		clip (voice->modlfoToPitch, -12000.0, 12000.0);
		break;

	case GEN_MODLFOTOVOL:
		voice->modlfoToVol = _GEN (voice, GEN_MODLFOTOVOL);
		clip (voice->modlfoToVol, -960.0, 960.0);
		break;

	case GEN_MODLFOTOFILTERFC:
		voice->modlfoToFc = _GEN (voice, GEN_MODLFOTOFILTERFC);
		clip (voice->modlfoToFc, -12000, 12000);
		break;

	case GEN_MODLFODELAY:
		x = _GEN (voice, GEN_MODLFODELAY);
		clip (x, -12000.0f, 5000.0f);
		voice->modlfoDelay =
			(U32) (voice->outputRate * tc2secDelay (x));
		break;

	case GEN_MODLFOFREQ:
		/* - the frequency is converted into a delta value, per buffer of BUFSIZE samples
		 * - the delay into a sample delay
		 */
		x = _GEN (voice, GEN_MODLFOFREQ);
		clip (x, -16000.0f, 4500.0f);
		voice->modlfoIncr =
			(4.0f * BUFSIZE * act2hz (x) / voice->outputRate);
		break;

	case GEN_VIBLFOFREQ:
		/* vib lfo
		 *
		 * - the frequency is converted into a delta value, per buffer of BUFSIZE samples
		 * - the delay into a sample delay
		 */
		x = _GEN (voice, GEN_VIBLFOFREQ);
		clip (x, -16000.0f, 4500.0f);
		voice->viblfoIncr =
			(4.0f * BUFSIZE * act2hz (x) / voice->outputRate);
		break;

	case GEN_VIBLFODELAY:
		x = _GEN (voice, GEN_VIBLFODELAY);
		clip (x, -12000.0f, 5000.0f);
		voice->viblfoDelay =
			(U32) (voice->outputRate * tc2secDelay (x));
		break;

	case GEN_VIBLFOTOPITCH:
		voice->viblfoToPitch = _GEN (voice, GEN_VIBLFOTOPITCH);
		clip (voice->viblfoToPitch, -12000.0, 12000.0);
		break;

	case GEN_KEYNUM:
		/* GEN_KEYNUM: SF2.01 page 46, item 46
		 *
		 * If this generator is active, it forces the key number to its
		 * value.  Non-realtime controller.
		 *
		 * There is a flag, which should indicate, whether a generator is
		 * enabled or not.  But here we rely on the default value of -1.
		 * */
		x = _GEN (voice, GEN_KEYNUM);
		if (x >= 0) {
			voice->key = x;
		}
		break;

	case GEN_VELOCITY:
		/* GEN_VELOCITY: SF2.01 page 46, item 47
		 *
		 * If this generator is active, it forces the velocity to its
		 * value. Non-realtime controller.
		 *
		 * There is a flag, which should indicate, whether a generator is
		 * enabled or not. But here we rely on the default value of -1.  */
		x = _GEN (voice, GEN_VELOCITY);
		if (x > 0) {
			voice->vel = x;
		}
		break;

	case GEN_MODENVTOPITCH:
		voice->modenvToPitch = _GEN (voice, GEN_MODENVTOPITCH);
		clip (voice->modenvToPitch, -12000.0, 12000.0);
		break;

	case GEN_MODENVTOFILTERFC:
		voice->modenvToFc = _GEN (voice, GEN_MODENVTOFILTERFC);

		/* Range: SF2.01 section 8.1.3 # 1
		 * Motivation for range checking:
		 * Filter is reported to make funny noises now and then
		 */
		clip (voice->modenvToFc, -12000.0, 12000.0);
		break;


		/* sample start and ends points
		 *
		 * Range checking is initiated via the
		 * voice->checkSampleSanity flag,
		 * because it is impossible to check here:
		 * During the voice setup, all modulators are processed, while
		 * the voice is inactive. Therefore, illegal settings may
		 * occur during the setup (for example: First move the loop
		 * end point ahead of the loop start point => invalid, then
		 * move the loop start point forward => valid again.
		 */
	case GEN_STARTADDROFS:				/* SF2.01 section 8.1.3 # 0 */
	case GEN_STARTADDRCOARSEOFS:	/* SF2.01 section 8.1.3 # 4 */
		if (voice->sampleP != NULL) {
			voice->start = (voice->sampleP->startIdx
											+ (int) _GEN (voice, GEN_STARTADDROFS)
											+ 32768 * (int) _GEN (voice, GEN_STARTADDRCOARSEOFS));
			voice->checkSampleSanityFlag = SAMPLESANITY_CHECK;
		}
		break;
	case GEN_ENDADDROFS:					/* SF2.01 section 8.1.3 # 1 */
	case GEN_ENDADDRCOARSEOFS:		/* SF2.01 section 8.1.3 # 12 */
		if (voice->sampleP != NULL) {
			voice->end = (voice->sampleP->endIdx + (int) _GEN (voice, GEN_ENDADDROFS)
										+ 32768 * (int) _GEN (voice, GEN_ENDADDRCOARSEOFS));
			voice->checkSampleSanityFlag = SAMPLESANITY_CHECK;
		}
		break;
	case GEN_STARTLOOPADDROFS:		/* SF2.01 section 8.1.3 # 2 */
	case GEN_STARTLOOPADDRCOARSEOFS:	/* SF2.01 section 8.1.3 # 45 */
		if (voice->sampleP != NULL) {
			voice->loopstart = (voice->sampleP->loopStartIdx
													+ (int) _GEN (voice, GEN_STARTLOOPADDROFS)
													+ 32768 * (int) _GEN (voice,
																								GEN_STARTLOOPADDRCOARSEOFS));
			voice->checkSampleSanityFlag = SAMPLESANITY_CHECK;
		}
		break;

	case GEN_ENDLOOPADDROFS:			/* SF2.01 section 8.1.3 # 3 */
	case GEN_ENDLOOPADDRCOARSEOFS:	/* SF2.01 section 8.1.3 # 50 */
		if (voice->sampleP != NULL) {
			voice->loopend = (voice->sampleP->loopEndIdx
												+ (int) _GEN (voice, GEN_ENDLOOPADDROFS)
												+ 32768 * (int) _GEN (voice,
																							GEN_ENDLOOPADDRCOARSEOFS));
			voice->checkSampleSanityFlag = SAMPLESANITY_CHECK;
		}
		break;

		/* Conversion functions differ in range limit */
#define NUM_BUFFERS_DELAY(_v)   (U32) (voice->outputRate * tc2secDelay(_v) / BUFSIZE)
#define NUM_BUFFERS_ATTACK(_v)  (U32) (voice->outputRate * tc2secAttack(_v) / BUFSIZE)
#define NUM_BUFFERS_RELEASE(_v) (U32) (voice->outputRate * tc2secRelease(_v) / BUFSIZE)

		/* volume envelope
		 *
		 * - delay and hold times are converted to absolute number of samples
		 * - sustain is converted to its absolute value
		 * - attack, decay and release are converted to their increment per sample
		 */
	case GEN_VOLENVDELAY:				/* SF2.01 section 8.1.3 # 33 */
		x = _GEN (voice, GEN_VOLENVDELAY);
		clip (x, -12000.0f, 5000.0f);
		count = NUM_BUFFERS_DELAY (x);
		voice->volenvData[VOICE_ENVDELAY].count = count;
		voice->volenvData[VOICE_ENVDELAY].coeff = 0.0f;
		voice->volenvData[VOICE_ENVDELAY].incr = 0.0f;
		voice->volenvData[VOICE_ENVDELAY].min = -1.0f;
		voice->volenvData[VOICE_ENVDELAY].max = 1.0f;
		break;

	case GEN_VOLENVATTACK:				/* SF2.01 section 8.1.3 # 34 */
		x = _GEN (voice, GEN_VOLENVATTACK);
		clip (x, -12000.0f, 8000.0f);
		count = 1 + NUM_BUFFERS_ATTACK (x);
		voice->volenvData[VOICE_ENVATTACK].count = count;
		voice->volenvData[VOICE_ENVATTACK].coeff = 1.0f;
		voice->volenvData[VOICE_ENVATTACK].incr =
			count ? 1.0f / count : 0.0f;
		voice->volenvData[VOICE_ENVATTACK].min = -1.0f;
		voice->volenvData[VOICE_ENVATTACK].max = 1.0f;
		break;

	case GEN_VOLENVHOLD:					/* SF2.01 section 8.1.3 # 35 */
	case GEN_KEYTOVOLENVHOLD:		/* SF2.01 section 8.1.3 # 39 */
		count = calculateHoldDecayBuffers (voice, GEN_VOLENVHOLD, GEN_KEYTOVOLENVHOLD, 0);	/* 0 means: hold */
		voice->volenvData[VOICE_ENVHOLD].count = count;
		voice->volenvData[VOICE_ENVHOLD].coeff = 1.0f;
		voice->volenvData[VOICE_ENVHOLD].incr = 0.0f;
		voice->volenvData[VOICE_ENVHOLD].min = -1.0f;
		voice->volenvData[VOICE_ENVHOLD].max = 2.0f;
		break;

	case GEN_VOLENVDECAY:				/* SF2.01 section 8.1.3 # 36 */
	case GEN_VOLENVSUSTAIN:			/* SF2.01 section 8.1.3 # 37 */
	case GEN_KEYTOVOLENVDECAY:		/* SF2.01 section 8.1.3 # 40 */
		y = 1.0f - 0.001f * _GEN (voice, GEN_VOLENVSUSTAIN);
		clip (y, 0.0f, 1.0f);
		count = calculateHoldDecayBuffers (voice, GEN_VOLENVDECAY, GEN_KEYTOVOLENVDECAY, 1);	/* 1 for decay */
		voice->volenvData[VOICE_ENVDECAY].count = count;
		voice->volenvData[VOICE_ENVDECAY].coeff = 1.0f;
		voice->volenvData[VOICE_ENVDECAY].incr =
			count ? -1.0f / count : 0.0f;
		voice->volenvData[VOICE_ENVDECAY].min = y;
		voice->volenvData[VOICE_ENVDECAY].max = 2.0f;
		break;

	case GEN_VOLENVRELEASE:			/* SF2.01 section 8.1.3 # 38 */
		x = _GEN (voice, GEN_VOLENVRELEASE);
		clip (x, MIN_VOLENVRELEASE, 8000.0f);
		count = 1 + NUM_BUFFERS_RELEASE (x);
		voice->volenvData[VOICE_ENVRELEASE].count = count;
		voice->volenvData[VOICE_ENVRELEASE].coeff = 1.0f;
		voice->volenvData[VOICE_ENVRELEASE].incr =
			count ? -1.0f / count : 0.0f;
		voice->volenvData[VOICE_ENVRELEASE].min = 0.0f;
		voice->volenvData[VOICE_ENVRELEASE].max = 1.0f;
		break;

		/* Modulation envelope */
	case GEN_MODENVDELAY:				/* SF2.01 section 8.1.3 # 25 */
		x = _GEN (voice, GEN_MODENVDELAY);
		clip (x, -12000.0f, 5000.0f);
		voice->modenvData[VOICE_ENVDELAY].count = NUM_BUFFERS_DELAY (x);
		voice->modenvData[VOICE_ENVDELAY].coeff = 0.0f;
		voice->modenvData[VOICE_ENVDELAY].incr = 0.0f;
		voice->modenvData[VOICE_ENVDELAY].min = -1.0f;
		voice->modenvData[VOICE_ENVDELAY].max = 1.0f;
		break;

	case GEN_MODENVATTACK:				/* SF2.01 section 8.1.3 # 26 */
		x = _GEN (voice, GEN_MODENVATTACK);
		clip (x, -12000.0f, 8000.0f);
		count = 1 + NUM_BUFFERS_ATTACK (x);
		voice->modenvData[VOICE_ENVATTACK].count = count;
		voice->modenvData[VOICE_ENVATTACK].coeff = 1.0f;
		voice->modenvData[VOICE_ENVATTACK].incr =
			count ? 1.0f / count : 0.0f;
		voice->modenvData[VOICE_ENVATTACK].min = -1.0f;
		voice->modenvData[VOICE_ENVATTACK].max = 1.0f;
		break;

	case GEN_MODENVHOLD:					/* SF2.01 section 8.1.3 # 27 */
	case GEN_KEYTOMODENVHOLD:		/* SF2.01 section 8.1.3 # 31 */
		count = calculateHoldDecayBuffers (voice, GEN_MODENVHOLD, GEN_KEYTOMODENVHOLD, 0);	/* 1 means: hold */
		voice->modenvData[VOICE_ENVHOLD].count = count;
		voice->modenvData[VOICE_ENVHOLD].coeff = 1.0f;
		voice->modenvData[VOICE_ENVHOLD].incr = 0.0f;
		voice->modenvData[VOICE_ENVHOLD].min = -1.0f;
		voice->modenvData[VOICE_ENVHOLD].max = 2.0f;
		break;

	case GEN_MODENVDECAY:				/* SF 2.01 section 8.1.3 # 28 */
	case GEN_MODENVSUSTAIN:			/* SF 2.01 section 8.1.3 # 29 */
	case GEN_KEYTOMODENVDECAY:		/* SF 2.01 section 8.1.3 # 32 */
		count = calculateHoldDecayBuffers (voice, GEN_MODENVDECAY, GEN_KEYTOMODENVDECAY, 1);	/* 1 for decay */
		y = 1.0f - 0.001f * _GEN (voice, GEN_MODENVSUSTAIN);
		clip (y, 0.0f, 1.0f);
		voice->modenvData[VOICE_ENVDECAY].count = count;
		voice->modenvData[VOICE_ENVDECAY].coeff = 1.0f;
		voice->modenvData[VOICE_ENVDECAY].incr =
			count ? -1.0f / count : 0.0f;
		voice->modenvData[VOICE_ENVDECAY].min = y;
		voice->modenvData[VOICE_ENVDECAY].max = 2.0f;
		break;

	case GEN_MODENVRELEASE:			/* SF 2.01 section 8.1.3 # 30 */
		x = _GEN (voice, GEN_MODENVRELEASE);
		clip (x, -12000.0f, 8000.0f);
		count = 1 + NUM_BUFFERS_RELEASE (x);
		voice->modenvData[VOICE_ENVRELEASE].count = count;
		voice->modenvData[VOICE_ENVRELEASE].coeff = 1.0f;
		voice->modenvData[VOICE_ENVRELEASE].incr =
			count ? -1.0f / count : 0.0;
		voice->modenvData[VOICE_ENVRELEASE].min = 0.0f;
		voice->modenvData[VOICE_ENVRELEASE].max = 2.0f;
		break;

	}															/* switch gen */
}

/**
 * voiceModulate
 *
 * In this implementation, I want to make sure that all controllers
 * are event based: the parameter values of the DSP algorithm should
 * only be updates when a controller event arrived and not at every
 * iteration of the audio cycle (which would probably be feasible if
 * the synth was made in silicon).
 *
 * The update is done in three steps:
 *
 * - first, we look for all the modulators that have the changed
 * controller as a source. This will yield a list of generators that
 * will be changed because of the controller event.
 *
 * - For every changed generator, calculate its new value. This is the
 * sum of its original value plus the values of al the attached
 * modulators.
 *
 * - For every changed generator, convert its value to the correct
 * unit of the corresponding DSP parameter
 *
 * @fn int voiceModulate(Voice* voice, int cc, int ctrl, int val)
 * @param voice the synthesis voice
 * @param cc flag to distinguish between continuous control and channel control (pitch bend, ...)
 * @param ctrl the control number
 * */
int voiceModulate (Voice * voice, int cc, int ctrl) {  
	int i, k;
	Modulator *mod;
	int gen;
	realT modval;


	for (i = 0; i < voice->nMods; i++) {

		mod = &voice->mod[i];

		/* step 1: find all the modulators that have the changed controller
		 * as input source. */
		if (modHasSource (mod, cc, ctrl)) {

			gen = mod->dest;
			modval = 0.0;

			/* step 2: for every changed modulator, calculate the modulation
			 * value of its associated generator */
			for (k = 0; k < voice->nMods; k++) {
				if (modHasDest (&voice->mod[k], gen)) {
					modval += modGetValue (&voice->mod[k], voice->channel, voice);
				}
			}

      voice->gen[gen].mod = modval;

			/* step 3: now that we have the new value of the generator,
			 * recalculate the parameter values that are derived from the
			 * generator */
			voiceUpdateParam (voice, gen);
		}
	}
	return OK;
}



/**
 * voiceModulateAll
 *
 * Update all the modulators. This function is called after a
 * ALL_CTRL_OFF MIDI message has been received (CC 121).
 *
 */
int voiceModulateAll (Voice * voice) {
	Modulator *mod;
	int i, k, gen;
	realT modval;

	/* Loop through all the modulators.

	   FIXME: we should loop through the set of generators instead of
	   the set of modulators. We risk to call 'voiceUpdateParam'
	   several times for the same generator if several modulators have
	   that generator as destination. It's not an error, just a wast of
	   energy (think polution, global warming, unhappy musicians,
	   ...) */

	for (i = 0; i < voice->nMods; i++) {

		mod = &voice->mod[i];
		gen = mod->dest;
		modval = 0.0;

		/* Accumulate the modulation values of all the modulators with
		 * destination generator 'gen' */
		for (k = 0; k < voice->nMods; k++) {
			if (modHasDest (&voice->mod[k], gen)) {
				modval += modGetValue (&voice->mod[k], voice->channel, voice);
			}
		}

		voice->gen[gen].mod = modval;

		/* Update the parameter values that are depend on the generator
		 * 'gen' */
		voiceUpdateParam (voice, gen);
	}

	return OK;
}

/*
 * voiceNoteoff
 */
int voiceNoteoff (Voice * voice) {
	U32 atTick;

	atTick = channelGetMinNoteLengthTicks (voice->channel);

	if (atTick > voice->ticks) {
		/* Delay noteoff */
		voice->noteoffTicks = atTick;
		return OK;
	}

// Moving this here because I don't feel like screaming at the circular dependency hell again.
#define channelSustained(_c)             ((_c)->cc[SUSTAIN_SWITCH] >= 64)
	if (voice->channel && channelSustained (voice->channel)) {
		voice->status = VOICE_SUSTAINED;
	} else {
		if (voice->volenvSection == VOICE_ENVATTACK) {
			/* A voice is turned off during the attack section of the volume
			 * envelope.  The attack section ramps up linearly with
			 * amplitude. The other sections use logarithmic scaling. Calculate new
			 * volenvVal to achieve equievalent amplitude during the release phase
			 * for seamless volume transition.
			 */
			if (voice->volenvVal > 0) {
				realT lfo = voice->modlfoVal * -voice->modlfoToVol;
				realT amp = voice->volenvVal * pow (10.0, lfo / -200);
				realT envValue =
					-((-200 * log (amp) / log (10.0) - lfo) / 960.0 - 1);
				clip (envValue, 0.0, 1.0);
				voice->volenvVal = envValue;
			}
		}
		voice->volenvSection = VOICE_ENVRELEASE;
		voice->volenvCount = 0;
		voice->modenvSection = VOICE_ENVRELEASE;
		voice->modenvCount = 0;
	}

	return OK;
}

/*
 * voiceKillExcl
 *
 * Percussion sounds can be mutually exclusive: for example, a 'closed
 * hihat' sound will terminate an 'open hihat' sound ringing at the
 * same time. This behaviour is modeled using 'exclusive classes',
 * turning on a voice with an exclusive class other than 0 will kill
 * all other voices having that exclusive class within the same preset
 * or channel.  voiceKillExcl gets called, when 'voice' is to
 * be killed for that reason.
 */
int voiceKillExcl (Voice * voice) {

	if (!_PLAYING (voice)) {
		return OK;
	}

	/* Turn off the exclusive class information for this voice,
	   so that it doesn't get killed twice
	 */
	voiceGenSet (voice, GEN_EXCLUSIVECLASS, 0);

	/* If the voice is not yet in release state, put it into release state */
	if (voice->volenvSection != VOICE_ENVRELEASE) {
		voice->volenvSection = VOICE_ENVRELEASE;
		voice->volenvCount = 0;
		voice->modenvSection = VOICE_ENVRELEASE;
		voice->modenvCount = 0;
	}

	/* Speed up the volume envelope */
	/* The value was found through listening tests with hi-hat samples. */
	voiceGenSet (voice, GEN_VOLENVRELEASE, -200);
	voiceUpdateParam (voice, GEN_VOLENVRELEASE);

	/* Speed up the modulation envelope */
	voiceGenSet (voice, GEN_MODENVRELEASE, -200);
	voiceUpdateParam (voice, GEN_MODENVRELEASE);

	return OK;
}

/*
 * voiceOff
 *
 * Purpose:
 * Turns off a voice, meaning that it is not processed
 * anymore by the DSP loop.
 */
int voiceOff (Voice * voice) {
	voice->chan = NO_CHANNEL;
	voice->volenvSection = VOICE_ENVFINISHED;
	voice->volenvCount = 0;
	voice->modenvSection = VOICE_ENVFINISHED;
	voice->modenvCount = 0;
	voice->status = VOICE_OFF;

	/* Decrement the reference count of the sample. */
	if (voice->sampleP) 
		voice->sampleP = NULL;

	return OK;
}

/*
 * voiceAddMod
 *
 * Adds a modulator to the voice.  "mode" indicates, what to do, if
 * an identical modulator exists already.
 *
 * mode == VOICE_ADD: Identical modulators on preset level are added
 * mode == VOICE_OVERWRITE: Identical modulators on instrument level are overwritten
 * mode == VOICE_DEFAULT: This is a default modulator, there can be no identical modulator.
 *                             Don't check.
 */
void voiceAddMod (Voice * voice, Modulator * mod, int mode) {
	int i;

	/*
	 * Some soundfonts come with a huge number of non-standard
	 * controllers, because they have been designed for one particular
	 * sound card.  Discard them, maybe print a warning.
	 */

	if (((mod->xformType1 & MOD_CC) == 0)
			&& ((mod->src1 != 0)			/* SF2.01 section 8.2.1: Constant value */
					&&(mod->src1 != 2)		/* Note-on velocity */
					&&(mod->src1 != 3)		/* Note-on key number */
					&&(mod->src1 != 10)		/* Poly pressure */
					&&(mod->src1 != 13)		/* Channel pressure */
					&&(mod->src1 != 14)		/* Pitch wheel */
					&&(mod->src1 != 16))) {	/* Pitch wheel sensitivity */
		return;
	}

	if (mode == VOICE_ADD) {

		/* if identical modulator exists, add them */
		for (i = 0; i < voice->nMods; i++) {
			if (modTestIdentity (&voice->mod[i], mod)) {
				voice->mod[i].amount += mod->amount;
				return;
			}
		}

	} else if (mode == VOICE_OVERWRITE) {

		/* if identical modulator exists, replace it (only the amount has to be changed) */
		for (i = 0; i < voice->nMods; i++) {
			if (modTestIdentity (&voice->mod[i], mod)) {
				voice->mod[i].amount = mod->amount;
				return;
			}
		}
	}

	/* Add a new modulator (No existing modulator to add / overwrite).
	   Also, default modulators (VOICE_DEFAULT) are added without
	   checking whether the same modulator already exists. */
	if (voice->nMods < NUM_MOD) 
		modClone(&voice->mod[voice->nMods++], mod);
}

U32 voiceGetId (Voice * voice) {
	return voice->id;
}

/*
 * voiceGetLowerBoundaryForAttenuation
 *
 * Purpose:
 *
 * A lower boundary for the attenuation (as in 'the minimum
 * attenuation of this voice, with volume pedals, modulators
 * etc. resulting in minimum attenuation, cannot fall below x cB) is
 * calculated.  This has to be called during voiceInit, after
 * all modulators have been run on the voice once.  Also,
 * voice->attenuation has to be initialized.
 */
realT
voiceGetLowerBoundaryForAttenuation (Voice * voice) {
	int i;
	Modulator *mod;
	realT possibleAttReductionCB = 0;
	realT lowerBound;

	for (i = 0; i < voice->nMods; i++) {
		mod = &voice->mod[i];

		/* Modulator has attenuation as target and can change over time? */
		if ((mod->dest == GEN_ATTENUATION)
				&& ((mod->xformType1 & MOD_CC) || (mod->xformType2 & MOD_CC))) {

			realT currentVal =
				modGetValue (mod, voice->channel, voice);
			realT v = fabs (mod->amount);

			if ((mod->src1 == MOD_PITCHWHEEL)
					|| (mod->xformType1 & MOD_BIPOLAR)
					|| (mod->xformType2 & MOD_BIPOLAR)
					|| (mod->amount < 0)) {
				/* Can this modulator produce a negative contribution? */
				v *= -1.0;
			} else {
				/* No negative value possible. But still, the minimum contribution is 0. */
				v = 0;
			}

			/* For example:
			 * - currentVal=100
			 * - minVal=-4000
			 * - possibleAttReductionCB += 4100
			 */
			if (currentVal > v) {
				possibleAttReductionCB += (currentVal - v);
			}
		}
	}

	lowerBound = voice->attenuation - possibleAttReductionCB;

	/* SF2.01 specs do not allow negative attenuation */
	if (lowerBound < 0) {
		lowerBound = 0;
	}
	return lowerBound;
}


/* Purpose:
 *
 * Make sure, that sample start / end point and loop points are in
 * proper order. When starting up, calculate the initial phase.
 */
void voiceCheckSampleSanity (Voice * voice) {
	int minIndexNonloop = (int) voice->sampleP->startIdx;
	int maxIndexNonloop = (int) voice->sampleP->endIdx;

	/* make sure we have enough samples surrounding the loop */
	int minIndexLoop = (int) voice->sampleP->startIdx + MIN_LOOP_PAD;
	int maxIndexLoop = (int) voice->sampleP->endIdx - MIN_LOOP_PAD + 1;	/* 'end' is last valid sample, loopend can be + 1 */

	if (!voice->checkSampleSanityFlag) {
		return;
	}


	/* Keep the start point within the sample data */
	if (voice->start < minIndexNonloop) {
		voice->start = minIndexNonloop;
	} else if (voice->start > maxIndexNonloop) {
		voice->start = maxIndexNonloop;
	}

	/* Keep the end point within the sample data */
	if (voice->end < minIndexNonloop) {
		voice->end = minIndexNonloop;
	} else if (voice->end > maxIndexNonloop) {
		voice->end = maxIndexNonloop;
	}

	/* Keep start and end point in the right order */
	if (voice->start > voice->end) {
		int temp = voice->start;
		voice->start = voice->end;
		voice->end = temp;
	}

	/* Zero length? */
	if (voice->start == voice->end) {
		voiceOff (voice);
		return;
	}

	if ((_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE)
			|| (_SAMPLEMODE (voice) == LOOP_DURING_RELEASE)) {
		/* Keep the loop start point within the sample data */
		if (voice->loopstart < minIndexLoop) {
			voice->loopstart = minIndexLoop;
		} else if (voice->loopstart > maxIndexLoop) {
			voice->loopstart = maxIndexLoop;
		}

		/* Keep the loop end point within the sample data */
		if (voice->loopend < minIndexLoop) {
			voice->loopend = minIndexLoop;
		} else if (voice->loopend > maxIndexLoop) {
			voice->loopend = maxIndexLoop;
		}

		/* Keep loop start and end point in the right order */
		if (voice->loopstart > voice->loopend) {
			int temp = voice->loopstart;
			voice->loopstart = voice->loopend;
			voice->loopend = temp;
		}

		/* Loop too short? Then don't loop. */
		if (voice->loopend < voice->loopstart + MIN_LOOP_SIZE) {
			voice->gen[GEN_SAMPLEMODE].val = UNLOOPED;
		}

		/* The loop points may have changed. Obtain a new estimate for the loop volume. */
		/* Is the voice loop within the sample loop? */
		if ((int) voice->loopstart >= (int) voice->sampleP->loopStartIdx
				&& (int) voice->loopend <= (int) voice->sampleP->loopEndIdx) {
			/* Is there a valid peak amplitude available for the loop? */
			if (voice->sampleP->amplitudeThatReachesNoiseFloorIsValid) {
				voice->amplitudeThatReachesNoiseFloorLoop =
					voice->sampleP->amplitudeThatReachesNoiseFloor /
					voice->synthGain;
			} else {
				/* Worst case */
				voice->amplitudeThatReachesNoiseFloorLoop = voice->amplitudeThatReachesNoiseFloorNonloop;
			};
		};

	}															/* if sample mode is looped */

	/* Run startup specific code (only once, when the voice is started) */
	if (voice->checkSampleSanityFlag & SAMPLESANITY_STARTUP) {
		if (maxIndexLoop - minIndexLoop < MIN_LOOP_SIZE) {
			if ((_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE)
					|| (_SAMPLEMODE (voice) == LOOP_DURING_RELEASE)) {
				voice->gen[GEN_SAMPLEMODE].val = UNLOOPED;
			}
		}

		/* Set the initial phase of the voice (using the result from the
		   start offset modulators). */
		phaseSetInt (voice->phase, voice->start);
	}															/* if startup */

	/* Is this voice run in loop mode, or does it run straight to the
	   end of the waveform data? */
	if (((_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE)
			 && (voice->volenvSection < VOICE_ENVRELEASE))
			|| (_SAMPLEMODE (voice) == LOOP_DURING_RELEASE)) {
		/* Yes, it will loop as soon as it reaches the loop point.  In
		 * this case we must prevent, that the playback pointer (phase)
		 * happens to end up beyond the 2nd loop point, because the
		 * point has moved.  The DSP algorithm is unable to cope with
		 * that situation.  So if the phase is beyond the 2nd loop
		 * point, set it to the start of the loop. No way to avoid some
		 * noise here.  Note: If the sample pointer ends up -before the
		 * first loop point- instead, then the DSP loop will just play
		 * the sample, enter the loop and proceed as expected => no
		 * actions required.
		 */
		int indexInSample = phaseIndex (voice->phase);
		if (indexInSample >= voice->loopend) {
			phaseSetInt (voice->phase, voice->loopstart);
		}
	}

	/* Sample sanity has been assured. Don't check again, until some
	   sample parameter is changed by modulation. */
	voice->checkSampleSanityFlag = 0;
}


int
voiceSetParam (Voice * voice, int gen,
											 realT nrpnValue, int abs) {
	voice->gen[gen].nrpn = nrpnValue;
	voice->gen[gen].flags = (abs) ? GEN_ABS_NRPN : GEN_SET;
	voiceUpdateParam (voice, gen);
	return OK;
}

int voiceSetGain (Voice * voice, realT gain) {
	/* avoid division by zero */
	if (gain < 0.0000001) {
		gain = 0.0000001;
	}

	voice->synthGain = gain;
	voice->ampLeft = pan (voice->pan, 1) * gain / 32768.0f;
	voice->ampRight = pan (voice->pan, 0) * gain / 32768.0f;
	voice->ampReverb = voice->reverbSend * gain / 32768.0f;
	voice->ampChorus = voice->chorusSend * gain / 32768.0f;

	return OK;
}
