/* Synth - A Software Synthesizer
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

#ifndef _CONV_H
#define _CONV_H

#include "fluidbean.h"
#include <math.h>

/*
 Attenuation range in centibels.
 Attenuation range is the dynamic range of the volume envelope generator
 from 0 to the end of attack segment.
 fluidsynth is a 24 bit synth, it could (should??) be 144 dB of attenuation.
 However the spec makes no distinction between 16 or 24 bit synths, so use
 96 dB here.

 Note about usefulness of 24 bits:
 1)Even fluidsynth is a 24 bit synth, this format is only relevant if
 the sample format coming from the soundfont is 24 bits and the audio sample format
 chosen by the application (audio.sample.format) is not 16 bits.

 2)When the sample soundfont is 16 bits, the internal 24 bits number have
 16 bits msb and lsb to 0. Consequently, at the DAC output, the dynamic range of
 this 24 bit sample is reduced to the the dynamic of a 16 bits sample (ie 90 db)
 even if this sample is produced by the audio driver using an audio sample format
 compatible for a 24 bit DAC.

 3)When the audio sample format settings is 16 bits (audio.sample.format), the
 audio driver will make use of a 16 bit DAC, and the dynamic will be reduced to 96 dB
 even if the initial sample comes from a 24 bits soundfont.

 In both cases (2) or (3), the real dynamic range is only 96 dB.

 Other consideration for NOISE_FLOOR related to case (1),(2,3):
 - for case (1), NOISE_FLOOR should be the noise floor for 24 bits (i.e -138 dB).
 - for case (2) or (3), NOISE_FLOOR should be the noise floor for 16 bits (i.e -90 dB).
 */
#define PEAK_ATTENUATION  960.0f
#define CENTS_HZ_SIZE     1200
#define VEL_CB_SIZE       128
#define CB_AMP_SIZE       961
#define ATTEN_AMP_SIZE    1441
#define PAN_SIZE          1002

/* EMU 8k/10k don't follow spec in regards to volume attenuation.
 * This factor is used in the equation pow (10.0, cb / ATTEN_POWER_FACTOR).
 * By the standard this should be -200.0. */
/* 07/11/2008 modified by S. Christian Collins for increased velocity sensitivity.  Now it equals the response of EMU10K1 programming.*/
#define ATTEN_POWER_FACTOR  (-200.0)	/* was (-531.509) */

void conversionConfig (void);

realT ct2hzReal (realT cents);
realT ct2hz (realT cents);
realT cb2amp (realT cb);
realT atten2amp (realT atten);
realT tc2sec (realT tc);
realT tc2secDelay (realT tc);
realT tc2secAttack (realT tc);
realT tc2secRelease (realT tc);
realT act2hz (realT c);
realT hz2ct (realT c);
realT pan (realT c, S32 left);
realT concave (realT val);
realT convex (realT val);

extern realT ct2hzTab[CENTS_HZ_SIZE];
extern realT vel2cbTab[VEL_CB_SIZE];
extern realT cb2ampTab[CB_AMP_SIZE];
extern realT posbpTab[128];
extern realT concaveTab[128];
extern realT convexTab[128];
extern realT panTab[PAN_SIZE];


#endif /* _CONV_H */
