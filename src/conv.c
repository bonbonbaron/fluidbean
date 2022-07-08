#include "conv.h"

/* conversion tables */
realT ct2hzTab[CENTS_HZ_SIZE];
realT cb2ampTab[CB_AMP_SIZE];
realT atten2ampTab[ATTEN_AMP_SIZE];
realT posbpTab[128];
realT concaveTab[128];
realT convexTab[128];
realT panTab[PAN_SIZE];

/*
 * void synthInit
 *
 * Does all the initialization for this module.
 */
void conversionConfig (void) {
	int i;
	double x;

	for (i = 0; i < CENTS_HZ_SIZE; i++) {
		ct2hzTab[i] = (realT) pow (2.0, (double) i / 1200.0);
	}

	/* centibels to amplitude conversion
	 * Note: SF2.01 section 8.1.3: Initial attenuation range is
	 * between 0 and 144 dB. Therefore a negative attenuation is
	 * not allowed.
	 */
	for (i = 0; i < CB_AMP_SIZE; i++) {
		cb2ampTab[i] = (realT) pow (10.0, (double) i / -200.0);
	}

	/* NOTE: EMU8k and EMU10k devices don't conform to the SoundFont
	 * specification in regards to volume attenuation.  The below calculation
	 * is an approx. equation for generating a table equivelant to the
	 * cbToAmpTable[] in tables.c of the TiMidity++ source, which I'm told
	 * was generated from device testing.  By the spec this should be centibels.
	 */
	for (i = 0; i < ATTEN_AMP_SIZE; i++) {
		atten2ampTab[i] =
			(realT) pow (10.0, (double) i / ATTEN_POWER_FACTOR);
	}

	/* initialize the conversion tables (see mod.c
	   modGetValue cases 4 and 8) */

	/* concave unipolar positive transform curve */
	concaveTab[0] = 0.0;
	concaveTab[127] = 1.0;

	/* convex unipolar positive transform curve */
	convexTab[0] = 0;
	convexTab[127] = 1.0;
	x = log10 (128.0 / 127.0);

	/* There seems to be an error in the specs. The equations are
	   implemented according to the pictures on SF2.01 page 73. */

	for (i = 1; i < 127; i++) {
		x = -20.0 / 96.0 * log ((i * i) / (127.0 * 127.0)) / log (10.0);
		convexTab[i] = (realT) (1.0 - x);
		concaveTab[127 - i] = (realT) x;
	}

	/* initialize the pan conversion table */
	x = PI / 2.0 / (PAN_SIZE - 1.0);
	for (i = 0; i < PAN_SIZE; i++) {
		panTab[i] = (realT) sin (i * x);
	}
}

/*
 * ct2hz
 */
realT ct2hzReal (realT cents) {
	if (cents < 0)
		return (realT) 1.0;
	else if (cents < 900) {
		return (realT) 6.875 *ct2hzTab[(int) (cents + 300)];
	} else if (cents < 2100) {
		return (realT) 13.75 *ct2hzTab[(int) (cents - 900)];
	} else if (cents < 3300) {
		return (realT) 27.5 *ct2hzTab[(int) (cents - 2100)];
	} else if (cents < 4500) {
		return (realT) 55.0 *ct2hzTab[(int) (cents - 3300)];
	} else if (cents < 5700) {
		return (realT) 110.0 *ct2hzTab[(int) (cents - 4500)];
	} else if (cents < 6900) {
		return (realT) 220.0 *ct2hzTab[(int) (cents - 5700)];
	} else if (cents < 8100) {
		return (realT) 440.0 *ct2hzTab[(int) (cents - 6900)];
	} else if (cents < 9300) {
		return (realT) 880.0 *ct2hzTab[(int) (cents - 8100)];
	} else if (cents < 10500) {
		return (realT) 1760.0 *ct2hzTab[(int) (cents - 9300)];
	} else if (cents < 11700) {
		return (realT) 3520.0 *ct2hzTab[(int) (cents - 10500)];
	} else if (cents < 12900) {
		return (realT) 7040.0 *ct2hzTab[(int) (cents - 11700)];
	} else if (cents < 14100) {
		return (realT) 14080.0 *ct2hzTab[(int) (cents - 12900)];
	} else {
		return (realT) 1.0;	/* some loony trying to make you deaf */
	}
}

/*
 * ct2hz
 */
realT ct2hz (realT cents) {
	/* Filter fc limit: SF2.01 page 48 # 8 */
	if (cents >= 13500) {
		cents = 13500;							/* 20 kHz */
	} else if (cents < 1500) {
		cents = 1500;								/* 20 Hz */
	}
	return ct2hzReal (cents);
}

/*
 * cb2amp
 *
 * in: a value between 0 and 960, 0 is no attenuation
 * out: a value between 1 and 0
 */
realT cb2amp (realT cb) {
	/*
	 * cb: an attenuation in 'centibels' (1/10 dB)
	 * SF2.01 page 49 # 48 limits it to 144 dB.
	 * 96 dB is reasonable for 16 bit systems, 144 would make sense for 24 bit.
	 */

	/* minimum attenuation: 0 dB */
	if (cb < 0) {
		return 1.0;
	}
	if (cb >= CB_AMP_SIZE) {
		return 0.0;
	}
	return cb2ampTab[(int) cb];
}

/*
 * atten2amp
 *
 * in: a value between 0 and 1440, 0 is no attenuation
 * out: a value between 1 and 0
 *
 * Note: Volume attenuation is supposed to be centibels but EMU8k/10k don't
 * follow this.  Thats the reason for separate cb2amp and atten2amp.
 */
realT atten2amp (realT atten) {
	if (atten < 0)
		return 1.0;
	else if (atten >= ATTEN_AMP_SIZE)
		return 0.0;
	else
		return atten2ampTab[(int) atten];
}

/*
 * tc2secDelay
 */
realT tc2secDelay (realT tc) {
	/* SF2.01 section 8.1.2 items 21, 23, 25, 33
	 * SF2.01 section 8.1.3 items 21, 23, 25, 33
	 *
	 * The most negative number indicates a delay of 0. Range is limited
	 * from -12000 to 5000 */
	if (tc <= -32768.0f) {
		return (realT) 0.0f;
	};
	if (tc < -12000.) {
		tc = (realT) - 12000.0f;
	}
	if (tc > 5000.0f) {
		tc = (realT) 5000.0f;
	}
	return (realT) pow (2.0, (double) tc / 1200.0);
}

/*
 * tc2secAttack
 */
realT tc2secAttack (realT tc) {
	/* SF2.01 section 8.1.2 items 26, 34
	 * SF2.01 section 8.1.3 items 26, 34
	 * The most negative number indicates a delay of 0
	 * Range is limited from -12000 to 8000 */
	if (tc <= -32768.) {
		return (realT) 0.0;
	};
	if (tc < -12000.) {
		tc = (realT) - 12000.0;
	};
	if (tc > 8000.) {
		tc = (realT) 8000.0;
	};
	return (realT) pow (2.0, (double) tc / 1200.0);
}

/*
 * tc2sec
 */
realT tc2sec (realT tc) {
	/* No range checking here! */
	return (realT) pow (2.0, (double) tc / 1200.0);
}

/*
 * tc2secRelease
 */
realT tc2secRelease (realT tc) {
	/* SF2.01 section 8.1.2 items 30, 38
	 * SF2.01 section 8.1.3 items 30, 38
	 * No 'most negative number' rule here!
	 * Range is limited from -12000 to 8000 */
	if (tc <= -32768.) {
		return (realT) 0.0;
	};
	if (tc < -12000.) {
		tc = (realT) - 12000.0;
	};
	if (tc > 8000.) {
		tc = (realT) 8000.0;
	};
	return (realT) pow (2.0, (double) tc / 1200.0);
}

/*
 * act2hz
 *
 * Convert from absolute cents to Hertz
 */
realT act2hz (realT c) {
	return (realT) (8.176 * pow (2.0, (double) c / 1200.0));
}

/*
 * hz2ct
 *
 * Convert from Hertz to cents
 */
realT hz2ct (realT f) {
	return (realT) (6900 + 1200 * log (f / 440.0) / log (2.0));
}

/*
 * pan
 */
realT pan (realT c, int left) {
	if (left) {
		c = -c;
	}
	if (c < -500) {
		return (realT) 0.0;
	} else if (c > 500) {
		return (realT) 1.0;
	} else {
		return panTab[(int) (c + 500)];
	}
}

/*
 * concave
 */
realT concave (realT val) {
	if (val < 0) {
		return 0;
	} else if (val > 127) {
		return 1;
	}
	return concaveTab[(int) val];
}

/*
 * convex
 */
realT convex (realT val) {
	if (val < 0) {
		return 0;
	} else if (val > 127) {
		return 1;
	}
	return convexTab[(int) val];
}
