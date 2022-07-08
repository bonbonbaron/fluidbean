#include "fluidbean.h"
#include "rev.h"

/***************************************************************
 *
 *                           REVERB
 */

/* Denormalising:
 *
 * According to music-dsp thread 'Denormalise', Pentium processors
 * have a hardware 'feature', that is of interest here, related to
 * numeric underflow.  We have a recursive filter. The output decays
 * exponentially, if the input stops.  So the numbers get smaller and
 * smaller... At some point, they reach 'denormal' level.  This will
 * lead to drastic spikes in the CPU load.  The effect was reproduced
 * with the reverb - sometimes the average load over 10 s doubles!!.
 *
 * The 'undenormalise' macro fixes the problem: As soon as the number
 * is close enough to denormal level, the macro forces the number to
 * 0.0f.  The original macro is:
 *
 * #define undenormalise(sample) if(((*(U32*)&sample)&0x7f800000)==0) sample=0.0f
 *
 * This will zero out a number when it reaches the denormal level.
 * Advantage: Maximum dynamic range Disadvantage: We'll have to check
 * every sample, expensive.  The alternative macro comes from a later
 * mail from Jon Watte. It will zap a number before it reaches
 * denormal level. Jon suggests to run it once per block instead of
 * every sample.
 */

#if defined(WITH_FLOATX)
#define zapAlmostZero(sample) (((*(U32*)&(sample))&0x7f800000) < 0x08000000)?0.0f:(sample)
#else
/* 1e-20 was chosen as an arbitrary (small) threshold. */
#define zapAlmostZero(sample) fabs(sample)<1e-10 ? 0 : sample;
#endif

/* Denormalising part II:
 *
 * Another method fixes the problem cheaper: Use a small DC-offset in
 * the filter calculations.  Now the signals converge not against 0,
 * but against the offset.  The constant offset is invisible from the
 * outside world (i.e. it does not appear at the output.  There is a
 * very small turn-on transient response, which should not cause
 * problems.
 */


//#define DC_OFFSET 0
#define DC_OFFSET 1e-8
//#define DC_OFFSET 0.001f
typedef struct _fluidAllpass allpass;
typedef struct _fluidComb comb;

struct _fluidAllpass {
	S16 feedback;
	S16 *buffer;
	int bufsize;
	int bufidx;
};

void allpassSetbuffer (allpass * allpass, S16 * buf,
															int size);
void allpassInit (allpass * allpass);
void allpassSetfeedback (allpass * allpass, S16 val);
S16 allpassGetfeedback (allpass * allpass);

void
allpassSetbuffer (allpass * allpass, S16 * buf,
												 int size) {
	allpass->bufidx = 0;
	allpass->buffer = buf;
	allpass->bufsize = size;
}

void allpassInit (allpass * allpass) {
	int i;
	int len = allpass->bufsize;
	S16 *buf = allpass->buffer;
	for (i = 0; i < len; i++) {
		buf[i] = DC_OFFSET;					/* this is not 100 % correct. */
	}
}

void allpassSetfeedback (allpass * allpass, S16 val) {
	allpass->feedback = val;
}

S16 allpassGetfeedback (allpass * allpass) {
	return allpass->feedback;
}

#define allpassProcess(_allpass, _input) \
{ \
  S16 output; \
  S16 bufout; \
  bufout = _allpass.buffer[_allpass.bufidx]; \
  output = bufout-_input; \
  _allpass.buffer[_allpass.bufidx] = _input + (bufout * _allpass.feedback); \
  if (++_allpass.bufidx >= _allpass.bufsize) { \
    _allpass.bufidx = 0; \
  } \
  _input = output; \
}

struct _fluidComb {
	S16 feedback;
	S16 filterstore;
	S16 damp1;
	S16 damp2;
	S16 *buffer;
	int bufsize;
	int bufidx;
};

void combSetbuffer (comb * comb, S16 * buf, int size);
void combInit (comb * comb);
void combSetdamp (comb * comb, S16 val);
S16 combGetdamp (comb * comb);
void combSetfeedback (comb * comb, S16 val);
S16 combGetfeedback (comb * comb);

void combSetbuffer (comb * comb, S16 * buf, int size) {
	comb->filterstore = 0;
	comb->bufidx = 0;
	comb->buffer = buf;
	comb->bufsize = size;
}

void combInit (comb * comb) {
	int i;
	S16 *buf = comb->buffer;
	int len = comb->bufsize;
	for (i = 0; i < len; i++) {
		buf[i] = DC_OFFSET;					/* This is not 100 % correct. */
	}
}

void combSetdamp (comb * comb, S16 val) {
	comb->damp1 = val;
	comb->damp2 = 1 - val;
}

S16 combGetdamp (comb * comb) {
	return comb->damp1;
}

void combSetfeedback (comb * comb, S16 val) {
	comb->feedback = val;
}

S16 combGetfeedback (comb * comb) {
	return comb->feedback;
}

#define combProcess(_comb, _input, _output) \
{ \
  S16 _tmp = _comb.buffer[_comb.bufidx]; \
  _comb.filterstore = (_tmp * _comb.damp2) + (_comb.filterstore * _comb.damp1); \
  _comb.buffer[_comb.bufidx] = _input + (_comb.filterstore * _comb.feedback); \
  if (++_comb.bufidx >= _comb.bufsize) { \
    _comb.bufidx = 0; \
  } \
  _output += _tmp; \
}

#define numcombs 8
#define numallpasses 4
#define	fixedgain 0.015f
#define scalewet 3.0f
#define scaledamp 1.0f
#define scaleroom 0.28f
#define offsetroom 0.7f
#define initialroom 0.5f
#define initialdamp 0.2f
#define initialwet 1
#define initialdry 0
#define initialwidth 1
#define stereospread 23

/*
 These values assume 44.1KHz sample rate
 they will probably be OK for 48KHz sample rate
 but would need scaling for 96KHz (or other) sample rates.
 The values were obtained by listening tests.
*/
#define combtuningL1 1116
#define combtuningR1 1116 + stereospread
#define combtuningL2 1188
#define combtuningR2 1188 + stereospread
#define combtuningL3 1277
#define combtuningR3 1277 + stereospread
#define combtuningL4 1356
#define combtuningR4 1356 + stereospread
#define combtuningL5 1422
#define combtuningR5 1422 + stereospread
#define combtuningL6 1491
#define combtuningR6 1491 + stereospread
#define combtuningL7 1557
#define combtuningR7 1557 + stereospread
#define combtuningL8 1617
#define combtuningR8 1617 + stereospread
#define allpasstuningL1 556
#define allpasstuningR1 556 + stereospread
#define allpasstuningL2 441
#define allpasstuningR2 441 + stereospread
#define allpasstuningL3 341
#define allpasstuningR3 341 + stereospread
#define allpasstuningL4 225
#define allpasstuningR4 225 + stereospread

struct _fluidRevmodelT {
	S16 roomsize;
	S16 damp;
	S16 wet, wet1, wet2;
	S16 width;
	S16 gain;
	/*
	   The following are all declared inline
	   to remove the need for dynamic allocation
	   with its subsequent error-checking messiness
	 */
	/* Comb filters */
	comb combL[numcombs];
	comb combR[numcombs];
	/* Allpass filters */
	allpass allpassL[numallpasses];
	allpass allpassR[numallpasses];
	/* Buffers for the combs */
	S16 bufcombL1[combtuningL1];
	S16 bufcombR1[combtuningR1];
	S16 bufcombL2[combtuningL2];
	S16 bufcombR2[combtuningR2];
	S16 bufcombL3[combtuningL3];
	S16 bufcombR3[combtuningR3];
	S16 bufcombL4[combtuningL4];
	S16 bufcombR4[combtuningR4];
	S16 bufcombL5[combtuningL5];
	S16 bufcombR5[combtuningR5];
	S16 bufcombL6[combtuningL6];
	S16 bufcombR6[combtuningR6];
	S16 bufcombL7[combtuningL7];
	S16 bufcombR7[combtuningR7];
	S16 bufcombL8[combtuningL8];
	S16 bufcombR8[combtuningR8];
	/* Buffers for the allpasses */
	S16 bufallpassL1[allpasstuningL1];
	S16 bufallpassR1[allpasstuningR1];
	S16 bufallpassL2[allpasstuningL2];
	S16 bufallpassR2[allpasstuningR2];
	S16 bufallpassL3[allpasstuningL3];
	S16 bufallpassR3[allpasstuningR3];
	S16 bufallpassL4[allpasstuningL4];
	S16 bufallpassR4[allpasstuningR4];
};

void revmodelUpdate (revmodelT * rev);
void revmodelInit (revmodelT * rev);

revmodelT *newRevmodel () {
	revmodelT *rev;
	rev = NEW (revmodelT);
	if (rev == NULL) {
		return NULL;
	}

	/* Tie the components to their buffers */
	combSetbuffer (&rev->combL[0], rev->bufcombL1, combtuningL1);
	combSetbuffer (&rev->combR[0], rev->bufcombR1, combtuningR1);
	combSetbuffer (&rev->combL[1], rev->bufcombL2, combtuningL2);
	combSetbuffer (&rev->combR[1], rev->bufcombR2, combtuningR2);
	combSetbuffer (&rev->combL[2], rev->bufcombL3, combtuningL3);
	combSetbuffer (&rev->combR[2], rev->bufcombR3, combtuningR3);
	combSetbuffer (&rev->combL[3], rev->bufcombL4, combtuningL4);
	combSetbuffer (&rev->combR[3], rev->bufcombR4, combtuningR4);
	combSetbuffer (&rev->combL[4], rev->bufcombL5, combtuningL5);
	combSetbuffer (&rev->combR[4], rev->bufcombR5, combtuningR5);
	combSetbuffer (&rev->combL[5], rev->bufcombL6, combtuningL6);
	combSetbuffer (&rev->combR[5], rev->bufcombR6, combtuningR6);
	combSetbuffer (&rev->combL[6], rev->bufcombL7, combtuningL7);
	combSetbuffer (&rev->combR[6], rev->bufcombR7, combtuningR7);
	combSetbuffer (&rev->combL[7], rev->bufcombL8, combtuningL8);
	combSetbuffer (&rev->combR[7], rev->bufcombR8, combtuningR8);
	allpassSetbuffer (&rev->allpassL[0], rev->bufallpassL1,
													 allpasstuningL1);
	allpassSetbuffer (&rev->allpassR[0], rev->bufallpassR1,
													 allpasstuningR1);
	allpassSetbuffer (&rev->allpassL[1], rev->bufallpassL2,
													 allpasstuningL2);
	allpassSetbuffer (&rev->allpassR[1], rev->bufallpassR2,
													 allpasstuningR2);
	allpassSetbuffer (&rev->allpassL[2], rev->bufallpassL3,
													 allpasstuningL3);
	allpassSetbuffer (&rev->allpassR[2], rev->bufallpassR3,
													 allpasstuningR3);
	allpassSetbuffer (&rev->allpassL[3], rev->bufallpassL4,
													 allpasstuningL4);
	allpassSetbuffer (&rev->allpassR[3], rev->bufallpassR4,
													 allpasstuningR4);
	/* Set default values */
	allpassSetfeedback (&rev->allpassL[0], 0.5f);
	allpassSetfeedback (&rev->allpassR[0], 0.5f);
	allpassSetfeedback (&rev->allpassL[1], 0.5f);
	allpassSetfeedback (&rev->allpassR[1], 0.5f);
	allpassSetfeedback (&rev->allpassL[2], 0.5f);
	allpassSetfeedback (&rev->allpassR[2], 0.5f);
	allpassSetfeedback (&rev->allpassL[3], 0.5f);
	allpassSetfeedback (&rev->allpassR[3], 0.5f);

	/* set values manually, since calling set functions causes update
	   and all values should be initialized for an update */
	rev->roomsize = initialroom * scaleroom + offsetroom;
	rev->damp = initialdamp * scaledamp;
	rev->wet = initialwet * scalewet;
	rev->width = initialwidth;
	rev->gain = fixedgain;

	/* now its okay to update reverb */
	revmodelUpdate (rev);

	/* Clear all buffers */
	revmodelInit (rev);
	return rev;
}

void deleteRevmodel (revmodelT * rev) {
	FREE (rev);
}

void revmodelInit (revmodelT * rev) {
	int i;
	for (i = 0; i < numcombs; i++) {
		combInit (&rev->combL[i]);
		combInit (&rev->combR[i]);
	}
	for (i = 0; i < numallpasses; i++) {
		allpassInit (&rev->allpassL[i]);
		allpassInit (&rev->allpassR[i]);
	}
}

void revmodelReset (revmodelT * rev) {
	revmodelInit (rev);
}

void
revmodelProcessreplace (revmodelT * rev, S16 * in,
															 S16 * leftOut,
															 S16 * rightOut) {
	int i, k = 0;
	S16 outL, outR, input;

	for (k = 0; k < BUFSIZE; k++) {

		outL = outR = 0;

		/* The original Freeverb code expects a stereo signal and 'input'
		 * is set to the sum of the left and right input sample. Since
		 * this code works on a mono signal, 'input' is set to twice the
		 * input sample. */
		input = (2 * in[k] + DC_OFFSET) * rev->gain;

		/* Accumulate comb filters in parallel */
		for (i = 0; i < numcombs; i++) {
			combProcess (rev->combL[i], input, outL);
			combProcess (rev->combR[i], input, outR);
		}
		/* Feed through allpasses in series */
		for (i = 0; i < numallpasses; i++) {
			allpassProcess (rev->allpassL[i], outL);
			allpassProcess (rev->allpassR[i], outR);
		}

		/* Remove the DC offset */
		outL -= DC_OFFSET;
		outR -= DC_OFFSET;

		/* Calculate output REPLACING anything already there */
		leftOut[k] = outL * rev->wet1 + outR * rev->wet2;
		rightOut[k] = outR * rev->wet1 + outL * rev->wet2;
	}
}

void
revmodelProcessmix (revmodelT * rev, S16 * in,
													 S16 * leftOut, S16 * rightOut) 
{
	int i, k = 0;
	S16 outL, outR, input;

	for (k = 0; k < BUFSIZE; k++) {

		outL = outR = 0;

		/* The original Freeverb code expects a stereo signal and 'input'
		 * is set to the sum of the left and right input sample. Since
		 * this code works on a mono signal, 'input' is set to twice the
		 * input sample. */
		input = (2 * in[k] + DC_OFFSET) * rev->gain;

		/* Accumulate comb filters in parallel */
		for (i = 0; i < numcombs; i++) {
			combProcess (rev->combL[i], input, outL);
			combProcess (rev->combR[i], input, outR);
		}
		/* Feed through allpasses in series */
		for (i = 0; i < numallpasses; i++) {
			allpassProcess (rev->allpassL[i], outL);
			allpassProcess (rev->allpassR[i], outR);
		}

		/* Remove the DC offset */
		outL -= DC_OFFSET;
		outR -= DC_OFFSET;

		/* Calculate output MIXING with anything already there */
		leftOut[k] += outL * rev->wet1 + outR * rev->wet2;
		rightOut[k] += outR * rev->wet1 + outL * rev->wet2;
	}
}

void revmodelUpdate (revmodelT * rev) {
	/* Recalculate internal values after parameter change */
	int i;

	rev->wet1 = rev->wet * (rev->width / 2 + 0.5f);
	rev->wet2 = rev->wet * ((1 - rev->width) / 2);

	for (i = 0; i < numcombs; i++) {
		combSetfeedback (&rev->combL[i], rev->roomsize);
		combSetfeedback (&rev->combR[i], rev->roomsize);
	}

	for (i = 0; i < numcombs; i++) {
		combSetdamp (&rev->combL[i], rev->damp);
		combSetdamp (&rev->combR[i], rev->damp);
	}
}

/*
 The following get/set functions are not inlined, because
 speed is never an issue when calling them, and also
 because as you develop the reverb model, you may
 wish to take dynamic action when they are called.
*/
void revmodelSetroomsize (revmodelT * rev, S16 value) {
/*   clip(value, 0.0f, 1.0f); */
	rev->roomsize = (value * scaleroom) + offsetroom;
	revmodelUpdate (rev);
}

S16 revmodelGetroomsize (revmodelT * rev) {
	return (rev->roomsize - offsetroom) / scaleroom;
}

void revmodelSetdamp (revmodelT * rev, S16 value) {
/*   clip(value, 0.0f, 1.0f); */
	rev->damp = value * scaledamp;
	revmodelUpdate (rev);
}

S16 revmodelGetdamp (revmodelT * rev) {
	return rev->damp / scaledamp;
}

void revmodelSetlevel (revmodelT * rev, S16 value) {
	clip (value, 0.0f, 1.0f);
	rev->wet = value * scalewet;
	revmodelUpdate (rev);
}

S16 revmodelGetlevel (revmodelT * rev) {
	return rev->wet / scalewet;
}

void revmodelSetwidth (revmodelT * rev, S16 value) {
/*   clip(value, 0.0f, 1.0f); */
	rev->width = value;
	revmodelUpdate (rev);
}

S16 revmodelGetwidth (revmodelT * rev) {
	return rev->width;
}
