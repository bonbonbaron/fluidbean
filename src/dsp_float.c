#include "fluidbean.h"
#include "synth.h"
#include "voice.h"
#include <math.h>

/* Interpolation (find a value between two samples of the original waveform) */
/* Linear interpolation table (2 coefficients centered on 1st) */
static realT interpCoeffLinear[INTERP_MAX][2];
/* 4th order (cubic) interpolation table (4 coefficients centered on 2nd) */
static realT interpCoeff[INTERP_MAX][4];
/* 7th order interpolation (7 coefficients centered on 3rd) */
static realT sincTable7[INTERP_MAX][7];


#define SINC_INTERP_ORDER 7			/* 7th order constant */


/* Initializes interpolation tables */
void dspFloatConfig (void) {
	int i, i2;
	double x, v;
	double iShifted;

	/* Initialize the coefficients for the interpolation. The math comes
	 * from a mail, posted by Olli Niemitalo to the music-dsp mailing
	 * list (I found it in the music-dsp archives
	 * http://www.smartelectronix.com/musicdsp/).  */

	for (i = 0; i < INTERP_MAX; i++) {
		x = (double) i / (double) INTERP_MAX;

		interpCoeff[i][0] = (realT) (x * (-0.5 + x * (1 - 0.5 * x)));
		interpCoeff[i][1] = (realT) (1.0 + x * x * (1.5 * x - 2.5));
		interpCoeff[i][2] = (realT) (x * (0.5 + x * (2.0 - 1.5 * x)));
		interpCoeff[i][3] = (realT) (0.5 * x * x * (x - 1.0));

		interpCoeffLinear[i][0] = (realT) (1.0 - x);
		interpCoeffLinear[i][1] = (realT) x;
	}

	/* i: Offset in terms of whole samples */
	for (i = 0; i < SINC_INTERP_ORDER; i++) {	/* i2: Offset in terms of fractional samples ('subsamples') */
		for (i2 = 0; i2 < INTERP_MAX; i2++) {
			/* center on middle of table */
			iShifted = (double) i - ((double) SINC_INTERP_ORDER / 2.0)
				+ (double) i2 / (double) INTERP_MAX;

			/* sinc(0) cannot be calculated straightforward (limit needed for 0/0) */
			if (fabs (iShifted) > 0.000001) {
				v = (realT) sin (iShifted * M_PI) / (M_PI * iShifted);
				/* Hamming window */
				v *=
					(realT) 0.5 *(1.0 +
															 cos (2.0 * M_PI * iShifted /
																		(realT) SINC_INTERP_ORDER));
			} else
				v = 1.0;

			sincTable7[INTERP_MAX - i2 - 1][i] = v;
		}
	}

#if 0
	for (i = 0; i < INTERP_MAX; i++) {
		printf ("%d %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f %0.3f\n",
						i, sincTable7[0][i], sincTable7[1][i], sincTable7[2][i],
						sincTable7[3][i], sincTable7[4][i], sincTable7[5][i],
						sincTable7[6][i]);
	}
#endif
}


/* No interpolation. Just take the sample, which is closest to
  * the playback pointer.  Questionable quality, but very
  * efficient. */
int dspFloatInterpolateNone (Voice * voice) {
	phaseT dspPhase = voice->phase;
	phaseT dspPhaseIncr;
	short int *dspData = voice->sampleP->pcmDataP;
	realT *dspBuf = voice->dspBuf;
	realT dspAmp = voice->amp;
	realT dspAmpIncr = voice->ampIncr;
	U32 dspI = 0;
	U32 dspPhaseIndex;
	U32 endIndex;
	int looping;

	/* Convert playback "speed" floating point value to phase index/fract */
	phaseSetFloat (dspPhaseIncr, voice->phaseIncr);

	/* voice is currently looping? */
	looping = _SAMPLEMODE (voice) == LOOP_DURING_RELEASE
		|| (_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE
				&& voice->volenvSection < VOICE_ENVRELEASE);

	endIndex = looping ? voice->loopend - 1 : voice->end;

	while (1) {
		dspPhaseIndex = phaseIndexRound (dspPhase);	/* round to nearest point */

		/* interpolate sequence of sample points */
		for (; dspI < BUFSIZE && dspPhaseIndex <= endIndex; dspI++) {
			dspBuf[dspI] = dspAmp * dspData[dspPhaseIndex];

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndexRound (dspPhase);	/* round to nearest point */
			dspAmp += dspAmpIncr;
		}

		/* break out if not looping (buffer may not be full) */
		if (!looping)
			break;

		/* go back to loop start */
		if (dspPhaseIndex > endIndex) {
			phaseSubInt (dspPhase, voice->loopend - voice->loopstart);
			voice->hasLooped = 1;
		}

		/* break out if filled buffer */
		if (dspI >= BUFSIZE)
			break;
	}

	voice->phase = dspPhase;
	voice->amp = dspAmp;

	return (dspI);
}

/* Straight line interpolation.
 * Returns number of samples processed (usually BUFSIZE but could be
 * smaller if end of sample occurs).
 */
int dspFloatInterpolateLinear (Voice * voice) {
	phaseT dspPhase = voice->phase;
	phaseT dspPhaseIncr;
	short int *dspData = voice->sampleP->pcmDataP;
	realT *dspBuf = voice->dspBuf;
	realT dspAmp = voice->amp;
	realT dspAmpIncr = voice->ampIncr;
	U32 dspI = 0;
	U32 dspPhaseIndex;
	U32 endIndex;
	short int point;
	realT *coeffs;
	int looping;

	/* Convert playback "speed" floating point value to phase index/fract */
	phaseSetFloat (dspPhaseIncr, voice->phaseIncr);

	/* voice is currently looping? */
	looping = _SAMPLEMODE (voice) == LOOP_DURING_RELEASE
		|| (_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE
				&& voice->volenvSection < VOICE_ENVRELEASE);

	/* last index before 2nd interpolation point must be specially handled */
	endIndex = (looping ? voice->loopend - 1 : voice->end) - 1;

	/* 2nd interpolation point to use at end of loop or sample */
	if (looping)
		point = dspData[voice->loopstart];	/* loop start */
	else
		point = dspData[voice->end];	/* duplicate end for samples no longer looping */

	while (1) {
		dspPhaseIndex = phaseIndex (dspPhase);

		/* interpolate the sequence of sample points */
		for (; dspI < BUFSIZE && dspPhaseIndex <= endIndex; dspI++) {
			coeffs = interpCoeffLinear[phaseFractToTablerow (dspPhase)];
			dspBuf[dspI] = dspAmp * (coeffs[0] * dspData[dspPhaseIndex]
																	+ coeffs[1] * dspData[dspPhaseIndex +
																												 1]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		/* break out if buffer filled */
		if (dspI >= BUFSIZE)
			break;

		endIndex++;								/* we're now interpolating the last point */

		/* interpolate within last point */
		for (; dspPhaseIndex <= endIndex && dspI < BUFSIZE; dspI++) {
			coeffs = interpCoeffLinear[phaseFractToTablerow (dspPhase)];
			dspBuf[dspI] = dspAmp * (coeffs[0] * dspData[dspPhaseIndex]
																	+ coeffs[1] * point);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;	/* increment amplitude */
		}

		if (!looping)
			break;										/* break out if not looping (end of sample) */

		/* go back to loop start (if past */
		if (dspPhaseIndex > endIndex) {
			phaseSubInt (dspPhase, voice->loopend - voice->loopstart);
			voice->hasLooped = 1;
		}

		/* break out if filled buffer */
		if (dspI >= BUFSIZE)
			break;

		endIndex--;								/* set end back to second to last sample point */
	}

	voice->phase = dspPhase;
	voice->amp = dspAmp;

	return (dspI);
}

/* 4th order (cubic) interpolation.
 * Returns number of samples processed (usually BUFSIZE but could be
 * smaller if end of sample occurs).
 */
int dspFloatInterpolate_4thOrder (Voice * voice) {
	phaseT dspPhase = voice->phase;
	phaseT dspPhaseIncr;
	short int *dspData = voice->sampleP->pcmDataP;
	realT *dspBuf = voice->dspBuf;
	realT dspAmp = voice->amp;
	realT dspAmpIncr = voice->ampIncr;
	U32 dspI = 0;
	U32 dspPhaseIndex;
	U32 startIndex, endIndex;
	short int startPoint, endPoint1, endPoint2;
	realT *coeffs;
	int looping;

	/* Convert playback "speed" floating point value to phase index/fract */
	phaseSetFloat (dspPhaseIncr, voice->phaseIncr);

	/* voice is currently looping? */
	looping = _SAMPLEMODE (voice) == LOOP_DURING_RELEASE
		|| (_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE
				&& voice->volenvSection < VOICE_ENVRELEASE);

	/* last index before 4th interpolation point must be specially handled */
	endIndex = (looping ? voice->loopend - 1 : voice->end) - 2;

	if (voice->hasLooped) {			/* set startIndex and start point if looped or not */
		startIndex = voice->loopstart;
		startPoint = dspData[voice->loopend - 1];	/* last point in loop (wrap around) */
	} else {
		startIndex = voice->start;
		startPoint = dspData[voice->start];	/* just duplicate the point */
	}

	/* get points off the end (loop start if looping, duplicate point if end) */
	if (looping) {
		endPoint1 = dspData[voice->loopstart];
		endPoint2 = dspData[voice->loopstart + 1];
	} else {
		endPoint1 = dspData[voice->end];
		endPoint2 = endPoint1;
	}

	while (1) {
		dspPhaseIndex = phaseIndex (dspPhase);

		/* interpolate first sample point (start or loop start) if needed */
		for (; dspPhaseIndex == startIndex && dspI < BUFSIZE; dspI++) {
			coeffs = interpCoeff[phaseFractToTablerow (dspPhase)];
			dspBuf[dspI] = dspAmp * (coeffs[0] * startPoint
																	+ coeffs[1] * dspData[dspPhaseIndex]
																	+ coeffs[2] * dspData[dspPhaseIndex + 1]
																	+ coeffs[3] * dspData[dspPhaseIndex +
																												 2]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		/* interpolate the sequence of sample points */
		for (; dspI < BUFSIZE && dspPhaseIndex <= endIndex; dspI++) {
			coeffs = interpCoeff[phaseFractToTablerow (dspPhase)];
			dspBuf[dspI] = dspAmp * (coeffs[0] * dspData[dspPhaseIndex - 1]
																	+ coeffs[1] * dspData[dspPhaseIndex]
																	+ coeffs[2] * dspData[dspPhaseIndex + 1]
																	+ coeffs[3] * dspData[dspPhaseIndex +
																												 2]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		/* break out if buffer filled */
		if (dspI >= BUFSIZE)
			break;

		endIndex++;								/* we're now interpolating the 2nd to last point */

		/* interpolate within 2nd to last point */
		for (; dspPhaseIndex <= endIndex && dspI < BUFSIZE; dspI++) {
			coeffs = interpCoeff[phaseFractToTablerow (dspPhase)];
			dspBuf[dspI] = dspAmp * (coeffs[0] * dspData[dspPhaseIndex - 1]
																	+ coeffs[1] * dspData[dspPhaseIndex]
																	+ coeffs[2] * dspData[dspPhaseIndex + 1]
																	+ coeffs[3] * endPoint1);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		endIndex++;								/* we're now interpolating the last point */

		/* interpolate within the last point */
		for (; dspPhaseIndex <= endIndex && dspI < BUFSIZE; dspI++) {
			coeffs = interpCoeff[phaseFractToTablerow (dspPhase)];
			dspBuf[dspI] = dspAmp * (coeffs[0] * dspData[dspPhaseIndex - 1]
																	+ coeffs[1] * dspData[dspPhaseIndex]
																	+ coeffs[2] * endPoint1
																	+ coeffs[3] * endPoint2);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		if (!looping)
			break;										/* break out if not looping (end of sample) */

		/* go back to loop start */
		if (dspPhaseIndex > endIndex) {
			phaseSubInt (dspPhase, voice->loopend - voice->loopstart);

			if (!voice->hasLooped) {
				voice->hasLooped = 1;
				startIndex = voice->loopstart;
				startPoint = dspData[voice->loopend - 1];
			}
		}

		/* break out if filled buffer */
		if (dspI >= BUFSIZE)
			break;

		endIndex -= 2;							/* set end back to third to last sample point */
	}

	voice->phase = dspPhase;
	voice->amp = dspAmp;

	return (dspI);
}

/* 7th order interpolation.
 * Returns number of samples processed (usually BUFSIZE but could be
 * smaller if end of sample occurs).
 */
int dspFloatInterpolate_7thOrder (Voice * voice) {
	phaseT dspPhase = voice->phase;
	phaseT dspPhaseIncr;
	short int *dspData = voice->sampleP->pcmDataP;
	realT *dspBuf = voice->dspBuf;
	realT dspAmp = voice->amp;
	realT dspAmpIncr = voice->ampIncr;
	U32 dspI = 0;
	U32 dspPhaseIndex;
	U32 startIndex, endIndex;
	short int startPoints[3];
	short int endPoints[3];
	realT *coeffs;
	int looping;

	/* Convert playback "speed" floating point value to phase index/fract */
	phaseSetFloat (dspPhaseIncr, voice->phaseIncr);

	/* add 1/2 sample to dspPhase since 7th order interpolation is centered on
	 * the 4th sample point */
	phaseIncr (dspPhase, (phaseT) 0x80000000);

	/* voice is currently looping? */
	looping = _SAMPLEMODE (voice) == LOOP_DURING_RELEASE
		|| (_SAMPLEMODE (voice) == LOOP_UNTIL_RELEASE
				&& voice->volenvSection < VOICE_ENVRELEASE);

	/* last index before 7th interpolation point must be specially handled */
	endIndex = (looping ? voice->loopend - 1 : voice->end) - 3;

	if (voice->hasLooped) {			/* set startIndex and start point if looped or not */
		startIndex = voice->loopstart;
		startPoints[0] = dspData[voice->loopend - 1];
		startPoints[1] = dspData[voice->loopend - 2];
		startPoints[2] = dspData[voice->loopend - 3];
	} else {
		startIndex = voice->start;
		startPoints[0] = dspData[voice->start];	/* just duplicate the start point */
		startPoints[1] = startPoints[0];
		startPoints[2] = startPoints[0];
	}

	/* get the 3 points off the end (loop start if looping, duplicate point if end) */
	if (looping) {
		endPoints[0] = dspData[voice->loopstart];
		endPoints[1] = dspData[voice->loopstart + 1];
		endPoints[2] = dspData[voice->loopstart + 2];
	} else {
		endPoints[0] = dspData[voice->end];
		endPoints[1] = endPoints[0];
		endPoints[2] = endPoints[0];
	}

	while (1) {
		dspPhaseIndex = phaseIndex (dspPhase);

		/* interpolate first sample point (start or loop start) if needed */
		for (; dspPhaseIndex == startIndex && dspI < BUFSIZE; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp * (coeffs[0] * (realT) startPoints[2]
																	+ coeffs[1] * (realT) startPoints[1]
																	+ coeffs[2] * (realT) startPoints[0]
																	+
																	coeffs[3] *
																	(realT) dspData[dspPhaseIndex]
																	+
																	coeffs[4] *
																	(realT) dspData[dspPhaseIndex + 1]
																	+
																	coeffs[5] *
																	(realT) dspData[dspPhaseIndex + 2]
																	+
																	coeffs[6] *
																	(realT) dspData[dspPhaseIndex +
																													3]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		startIndex++;

		/* interpolate 2nd to first sample point (start or loop start) if needed */
		for (; dspPhaseIndex == startIndex && dspI < BUFSIZE; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp * (coeffs[0] * (realT) startPoints[1]
																	+ coeffs[1] * (realT) startPoints[0]
																	+
																	coeffs[2] *
																	(realT) dspData[dspPhaseIndex - 1]
																	+
																	coeffs[3] *
																	(realT) dspData[dspPhaseIndex]
																	+
																	coeffs[4] *
																	(realT) dspData[dspPhaseIndex + 1]
																	+
																	coeffs[5] *
																	(realT) dspData[dspPhaseIndex + 2]
																	+
																	coeffs[6] *
																	(realT) dspData[dspPhaseIndex +
																													3]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		startIndex++;

		/* interpolate 3rd to first sample point (start or loop start) if needed */
		for (; dspPhaseIndex == startIndex && dspI < BUFSIZE; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp * (coeffs[0] * (realT) startPoints[0]
																	+
																	coeffs[1] *
																	(realT) dspData[dspPhaseIndex - 2]
																	+
																	coeffs[2] *
																	(realT) dspData[dspPhaseIndex - 1]
																	+
																	coeffs[3] *
																	(realT) dspData[dspPhaseIndex]
																	+
																	coeffs[4] *
																	(realT) dspData[dspPhaseIndex + 1]
																	+
																	coeffs[5] *
																	(realT) dspData[dspPhaseIndex + 2]
																	+
																	coeffs[6] *
																	(realT) dspData[dspPhaseIndex +
																													3]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		startIndex -= 2;						/* set back to original start index */


		/* interpolate the sequence of sample points */
		for (; dspI < BUFSIZE && dspPhaseIndex <= endIndex; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp
				* (coeffs[0] * (realT) dspData[dspPhaseIndex - 3]
					 + coeffs[1] * (realT) dspData[dspPhaseIndex - 2]
					 + coeffs[2] * (realT) dspData[dspPhaseIndex - 1]
					 + coeffs[3] * (realT) dspData[dspPhaseIndex]
					 + coeffs[4] * (realT) dspData[dspPhaseIndex + 1]
					 + coeffs[5] * (realT) dspData[dspPhaseIndex + 2]
					 + coeffs[6] * (realT) dspData[dspPhaseIndex + 3]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		/* break out if buffer filled */
		if (dspI >= BUFSIZE)
			break;

		endIndex++;								/* we're now interpolating the 3rd to last point */

		/* interpolate within 3rd to last point */
		for (; dspPhaseIndex <= endIndex && dspI < BUFSIZE; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp
				* (coeffs[0] * (realT) dspData[dspPhaseIndex - 3]
					 + coeffs[1] * (realT) dspData[dspPhaseIndex - 2]
					 + coeffs[2] * (realT) dspData[dspPhaseIndex - 1]
					 + coeffs[3] * (realT) dspData[dspPhaseIndex]
					 + coeffs[4] * (realT) dspData[dspPhaseIndex + 1]
					 + coeffs[5] * (realT) dspData[dspPhaseIndex + 2]
					 + coeffs[6] * (realT) endPoints[0]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		endIndex++;								/* we're now interpolating the 2nd to last point */

		/* interpolate within 2nd to last point */
		for (; dspPhaseIndex <= endIndex && dspI < BUFSIZE; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp
				* (coeffs[0] * (realT) dspData[dspPhaseIndex - 3]
					 + coeffs[1] * (realT) dspData[dspPhaseIndex - 2]
					 + coeffs[2] * (realT) dspData[dspPhaseIndex - 1]
					 + coeffs[3] * (realT) dspData[dspPhaseIndex]
					 + coeffs[4] * (realT) dspData[dspPhaseIndex + 1]
					 + coeffs[5] * (realT) endPoints[0]
					 + coeffs[6] * (realT) endPoints[1]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		endIndex++;								/* we're now interpolating the last point */

		/* interpolate within last point */
		for (; dspPhaseIndex <= endIndex && dspI < BUFSIZE; dspI++) {
			coeffs = sincTable7[phaseFractToTablerow (dspPhase)];

			dspBuf[dspI] = dspAmp
				* (coeffs[0] * (realT) dspData[dspPhaseIndex - 3]
					 + coeffs[1] * (realT) dspData[dspPhaseIndex - 2]
					 + coeffs[2] * (realT) dspData[dspPhaseIndex - 1]
					 + coeffs[3] * (realT) dspData[dspPhaseIndex]
					 + coeffs[4] * (realT) endPoints[0]
					 + coeffs[5] * (realT) endPoints[1]
					 + coeffs[6] * (realT) endPoints[2]);

			/* increment phase and amplitude */
			phaseIncr (dspPhase, dspPhaseIncr);
			dspPhaseIndex = phaseIndex (dspPhase);
			dspAmp += dspAmpIncr;
		}

		if (!looping)
			break;										/* break out if not looping (end of sample) */

		/* go back to loop start */
		if (dspPhaseIndex > endIndex) {
			phaseSubInt (dspPhase, voice->loopend - voice->loopstart);

			if (!voice->hasLooped) {
				voice->hasLooped = 1;
				startIndex = voice->loopstart;
				startPoints[0] = dspData[voice->loopend - 1];
				startPoints[1] = dspData[voice->loopend - 2];
				startPoints[2] = dspData[voice->loopend - 3];
			}
		}

		/* break out if filled buffer */
		if (dspI >= BUFSIZE)
			break;

		endIndex -= 3;							/* set end back to 4th to last sample point */
	}

	/* sub 1/2 sample from dspPhase since 7th order interpolation is centered on
	 * the 4th sample point (correct back to real value) */
	phaseDecr (dspPhase, (phaseT) 0x80000000);

	voice->phase = dspPhase;
	voice->amp = dspAmp;

	return (dspI);
}
